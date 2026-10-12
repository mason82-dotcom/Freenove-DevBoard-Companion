#pragma once
#include <stdint.h>

namespace CompanionUi {
// Initializes ST7789/CFG1, LVGL 8.3.11, and the six-tile navigation shell.
// Returns false if the display dimensions or LVGL driver registration fail.
bool begin(bool touchPresent);

// Diagnostic boot: stage 0 pauses LVGL, 1 runs LVGL without TFT SPI writes,
// 2 enables normal TFT SPI output. Touch becomes active only at stage 2.
void setDiagnosticStage(uint8_t stage);
uint8_t diagnosticStage();
void poll();
}
