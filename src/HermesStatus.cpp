#include "HermesStatus.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <memory>
#include <new>

#if __has_include("HermesLocalConfig.h")
#include "HermesLocalConfig.h"
#define HERMES_LOCAL_CONFIG_PRESENT 1
#else
#define HERMES_LOCAL_CONFIG_PRESENT 0
#endif

namespace {
HermesStatus::Snapshot current;
portMUX_TYPE snapshotLock = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t workerHandle = nullptr;
constexpr uint32_t kPollMs = 5000;
constexpr size_t kMaxBody = 16384;
// Arduino-ESP32 3.x: connect/read timeouts are milliseconds, the TLS
// handshake timeout is seconds (default 120 s would stall the worker).
constexpr int32_t kConnectTimeoutMs = 4000;
constexpr uint16_t kReadTimeoutMs = 4000;
constexpr unsigned long kHandshakeTimeoutS = 8;
constexpr uint8_t kLogEveryPolls = 12;  // unchanged success: ~1 line/minute

void publish(const HermesStatus::Snapshot& s) {
    portENTER_CRITICAL(&snapshotLock);
    current = s;
    portEXIT_CRITICAL(&snapshotLock);
}

#if HERMES_LOCAL_CONFIG_PRESENT
bool configured() {
    return strlen(HERMES_WIFI_SSID) > 0 &&
           strcmp(HERMES_WIFI_SSID, "YOUR_WIFI_SSID") != 0 &&
           strlen(HERMES_DEVICE_TOKEN) >= 24 &&
           strncmp(HERMES_DEVICE_TOKEN, "YOUR_", 5) != 0 &&
           strstr(HERMES_CA_PEM, "REPLACE_WITH_") == nullptr &&
           strstr(HERMES_CA_PEM, "BEGIN CERTIFICATE") != nullptr;
}

bool getBool(JsonVariantConst v) {
    return v.is<bool>() && v.as<bool>();
}

bool isNumber(JsonVariantConst v) {
    return v.is<float>() || v.is<int>();
}

// Applies a validated schema-v1 document to s; false leaves s untouched.
bool applyDocument(const JsonDocument& doc, HermesStatus::Snapshot& s) {
    if (!getBool(doc["ok"]) || doc["schema_version"].as<int>() != 1) {
        Serial.printf("Hermes JSON: schema_invalid ok=%d version=%d\n",
                      static_cast<int>(getBool(doc["ok"])), doc["schema_version"].as<int>());
        return false;
    }
    JsonVariantConst sys = doc["system"];
    JsonVariantConst hermes = doc["hermes"];
    if (!sys.is<JsonObjectConst>() || !hermes.is<JsonObjectConst>()) {
        Serial.println("Hermes JSON: system/hermes object missing");
        return false;
    }
    s.valid = true;
    s.fresh = getBool(hermes["probe_fresh"]);
    s.apiReachable = getBool(hermes["api_reachable"]);
    s.apiHealthy = getBool(hermes["api_healthy"]);
    s.dashboardReachable = getBool(hermes["dashboard_reachable"]);
    s.gatewayKnown = hermes["gateway_running"].is<bool>();
    s.gatewayRunning = getBool(hermes["gateway_running"]);
    s.chatEnabled = getBool(doc["chat"]["enabled"]);
    s.hasCpuTemp = isNumber(sys["cpu_temp_c"]);
    s.hasRam = isNumber(sys["ram_used_pct"]);
    s.hasLoad = isNumber(sys["load1"]);
    s.cpuTempC = s.hasCpuTemp ? sys["cpu_temp_c"].as<float>() : 0;
    s.ramUsedPct = s.hasRam ? sys["ram_used_pct"].as<float>() : 0;
    s.load1 = s.hasLoad ? sys["load1"].as<float>() : 0;
    s.probeAgeS = hermes["probe_age_s"].isNull() ? 0 : hermes["probe_age_s"].as<float>();
    s.updatedAtMs = millis();
    return true;
}

// One bounded HTTPS GET. Returns true only for a schema-valid response.
bool fetch(HermesStatus::Snapshot& s, uint32_t& endMs) {
    // Declaration order matters: http (holding a client reference) is
    // destroyed before client.
    auto client = std::unique_ptr<WiFiClientSecure>(new (std::nothrow) WiFiClientSecure());
    auto http = std::unique_ptr<HTTPClient>(new (std::nothrow) HTTPClient());
    if (!client || !http) {
        Serial.println("Hermes: network object allocation failed");
        s.httpCode = HTTPC_ERROR_TOO_LESS_RAM;
        return false;
    }
    client->setCACert(HERMES_CA_PEM);
    client->setHandshakeTimeout(kHandshakeTimeoutS);
    const String url = String("https://") + HERMES_SERVER_HOST + ":" +
                       String(HERMES_SERVER_PORT) + "/v1/status";
    if (!http->begin(*client, url)) {
        s.httpCode = HTTPC_ERROR_CONNECTION_REFUSED;
        return false;
    }
    http->setConnectTimeout(kConnectTimeoutMs);
    http->setTimeout(kReadTimeoutMs);
    // The bridge is polled periodically; explicitly disable HTTP keep-alive.
    // This avoids reusing a TLS socket the server may already have closed.
    http->setReuse(false);
    http->setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http->addHeader("X-Hermes-Touch-Token", HERMES_DEVICE_TOKEN);
    http->addHeader("Accept", "application/json");
    s.httpCode = http->GET();
    bool ok = false;
    if (s.httpCode == HTTP_CODE_OK) {
        const int length = http->getSize();
        if (length <= 0 || length > static_cast<int>(kMaxBody)) {
            // Reject unknown/chunked lengths; never parse an unbounded stream.
            Serial.printf("Hermes RX: unsupported content length=%d (max %u)\n",
                          length, static_cast<unsigned>(kMaxBody));
        } else {
            // A fixed-size input buffer caps memory regardless of response data.
            auto body = std::unique_ptr<char[]>(new (std::nothrow) char[static_cast<size_t>(length) + 1]);
            if (!body) {
                Serial.println("Hermes: response buffer allocation failed");
            } else {
                Stream& stream = http->getStream();
                const size_t received = stream.readBytes(body.get(), static_cast<size_t>(length));
                body[received] = 0;
                if (received != static_cast<size_t>(length)) {
                    Serial.printf("Hermes RX: expected=%d received=%u (incomplete)\n",
                                  length, static_cast<unsigned>(received));
                } else {
                    JsonDocument doc;
                    const DeserializationError error = deserializeJson(
                        doc, body.get(), received, DeserializationOption::NestingLimit(6));
                    if (error) {
                        Serial.printf("Hermes JSON: parse_error=%s length=%d\n",
                                      error.c_str(), length);
                    } else {
                        ok = applyDocument(doc, s);
                    }
                }
            }
        }
    }
    const uint32_t endStart = millis();
    http->end();
    endMs = millis() - endStart;
    return ok;
}

void poll(HermesStatus::Snapshot& s, uint8_t& quietPolls) {
    const bool wasConnected = s.connected;
    const int previousCode = s.httpCode;
    s.connected = WiFi.status() == WL_CONNECTED;
    s.rssi = s.connected ? static_cast<int8_t>(WiFi.RSSI()) : 0;
    if (!s.connected) {
        if (wasConnected) Serial.println("Hermes: WLAN getrennt");
        publish(s);
        return;
    }
    uint32_t endMs = 0;
    if (fetch(s, endMs)) s.failures = 0;
    else if (s.failures < UINT16_MAX) ++s.failures;
    publish(s);

    // Log every failure and every change; unchanged success only periodically.
    const bool changed = !wasConnected || s.httpCode != previousCode;
    if (changed || s.failures > 0 || ++quietPolls >= kLogEveryPolls) {
        quietPolls = 0;
        Serial.printf("Hermes: http=%d ok=%d fails=%u fresh=%d rssi=%d end_ms=%lu "
                      "stack_free=%u heap=%u\n",
                      s.httpCode, static_cast<int>(s.failures == 0),
                      static_cast<unsigned>(s.failures), static_cast<int>(s.fresh),
                      static_cast<int>(s.rssi), static_cast<unsigned long>(endMs),
                      static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                      static_cast<unsigned>(ESP.getFreeHeap()));
    }
}

void worker(void*) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(HERMES_WIFI_SSID, HERMES_WIFI_PASSWORD);
    // Persistent worker state: the last valid values survive later failures
    // and are aged out by HermesStatus::format() via updatedAtMs.
    HermesStatus::Snapshot state;
    state.configured = true;
    uint8_t quietPolls = 0;
    for (;;) {
        // WiFi.begin() and auto-reconnect manage association asynchronously.
        // Never force a reconnect every poll while STA is still associating.
        poll(state, quietPolls);
        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}
#endif
}

namespace HermesStatus {
void begin() {
#if HERMES_LOCAL_CONFIG_PRESENT
    if (!configured()) return;
    if (workerHandle != nullptr) return;
    BaseType_t result = xTaskCreatePinnedToCore(worker, "hermes-status", 12288,
                                                 nullptr, 1, &workerHandle, 0);
    if (result != pdPASS) workerHandle = nullptr;
#endif
}

Snapshot read() {
    Snapshot s;
    portENTER_CRITICAL(&snapshotLock);
    s = current;
    portEXIT_CRITICAL(&snapshotLock);
#if HERMES_LOCAL_CONFIG_PRESENT
    s.configured = configured();
#endif
    return s;
}
}
