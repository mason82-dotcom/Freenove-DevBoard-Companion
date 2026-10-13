#pragma once
#include <stdint.h>
#include <GUIslice.h>
#include "CompanionGuiModules.h"

// Native GUIslice drawn widgets; only the GUI loop may draw to the ST7789.
// No second TFT_eSPI/LovyanGFX display instance is created.
namespace ModuleGraphics {
void bind(uint8_t index, gslc_tsElemRef* canvas);
void bindPulseWave(gslc_tsElemRef* canvas);
void refreshPulse(gslc_tsGui* gui);
void resetPulseHeart();
void sampleSystem(uint32_t freeHeap);
bool draw(void* gui, void* elemRef, gslc_teRedrawType redraw);
void refresh(gslc_tsGui* gui, uint8_t index);
}
