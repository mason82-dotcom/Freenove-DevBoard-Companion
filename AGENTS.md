# Codex Agent Instructions — Freenove-DevBoard-Companion

## Scope
Work exclusively within this repository unless explicitly authorized to inspect read-only reference projects. The target is the **Freenove FNK0086 ESP32-S3**, 8 MB Flash and 8 MB PSRAM. The **only approved TFT_eSPI setup** is `FNK0086A_2P8_CFG1_240x320_ST7789`. Never select CFG2, implement a profile selector, or silently infer a replacement profile.

## Immediate task (v0.1.0, Phase 0)
1. Inspect this repository and the documented CFG1 import tool.
2. Find the official Freenove CFG1 TFT_eSPI setup among pre-existing local source copies on the Pi, if available. If missing, explain which source is needed; do not fabricate a known-good setup.
3. Run `python3 -m unittest discover -s tests -v`; run `pio run -e fnk0086` when the configuration is present.
4. Fix reproducible build failures in this repository only. Preserve the exact CFG1 hardware setup and document each change.
5. Obtain serial device identity from `pio device list` and `/dev/serial/by-id/` before proposing a flash.
6. Provide a concise final report: changed files, test results, build result, board identity, remaining blockers.

## Safety boundaries
- **Never flash automatically.** Flash only after the user explicitly approves the target serial device and upload operation. A general request for development does not authorize selecting an ambiguous port.
- Never touch projects like Webradio_v2, OpenNextion_Hermes_Dashboard, OpenNextion_ELM327_Interface, FH-Clone, or SkyHub except read-only comparisons.
- No vehicle ECU configuration, fault-code clearing, drone flight commands, camera recording, Wi-Fi secrets or network control in Phase 0.
- Never use `setInsecure()` or upload credentials/certificates to GitHub.
- Do not claim a real-device hardware test or build succeeded without running it and reporting the evidence.
- Before updating the repository, inspect `git status` and avoid destructive commands.

## Next milestone
After successful CFG1 RGB and FT6336U hardware validation, integrate LVGL 8.3.11 with a modular application shell. Keep each new feature gated by tests.
