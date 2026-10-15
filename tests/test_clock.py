"""Contracts for the NTP wall clock (home WLAN only, never guessed)."""
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


class ClockTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.clock = code_only((ROOT / "src" / "Clock.cpp").read_text(encoding="utf-8"))
        cls.header = (ROOT / "include" / "Clock.h").read_text(encoding="utf-8")
        cls.gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")
        cls.log = code_only((ROOT / "src" / "Obd2Log.cpp").read_text(encoding="utf-8"))

    def test_ntp_with_berlin_time_zone_started_once(self):
        self.assertIn('kTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3"', self.header)
        self.assertIn('configTzTime(kTimeZone, "fritz.box", "de.pool.ntp.org", "pool.ntp.org")',
                      self.clock)
        self.assertIn("if (started) return;", self.clock)
        self.assertLess(self.gui.index("WifiLink::begin();"), self.gui.index("Clock::begin();"))

    def test_unknown_time_is_never_shown_as_real(self):
        self.assertIn("return started && plausible(time(nullptr));", self.clock)
        self.assertIn("Clock::local(t) ? &t : nullptr", self.gui)
        self.assertIn("Clock::local(t) ? &t : nullptr", self.log)
        self.assertIn('"%s | Up %lu:%02lu:%02lu', self.gui)

    def test_native_formats(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-clock-") as tmp:
            binary = Path(tmp) / "clock_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "include"), str(ROOT / "tests" / "clock_smoke.cpp"),
                   str(ROOT / "src" / "ClockFormat.cpp"), "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
