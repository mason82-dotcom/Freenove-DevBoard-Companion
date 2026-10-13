#include "WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#if __has_include("HermesLocalConfig.h")
#include "HermesLocalConfig.h"
#define WIFILINK_HOME 1
#else
#define WIFILINK_HOME 0
#endif
#if __has_include("Obd2LocalConfig.h")
#include "Obd2LocalConfig.h"
#endif
#if defined(OBD2_PROFILE_VEHICLE) && OBD2_PROFILE_VEHICLE
#define WIFILINK_VEHICLE 1
#else
#define WIFILINK_VEHICLE 0
#endif

namespace {
std::atomic<WifiLink::Profile> current{WifiLink::Profile::None};
SemaphoreHandle_t switchLock = nullptr;
bool started = false;

const char* name(WifiLink::Profile p) {
    return p == WifiLink::Profile::Home ? "home" :
           p == WifiLink::Profile::Vehicle ? "vehicle" : "none";
}
}  // namespace

namespace WifiLink {
bool homeConfigured() {
#if WIFILINK_HOME
    return strlen(HERMES_WIFI_SSID) > 0 && strcmp(HERMES_WIFI_SSID, "YOUR_WIFI_SSID") != 0;
#else
    return false;
#endif
}
bool vehicleConfigured() { return WIFILINK_VEHICLE != 0; }

void begin() {
    if (started || (!homeConfigured() && !vehicleConfigured())) return;
    switchLock = xSemaphoreCreateMutex();
    if (switchLock == nullptr) return;
    // Switching profiles must not rewrite credentials to flash every time.
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    started = true;
    if (homeConfigured()) request(Profile::Home);
}

bool request(Profile profile) {
    if (!started || current.load() == profile) return started;
    if ((profile == Profile::Home && !homeConfigured()) ||
        (profile == Profile::Vehicle && !vehicleConfigured())) return false;
    if (xSemaphoreTake(switchLock, pdMS_TO_TICKS(2000)) != pdTRUE) return false;
    const Profile previous = current.load();
    if (previous != profile) {
        // Mark the switch first so no worker trusts the old association.
        current.store(profile);
        if (previous != Profile::None) WiFi.disconnect(false, false);
        switch (profile) {
#if WIFILINK_HOME
            case Profile::Home: WiFi.begin(HERMES_WIFI_SSID, HERMES_WIFI_PASSWORD); break;
#endif
#if WIFILINK_VEHICLE
            case Profile::Vehicle: WiFi.begin(OBD2_WIFI_SSID, OBD2_WIFI_PASSWORD); break;
#endif
            default: break;
        }
        Serial.printf("WLAN: profile %s -> %s\n", name(previous), name(profile));
    }
    xSemaphoreGive(switchLock);
    return true;
}

Profile selected() { return current.load(); }

bool connectedTo(Profile p) {
    return started && current.load() == p && WiFi.status() == WL_CONNECTED;
}
}  // namespace WifiLink
