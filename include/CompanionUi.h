#pragma once

namespace CompanionUi {
// Initializes ST7789/CFG1, LVGL 8.3.11, and the six-tile navigation shell.
// Returns false if the display dimensions or LVGL driver registration fail.
bool begin(bool touchPresent);
void poll();
}
