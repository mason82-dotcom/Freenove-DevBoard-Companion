"""Expose the framework SPI headers to GUIslice's separately compiled driver.

PlatformIO's Library Dependency Finder correctly installs framework SPI, but
GUIslice's independent driver translation unit does not inherit SPI's include
path. Resolve it from the actual pioarduino package instead of hardcoding the
user's home directory or platform installation path.
"""
from pathlib import Path

Import("env")  # noqa: F821 - provided by PlatformIO/SCons at build time

framework = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
if not framework:
    raise RuntimeError("pioarduino's Arduino-ESP32 framework package was not resolved")

spi_headers = Path(framework) / "libraries" / "SPI" / "src"
if not spi_headers.is_dir():
    raise RuntimeError(f"Arduino SPI library headers not found: {spi_headers}")

env.Append(CPPPATH=[str(spi_headers)])
