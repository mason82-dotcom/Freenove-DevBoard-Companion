# LVGL 8.3.11 ESP32-S3 start crash — staged diagnostic

## Evidence from the first crash (2026-10-12)

The first `feature/lvgl-companion-shell` firmware (before this diagnostic change) built and flashed successfully, and printed:

```text
FT6336U probe: FOUND
PSRAM at boot: 8382844 bytes free
LVGL shell ready: 320x240, touch=OK, modules=6
```

It then entered a reboot loop, with Core 0 cache/MMU fault and a secondary IDLE0 stack-canary warning. The core dump partition/config warning does not establish the original cause.

The user ran `tools/decode_panic.sh` against the original `firmware.elf`, SHA256 `819fb482e33aa7c05eb37124924cceee9d36d11ad31a9895c90cc454bdf0682a`, matching the ELF ID printed at the crash. The symbolicated frames resolve to ESP-IDF panic/backtrace functions and CPU wait, **not the initiating application frame**. The corrupt backtrace cannot conclusively establish root cause.

## New diagnostic stages

The diagnostic firmware **boots at stage 0**, holding the LVGL timer. The TFT controller is initialized and LVGL objects are created, but LVGL's rendering/flush loop is not executed until instructed.

Use the USB serial console at 115200 baud. **Send one digit followed by Enter**, e.g. with PlatformIO's `send_on_enter` filter.

| Digit | Stage | Effect | What it isolates |
|---|---|---|---|
| `0` | Pause | LVGL timer is not called | Baseline initialization and heap/PSRAM |
| `1` | No SPI | LVGL drawing/timer proceeds but display flush returns without SPI write; touch input disabled | LVGL rendering, timers, objects, draw buffer, loop-task stack |
| `2` | SPI | LVGL drawing and TFT_eSPI SPI flushing active; touch input enabled | First physical TFT transfer and then navigation |

On stage changes the framebuffer is invalidated and flush log count resets. The first few flush areas and the first TFT transfer boundaries are logged.

**Do not send `2` until stage `1` has remained stable for about 20 seconds.** A blank/stale display in stage 0/1 is expected.

### How to test safely on the Raspberry Pi 5

First stop the existing `pio device monitor` with Ctrl+C. The previous firmware currently reboots; the new version is a diagnostic, not a validated fix.

```bash
cd ~/Projekte/Freenove-DevBoard-Companion-lvgl-test
git status --short
git fetch origin feature/lvgl-companion-shell
git switch --detach origin/feature/lvgl-companion-shell
python3 -m unittest discover -s tests -v
pio run -e fnk0086
```

Stop and reconcile if `git status` shows unexpected changes. Do not overwrite the local `include/VerifiedDisplaySetup.h` (verified CFG1) or other worktree edits.

Verify the known device is still the intended FNK0086 before uploading:

```bash
pio device list
ls -l /dev/serial/by-id/
pio run -e fnk0086 -t upload --upload-port \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00
pio device monitor -p \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00 \
  -b 115200 -f send_on_enter
```

Wait for 2–3 `DIAG heartbeat` messages in stage 0. Enter `1`, wait ~20 seconds while capturing logs, then enter `2`. If a panic occurs, note which stage, whether `DIAG FLUSH`, `DIAG SPI ENTER` or `DIAG SPI EXIT` appeared, and copy the full panic output.

### Result interpretation

- **Crashes without sending a command**: initialization or concurrent system failure; stage 0 is not safe.
- **Stage 0 stable, stage 1 crashes**: rendering or LVGL timer stack/memory; TFT SPI driver not required to trigger the crash.
- **Stage 1 stable, stage 2 crashes before SPI EXIT**: first TFT write path is implicated; this does **not** by itself prove the exact defective function.
- **Stage 2 stable until touch**: input handling or LVGL events/navigation implicated.
- **Stage 2 stable for 10 minutes with navigation**: candidate for further validation; no production readiness is implied.

The serial heartbeat logs `loop_stack_hwm`, the Arduino loop task's remaining stack high-watermark, and heap/PSRAM. Zero or persistently very low stack reserve supports investigating stack size; it does not independently prove a buffer overflow.

The known-good **5/5** FNK0086 touch firmware remains in `~/Projekte/Freenove-DevBoard-Companion-touch-test` and may be restored after verifying the board port. CFG1 is unchanged in every stage. No network, vehicle, drone or camera commands are executed.
