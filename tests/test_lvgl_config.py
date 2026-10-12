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

    def test_fixed_cfg1_only(self):
        self.assertIn("include/VerifiedDisplaySetup.h", self.env["build_flags"])
        self.assertNotIn("CFG2", self.env["build_flags"])
        self.assertEqual(self.env["board_build.flash_size"], "8MB")


if __name__ == "__main__":
    unittest.main()
