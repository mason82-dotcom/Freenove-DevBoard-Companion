#!/usr/bin/env python3
"""Provision the FNK0086 Hermes HTTPS experiment from the already working
OpenNextion Hermes Touch local configuration; never print any secret material.

Usage from the Freenove repo root:
  python3 prototypes/guislice-lovyangfx/scripts/provision_hermes_from_pi.py

Requirements: Existing OpenNextion private Config.h and public LocalCa.h,
and a passing scripts/pi/tls-smoke.sh. This tool NEVER contacts the network,
changes the Pi Hermes service, or flashes the ESP32.
"""
import os
import re
from pathlib import Path

HERE = Path(__file__).resolve()
FW_INCLUDE = HERE.parents[1] / "include"
SOURCE = Path.home() / "Projekte" / "OpenNextion_Hermes_Dashboard" / "firmware" / "HermesTouch"

def main():
    source_config = SOURCE / "Config.h"
    source_ca = SOURCE / "LocalCa.h"
    target_config = FW_INCLUDE / "HermesDeviceConfig.h"
    target_ca = FW_INCLUDE / "HermesLocalCa.h"
    if not source_config.is_file() or not source_ca.is_file():
        raise SystemExit("Missing existing local Hermes Touch Config.h or LocalCa.h; run its setup first.")
    if target_config.exists() or target_ca.exists():
        raise SystemExit("Local Hermes config/CA already exists; no overwrite. Edit or remove it manually.")
    config = source_config.read_text(encoding="utf-8")
    cert = source_ca.read_text(encoding="utf-8")
    for name in ("HT_WIFI_SSID", "HT_WIFI_PASSWORD", "HT_BRIDGE_URL", "HT_DEVICE_TOKEN"):
        if not re.search(r"^\s*#\s*define\s+" + name + r"\b", config, flags=re.MULTILINE):
            raise SystemExit(f"Missing local configuration item: {name}")
    if not re.search(r"^\s*#\s*define\s+HT_DEMO_MODE\s+0\b", config, re.MULTILINE):
        raise SystemExit("The source Hermes firmware must be locally configured for connected mode.")
    if "HT_CA_CERT" not in cert or "BEGIN CERTIFICATE" not in cert:
        raise SystemExit("Existing LocalCa.h has no recognizable trust anchor.")
    if "YOUR_WIFI" in config or "PASTE_YOUR" in config:
        raise SystemExit("Source is a sample template, not a locally configured device.")
    # Only enable read-only Hermes status retrieval. Other original optional
    # features in Config.h are NOT used by the FNK0086 firmware.
    config = ("#pragma once\n#define HERMES_LIVE_ENABLED 1\n"
              "// Local private file: never commit, log or share.\n" + config)
    FW_INCLUDE.mkdir(parents=True, exist_ok=True)
    for output, payload in ((target_config, config), (target_ca, cert)):
        fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(fd, "w", encoding="utf-8") as handle:
            handle.write(payload)
    print("Hermes HTTPS device configuration provisioned locally (mode 0600).")
    print("No passwords, tokens or certificates were printed.")
    print("Next: build and test; there is NO automatic firmware upload.")

if __name__ == "__main__":
    main()
