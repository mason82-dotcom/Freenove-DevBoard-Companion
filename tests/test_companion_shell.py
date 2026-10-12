"""Compile pure C++ module metadata and check read-only GUI integration contracts."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = r'''
#include "CompanionModules.h"
using namespace CompanionModules;
static_assert(kModuleCount == 6, "Exactly six home modules");
static_assert(kModules[0].id == Id::Hermes, "Hermes tile");
static_assert(kModules[1].id == Id::Webradio, "Webradio tile");
static_assert(kModules[2].id == Id::Obd2, "OBD tile");
static_assert(kModules[3].id == Id::Drone, "Drone tile");
static_assert(kModules[4].id == Id::System, "System tile");
static_assert(kModules[5].id == Id::Camera, "Camera tile");
int main() { return 0; }
'''


class CompanionShellTests(unittest.TestCase):
    def test_module_registry_compiles_for_host(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("No C++ compiler installed")
        with tempfile.TemporaryDirectory() as path:
            source = Path(path) / "modules.cpp"
            source.write_text(CPP, encoding="utf-8")
            result = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-pedantic", "-fsyntax-only", "-I", str(ROOT / "include"),
                 str(source)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_lvgl_display_and_touch_are_wired(self):
        source = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        for expected in (
            "screen.setRotation(1)",
            "lv_disp_draw_buf_init(",
            "lv_disp_drv_register(",
            "lv_indev_drv_register(",
            "TouchTransform::rawToLandscape(",
            "lv_tick_inc(",
            "lv_timer_handler(",
        ):
            with self.subTest(symbol=expected):
                self.assertIn(expected, source)

    def test_lvgl_version_assertion_uses_correct_macros(self):
        source = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        self.assertIn("LVGL_VERSION_MAJOR == 8", source)
        self.assertIn("LVGL_VERSION_MINOR == 3", source)
        self.assertNotIn("LV_VERSION_MAJOR", source)
        self.assertNotIn("LV_VERSION_MINOR", source)

    def test_diagnostic_stages_pause_lvgl_and_bypass_spi(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        header = (ROOT / "include" / "CompanionUi.h").read_text(encoding="utf-8")
        for expected in ("diagStage = 0", "diagStage == 0",
                         "diagStage != 2", "lv_disp_flush_ready(drv)",
                         "DIAG SPI ENTER", "DIAG SPI EXIT"):
            with self.subTest(symbol=expected):
                self.assertIn(expected, gui)
        self.assertIn("void setDiagnosticStage(uint8_t stage)", header)
        self.assertIn("CompanionUi::setDiagnosticStage(", main)
        self.assertIn("uxTaskGetStackHighWaterMark(", main)
        self.assertIn("Serial.available()", main)

    def test_no_transport_or_control_in_shell(self):
        source = "\n".join(
            (ROOT / "src" / part).read_text(encoding="utf-8")
            for part in ("main.cpp", "BoardTouch.cpp", "CompanionUi.cpp")
        )
        for prohibited in ("WiFi.begin(", "HTTPClient", "esp_wifi", "setInsecure(",
                           "ATZ", "ATSH", "DroneControl", "flightCommand"):
            with self.subTest(symbol=prohibited):
                self.assertNotIn(prohibited, source)


if __name__ == "__main__":
    unittest.main()
