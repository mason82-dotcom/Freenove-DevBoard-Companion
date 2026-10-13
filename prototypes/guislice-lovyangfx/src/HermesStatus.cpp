#include "HermesStatus.h"
#include <Arduino.h>

// The SIX-MODULE production baseline is offline and unchanged by default.
// Only a local, gitignored device config can opt into Hermes HTTPS polling.
#if __has_include("HermesDeviceConfig.h")
#include "HermesDeviceConfig.h"
#endif
#ifndef HERMES_LIVE_ENABLED
#define HERMES_LIVE_ENABLED 0
#endif
#if HERMES_LIVE_ENABLED != 0 && HERMES_LIVE_ENABLED != 1
#error "HERMES_LIVE_ENABLED must be 0 or 1"
#endif

#if HERMES_LIVE_ENABLED
#if !__has_include("HermesLocalCa.h")
#error "Hermes HTTPS requires a local CA header: HermesLocalCa.h"
#endif
#include "HermesLocalCa.h"
#if !defined(HT_WIFI_SSID) || !defined(HT_WIFI_PASSWORD) || \
    !defined(HT_BRIDGE_URL) || !defined(HT_DEVICE_TOKEN)
#error "Hermes HTTPS requires locally configured WiFi, URL and DEVICE token."
#endif
#if defined(HT_DEMO_MODE) && HT_DEMO_MODE != 0
#error "Hermes HTTPS must not be enabled with HT_DEMO_MODE."
#endif
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cmath>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#endif

namespace HermesStatus {
#if HERMES_LIVE_ENABLED
namespace {
constexpr uint32_t kPollMs = 5000;
constexpr uint32_t kRetryMs = 15000;
constexpr size_t kMaxResponseBytes = 8192;
static SemaphoreHandle_t mutex = nullptr;
static TaskHandle_t workerHandle = nullptr;
static Snapshot shared;

void publish(const Snapshot& next) {
    if (mutex && xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
        shared = next;
        xSemaphoreGive(mutex);
    }
}

bool configValid() {
    const String base(HT_BRIDGE_URL);
    // Only the established dedicated Pi TLS service is authorized. No HTTP,
    // credentials in URLs, query strings, redirects or other arbitrary paths.
    if (!base.startsWith("https://") || !base.endsWith(":8766")
        || base.indexOf('@') >= 0 || base.indexOf('?') >= 0
        || base.indexOf('#') >= 0 || base.indexOf('/', 8) >= 0) return false;
    if (strlen(HT_CA_CERT) < 100 || !strlen(HT_WIFI_SSID)
        || !strlen(HT_DEVICE_TOKEN)) return false;
    return true;
}

void poll(Snapshot& next) {
    if (WiFi.status() != WL_CONNECTED) {
        next.phase = Phase::WIFI_DOWN;
        return;
    }
    // A valid UTC clock is mandatory for full TLS certificate verification.
    if (time(nullptr) < 1767225600L) {
        next.phase = Phase::CLOCK_WAIT;
        return;
    }
    if (!configValid()) {
        next.phase = Phase::CONFIG_ERROR;
        return;
    }
    WiFiClientSecure client;
    client.setCACert(HT_CA_CERT);
    HTTPClient http;
    if (!http.begin(client, String(HT_BRIDGE_URL) + "/v1/status")) {
        next.phase = Phase::HTTP_ERROR;
        return;
    }
    http.setConnectTimeout(4500);
    http.setTimeout(4500);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.addHeader("X-Hermes-Touch-Token", HT_DEVICE_TOKEN);
    http.addHeader("Accept", "application/json");
    const int result = http.GET();
    next.httpCode = result;
    if (result != HTTP_CODE_OK || http.getSize() > static_cast<int>(kMaxResponseBytes)) {
        next.phase = Phase::HTTP_ERROR;
        http.end();
        return;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(
        doc, http.getStream(), DeserializationOption::NestingLimit(8));
    http.end();
    if (err || !(doc["ok"] | false) || (doc["schema_version"] | 0) != 1) {
        next.phase = Phase::INVALID_RESPONSE;
        return;
    }
    const JsonVariant api = doc["hermes"]["api_healthy"];
    const JsonVariant dash = doc["hermes"]["dashboard_status_available"];
    const JsonVariant gateway = doc["hermes"]["gateway_running"];
    const JsonVariant probe = doc["hermes"]["probe_fresh"];
    if (!api.is<bool>() || !dash.is<bool>() || !probe.is<bool>()) {
        next.phase = Phase::INVALID_RESPONSE;
        return;
    }

    const JsonVariant temp = doc["system"]["cpu_temp_c"];
    const JsonVariant ram = doc["system"]["ram_used_pct"];
    const JsonVariant load = doc["system"]["load1"];
    const float t = temp.as<float>();
    const float r = ram.as<float>();
    const float l = load.as<float>();
    next.piTempValid = !temp.isNull() && std::isfinite(t) && t >= -20 && t <= 150;
    next.piRamValid = !ram.isNull() && std::isfinite(r) && r >= 0 && r <= 100;
    next.piLoadValid = !load.isNull() && std::isfinite(l) && l >= 0 && l < 1000;
    next.piTempC = next.piTempValid ? t : 0;
    next.piRamPct = next.piRamValid ? r : 0;
    next.piLoad = next.piLoadValid ? l : 0;
    next.apiHealthy = api.as<bool>();
    next.dashboardAvailable = dash.as<bool>();
    next.gatewayKnown = gateway.is<bool>();
    next.gatewayRunning = next.gatewayKnown && gateway.as<bool>();
    next.probeFresh = probe.as<bool>();
    next.lastSuccessMs = millis();
    next.haveSuccess = true;
    ++next.sequence;
    next.phase = Phase::READY;
}

void task(void*) {
    Snapshot state;
    state.phase = Phase::STARTING;
    publish(state);
    WiFi.mode(WIFI_STA);
    WiFi.begin(HT_WIFI_SSID, HT_WIFI_PASSWORD);
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    uint32_t lastRetry = millis();
    for (;;) {
        poll(state);
        publish(state);
        if (WiFi.status() != WL_CONNECTED
            && static_cast<uint32_t>(millis() - lastRetry) >= kRetryMs) {
            WiFi.reconnect();
            lastRetry = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(kPollMs));
    }
}
} // namespace
#endif

bool enabled() {
#if HERMES_LIVE_ENABLED
    return true;
#else
    return false;
#endif
}

bool begin() {
#if HERMES_LIVE_ENABLED
    if (!configValid()) return false;
    if (mutex || workerHandle) return false;
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return false;
    shared.phase = Phase::STARTING;
    if (xTaskCreate(task, "hermes-read", 14336, nullptr, 1,
                    &workerHandle) != pdPASS) {
        shared.phase = Phase::TASK_FAILED;
        workerHandle = nullptr;
        return false;
    }
    return true;
#else
    // This build never starts a Wi-Fi/TLS task.
    return true;
#endif
}

bool copy(Snapshot& out) {
#if HERMES_LIVE_ENABLED
    if (!mutex || xSemaphoreTake(mutex, 0) != pdTRUE) return false;
    out = shared;
    xSemaphoreGive(mutex);
    return true;
#else
    out = Snapshot{};
    return true;
#endif
}
}  // namespace HermesStatus
