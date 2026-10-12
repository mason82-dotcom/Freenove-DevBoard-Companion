"""Pin a reproducible LVGL 9.6 / pioarduino / TFT_eSPI dependency stack."""
import configparser
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

    def test_simple_include_and_project_header_path(self):
        self.assertIn("-DLV_CONF_INCLUDE_SIMPLE=1", self.flags)
        self.assertIn("-Iinclude", self.flags)
        self.assertNotIn("-DLV_CONF_SKIP", self.flags)
        self.assertIn("-DUSER_SETUP_LOADED=1", self.flags)
        self.assertIn("-include", self.flags)
        self.assertIn("include/VerifiedDisplaySetup.h", self.flags)

    def test_dependency_versions_are_pinned(self):
        self.assertIn("lvgl/lvgl@9.6.0", self.env["lib_deps"])
        self.assertIn("bodmer/TFT_eSPI@2.5.43", self.env["lib_deps"])
        self.assertIn("55.03.39/platform-espressif32.zip", self.env["platform"])
        self.assertNotIn("releases/download/stable/", self.env["platform"])

    def test_conf_guard_rgb565_and_memory(self):
        for name, value in [
            ("LV_CONF_H", None),
            ("LV_COLOR_FORMAT_DEFAULT", "LV_COLOR_FORMAT_RGB565"),
            ("LV_USE_STDLIB_MALLOC", "LV_STDLIB_BUILTIN"),
            ("LV_USE_OS", "LV_OS_NONE"),
            ("LV_USE_DRAW_SW", "1"),
            ("LV_USE_LABEL", "1"),
            ("LV_USE_BUTTON", "1"),
        ]:
            pattern = rf"^\s*#\s*define\s+{name}\b"
            if value is not None:
                pattern += rf"\s+{value}\b"
            self.assertRegex(self.lv_conf, "(?m)" + pattern)
        self.assertIn("LV_MEM_SIZE (64U * 1024U)", self.lv_conf)
        self.assertNotIn("#define LV_COLOR_DEPTH ", self.lv_conf)

    def test_extended_profile_enabled_for_every_lvgl_translation_unit(self):
        self.assertRegex(self.lv_conf, r"(?m)^#define COMPANION_LVGL_PROFILE_EXTENDED 1$")
        # All third-party LVGL compilation units see the same configuration
        # because these flags apply to the entire PlatformIO environment.
        self.assertIn("-DLV_CONF_INCLUDE_SIMPLE=1", self.flags)
        self.assertIn("-Iinclude", self.flags)
        enabled = (
            "LV_USE_LABEL", "LV_USE_BUTTON", "LV_USE_CHART",
            "LV_USE_BAR", "LV_USE_SLIDER", "LV_USE_IMAGE",
            "LV_USE_FLEX", "LV_USE_GRID", "LV_USE_DRAW_SW",
            "LV_USE_THEME_DEFAULT",
        )
        for flag in enabled:
            with self.subTest(enabled=flag):
                self.assertRegex(self.lv_conf, rf"(?m)^#define {flag} 1$")

    def test_unneeded_widgets_are_disabled(self):
        disabled = (
            "LV_USE_ANIMIMG", "LV_USE_ARC", "LV_USE_ARCLABEL",
            "LV_USE_BARCODE", "LV_USE_BUTTONMATRIX", "LV_USE_CALENDAR",
            "LV_USE_CANVAS", "LV_USE_CHECKBOX", "LV_USE_DROPDOWN",
            "LV_USE_IMAGEBUTTON", "LV_USE_KEYBOARD", "LV_USE_LED",
            "LV_USE_LINE", "LV_USE_LIST", "LV_USE_MENU", "LV_USE_MSGBOX",
            "LV_USE_ROLLER", "LV_USE_SCALE", "LV_USE_SPAN",
            "LV_USE_SPINBOX", "LV_USE_SPINNER", "LV_USE_SWITCH",
            "LV_USE_TEXTAREA", "LV_USE_TABLE", "LV_USE_TABVIEW",
            "LV_USE_TILEVIEW", "LV_USE_WIN", "LV_USE_OBSERVER",
            "LV_USE_THEME_SIMPLE", "LV_USE_THEME_MONO",
        )
        for flag in disabled:
            with self.subTest(disabled=flag):
                self.assertRegex(self.lv_conf, rf"(?m)^#define {flag} 0$")

    def test_fixed_cfg1_only(self):
        self.assertIn("include/VerifiedDisplaySetup.h", self.env["build_flags"])
        self.assertNotIn("CFG2", self.env["build_flags"])
        self.assertEqual(self.env["board_build.flash_size"], "8MB")


if __name__ == "__main__":
    unittest.main()
