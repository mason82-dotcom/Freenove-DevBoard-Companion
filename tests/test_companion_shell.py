"""Regression contracts for the active LVGL 8 eight-tile shell."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = r'''
#include "CompanionModules.h"
using namespace CompanionModules;
static_assert(kModuleCount == 8, "Exactly eight home tiles");
static_assert(kModules[0].id == Id::Hermes, "Hermes tile");
static_assert(kModules[1].id == Id::Webradio, "Webradio tile");
static_assert(kModules[2].id == Id::Obd2, "OBD tile");
static_assert(kModules[3].id == Id::Drone, "Drone tile");
static_assert(kModules[4].id == Id::System, "System tile");
static_assert(kModules[5].id == Id::PulsOx, "PulsOx tile");
static_assert(kModules[6].id == Id::Kids, "Kids placeholder tile");
static_assert(kModules[7].id == Id::Spare, "Spare placeholder tile");
int main() { return 0; }
'''


class CompanionShellTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        cls.main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        cls.header = (ROOT / "include" / "CompanionUi.h").read_text(encoding="utf-8")

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

    def test_lvgl8_display_and_touch_are_wired(self):
        for expected in (
            "screen.setRotation(1)", "lv_disp_draw_buf_init(&lvDrawBuffer, drawPixels, nullptr,",
            "lv_disp_drv_init(&lvDisplayDriver)", "lvDisplayDriver.hor_res = kWidth",
            "lvDisplayDriver.ver_res = kHeight", "lvDisplayDriver.flush_cb = flushDisplay",
            "lvDisplayDriver.draw_buf = &lvDrawBuffer",
            "lv_disp_drv_register(&lvDisplayDriver)",
            "lv_indev_drv_init(&lvPointerDriver)", "lvPointerDriver.type = LV_INDEV_TYPE_POINTER",
            "lvPointerDriver.read_cb = readPointer",
            "lv_indev_drv_register(&lvPointerDriver)",
            "TouchTransform::rawToLandscape(x, y)", "lv_tick_inc(elapsed)",
            "lv_timer_handler()", "lv_scr_act()", "lv_btn_create(parent)",
        ):
            with self.subTest(symbol=expected):
                self.assertIn(expected, self.gui)
        for obsolete in ("lv_display_create(", "lv_indev_create(", "lv_button_create("):
            with self.subTest(obsolete=obsolete):
                self.assertNotIn(obsolete, self.gui)

    def test_lvgl_version_and_driver_storage_lifetime(self):
        self.assertIn("LVGL_VERSION_MAJOR == 8 && LVGL_VERSION_MINOR == 3", self.gui)
        namespace = self.gui.split("namespace {", 1)[1].split("namespace CompanionUi {", 1)[0]
        for declaration in ("static lv_disp_draw_buf_t lvDrawBuffer;",
                            "static lv_disp_drv_t lvDisplayDriver;",
                            "static lv_indev_drv_t lvPointerDriver;",
                            "alignas(4) static lv_color_t drawPixels[kWidth * kLinesPerFlush];"):
            self.assertIn(declaration, namespace)
        begin = self.gui.split("bool begin(bool touchPresent)", 1)[1].split("void setDiagnosticStage", 1)[0]
        self.assertNotRegex(begin, r"(?:lv_disp_drv_t|lv_indev_drv_t|lv_disp_draw_buf_t)\s+\w+\s*;")
        self.assertIn("constexpr int kWidth = 320", self.gui)
        self.assertIn("constexpr int kHeight = 240", self.gui)
        self.assertIn("constexpr int kLinesPerFlush = 20", self.gui)
        self.assertIn("screen.pushColors(reinterpret_cast<uint16_t*>(colors), w * h, true)", self.gui)
        self.assertIn("lv_disp_flush_ready(disp)", self.gui)

    def test_six_tiles_and_navigation_are_deferred_until_after_timer(self):
        self.assertIn("uint8_t moduleNumbers[CompanionModules::kModuleCount] = {1, 2, 3, 4, 5, 6, 7, 8}", self.gui)
        self.assertIn("for (size_t i = 0; i < CompanionModules::kModuleCount; ++i)", self.gui)
        self.assertIn("CompanionModules::kModules[i].title", self.gui)
        self.assertIn("lv_obj_add_event_cb(tile, onTileClicked, LV_EVENT_CLICKED, &moduleNumbers[i])", self.gui)
        self.assertIn("requestedPage = *number", self.gui)
        self.assertIn("requestedPage = 0", self.gui)
        poll = self.gui.split("void poll()", 1)[1]
        self.assertLess(poll.index("lv_timer_handler()"), poll.index("if (requestedPage != shownPage)"))
        self.assertIn("CompanionModules::kModules[shownPage - 1].title", poll)
        self.assertIn("showModule(shownPage)", poll)
        self.assertIn("showHome()", poll)
        self.assertIn("module.detail", self.gui)
        self.assertIn("ESP.getFreeHeap()", self.gui)
        self.assertIn("ESP.getFreePsram()", self.gui)
        self.assertIn("now - lastSystemRefresh >= 1000", self.gui)

    def test_normal_boot_auto_starts_stage_2(self):
        self.assertIn("#define COMPANION_DIAGNOSTIC_BOOT 0", self.gui)
        self.assertIn("diagStage = COMPANION_DIAGNOSTIC_BOOT ? 0 : 2;", self.gui)
        self.assertIn('UI BOOT: stage %u', self.main)
        self.assertIn("COMPANION_DIAGNOSTIC_BOOT=1", self.header)

    def test_diagnostic_stages_pause_lvgl_and_bypass_spi(self):
        for expected in ("diagStage = 0", "diagStage == 0", "diagStage != 2",
                         "lv_disp_flush_ready(disp)", "DIAG SPI ENTER", "DIAG SPI EXIT",
                         "if (stage != 0) lv_obj_invalidate(lv_scr_act())"):
            with self.subTest(symbol=expected):
                self.assertIn(expected, self.gui)
        self.assertIn("void setDiagnosticStage(uint8_t stage)", self.header)
        self.assertIn("CompanionUi::setDiagnosticStage(", self.main)
        self.assertIn("uxTaskGetStackHighWaterMark(", self.main)
        self.assertIn("Serial.available()", self.main)

    def test_serial_stage_commands_need_whole_lines(self):
        # A host tty echoing the firmware's output (digits) once switched the
        # display into stage 1 (white screen). Single bytes must not do that.
        self.assertNotIn("c >= '0' && c <= '2'", self.main)
        self.assertIn('if (length != 5 || strncmp(line, "diag", 4) != 0) return -1;', self.main)
        self.assertIn("if (c == '\\r' || c == '\\n')", self.main)
        self.assertIn("commandOverflow ? -1 : diagnosticCommand(", self.main)

    def test_no_transport_or_control_in_shell(self):
        source = "\n".join((ROOT / "src" / part).read_text(encoding="utf-8")
                           for part in ("main.cpp", "BoardTouch.cpp", "CompanionUi.cpp"))
        for prohibited in ("WiFi.begin(", "HTTPClient", "esp_wifi", "setInsecure(",
                           "ATZ", "ATSH", "DroneControl", "flightCommand"):
            with self.subTest(symbol=prohibited):
                self.assertNotIn(prohibited, source)


if __name__ == "__main__":
    unittest.main()
