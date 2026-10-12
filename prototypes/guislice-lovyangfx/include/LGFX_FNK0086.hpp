#pragma once
// FNK0086 EXPERIMENT: LovyanGFX owns ST7789 SPI, not TFT_eSPI.
// Read ONLY the hardware-verified CFG1 macros from VerifiedDisplaySetup.h.
// No CFG2 support, board autodetection, touch-register writes, or fallbacks.
// Inspect the user's verified source macros BEFORE importing the independent
// LovyanGFX header tree. This prevents conflicts with any library internals.
#include <VerifiedDisplaySetup.h>

#if !defined(ST7789_DRIVER) || !defined(TFT_WIDTH) || !defined(TFT_HEIGHT)
#error "Only the verified FNK0086 ST7789 CFG1 setup can be used."
#endif
#if TFT_WIDTH != 240 || TFT_HEIGHT != 320
#error "This experiment requires a verified 240x320 portrait ST7789 panel."
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
#if defined(TFT_RGB_ORDER) && defined(TFT_BGR)
constexpr bool kRgbOrder = TFT_RGB_ORDER == TFT_BGR;
#else
constexpr bool kRgbOrder = false;
#endif
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
        busCfg.spi_mode = 0;
        busCfg.freq_write = FnkVerifiedCfg1::kSpiWriteHz;
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
