# Dependency migration: LVGL 9.6.0 (Freenove FNK0086)

**Status:** Migration implemented on a separate feature branch. Unit tests run via GitHub Actions; an actual ESP32-S3 PlatformIO build and hardware validation are still pending. No firmware was uploaded remotely.

## Versions

| Component | Previous | Proposed pinned version |
|---|---|---|
| LVGL | 8.3.11 | **9.6.0** (stable release 2026-09-16) |
| pioarduino `platform-espressif32` | 55.03.32 / Arduino-ESP32 3.3.2 | **55.03.39** / Arduino-ESP32 3.3.9 (verified immutable release) |
| TFT_eSPI | 2.5.43 | **2.5.43** (already latest published TFT_eSPI release) |
| Wire, SPI | bundled with old Arduino framework | bundled with the updated Arduino framework |
| Board-specific TFT setup | Freenove CFG1 | **Unchanged, locked to CFG1** |
| Touch mapping | FT6336U measured 5/5 | **Unchanged** |

The moving pioarduino `stable` URL is intentionally **not** used: reproducible dependency pins are preferred. The current pioarduino stable channel may provide a newer Arduino core than 3.3.9; do not claim 55.03.39 is the absolute latest until a newer fixed release URL is independently verified.

## API changes made

LVGL 9 no longer uses LVGL 8's `lv_disp_drv_t`, `lv_disp_draw_buf_t`, and `lv_indev_drv_t` registration APIs. This migration replaces them with:

- `lv_display_create(320, 240)`
- `lv_display_set_color_format(..., LV_COLOR_FORMAT_RGB565)`
- `lv_display_set_buffers(..., LV_DISPLAY_RENDER_MODE_PARTIAL)`
- `lv_display_set_flush_cb(...)` and `lv_display_flush_ready(...)`
- `lv_indev_create()`, `lv_indev_set_read_cb(...)` and `lv_indev_set_display(...)`
- `lv_button_create()`, `lv_screen_active()`, `lv_obj_remove_flag()`

LVGL 9 internally owns the created display/input objects, eliminating the invalid stack-lifetime pattern found in the LVGL 8 prototype. The partial 320 × 20 × 2 byte RGB565 draw buffer stays static and 4-byte aligned in internal RAM. The UI remains 320 × 240, CFG1 rotation 1, with the previously validated transformation `x=rawY; y=239-rawX`.

The **staged diagnostic boot is retained**: enter `1` to test the renderer without sending TFT SPI transfers, then `2` only if stage 1 is stable. This migration has not yet passed those stages on hardware. It does not enable Wi-Fi, vehicle commands, drone control, sound or camera capture.

## Safe Raspberry Pi test worktree

The original Pi checkout and the earlier LVGL 8 crash-debug worktree must remain unchanged. Use a new working directory:

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git fetch origin feature/deps-lvgl9
git worktree add --detach ../Freenove-DevBoard-Companion-lvgl9-test \
  origin/feature/deps-lvgl9
test -f include/VerifiedDisplaySetup.h
cp include/VerifiedDisplaySetup.h \
  ../Freenove-DevBoard-Companion-lvgl9-test/include/
cd ../Freenove-DevBoard-Companion-lvgl9-test
python3 -m unittest discover -s tests -v
pio run -e fnk0086
```

If the original verified CFG1 header is not available, do not substitute CFG2; see `tools/import_verified_display_setup.py`. The first build may download significant new packages and take longer than prior builds.

**Do not flash immediately after a successful build without reviewing the compilation report and re-confirming the board's USB port.** The existing hardware-validated five-point touch test remains available at `~/Projekte/Freenove-DevBoard-Companion-touch-test`.

## Acceptance criteria

1. Host Python and constexpr C++ tests green.
2. Actual `pio run -e fnk0086` on Pi reports SUCCESS with no linker errors.
3. Stages 0, 1 and 2 stable; LVGL flush reaches `DIAG SPI EXIT` and renders six tiles.
4. All six tiles respond to touch and back navigation works.
5. Ten-minute serial test with stable heap/PSRAM and no Guru Meditation Error.

No merge to `main` until both the LVGL9 build and real-device validation succeed.

## References

- LVGL release notes: https://github.com/lvgl/lvgl/releases/tag/v9.6.0
- LVGL 9.6 display API: https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/display/lv_display.h
- LVGL 9.6 input API: https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/indev/lv_indev.h
- pioarduino release: https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.39
- TFT_eSPI releases: https://github.com/Bodmer/TFT_eSPI/releases
