#pragma once
// GUIslice 0.17.2 requests <TFT_eSPI.h> from its upstream display backend.
// Do NOT link bodmer/TFT_eSPI: map that interface to LovyanGFX's maintained
// TFT_eSPI-compatible API instead; LovyanGFX is the sole SPI driver here.
// Compatibility is experimental and must pass a full PlatformIO build.
#include "LGFX_FNK0086.hpp"
#include <LGFX_TFT_eSPI.hpp>
