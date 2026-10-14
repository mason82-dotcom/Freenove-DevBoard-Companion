"""Compile and run the active MAX30102 formatter on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PulsOxFormatterSmoke(unittest.TestCase):
    def test_native_cpp_max30102_formatting(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-pulsox-") as tmp:
            binary = Path(tmp) / "pulsox_format_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                   "-pedantic", "-I" + str(ROOT / "include"),
                   str(ROOT / "tests" / "pulsox_format_smoke.cpp"),
                   str(ROOT / "src" / "PulsOxDisplay.cpp"),
                   "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
