#pragma once
// FNK0086 EXPERIMENT: LovyanGFX owns ST7789 SPI, not TFT_eSPI.
// Read ONLY the hardware-verified CFG1 macros from VerifiedDisplaySetup.h.
// No CFG2 support, board autodetection, touch-register writes, or fallbacks.
#include <LovyanGFX.hpp>
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

// Name "LGFX" is required by LovyanGFX's upstream TFT_eSPI alias header.
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 panel_;
    lgfx::Bus_SPI bus_;

public:
    LGFX() {
        auto busCfg = bus_.config();
        busCfg.spi_host = SPI2_HOST;
        busCfg.spi_mode = 0;
#ifdef SPI_FREQUENCY
        busCfg.freq_write = SPI_FREQUENCY;
#else
        busCfg.freq_write = 27000000; // Cautious prototype fallback; not CFG2.
#endif
        busCfg.freq_read = 8000000;
        busCfg.pin_sclk = TFT_SCLK;
        busCfg.pin_mosi = TFT_MOSI;
        busCfg.pin_miso = -1;    // Write-only ST7789
        busCfg.pin_dc = TFT_DC;
        busCfg.spi_3wire = false;
        busCfg.use_lock = true;
        busCfg.dma_channel = 0; // Start without DMA; measure/enable later.
        bus_.config(busCfg);
        panel_.setBus(&bus_);

        auto panelCfg = panel_.config();
#ifdef TFT_CS
        panelCfg.pin_cs = TFT_CS;
#else
        panelCfg.pin_cs = -1;   // On verified FNK0086 the display CS is grounded.
#endif
#ifdef TFT_RST
        panelCfg.pin_rst = TFT_RST;
#else
        panelCfg.pin_rst = -1;
#endif
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
#ifdef TFT_INVERSION_ON
        panelCfg.invert = true;
#elif defined(TFT_INVERSION_OFF)
        panelCfg.invert = false;
#endif
#if defined(TFT_RGB_ORDER) && defined(TFT_BGR)
        panelCfg.rgb_order = (TFT_RGB_ORDER == TFT_BGR);
#endif
        panel_.config(panelCfg);
        setPanel(&panel_);
    }
};
