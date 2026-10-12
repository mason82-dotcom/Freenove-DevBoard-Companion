# Dependency migration: LVGL 9.6.0 (Freenove FNK0086)

**Status:** LVGL 9 migration implemented, **Raspberry Pi 5 PlatformIO build passed** and **all three staged on-device LVGL rendering checks passed** on 2026-10-12. GUI rendering and touch navigation are now observed working on board; ten-minute stability validation is pending. No firmware was uploaded remotely by GitHub.

## Real Raspberry Pi 5 build — PASS (2026-10-12)

The operator built `~/Projekte/Freenove-DevBoard-Companion-lvgl9-test` using `pio run -e fnk0086` and supplied the complete successful linker/size/firmware-image tail:

```text
RAM:   31.5% (used 103124 bytes from 327680 bytes)
Flash: 21.1% (used 664910 bytes from 3145728 bytes)
esptool v5.3.0
Successfully created ESP32-S3 image.
Successfully created combined binary image.
======== [SUCCESS] Took 672.05 seconds ========
```

The combined `firmware.factory.bin` image is an additional build artifact, **not** an instruction to flash at address zero. For normal testing, use the existing `pio run -e fnk0086 -t upload` workflow with the explicitly verified serial board port.

The current LVGL 9 shell remains a diagnostic firmware with staged activation (stage 0 pause, stage 1 LVGL without TFT SPI, stage 2 full rendering). Do not merge to `main` until on-device stages and navigation have been tested. Do not assume the LVGL 8 runtime crash is cured solely because LVGL 9 builds.

## Real FNK0086 diagnostic runtime — stages 0/1/2 PASS (2026-10-12)

The operator has flashed the LVGL 9.6.0 build and supplied live UART logs from the board:

| Stage | Observed evidence | Result |
|---|---|---|
| 0, LVGL timer paused | Uptime through 26 seconds; heap=266024, free PSRAM=8383572, loop stack HWM=6296 | PASS |
| 1, LVGL rendering without TFT SPI | `DIAG FLUSH #1..#3` for contiguous 320x20 slices; uptime through 40 seconds, heap=266024, PSRAM=8383572, loop stack HWM=3720 | PASS (about 14 seconds) |
| 2, LVGL rendering with TFT SPI | `DIAG SPI ENTER pixels=6400`, `DIAG SPI EXIT`; uptime through 52 seconds, heap=266000, PSRAM=8383524, loop stack HWM=3720 | PASS (about 12 seconds) |

The initial LVGL 8.3 `InstructionFetchError` and corrupted backtrace did not recur during these checks. All three stages now execute and the actual SPI flush finishes. **A user-supplied photo now confirms all six tiles visibly rendered without obvious corruption, and the user reports touch navigation functions without problems. Per-tile serial logs and the 10-minute soak remain unconfirmed.** Do not call this a full hardware acceptance or merge the PR to main on these logs alone.

## Hardware photo and navigation — 2026-10-12

The user supplied a real FNK0086 photograph showing all six module tiles (Hermes, Webradio, OBD2, Drohne, System, Kamera), readable typography and footer. No obvious rendering artifacts. The photographed board is upright while the GUI uses the intended landscape layout. The user confirms navigation works without problems. No source image was copied into the repository. A ten-minute stability run is still pending.

## Extended stage-2 navigation/uptime log — partial soak PASS (2026-10-12)

The user supplied additional live UART logs extending through **uptime 270102 ms** (~4 min 30 s since boot). Stage 2 was activated at approximately 40000 ms; the **verified stage-2 duration is therefore approximately 230 s (3 min 50 s), not yet 10 minutes**.

Every sampled heartbeat from 204062–270102 ms reports:
- `stage=2`
- `heap=266000` bytes free (unchanged)
- `psram=8383524` bytes free (unchanged)
- `loop_stack_hwm=3672` (unchanged across supplied interval; 48 below earlier 3720-byte observed minimum)

The operator's UART log confirms navigation events with back transitions: `UI NAV: System`, `Home`, `Kamera`, `Home`, `Drohne`, `Home`, `Webradio`, `Home`, and later `System`. No panic, reset or progressive loss of free heap/PSRAM is present in the supplied interval. The trace does not separately show Hermes and OBD2 navigation.

**Status: extended short soak successful, 10-minute stage-2 criterion still open.** With stage 2 starting near uptime 40 seconds, continue to approximately `uptime >= 640000 ms` while keeping stage 2 active to obtain ten full minutes after enabling the display. No reflash is needed merely to complete the test.

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

1. Host Python and constexpr C++ tests green (**passed: 17/17**).
2. Actual `pio run -e fnk0086` on Pi reports SUCCESS with no linker errors (**passed 2026-10-12**).
3. Stages 0, 1 and 2 stable and LVGL flush reaches `DIAG SPI EXIT` (**passed in the short staged hardware test**); all six tiles now **visually confirmed by user photo**.
4. Touch navigation works (**confirmed by user**); explicit per-tile serial checks are optional.
5. Ten-minute serial test with stable heap/PSRAM and no Guru Meditation Error (**partial pass up to uptime 270102 ms; target approximately uptime >= 640000 ms for 10 minutes at stage 2**).

No merge to `main` until both the LVGL9 build and real-device validation succeed.

## References

- LVGL release notes: https://github.com/lvgl/lvgl/releases/tag/v9.6.0
- LVGL 9.6 display API: https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/display/lv_display.h
- LVGL 9.6 input API: https://github.com/lvgl/lvgl/blob/v9.6.0/include/lvgl/indev/lv_indev.h
- pioarduino release: https://github.com/pioarduino/platform-espressif32/releases/tag/55.03.39
- TFT_eSPI releases: https://github.com/Bodmer/TFT_eSPI/releases
