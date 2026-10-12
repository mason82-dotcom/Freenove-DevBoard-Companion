"""Guard the LVGL 8.x include contract across PlatformIO C/C++ library files."""
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
        cls.flags = cls.ini["env:fnk0086"]["build_flags"].split()
        cls.lv_conf = (ROOT / "include" / "lv_conf.h").read_text(encoding="utf-8")

    def test_simple_include_and_project_header_path(self):
        self.assertIn("-DLV_CONF_INCLUDE_SIMPLE=1", self.flags)
        self.assertIn("-Iinclude", self.flags)
        self.assertNotIn("-DLV_CONF_SKIP", self.flags)
        self.assertIn("-DLV_CONF_INCLUDE_SIMPLE=1", self.flags)

    def test_lvgl_version_pinned(self):
        self.assertIn("lvgl/lvgl@8.3.11", self.ini["env:fnk0086"]["lib_deps"])

    def test_conf_guard_rgb565_and_memory(self):
        for name, value in [
            ("LV_CONF_H", None),
            ("LV_COLOR_DEPTH", "16"),
            ("LV_COLOR_16_SWAP", "0"),
            ("LV_MEM_CUSTOM", "0"),
            ("LV_TICK_CUSTOM", "0"),
        ]:
            pattern = rf"^\s*#\s*define\s+{name}\b"
            if value is not None:
                pattern += rf"\s+{value}\b"
            self.assertRegex(self.lv_conf, "(?m)" + pattern)
        self.assertIn("LV_MEM_SIZE (64U * 1024U)", self.lv_conf)


if __name__ == "__main__":
    unittest.main()
