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
constexpr uint32_t kStaleMs = 15000;
constexpr size_t kMaxBody = 16384;

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

void query() {
    HermesStatus::Snapshot s;
    s.configured = true;
    s.connected = WiFi.status() == WL_CONNECTED;
    if (!s.connected) {
        publish(s);
        return;
    }
    auto client = std::unique_ptr<WiFiClientSecure>(new (std::nothrow) WiFiClientSecure());
    auto http = std::unique_ptr<HTTPClient>(new (std::nothrow) HTTPClient());
    if (!client || !http) {
        Serial.println("Hermes: network object allocation failed");
        publish(s);
        return;
    }
    client->setCACert(HERMES_CA_PEM);
    client->setTimeout(4);
    const String url = String("https://") + HERMES_SERVER_HOST + ":" +
                       String(HERMES_SERVER_PORT) + "/v1/status";
    if (!http->begin(*client, url)) {
        publish(s);
        return;
    }
    http->setTimeout(4000);
    http->setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http->addHeader("X-Hermes-Touch-Token", HERMES_DEVICE_TOKEN);
    http.addHeader("Accept", "application/json");
    s.httpCode = http->GET();
    if (s.httpCode == HTTP_CODE_OK) {
        const int length = http->getSize();
        if (length > 0 && length <= static_cast<int>(kMaxBody)) {
            // Reject unknown/chunked lengths; never parse an unbounded stream.
            // A fixed-size input buffer caps memory regardless of response data.
            auto body = std::unique_ptr<char[]>(new (std::nothrow) char[static_cast<size_t>(length) + 1]);
            if (!body) {
                Serial.println("Hermes: response buffer allocation failed");
                http->end();
                publish(s);
                return;
            }
            Stream& stream = http->getStream();
            const size_t received = stream.readBytes(body.get(), static_cast<size_t>(length));
            body[received] = 0;
            JsonDocument doc;
            DeserializationError error = received == static_cast<size_t>(length)
                ? deserializeJson(doc, body.get(), received, DeserializationOption::NestingLimit(6))
                : DeserializationError::IncompleteInput;
            if (!error && getBool(doc["ok"]) && doc["schema_version"].as<int>() == 1) {
                JsonVariantConst sys = doc["system"];
                JsonVariantConst hermes = doc["hermes"];
                if (sys.is<JsonObjectConst>() && hermes.is<JsonObjectConst>()) {
                    s.valid = true;
                    s.fresh = getBool(hermes["probe_fresh"]);
                    s.apiReachable = getBool(hermes["api_reachable"]);
                    s.apiHealthy = getBool(hermes["api_healthy"]);
                    s.dashboardReachable = getBool(hermes["dashboard_reachable"]);
                    s.gatewayKnown = hermes["gateway_running"].is<bool>();
                    s.gatewayRunning = getBool(hermes["gateway_running"]);
                    s.chatEnabled = getBool(doc["chat"]["enabled"]);
                    s.hasCpuTemp = sys["cpu_temp_c"].is<float>() || sys["cpu_temp_c"].is<int>();
                    s.hasRam = sys["ram_used_pct"].is<float>() || sys["ram_used_pct"].is<int>();
                    s.hasLoad = sys["load1"].is<float>() || sys["load1"].is<int>();
                    if (s.hasCpuTemp) s.cpuTempC = sys["cpu_temp_c"].as<float>();
                    if (s.hasRam) s.ramUsedPct = sys["ram_used_pct"].as<float>();
                    if (s.hasLoad) s.load1 = sys["load1"].as<float>();
                    s.probeAgeS = hermes["probe_age_s"].isNull() ? 0 :
                                  hermes["probe_age_s"].as<float>();
                    s.updatedAtMs = millis();
                }
            }
        }
    }
    http->end();
    Serial.printf("Hermes: wifi=%d http=%d valid=%d fresh=%d stack_free=%u heap=%u\\n",
                  static_cast<int>(s.connected), s.httpCode, static_cast<int>(s.valid),
                  static_cast<int>(s.fresh),
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    publish(s);
}

void worker(void*) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(HERMES_WIFI_SSID, HERMES_WIFI_PASSWORD);
    for (;;) {
        // WiFi.begin() and auto-reconnect manage association asynchronously.
        // Do not call WiFi.reconnect() every 5 seconds while STA is connecting.
        query();
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
    if (s.valid && millis() - s.updatedAtMs > kStaleMs) {
        s.valid = false;
        s.fresh = false;
        s.apiHealthy = false;
        s.gatewayKnown = false;
    }
    return s;
}
}
