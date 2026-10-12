"""Safeguard the isolated GUIslice/LovyanGFX experiment.

These tests do NOT replace the full ESP32-S3 PlatformIO build or hardware test.
"""
import configparser
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROTO = ROOT / "prototypes" / "guislice-lovyangfx"


class GUIsliceLovyanPrototypeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ini = configparser.ConfigParser(interpolation=None)
        cls.ini.read(ROOT / "platformio.guislice-lgfx.ini")
        cls.cfg = (PROTO / "include" / "GUIslice_FNK0086.h").read_text()
        cls.device = (PROTO / "include" / "LGFX_FNK0086.hpp").read_text()
        cls.alias = (PROTO / "include" / "TFT_eSPI.h").read_text()
        cls.main = (PROTO / "src" / "main.cpp").read_text()

    def test_fully_isolated_and_pinned_no_tft_espi_dependency(self):
        env = self.ini["env:fnk0086_lgfx"]
        self.assertEqual(self.ini["platformio"]["src_dir"],
                         "prototypes/guislice-lovyangfx/src")
        self.assertIn("lovyan03/LovyanGFX@1.2.32", env["lib_deps"])
        self.assertIn("impulseadventure/GUIslice@0.17.2", env["lib_deps"])
        self.assertNotIn("bodmer/TFT_eSPI", env["lib_deps"])
        self.assertNotIn("lvgl/lvgl@", env["lib_deps"])
        self.assertIn("-include include/VerifiedDisplaySetup.h", env["build_flags"])
        self.assertIn("-DUSER_CONFIG_LOADED=1", env["build_flags"])
        self.assertIn("55.03.39/platform-espressif32.zip", env["platform"])
        self.assertEqual(env["board_build.flash_size"], "8MB")

    def test_lovyangfx_is_only_display_driver(self):
        for value in ("#include <LGFX_TFT_eSPI.hpp>",
                      '#include "LGFX_FNK0086.hpp"'):
            self.assertIn(value, self.alias)
        for text in ("class LGFX : public lgfx::LGFX_Device",
                     "lgfx::Panel_ST7789", "lgfx::Bus_SPI",
                     "#include <VerifiedDisplaySetup.h>",
                     "TFT_MOSI != 20", "TFT_SCLK != 21",
                     "TFT_DC != 0", "setPanel(&panel_)"):
            self.assertIn(text, self.device)

    def test_real_verified_cfg1_without_tft_dimension_macros(self):
        # SHA256-identical headers in the user's LVGL and GUIslice worktrees
        # contain ST7789_DRIVER and the known GPIOs, but do NOT define
        # TFT_WIDTH / TFT_HEIGHT. They must remain optional, with validation
        # if an explicit dimension is supplied by a different header.
        self.assertIn("#ifndef ST7789_DRIVER", self.device)
        self.assertIn("#if defined(TFT_WIDTH) && TFT_WIDTH != 240", self.device)
        self.assertIn("#if defined(TFT_HEIGHT) && TFT_HEIGHT != 320", self.device)
        self.assertNotIn("!defined(TFT_WIDTH)", self.device)
        self.assertNotIn("!defined(TFT_HEIGHT)", self.device)
        self.assertIn("panelCfg.panel_width = 240;", self.device)
        self.assertIn("panelCfg.panel_height = 320;", self.device)

        workflow = (ROOT / ".github" / "workflows" /
                    "guislice-lovyangfx.yml").read_text(encoding="utf-8")
        self.assertIn("#define ST7789_DRIVER", workflow)
        self.assertNotIn("#define TFT_WIDTH", workflow)
        self.assertNotIn("#define TFT_HEIGHT", workflow)
        self.assertIn("#define SPI_FREQUENCY 80000000", workflow)

    def test_st7789_first_boot_spi_mode_color_order_and_clock(self):
        # The actual vendor CFG1 has TFT_RGB_ORDER=TFT_BGR and
        # TFT_INVERSION_OFF; TFT_BGR is not a standalone numeric macro.
        self.assertIn("busCfg.spi_mode = 3;", self.device)
        self.assertIn("#if TFT_RGB_ORDER == 1", self.device)
        self.assertIn("constexpr bool kRgbOrder = true;  // BGR", self.device)
        self.assertIn("panelCfg.rgb_order = FnkVerifiedCfg1::kRgbOrder;",
                      self.device)
        self.assertIn("panelCfg.invert = FnkVerifiedCfg1::kInvert;",
                      self.device)
        self.assertIn("kSpiWriteHz <= 40000000U ? kSpiWriteHz : 40000000U",
                      self.device)
        self.assertIn("busCfg.freq_write = FnkVerifiedCfg1::kSafeFirstBootHz;",
                      self.device)
        workflow = (ROOT / ".github" / "workflows" /
                    "guislice-lovyangfx.yml").read_text(encoding="utf-8")
        self.assertIn("#define TFT_RGB_ORDER TFT_BGR", workflow)
        self.assertIn("#define TFT_INVERSION_OFF", workflow)

    def test_system_live_values_trigger_redraw(self):
        # GUIslice uses strncmp(element_buffer, incoming_text) before copying
        # and setting GSLC_REDRAW_INC. Writing into its retained buffer
        # first makes the two operands identical, freezing the display.
        for existing, incoming in (
            ("uptimeText", "nextUptime"),
            ("heapText", "nextHeap"),
            ("psramText", "nextPsram"),
        ):
            self.assertIn(f"char {incoming}[sizeof({existing})];",
                          self.main)
            self.assertIn(f"snprintf({incoming}, sizeof({incoming}),",
                          self.main)
            self.assertNotIn(f"snprintf({existing},", self.main)
        self.assertIn("gslc_ElemSetTxtStr(&gui, statusUptime, nextUptime);",
                      self.main)
        self.assertIn("gslc_ElemSetTxtStr(&gui, statusHeap, nextHeap);",
                      self.main)
        self.assertIn("gslc_ElemSetTxtStr(&gui, statusPsram, nextPsram);",
                      self.main)
        # Clearing the prior glyphs makes updates readable on the ST7789.
        self.assertIn("gslc_ElemSetFillEn(&gui, ref, true);", self.main)
        self.assertIn("gslc_ElemSetCol(&gui, ref, GSLC_COL_BLACK,",
                      self.main)
        self.assertIn("currentPage == 5 && now - lastStatusMs >= 1000",
                      self.main)

    def test_touch_is_single_mapped_source(self):
        self.assertRegex(self.cfg, r"(?m)^\s*#define DRV_TOUCH_HANDLER\b")
        self.assertNotRegex(self.cfg, r"(?m)^\s*#define DRV_TOUCH_NONE\b")
        self.assertIn("TouchTransform::rawToLandscape(rawX, rawY)", self.main)
        self.assertIn("gslc_SetTouchRemapEn(&gui, false)", self.main)
        self.assertIn("BoardTouch::readRaw(rawX, rawY)", self.main)

    def test_read_only_navigation_and_seven_pages(self):
        self.assertIn("constexpr uint8_t kPages = 7", self.main)
        self.assertIn("gslc_ElemCreateBtnTxt(", self.main)
        self.assertIn("gslc_SetPageCur(&gui, currentPage)", self.main)
        for prohibited in ("WiFi.begin(", "HTTPClient", "ATZ",
                           "ATSH", "drone_command", "flightCommand"):
            self.assertNotIn(prohibited, self.main)


if __name__ == "__main__":
    unittest.main()
