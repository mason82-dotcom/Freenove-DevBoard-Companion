#pragma once
// FNK0086 EXPERIMENT: LovyanGFX owns ST7789 SPI, not TFT_eSPI.
// Read ONLY the hardware-verified CFG1 macros from VerifiedDisplaySetup.h.
// No CFG2 support, board autodetection, touch-register writes, or fallbacks.
// Inspect the user's verified source macros BEFORE importing the independent
// LovyanGFX header tree. This prevents conflicts with any library internals.
#include <VerifiedDisplaySetup.h>

#ifndef ST7789_DRIVER
#error "The verified FNK0086 CFG1 must select ST7789_DRIVER."
#endif
// The user's SHA256-verified Freenove CFG1 header does not define
// TFT_WIDTH or TFT_HEIGHT. The real panel resolution, 240x320 portrait,
// was verified on hardware. Validate dimensions IF explicitly provided,
// but do not require invented definitions absent from the actual CFG1.
#if defined(TFT_WIDTH) && TFT_WIDTH != 240
#error "FNK0086 CFG1 TFT_WIDTH must be 240 when specified."
#endif
#if defined(TFT_HEIGHT) && TFT_HEIGHT != 320
#error "FNK0086 CFG1 TFT_HEIGHT must be 320 when specified."
#endif
#if !defined(TFT_MOSI) || !defined(TFT_SCLK) || !defined(TFT_DC)
#error "CFG1 header must declare the verified SPI MOSI, SCLK and DC pins."
#endif
#if TFT_MOSI != 20 || TFT_SCLK != 21 || TFT_DC != 0
#error "FNK0086 validated wiring expected: MOSI=20, SCLK=21, DC=0."
#endif

// Snapshot the verified TFT_eSPI-style CFG1 settings before LovyanGFX's
// includes. Do not modify the locally copied, untracked vendor setup file.
namespace FnkVerifiedCfg1 {
constexpr int kMosi = TFT_MOSI;
constexpr int kSclk = TFT_SCLK;
constexpr int kDc = TFT_DC;
#ifdef TFT_CS
constexpr int kCs = TFT_CS;
#else
constexpr int kCs = -1;  // Verified board has display CS grounded.
#endif
#ifdef TFT_RST
constexpr int kRst = TFT_RST;
#else
constexpr int kRst = -1;
#endif
#ifdef SPI_FREQUENCY
constexpr unsigned kSpiWriteHz = SPI_FREQUENCY;
#else
constexpr unsigned kSpiWriteHz = 27000000;
#endif
#ifdef TFT_INVERSION_ON
constexpr bool kInvert = true;
#elif defined(TFT_INVERSION_OFF)
constexpr bool kInvert = false;
#else
constexpr bool kInvert = false;
#endif
// TFT_eSPI ST7789 convention: TFT_RGB=1, TFT_BGR=0.
// The real Freenove CFG1 uses "#define TFT_RGB_ORDER TFT_BGR" but TFT_BGR
// itself is not defined in that standalone setup file. In a preprocessor
// #if expression its value is 0, exactly as TFT_eSPI's ST7789 driver reads it.
// LovyanGFX's rgb_order=true selects BGR (red/blue exchanged).
#if defined(TFT_RGB_ORDER)
  #if TFT_RGB_ORDER == 1
constexpr bool kRgbOrder = false; // RGB
  #else
constexpr bool kRgbOrder = true;  // BGR
  #endif
#else
constexpr bool kRgbOrder = false;
#endif

// The verified TFT_eSPI configuration calls the ST7789 in SPI mode 3.
// Start LovyanGFX at a conservative <=40 MHz while validating its new driver.
// This does NOT change the SHA256-verified CFG1 header (80 MHz remains there).
constexpr unsigned kSafeFirstBootHz =
    kSpiWriteHz <= 40000000U ? kSpiWriteHz : 40000000U;
}

#include <LovyanGFX.hpp>

// Name "LGFX" is required by LovyanGFX's upstream TFT_eSPI alias header.
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_SPI bus_;

public:
    LGFX() {
        auto busCfg = bus_.config();
        busCfg.spi_host = SPI2_HOST;
        busCfg.spi_mode = 3; // TFT_eSPI ST7789 uses SPI_MODE3 by default.
        busCfg.freq_write = FnkVerifiedCfg1::kSafeFirstBootHz;
        busCfg.freq_read = 8000000;
        busCfg.pin_sclk = FnkVerifiedCfg1::kSclk;
        busCfg.pin_mosi = FnkVerifiedCfg1::kMosi;
        busCfg.pin_miso = -1;    // Write-only ST7789
        busCfg.pin_dc = FnkVerifiedCfg1::kDc;
        busCfg.spi_3wire = false;
        busCfg.use_lock = true;
        busCfg.dma_channel = 0; // Start without DMA; measure/enable later.
        bus_.config(busCfg);
        panel_.setBus(&bus_);

        auto panelCfg = panel_.config();
        panelCfg.pin_cs = FnkVerifiedCfg1::kCs;
        panelCfg.pin_rst = FnkVerifiedCfg1::kRst;
        panelCfg.pin_busy = -1;
        panelCfg.panel_width = 240;
        panelCfg.panel_height = 320;
        panelCfg.memory_width = 240;
        panelCfg.memory_height = 320;
        panelCfg.offset_x = 0;
        panelCfg.offset_y = 0;
        panelCfg.offset_rotation = 0;
        panelCfg.readable = false;
        panelCfg.bus_shared = false;
        panelCfg.invert = FnkVerifiedCfg1::kInvert;
        panelCfg.rgb_order = FnkVerifiedCfg1::kRgbOrder;
        panel_.config(panelCfg);
        setPanel(&panel_);
    }
};
