# PulsOx module lifecycle: stop measurement on Back

This update (PR #8, based on PR #7) only affects the GUIslice/LovyanGFX
`PulseOx` module and the MAX30102 sensor worker. The verified CFG1, LVGL9
baseline, remaining five module backends and FT6336U mapping are untouched.

## State transitions

| Navigation | UI action | Sensor-worker action | Expected Serial |
| --- | --- | --- | --- |
| Boot (Home) | No activation | Parked, no MAX30102 I2C access | `PulsOx worker: STARTED` |
| Home -> PulsOx | `requestActive(true)` | Start fresh session, initialize MAX30102, poll FIFO | `UI PulsOx: START REQUESTED`, `PulsOx MAX30102: START`, `PulsOx MAX30102: FOUND` |
| PulsOx -> Home (Back) | `requestActive(false)` | Block further sampling, acquire SharedI2C mutex, `sensor.shutDown()`, clear values/wave and park | `UI PulsOx: STOP REQUESTED`, `PulsOx MAX30102: SLEEP`, `PulsOx MAX30102: STOPPED` |
| Home -> PulsOx again | `requestActive(true)` | Reinitialize sensor, clear stale session, resume FIFO polling | Same START/FOUND pair as first visit |

The status strings and curve are invalidated immediately on a stop request.
Only the MAX30102 worker performs power management or I2C; the touch callback
and GUI loop do not block on the sensor bus. The FreeRTOS worker continues to
exist but sleeps indefinitely when PulsOx is inactive; thus *stopping the
measurement does not delete the task itself*. Quick repeated touch navigation
is handled by a separate latched pending-stop flag.

## Isolated build and hardware verification on Raspberry Pi

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git fetch origin feature/guislice-pulsox-stop-on-back
git worktree add --detach ../Freenove-DevBoard-Companion-pulsox-lifecycle \
  origin/feature/guislice-pulsox-stop-on-back
cp ~/Projekte/Freenove-DevBoard-Companion-pulsox/include/VerifiedDisplaySetup.h \
  ../Freenove-DevBoard-Companion-pulsox-lifecycle/include/
cd ../Freenove-DevBoard-Companion-pulsox-lifecycle
python3 -m unittest discover -s tests -v
pio run -c platformio.guislice-lgfx.ini -e fnk0086_lgfx
```

Only after a successful local build, **manually** flash the known serial
port with PlatformIO's standard `-t upload --upload-port ...` workflow,
**not** `firmware.factory.bin`. Do not flash CI-generated artifacts, as they
use a synthetic CFG1 header. Keep the working PR #7 firmware for rollback.

Record serial messages during 10–20 rapid page open/back cycles and check:

1. Every enter emits `START REQUESTED`, then `START` and `FOUND` if connected.
2. Every back emits `STOP REQUESTED`, followed by worker `SLEEP` and `STOPPED`.
3. Leaving PulsOx halts the sensor LED and no new BPM, SpO2 or IR wave samples
   are shown on return until a genuinely new measurement is acquired.
4. Other modules, touch navigation, heap and PSRAM remain stable.

`PulsOx worker: STARTED` alone means that the worker was created; it
**does not** imply that the MAX30102 was activated. This product is a
non-medical demonstration only.
