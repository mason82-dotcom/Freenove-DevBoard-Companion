"""Check the active LVGL 8.3.11, TFT_eSPI and CFG1 build configuration."""
import configparser
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class LvglBuildConfigurationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ini = configparser.ConfigParser(interpolation=None)
        cls.ini.read(ROOT / "platformio.ini")
        cls.env = cls.ini["env:fnk0086"]
        cls.flags = cls.env["build_flags"].split()
        cls.lv_conf = (ROOT / "include" / "lv_conf.h").read_text(encoding="utf-8")
        cls.macros = dict(re.findall(r"(?m)^#define\s+(\w+)\s+(.+)$", cls.lv_conf))

    def test_simple_include_and_project_header_path(self):
        for flag in ("-DLV_CONF_INCLUDE_SIMPLE=1", "-Iinclude",
                     "-DUSER_SETUP_LOADED=1", "-include",
                     "include/VerifiedDisplaySetup.h"):
            self.assertIn(flag, self.flags)
        self.assertNotIn("-DLV_CONF_SKIP", self.flags)

    def test_dependency_versions_are_pinned(self):
        self.assertIn("lvgl/lvgl@8.3.11", self.env["lib_deps"])
        self.assertIn("bodmer/TFT_eSPI@2.5.43", self.env["lib_deps"])
        self.assertIn("SparkFun MAX3010x Pulse and Proximity Sensor Library@1.1.2",
                      self.env["lib_deps"])
        self.assertIn("55.03.39/platform-espressif32.zip", self.env["platform"])
        self.assertNotIn("releases/download/stable/", self.env["platform"])
        self.assertEqual(self.env["board_build.flash_size"], "8MB")
        self.assertEqual(self.env["board_build.arduino.memory_type"], "qio_opi")
        self.assertIn("-DBOARD_HAS_PSRAM", self.flags)

    def test_lvgl8_rgb565_buffer_memory_and_font(self):
        expected = {
            "LV_COLOR_DEPTH": "16", "LV_COLOR_16_SWAP": "0",
            "LV_MEM_CUSTOM": "0", "LV_MEM_SIZE": "(64U * 1024U)",
            "LV_TICK_CUSTOM": "0", "LV_FONT_MONTSERRAT_12": "1",
            "LV_FONT_DEFAULT": "&lv_font_montserrat_12",
            "LV_USE_ASSERT_NULL": "1", "LV_USE_ASSERT_MALLOC": "1",
        }
        for name, value in expected.items():
            with self.subTest(macro=name):
                self.assertEqual(self.macros.get(name), value)
        self.assertIn("#ifndef LV_CONF_H", self.lv_conf)
        self.assertNotIn("LV_COLOR_FORMAT_DEFAULT", self.macros)

    def test_enabled_lvgl8_widgets(self):
        for name in ("LV_USE_THEME_DEFAULT", "LV_USE_FLEX", "LV_USE_GRID",
                     "LV_USE_LABEL", "LV_USE_BTN", "LV_USE_CHART", "LV_USE_BAR",
                     "LV_USE_SLIDER", "LV_USE_IMG", "LV_USE_CANVAS",
                     "LV_USE_LINE", "LV_USE_ARC"):
            with self.subTest(macro=name):
                self.assertEqual(self.macros.get(name), "1")
        self.assertNotIn("LV_USE_BUTTON", self.macros)
        self.assertNotIn("LV_USE_IMAGE", self.macros)

    def test_unused_lvgl8_widgets_are_disabled(self):
        for name in ("LV_USE_ANIMIMG", "LV_USE_TABVIEW", "LV_USE_WIN",
                     "LV_USE_MSGBOX", "LV_USE_KEYBOARD", "LV_USE_TEXTAREA",
                     "LV_USE_LIST", "LV_USE_DROPDOWN", "LV_USE_ROLLER",
                     "LV_USE_SPINNER", "LV_USE_SWITCH", "LV_USE_TABLE",
                     "LV_USE_CHECKBOX", "LV_USE_BTNMATRIX", "LV_USE_SPINBOX",
                     "LV_USE_CALENDAR", "LV_USE_MENU", "LV_USE_TILEVIEW",
                     "LV_USE_METER", "LV_USE_LED", "LV_USE_SPAN",
                     "LV_USE_COLORWHEEL", "LV_USE_IMGBTN", "LV_USE_MSG"):
            with self.subTest(macro=name):
                self.assertEqual(self.macros.get(name), "0")

    def test_fixed_cfg1_only(self):
        self.assertIn("include/VerifiedDisplaySetup.h", self.env["build_flags"])
        self.assertNotIn("CFG2", self.env["build_flags"])
        self.assertNotIn("platformio.guislice-lgfx.ini", self.env["build_flags"])


if __name__ == "__main__":
    unittest.main()
