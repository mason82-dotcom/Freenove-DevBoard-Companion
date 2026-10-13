#pragma once
#include <stdint.h>

namespace CompanionUi {
// Initializes the verified ST7789/CFG1 landscape display, LVGL 8.3 and
// the six-module read-only shell. Returns false on registration failure.
// Normal firmware enters stage 2 automatically. To pause at startup,
// compile with -DCOMPANION_DIAGNOSTIC_BOOT=1.
bool begin(bool touchPresent);

// Optional, non-persistent diagnostics:
// 0: pause LVGL; 1: LVGL rendering without TFT SPI; 2: normal GUI/SPI.
void setDiagnosticStage(uint8_t stage);
uint8_t diagnosticStage();
void poll();
}
