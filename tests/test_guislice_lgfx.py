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
