"""Compile+exercise all six module adapters without installing PlatformIO.

Host tests verify formatting and the real System telemetry API contract. The
firmware itself still requires separate ESP32-S3 and hardware validation.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROTO = ROOT / "prototypes" / "guislice-lovyangfx"


class SixModuleAdapterSmoke(unittest.TestCase):
    def test_native_cpp_six_module_adapter_smoke(self):
        compiler = shutil.which("g++")
        if not compiler:
            self.skipTest("Native g++ compiler unavailable")
        src = PROTO / "src"
        sources = [
            ROOT / "tests" / "module_diagnostics_smoke.cpp",
            src / "ModuleDiagnostics.cpp",
            *(src / "modules" / name for name in (
                "HermesModule.cpp", "WebradioModule.cpp",
                "Obd2Module.cpp", "DroneModule.cpp",
                "SystemModule.cpp", "CameraModule.cpp",
            )),
        ]
        with tempfile.TemporaryDirectory(prefix="fnk0086-modules-") as tmp:
            binary = Path(tmp) / "module_smoke"
            cmd = [
                compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I" + str(ROOT / "tests" / "fixtures"),
                "-I" + str(PROTO / "include"),
                "-I" + str(ROOT / "include"),
                *map(str, sources), "-o", str(binary),
            ]
            subprocess.run(cmd, check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True,
                           text=True)


if __name__ == "__main__":
    unittest.main()
