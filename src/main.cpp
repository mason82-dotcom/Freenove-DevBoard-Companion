#include <Arduino.h>

#include "BoardTouch.h"
#include "CompanionUi.h"

// v0.2.0: six-tile LVGL shell on the validated FNK0086 CFG1 landscape display.
// Hermes, Webradio, OBD2, drone and camera are still inert placeholders.
// System status is local and read-only.
namespace {
uint32_t lastHeartbeatMs = 0;
bool uiReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("Freenove-DevBoard-Companion v0.2.0 / LVGL 8.3.11");

    const bool touchDetected = BoardTouch::begin();
    Serial.printf("FT6336U probe: %s\n", touchDetected ? "FOUND" : "MISSING");
    Serial.printf("PSRAM at boot: %u bytes free\n",
                  static_cast<unsigned>(ESP.getFreePsram()));

    uiReady = CompanionUi::begin(touchDetected);
    if (!uiReady) {
        Serial.println("FATAL: LVGL shell could not initialize");
    }
}

void loop() {
    if (uiReady) CompanionUi::poll();

    const uint32_t now = millis();
    if (now - lastHeartbeatMs >= 5000) {
        lastHeartbeatMs = now;
        Serial.printf("uptime=%lu ms free_heap=%u free_psram=%u ui=%s\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()),
                      uiReady ? "ready" : "failed");
    }
    delay(5);
}
