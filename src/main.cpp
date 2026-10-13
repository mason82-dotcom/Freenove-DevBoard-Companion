#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BoardTouch.h"
#include "CompanionUi.h"
#include "SdCard.h"

// LVGL 8 boots directly into the validated 320x240 GUI.
// Rebuild with -DCOMPANION_DIAGNOSTIC_BOOT=1 for paused staged startup.
// Serial commands 0=pause, 1=LVGL without TFT SPI, 2=normal LVGL/SPI.
// No persistent settings, internet, OBD, drone, or camera actions.
#ifndef COMPANION_HEARTBEAT_MS
#define COMPANION_HEARTBEAT_MS 10000  // serial health line; 2000 for crash hunts
#endif

namespace {
uint32_t lastHeartbeatMs = 0;
bool uiReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println();
    Serial.println("Freenove-DevBoard-Companion v0.2.0 / LVGL 8.3.11");

    // Global SD card for all modules (FTP, future logging/recording).
    SdCard::begin();
    const bool touchDetected = BoardTouch::begin();
    Serial.printf("FT6336U probe: %s\n", touchDetected ? "FOUND" : "MISSING");
    Serial.printf("PSRAM at boot: %u bytes free\n",
                  static_cast<unsigned>(ESP.getFreePsram()));

    uiReady = CompanionUi::begin(touchDetected);
    if (!uiReady) {
        Serial.println("FATAL: LVGL shell could not initialize");
    } else {
        Serial.printf("UI BOOT: stage %u (%s)\n",
                      static_cast<unsigned>(CompanionUi::diagnosticStage()),
                      CompanionUi::diagnosticStage() == 2
                          ? "normal LVGL/TFT operation" : "diagnostic pause");
        Serial.println("Optional diagnostics: 0=pause, 1=LVGL/no SPI, 2=LVGL/SPI");
    }
}

void loop() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c >= '0' && c <= '2' && uiReady) {
            CompanionUi::setDiagnosticStage(static_cast<uint8_t>(c - '0'));
        }
    }

    if (uiReady) CompanionUi::poll();

    const uint32_t now = millis();
    if (now - lastHeartbeatMs >= COMPANION_HEARTBEAT_MS) {
        lastHeartbeatMs = now;
        Serial.printf(
            "DIAG heartbeat: uptime=%lu ms stage=%u heap=%u psram=%u loop_stack_hwm=%u\n",
            static_cast<unsigned long>(now),
            uiReady ? static_cast<unsigned>(CompanionUi::diagnosticStage()) : 255U,
            static_cast<unsigned>(ESP.getFreeHeap()),
            static_cast<unsigned>(ESP.getFreePsram()),
            static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    }
    delay(5);
}
