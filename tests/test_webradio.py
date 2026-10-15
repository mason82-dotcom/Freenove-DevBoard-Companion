"""Contracts for the Webradio module (engine port, player, page)."""
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def read(path):
    return code_only((ROOT / path).read_text(encoding="utf-8"))


class WebradioTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.engine = read("src/RadioEngine.cpp")
        cls.player = read("src/WebradioPlayer.cpp")
        cls.gui = read("src/CompanionUi.cpp")
        cls.ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")

    def test_library_is_pinned(self):
        self.assertIn("https://github.com/schreibfaul1/ESP32-audioI2S.git#3.4.7", self.ini)
        self.assertIn("-DCORE_DEBUG_LEVEL=1", self.ini)

    def test_engine_owns_audio_and_never_touches_lvgl(self):
        self.assertIn("Audio audio;", self.engine)
        for source in (self.engine, self.player):
            self.assertNotIn("lv_", source)
            self.assertNotIn("lvgl.h", source)
        for source in (self.gui, self.player):
            self.assertNotIn("Audio.h", source)
            self.assertNotIn("audio.", source)
        self.assertIn("audio.setPinout(BoardPins::kAudioBclk, BoardPins::kAudioWs,", self.engine)
        self.assertIn("if (e.gen != gen) continue;", self.player)
        self.assertIn("kConnectTimeoutMs = 15000", self.engine)

    def test_i2s_dma_stays_small_in_internal_ram(self):
        # The library default (32 x 256) starves the Hermes TLS handshake.
        desc = int(re.search(r"kDmaDescNum = (\d+);", self.engine).group(1))
        frames = int(re.search(r"kDmaFrameNum = (\d+);", self.engine).group(1))
        self.assertLessEqual(desc * frames * 8, 24 * 1024)
        self.assertGreaterEqual(desc * frames, 2048)  # >= ~46 ms at 44.1 kHz
        begin = self.engine.split("bool begin()", 1)[1]
        self.assertLess(begin.index("audio.settings.DMA_DESC_NUM = kDmaDescNum;"),
                        begin.index("audio.setPinout("))
        self.assertLess(begin.index("audio.settings.DMA_FRAME_NUM = kDmaFrameNum;"),
                        begin.index("audio.setPinout("))

    def test_titles_ascii_and_state_persisted_debounced(self):
        self.assertIn("toDisplayAscii(title, e.text, sizeof(title))", self.player)
        settings = read("src/WebradioSettings.cpp")
        self.assertIn('kNamespace[] = "webradio"', settings)
        self.assertEqual(sorted(set(re.findall(r'prefs\.(?:get|put)\w+\("(\w+)"', settings))),
                         ["playing", "station", "volume"])
        self.assertIn("prefs.begin(kNamespace, true)", settings)  # load is read-only
        # Saved only after kSaveDelayMs without changes and only when different.
        # Signed age: an unsigned one saved on every button press (seen on hardware).
        self.assertIn("static_cast<int32_t>(now - dirtyAtMs) >= static_cast<int32_t>(Settings::kSaveDelayMs)",
                      self.player)
        self.assertIn("v.station != saved.station || v.volume != saved.volume", self.player)
        self.assertIn("kSaveDelayMs = 5000", read("include/WebradioSettings.h"))

    def test_station_list_built_on_demand_and_freed_outside_callbacks(self):
        self.assertIn('LV_SYMBOL_LIST " Sender"', self.gui)
        self.assertIn("for (size_t i = 0; i < Webradio::kStationCount; ++i)", self.gui)
        self.assertIn("Webradio::select(index);", self.gui)
        # Never delete the list inside its own callback or asynchronously.
        self.assertNotIn("lv_obj_del_async(radioList)", self.gui)
        poll = self.gui.split("void poll()", 1)[1]
        self.assertLess(poll.index("lv_timer_handler();"), poll.index("reapRadioList();"))
        self.assertIn("radioListClosing = false;", self.gui.split("void makeScreenBlank()", 1)[1])
        select = self.player.split("void select(uint8_t index)", 1)[1].split("void changeVolume", 1)[0]
        self.assertIn("if (!ready || index >= kStationCount) return;", select)
        self.assertIn("wantsPlay = true;", select)
        self.assertIn("markDirty();", select)

    def test_no_server_storage_or_recording(self):
        for name in ("src/RadioEngine.cpp", "src/WebradioPlayer.cpp", "src/WebradioText.cpp"):
            text = read(name)
            for forbidden in ("WebServer", "Preferences", "nvs_", "SD.open", "SD_MMC.",
                              "FFat.", "Update.", "setInsecure(", "WiFi.begin("):
                with self.subTest(file=name, symbol=forbidden):
                    self.assertNotIn(forbidden, text)

    def test_player_waits_for_home_wifi_and_limits_retries(self):
        self.assertIn("WifiLink::connectedTo(WifiLink::Profile::Home)", self.player)
        self.assertIn("if (attempt < Webradio::kMaxRetries)", self.player)
        poll = self.gui.split("void poll()", 1)[1]
        self.assertIn("Webradio::service(now);", poll)
        self.assertIn("setTextIfChanged(radioTitle", self.gui)
        self.assertIn("LV_LABEL_LONG_SCROLL_CIRCULAR", self.gui)

    def test_native_text_and_stations(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-radio-") as tmp:
            binary = Path(tmp) / "webradio_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "include"), str(ROOT / "tests" / "webradio_smoke.cpp"),
                   str(ROOT / "src" / "WebradioText.cpp"), "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
