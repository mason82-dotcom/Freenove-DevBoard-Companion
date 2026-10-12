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

## Follow-up: Stage 0 stable, stage 1 immediately panics (2026-10-12)

The operator ran the three-stage diagnostic firmware on the FNK0086. **Stage 0** produced continuous heartbeats for at least 56 seconds, with stable `heap=266016`, `psram=8382844`, and `loop_stack_hwm=6332`.

Immediately after serial command `1`, the firmware printed `DIAG STAGE 1: LVGL render; TFT SPI bypassed`, then panicked on core 1:

```text
Guru Meditation Error: Core 1 panic'ed (InstructionFetchError)
PC      : 0x3fcec070
EXCVADDR: 0x3fcec070
A0      : 0x8201e85d
A8      : 0x8200c35c
LBEG    : 0x4200c33d
LEND    : 0x4200c34e
Backtrace: 0x3fcec06d:0x3fcebf90 |<-CORRUPTED
ELF file SHA256: 5e6a3b605
```

**No `DIAG FLUSH` message appeared before the panic**, so the TFT SPI transfer function did not run during this reproduction. The PC/EXCVADDR point into a RAM/stack address range, suggesting an invalid branch/function pointer or stack/memory corruption; exact cause is still unproven. Do **not** enter stage 2 yet or modify the fixed CFG1 display setup.

### Next evidence: decode *this* firmware's caller and loop addresses before rebuilding

With the currently existing `.pio/build/fnk0086/firmware.elf` file in the Pi GUI worktree (must have SHA256 prefix `5e6a3b605`), execute:

```bash
cd ~/Projekte/Freenove-DevBoard-Companion-lvgl-test
sha256sum .pio/build/fnk0086/firmware.elf
bash /tmp/companion-decode-panic.sh \
  0x4201e85d 0x4200c35c 0x4200c33d 0x4200c34e 0x3fcec070
```

In an Xtensa backtrace `A0` and `A8` high-bit-coded code addresses can be mapped to their `0x420...` equivalents. The `0x3f...` invalid PC is not expected to resolve to a valid application text symbol. The address decode may identify the caller, without being sufficient on its own to prove why the branch target was corrupted.

**Do not run `pio run` before collecting these symbols:** a re-link changes addresses and invalidates a match to the crash ELF.


## Root-cause code defect identified after symbolication (2026-10-12)

The second captured crash matched its ELF SHA256 exactly (`5e6a3b60570c3bb712ca2d6ab1caa8f6865387fa3af675bbe01d106b4dd6ab54`).

`addr2line` resolved the caller to `lv_timer_exec()/lv_timer_handler()` at `lv_timer.c:313`; execution then entered `refr_invalid_areas()` around `lv_refr.c:595–608`. This is immediately before the normal display rendering/flush sequence.

Inspection of **official LVGL v8.3.11 source** established an application bug:
- `lv_disp_drv_register()` stores `disp->driver = driver` and documents "Only its pointer is saved!"
- `lv_indev_drv_register()` stores `indev->driver = driver`, also retaining caller-owned memory
- Our previous `CompanionUi::begin()` used two **automatic local** driver structs (`lv_disp_drv_t displayDriver;` and `lv_indev_drv_t inputDriver;`). Their lifetime ended when `begin()` returned, producing dangling LVGL driver pointers when stage 1 triggered `lv_timer_handler()`.
- The invalid execution PC `0x3fcec070` is consistent with a function pointer read from the former stack storage.

**Fix:** `displayDriver` and `inputDriver` are now static namespace-scope objects in `src/CompanionUi.cpp`; initialization and callback registration continue to use their stable addresses. The existing global draw buffer was already persistent. A host regression test checks the persistent declarations and rejects local shadow declarations.

**Verification pending:** This is a definitive code ownership defect with a matching crash mechanism, but a new build and real-device stage-1 / stage-2 acceptance test are still required before claiming the GUI's instability is resolved. The staged diagnostics are intentionally retained. Do not reapply the original automated pointer-lifetime pattern.

