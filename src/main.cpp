#include <Arduino.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "BoardTouch.h"
#include "CompanionUi.h"
#include "SdCard.h"
#include "TlsMemory.h"

// LVGL 8 boots directly into the validated 320x240 GUI.
// Rebuild with -DCOMPANION_DIAGNOSTIC_BOOT=1 for paused staged startup.
// Serial line commands diag0=pause, diag1=LVGL without TFT SPI,
// diag2=normal LVGL/SPI. Whole lines only: a host tty that echoes the
// firmware's own output back (digits in uptime/heap) must never switch stages.
// No persistent settings, internet, OBD, drone, or camera actions.
#ifndef COMPANION_HEARTBEAT_MS
#define COMPANION_HEARTBEAT_MS 10000  // serial health line; 2000 for crash hunts
#endif

namespace {
uint32_t lastHeartbeatMs = 0;
bool uiReady = false;
char commandLine[8];
size_t commandLength = 0;
bool commandOverflow = false;

// Returns the stage for an exact "diag0".."diag2" line, otherwise -1.
int diagnosticCommand(const char* line, size_t length) {
    if (length != 5 || strncmp(line, "diag", 4) != 0) return -1;
    return line[4] >= '0' && line[4] <= '2' ? line[4] - '0' : -1;
}
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println();
    Serial.println("Freenove-DevBoard-Companion v0.2.0 / LVGL 8.3.11");

    // Before any module can open a TLS connection.
    Serial.printf("TLS memory: %s\n", TlsMemory::begin() ? "PSRAM" : "internal");
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
        Serial.println("Optional diagnostics (line + Enter): diag0=pause, diag1=LVGL/no SPI, diag2=LVGL/SPI");
    }
}

void loop() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == '\r' || c == '\n') {
            const int stage = commandOverflow ? -1 : diagnosticCommand(commandLine, commandLength);
            if (stage >= 0 && uiReady) CompanionUi::setDiagnosticStage(static_cast<uint8_t>(stage));
            commandLength = 0;
            commandOverflow = false;
        } else if (commandLength < sizeof(commandLine)) {
            commandLine[commandLength++] = static_cast<char>(c);
        } else {
            commandOverflow = true;
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
