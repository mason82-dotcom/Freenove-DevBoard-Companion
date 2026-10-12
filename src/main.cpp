#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BoardTouch.h"
#include "CompanionUi.h"

// v0.2.0 crash triage: safely halt LVGL before the first timer/flush.
// Commands on USB serial: 0=pause, 1=LVGL without TFT SPI, 2=LVGL with TFT SPI.
// No persistent settings, internet, OBD, drone, or camera actions.
namespace {
uint32_t lastHeartbeatMs = 0;
bool uiReady = false;
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println();
    Serial.println("Freenove-DevBoard-Companion v0.2.0 / LVGL DIAGNOSTIC");

    const bool touchDetected = BoardTouch::begin();
    Serial.printf("FT6336U probe: %s\n", touchDetected ? "FOUND" : "MISSING");
    Serial.printf("PSRAM at boot: %u bytes free\n",
                  static_cast<unsigned>(ESP.getFreePsram()));

    uiReady = CompanionUi::begin(touchDetected);
    if (!uiReady) {
        Serial.println("FATAL: LVGL shell could not initialize");
    } else {
        Serial.println("DIAG BOOT: stage 0, LVGL paused");
        Serial.println("DIAG COMMANDS: 0=pause, 1=LVGL/no SPI, 2=LVGL/SPI");
        Serial.println("Send 1, observe stability; only then send 2.");
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
    if (now - lastHeartbeatMs >= 2000) {
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
