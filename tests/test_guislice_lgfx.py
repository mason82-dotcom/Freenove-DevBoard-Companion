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

    def test_all_six_modules_live_values_trigger_redraw(self):
        # The previous GUIslice Uptime regression was caused by writing into
        # a retained text buffer before calling gslc_ElemSetTxtStr().
        self.assertIn("ModuleDiagnostics::Snapshot next{};", self.main)
        self.assertIn("ModuleDiagnostics::capture(", self.main)
        self.assertIn("statusText[CompanionModules::kModuleCount]", self.main)
        self.assertIn("gslc_ElemSetTxtStr(&gui, ref, next.text[row]);",
                      self.main)
        self.assertNotIn("snprintf(statusText", self.main)
        self.assertIn("gslc_ElemSetFillEn(&gui, ref, true);", self.main)
        self.assertIn("gslc_ElemSetCol(&gui, ref, GSLC_COL_BLACK,",
                      self.main)
        self.assertIn("statusRefs[i][row] = addText(", self.main)
        self.assertIn("currentPage > 0 && now - lastStatusMs >= 1000",
                      self.main)
        self.assertIn("if (currentPage > 0) updateModule(currentPage - 1, now);",
                      self.main)

    def test_separate_six_module_backends_are_read_only(self):
        backend = PROTO / "src" / "modules"
        sources = ["HermesModule.cpp", "WebradioModule.cpp",
                   "Obd2Module.cpp", "DroneModule.cpp",
                   "SystemModule.cpp", "PulseOxModule.cpp"]
        self.assertEqual(len(sources), 6)
        for filename in sources:
            body = (backend / filename).read_text()
            self.assertIn("void ", body)
            for forbidden in ("WiFi.begin(", "WiFiClient", "ATZ",
                              "ATSH", "Serial2.write(", "esp_camera_init(",
                              "esp_camera_fb_get(", "i2s_driver_install(",
                              "HTTPClient", "flightCommand(", "Wire.write("):
                self.assertNotIn(forbidden, body)
        header = (PROTO / "include" / "ModuleDiagnostics.h").read_text()
        dispatch = (PROTO / "src" / "ModuleDiagnostics.cpp").read_text()
        self.assertIn("constexpr uint8_t kRows = 4", header)
        for name in ("hermes", "webradio", "obd2", "drone",
                     "system", "pulseOx"):
            self.assertIn(f"Adapter::{name}(out, uptimeMs);", dispatch)
        self.assertIn("ESP.getFreeHeap()", (backend / "SystemModule.cpp").read_text())
        self.assertIn("ESP.getFreePsram()", (backend / "SystemModule.cpp").read_text())

    def test_six_graphical_pages_and_pulsox_from_source(self):
        graphics = (PROTO / "src" / "ModuleGraphics.cpp").read_text()
        touch = (PROTO / "src" / "BoardTouch.cpp").read_text()
        sensor = (PROTO / "src" / "PulsOxSensor.cpp").read_text()
        self.assertIn("ModuleGraphics::bind(i, canvasRefs[i]);", self.main)
        self.assertIn("gslc_ElemSetDrawFunc(&gui, canvasRefs[i], &ModuleGraphics::draw);", self.main)
        self.assertIn("gslc_ElemCreateBox(", self.main)
        self.assertIn("PulsOx::begin()", self.main)
        self.assertIn("case CompanionModules::Id::PulseOx: pulse(g,r);", graphics)
        self.assertIn("case CompanionModules::Id::System: sys(g,r);", graphics)
        self.assertIn("PulsOx::readWave(samples,128)", graphics)
        self.assertIn("gslc_DrawLine(", graphics)
        self.assertIn("gslc_DrawFillRect(", graphics)
        self.assertIn("SharedI2C::Guard lock(8);", touch)
        self.assertIn("SharedI2C::Guard lock(30);", sensor)
        self.assertIn("maxim_heart_rate_and_oxygen_saturation(", sensor)
        self.assertIn("checkForBeat(", sensor)
        self.assertIn("static_assert(kEffectiveHz==25", sensor)
        self.assertNotIn("TFT_eSPI s_tft", graphics)
        self.assertNotIn("TFT_eSPI s_tft", self.main)
        self.assertIn("SparkFun MAX3010x Pulse and Proximity Sensor Library@1.1.2",
                      self.ini["env:fnk0086_lgfx"]["lib_deps"])
        modules = (PROTO / "include" / "CompanionGuiModules.h").read_text()
        self.assertIn('Id::PulseOx, "PulsOx"', modules)
        self.assertNotIn("Id::Camera", modules)

    def test_pulsox_stops_on_back_and_restarts_on_reentry(self):
        sensor = (PROTO / "src" / "PulsOxSensor.cpp").read_text()
        header = (PROTO / "include" / "PulsOxSensor.h").read_text()
        self.assertIn("constexpr uint8_t kPulsOxPage = 6", self.main)
        self.assertIn("currentPage == kPulsOxPage && nextPage != kPulsOxPage",
                      self.main)
        self.assertIn("PulsOx::requestActive(false);", self.main)
        self.assertIn("currentPage != kPulsOxPage && nextPage == kPulsOxPage",
                      self.main)
        self.assertIn("PulsOx::requestActive(true)", self.main)
        # GUI can request a change but MUST NOT directly access Wire or
        # a SparkFun sensor instance in a button/touch callback.
        self.assertIn("bool requestActive(bool active);", header)
        self.assertIn("desiredActive.store(active);", sensor)
        self.assertIn("stopPending.exchange(false)", sensor)
        self.assertIn("sensor.shutDown();", sensor)
        self.assertIn("SharedI2C::Guard lock(150);", sensor)
        self.assertIn("ulTaskNotifyTake(pdTRUE, portMAX_DELAY);", sensor)
        self.assertIn("xTaskNotifyGive(sensorWorker);", sensor)
        self.assertIn("latest = Snapshot{};", sensor)
        self.assertIn("waveHead = waveCount = 0;", sensor)
        self.assertIn('Serial.println("PulsOx MAX30102: STOPPED");', sensor)
        self.assertIn('Serial.println("PulsOx MAX30102: START");', sensor)
        self.assertNotIn("vTaskDelete(", sensor)
        self.assertEqual(sensor.count("xTaskCreatePinnedToCore("), 1)
        self.assertIn("if (!desiredActive.load()) return Snapshot{};", sensor)
        self.assertIn("if (!out || !capacity || !desiredActive.load())", sensor)

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
