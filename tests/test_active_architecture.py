"""Source-level contracts for the firmware selected by platformio.ini."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def body(path):
    return (ROOT / path).read_text(encoding="utf-8")


class ActiveArchitectureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.gui = body("src/CompanionUi.cpp")
        cls.sensor = body("src/PulsOxSensor.cpp")
        cls.touch = body("src/BoardTouch.cpp")
        cls.bus = body("src/SharedI2C.cpp")
        cls.modules = body("include/CompanionModules.h")

    def test_six_module_names_and_page_order(self):
        entries = re.findall(r'\{Id::(\w+),\s*"([^"]+)",\s*"([^"]+)"\}', self.modules)
        self.assertEqual([(ident, title) for ident, title, _ in entries], [
            ("Hermes", "Hermes"), ("Webradio", "Webradio"),
            ("Obd2", "OBD2"), ("Drone", "Drohne"),
            ("System", "System"), ("PulsOx", "PulsOx"),
        ])
        self.assertEqual(len(entries), 6)
        self.assertTrue(all(detail for _, _, detail in entries))
        self.assertNotIn("Id::Camera", self.modules)

    def test_home_tiles_and_six_detail_pages(self):
        self.assertIn("CompanionModules::kModuleCount", self.gui)
        self.assertIn("CompanionModules::kModules[page - 1]", self.gui)
        self.assertIn("module.title", self.gui)
        self.assertIn("module.detail", self.gui)
        self.assertIn("lv_obj_add_event_cb(back, onBackClicked, LV_EVENT_CLICKED, nullptr)", self.gui)
        self.assertIn("requestedPage = 0", self.gui)
        self.assertIn("Nur lokale Anzeige | CFG1 320x240", self.gui)

    def test_system_live_values_and_periodic_refresh(self):
        self.assertIn("module.id == CompanionModules::Id::System", self.gui)
        for field in ("Uptime: %lu s", "Heap frei: %u B", "PSRAM frei: %u B"):
            self.assertIn(field, self.gui)
        for api in ("millis() / 1000", "ESP.getFreeHeap()", "ESP.getFreePsram()"):
            self.assertIn(api, self.gui)
        self.assertIn("showing(CompanionModules::Id::System) && now - lastSystemRefresh >= 1000", self.gui)

    def test_placeholder_modules_have_no_transport_actions(self):
        self.assertIn("Noch keine Verbindung / keine Befehle", self.gui)
        for source in (body("src/main.cpp"), self.gui):
            for forbidden in ("WiFi.begin(", "WiFiClient", "HTTPClient", "ATZ",
                              "ATSH", "Serial2.write(", "esp_camera_init(",
                              "esp_camera_fb_get(", "flightCommand("):
                self.assertNotIn(forbidden, source)

    def test_pulsox_page_entry_exit_and_refresh(self):
        self.assertIn("module.id == CompanionModules::Id::PulsOx", self.gui)
        self.assertIn("PulsOx::begin()", self.gui)
        poll = self.gui.split("void poll()", 1)[1]
        self.assertIn("if (showing(CompanionModules::Id::PulsOx)) PulsOx::requestActive(false);", poll)
        self.assertIn("if (showing(CompanionModules::Id::PulsOx)) PulsOx::requestActive(true);", poll)
        self.assertLess(poll.index("PulsOx::requestActive(false)"),
                        poll.index("shownPage = requestedPage"))
        self.assertLess(poll.index("showModule(shownPage)"),
                        poll.index("PulsOx::requestActive(true)"))
        self.assertIn("showing(CompanionModules::Id::PulsOx) && pulsOxLabel && now - lastPulsOxRefresh >= 500", poll)
        self.assertIn("PulsOx::format(PulsOx::read(), now, rows)", poll)
        self.assertIn('"%s\\n%s\\n%s\\n%s"', poll)

    def test_pulsox_worker_is_parked_and_stop_is_latched(self):
        self.assertEqual(self.sensor.count("xTaskCreatePinnedToCore("), 1)
        self.assertIn("if (!SharedI2C::ready()) return false", self.sensor)
        self.assertIn("std::atomic<bool> desiredActive{false}", self.sensor)
        self.assertIn("std::atomic<bool> stopPending{false}", self.sensor)
        self.assertIn("if (!active) stopPending.store(true)", self.sensor)
        self.assertIn("desiredActive.store(active)", self.sensor)
        self.assertIn("const bool mustStop = stopPending.exchange(false)", self.sensor)
        self.assertIn("if (!desiredActive.load() || mustStop)", self.sensor)
        self.assertIn("ulTaskNotifyTake(pdTRUE, portMAX_DELAY)", self.sensor)
        self.assertIn("xTaskNotifyGive(sensorWorker)", self.sensor)
        self.assertNotIn("vTaskDelete(", self.sensor)

    def test_pulsox_shutdown_and_snapshot_reset(self):
        for expected in ("sensor.shutDown()", "configured = false",
                         "s = Snapshot{}", "reset(s)", "publish(s)",
                         "latest = Snapshot{}", "waveHead = waveCount = 0",
                         "if (!desiredActive.load()) return Snapshot{}",
                         "if (!out || !capacity || !desiredActive.load()) return 0"):
            with self.subTest(symbol=expected):
                self.assertIn(expected, self.sensor)
        self.assertIn("PulsOx MAX30102: STOPPED", self.sensor)
        self.assertIn("PulsOx MAX30102: START", self.sensor)

    def test_sensor_processing_remains_on_worker_and_i2c_is_shared(self):
        for expected in ("SharedI2C::Guard lock(150)", "SharedI2C::Guard lock(30)",
                         "checkForBeat(", "maxim_heart_rate_and_oxygen_saturation(",
                         "sensor.setup(60,4,2,kRate,411,4096)",
                         "static_assert(kEffectiveHz==25"):
            self.assertIn(expected, self.sensor)
        self.assertIn("SharedI2C::Guard guard(30)", self.touch)
        self.assertIn("Wire.begin(BoardPins::kTouchSda, BoardPins::kTouchScl, 400000)", self.bus)
        self.assertNotIn("sensor.check()", self.gui)
        self.assertNotIn("Wire.", self.gui)

    def test_touch_display_and_cfg1_pins(self):
        pins = body("include/BoardPins.h")
        setup = body("include/VerifiedDisplaySetup.h")
        self.assertIn("screen.setRotation(1)", self.gui)
        self.assertIn("TouchTransform::rawToLandscape(x, y)", self.gui)
        self.assertIn("BoardTouch::readRaw(x, y)", self.gui)
        self.assertIn("diagStage != 2 || !touchEnabled", self.gui)
        for expected in ("kTouchSda = 2", "kTouchScl = 1", "kTouchAddress = 0x38"):
            self.assertIn(expected, pins)
        for macro, value in (("ST7789_DRIVER", None), ("TFT_MOSI", "20"),
                             ("TFT_SCLK", "21"), ("TFT_CS", "-1"),
                             ("TFT_DC", "0"), ("TFT_RGB_ORDER", "TFT_BGR"),
                             ("TFT_INVERSION_OFF", None)):
            pattern = rf"(?m)^\s*#define\s+{macro}\b"
            if value is not None:
                pattern += rf"\s+{value}\b"
            with self.subTest(macro=macro):
                self.assertRegex(setup, pattern)
        self.assertIn("ONLY FNK0086 CFG1", setup)
        self.assertNotIn("CFG2", setup)

    def test_touch_reads_one_ft6336u_point_without_writing_settings(self):
        self.assertIn("Wire.write(static_cast<uint8_t>(0x02))", self.touch)
        self.assertIn("static_cast<uint8_t>(5)", self.touch)
        self.assertIn("if (count != 1) return false", self.touch)
        self.assertIn("((xh & 0x0F) << 8) | xl", self.touch)
        self.assertIn("((yh & 0x0F) << 8) | yl", self.touch)
        self.assertEqual(self.touch.count("Wire.write("), 1)

    def test_pages_resolve_through_module_ids(self):
        poll = self.gui.split("void poll()", 1)[1]
        self.assertNotRegex(poll, r"shownPage == [1-9]")
        self.assertIn("CompanionModules::kModules[shownPage - 1].id == id", self.gui)

    def test_pulsox_page_draws_bounded_ir_trace(self):
        poll = self.gui.split("void poll()", 1)[1]
        self.assertIn("constexpr uint16_t kWavePoints = 100", self.gui)
        self.assertIn("PulsOx::readWave(raw, kWavePoints)", self.gui)
        self.assertIn("lv_chart_set_ext_y_array(pulsOxChart, pulsOxSeries, wavePoints)", self.gui)
        self.assertIn("pulsOxChart = nullptr", self.gui)
        self.assertIn("now - lastPulsOxWave >= 200", poll)
        self.assertNotIn("sensor.", self.gui)
        self.assertIn("sample.sensorOk && sample.finger", self.gui)
        self.assertIn("PulsOx::detrendTrace(raw, n, wavePoints + pad)", self.gui)


if __name__ == "__main__":
    unittest.main()
