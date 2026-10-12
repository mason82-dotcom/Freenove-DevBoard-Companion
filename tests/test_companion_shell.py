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

    def test_lvgl9_display_and_touch_are_wired(self):
        source = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        for expected in (
            "screen.setRotation(1)",
            "lv_display_create(",
            "lv_display_set_color_format(",
            "LV_COLOR_FORMAT_RGB565",
            "lv_display_set_buffers(",
            "LV_DISPLAY_RENDER_MODE_PARTIAL",
            "lv_display_set_flush_cb(",
            "lv_indev_create(",
            "lv_indev_set_read_cb(",
            "lv_indev_set_display(",
            "TouchTransform::rawToLandscape(",
            "lv_tick_inc(",
            "lv_timer_handler(",
            "lv_screen_active(",
            "lv_button_create(",
        ):
            with self.subTest(symbol=expected):
                self.assertIn(expected, source)
        for removed in ("lv_disp_drv_register(", "lv_indev_drv_register(",
                        "lv_disp_draw_buf_init(", "lv_scr_act(", "lv_btn_create("):
            with self.subTest(removed=removed):
                self.assertNotIn(removed, source)

    def test_lvgl_version_assertion_uses_correct_macros(self):
        source = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        self.assertIn("LVGL_VERSION_MAJOR == 9", source)
        self.assertIn("LVGL_VERSION_MINOR == 6", source)
        self.assertNotIn("LV_VERSION_MAJOR", source)
        self.assertNotIn("LV_VERSION_MINOR", source)

    def test_lvgl9_owned_display_and_input_objects(self):
        # LVGL 9 owns created objects and their driver state internally.
        gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        self.assertIn("static lv_display_t* lvDisplay", gui)
        self.assertIn("static lv_indev_t* lvPointer", gui)
        self.assertIn("lvDisplay = lv_display_create(", gui)
        self.assertIn("lvPointer = lv_indev_create(", gui)
        self.assertIn("lv_display_flush_ready(disp)", gui)
        self.assertIn("uint8_t* colors)", gui)
        self.assertIn("sizeof(drawPixels), LV_DISPLAY_RENDER_MODE_PARTIAL", gui)
        self.assertNotIn("lv_disp_drv_t", gui)
        self.assertNotIn("lv_indev_drv_t", gui)

    def test_normal_boot_auto_starts_stage_2(self):
        gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        header = (ROOT / "include" / "CompanionUi.h").read_text(encoding="utf-8")
        self.assertIn("#define COMPANION_DIAGNOSTIC_BOOT 0", gui)
        self.assertIn("diagStage = COMPANION_DIAGNOSTIC_BOOT ? 0 : 2;", gui)
        self.assertIn('UI BOOT: stage %u', main)
        self.assertIn("COMPANION_DIAGNOSTIC_BOOT=1", header)
        self.assertNotIn("DIAG BOOT: stage 0, LVGL paused", main)

    def test_diagnostic_stages_pause_lvgl_and_bypass_spi(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        header = (ROOT / "include" / "CompanionUi.h").read_text(encoding="utf-8")
        for expected in ("diagStage = 0", "diagStage == 0",
                         "diagStage != 2", "lv_display_flush_ready(disp)",
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
