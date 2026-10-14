# GUIslice + LovyanGFX (FNK0086 experiment)

**Status: ESP32-S3 Raspberry Pi 5 build with real SHA256-verified CFG1 PASSED on 2026-10-12; on-device GUI, touch and stability validation remain pending.** This is an isolated prototype. It does **not** change the LVGL 9 firmware, default `platformio.ini`, stable touch calibration, the current Raspberry Pi working tree, or any external system.

## Hardware and libraries

- **GUIslice 0.17.2:** six home buttons, seven pages, back navigation and read-only System page.
- **LovyanGFX 1.2.32:** ONLY ST7789 graphics driver, 240×320 physical panel, rotated to 320×240 by GUIslice's `GSLC_ROTATE 1`.
- **GUIslice TFT_eSPI adapter:** the existing upstream GUIslice `DRV_DISP_TFT_ESPI` backend is connected to LovyanGFX's upstream `LGFX_TFT_eSPI.hpp` compatibility layer using the project's `include/TFT_eSPI.h` shim. **The Bodmer TFT_eSPI package is NOT installed or linked.** This path is experimental and must be compiled to assess actual API compatibility.
- **FT6336U:** the already validated read-only `BoardTouch` I²C handler, via GUIslice `DRV_TOUCH_HANDLER`. It passes the existing `TouchTransform::rawToLandscape` positions with `gslc_SetTouchRemapEn(false)` to avoid double rotation.
- **CFG1 only:** the LovyanGFX panel class reads locally imported `include/VerifiedDisplaySetup.h` macros; requires ST7789, MOSI20, SCLK21, DC0. The hardware-confirmed panel size 240×320 is configured explicitly in the LovyanGFX panel because the real CFG1 header does not define `TFT_WIDTH`/`TFT_HEIGHT`; if they are defined, their values are still strictly validated. No default CFG2 and no board autodetect. Other ST7789 settings (color order, inversion, backlight control) need on-board validation.
- **ESP32-S3:** pioarduino 55.03.39, 8MB Flash/PSRAM, same partitions file as LVGL.

The prototype is read-only. Hermes, Webradio, OBD2, Drohne and Kamera are placeholder pages with **no network requests or device commands**. System shows only locally reported ESP32 data.

## Real CFG1 header verification and build fix (2026-10-12)

The user's `VerifiedDisplaySetup.h` in the GUIslice test worktree and the previously working LVGL 9 test worktree have the **same SHA256**: `209d0836b1d43a265a6b32a2c74a2ea523033e2b2aa704feff8ffc3d23487880`.

Its effective definitions include `ST7789_DRIVER`, `TFT_MOSI=20`, `TFT_SCLK=21`, `TFT_DC=0` and `SPI_FREQUENCY=80000000`. It intentionally has no `TFT_WIDTH` or `TFT_HEIGHT` macros. Initial prototype builds incorrectly rejected it.

The adapter was corrected to **require** `ST7789_DRIVER`, verify all three GPIOs, and reject conflicting dimensions **only if those optional macros are supplied**. It explicitly configures the already hardware-proven ST7789 physical resolution as 240×320. No edits to the user's CFG1 header and no board-profile fallback are necessary. The compile-only CI fixture likewise omits both dimension macros to protect this exact scenario. CI is only an approximation of the full vendor CFG1 file.

## Real Raspberry Pi 5 compile — PASS (2026-10-12)

The operator built this exact experimental GUI with the real locally validated CFG1 header. Full ESP32-S3 Arduino 3.3.9 compile and link succeeded:

```text
RAM:   10.9% (used 35816 bytes from 327680 bytes)
Flash: 14.2% (used 447619 bytes from 3145728 bytes)
Successfully created ESP32-S3 image.
Successfully created combined binary image.
========= [SUCCESS] Took 41.38 seconds =========
```

The previous LVGL9 Pi build reported 664910 bytes Flash and 103124 bytes static RAM. These are **different functionality baselines**: the GUI and read-only System status can be compared, but neither platform has the actual Hermes, OBD2, drone or camera integrations yet. The GUIslice image is **NOT yet approved on hardware**.

**Pre-flash review completed for the verified CFG1:** active `TFT_CS=-1`, no active `TFT_RST` or `TFT_BL` GPIO; `TFT_RGB_ORDER=TFT_BGR`, `TFT_INVERSION_OFF`, and `SPI_FREQUENCY=80000000`. The board serial adapter was checked as VID:PID `1A86:55D3`, serial `58FC003021`, symlink `/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00` → `/dev/ttyACM0`. To match the previously working TFT_eSPI ST7789 driver, LovyanGFX now uses **SPI mode 3** and the requested **BGR** color ordering, while the experimental first-boot SPI transfer rate is deliberately capped at **40 MHz** (no modification to the original 80-MHz CFG1 header). No automatic flashing. Confirm whether the GUI is readable (no inverted colors, wrong offsets or flicker), check the `FT6336U probe` log and every tile/back button before marking this test passed. The `firmware.factory.bin` artifact is not intended for manual flashing at 0x0.

## System-page live refresh — repair awaiting hardware confirmation

The operator's UART telemetry shows stable uptime progressing to **236,494 ms** (3:56.494), and free Heap **335,068 B** plus free PSRAM **8,383,268 B** constant in the observed log. The on-screen System Uptime was reported **not updating**; this is a GUIslice text rendering issue, not an ESP32 system timer stall.

The fixed `updateSystem()` formats the next Uptime, Heap and PSRAM strings into separate temporary buffers, then calls `gslc_ElemSetTxtStr` to copy and trigger GUIslice redraw only if the text changed. Dynamic labels also fill their own black background on repaint, clearing previous glyphs. `gslc_Update` runs every loop and `updateSystem` is called once per second while the System page is open. A specific host regression test checks this source contract.

**Hardware validation pending:** fetch the latest commit, compile with the *locally verified* CFG1 header, flash manually after checking the serial device, then verify the Uptime number increases once per second on the System page. Do not mark the live-refresh feature as passed from UART heartbeats alone.

## Six independent live status modules (stacked PR #5)

This next increment **preserves PR #4 as the working hardware baseline** and is implemented on `feature/guislice-six-modules`, based on the experiment branch. The firmware still has *zero* external-control side effects. The six pages use separate adapters in `src/modules/`, all dispatched by `ModuleDiagnostics::capture()`.

| Module | Current read-only fields | Not yet connected / deliberately disabled |
| --- | --- | --- |
| Hermes | Agent link, gateway configuration, response status, diagnostic uptime | Pi Hermes-Agent API/auth |
| Webradio | Audio status, verified I2S BCLK42/WS14/DOUT41, stream/SD state, diagnostic uptime | I2S startup, station selection, stream buffering |
| OBD2 | ELM327 link, PID data state, zero TX, diagnostic uptime | Wi-Fi ELM327 and *strictly read-only* PID polling |
| Drohne | Gateway, telemetry, command lockout, diagnostic uptime | FH-Clone / SkyHub / DJI telemetry API |
| System | Actual uptime, free heap, free PSRAM, CPU MHz / minimum heap | Additional history charts |
| Kamera | Camera sensor/preview status, actual available PSRAM, unverified camera pins | Camera GPIO configuration and frame capture |

**Display update contract:** GUIslice owns six sets of four persistent text buffers. Once a second, **only the active module** gets a fresh local snapshot, and all new strings are passed through `gslc_ElemSetTxtStr()`. This preserves the already fixed live Uptime behavior (no in-place mutation before redraw). Every module refreshes immediately upon opening, and the status fields clear their own black background on refresh.

**No connectivity implied:** A diagnostic string such as `Gateway: nicht verbunden` is a truthful unconfigured state, not a failed network probe; opening a module does **not** transmit OBD/ELM frames, drone commands, start I2S playback, activate the camera, or connect to remote services. Their adapters can be extended individually after credentials, safe read-only APIs and the physical pin map have been validated.

### Check PR #5 without touching PR #4

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git fetch origin feature/guislice-six-modules

git worktree add --detach ../Freenove-DevBoard-Companion-six-modules \
  origin/feature/guislice-six-modules

# Copy the identical, locally validated CFG1 header (already ignored by Git)
cp ~/Projekte/Freenove-DevBoard-Companion-lvgl9-test/include/VerifiedDisplaySetup.h \
   ../Freenove-DevBoard-Companion-six-modules/include/

cd ../Freenove-DevBoard-Companion-six-modules
python3 -m unittest discover -s tests -v
pio run -c platformio.guislice-lgfx.ini -e fnk0086_lgfx
```

Only after both builds pass and the user explicitly decides to do the first *new* hardware test, check `pio device list` and use PlatformIO's normal upload method. Do not use `firmware.factory.bin` or merge this experiment into `main`.

## Graphische GUIslice-Dashboards und PulsOxMonitor (neuer Branch)

Dieser Zweig ersetzt **Kamera durch PulsOx**, enthält aber weiterhin **genau sechs Module**. Er basiert ausschließlich auf dem visuell bestätigten Sechs-Modul-PR #5, **nicht** auf dem separaten Hermes-HTTPS-PR #6. Es wird keine bestehende Firmware automatisch ersetzt oder geflasht.

### Bildschirme
- **Hermes:** vernetztes Status-/Knotendiagramm, bis zur separaten TLS-Freigabe grau und offline
- **Webradio:** ruhender Equalizer, solange kein I2S-Player verbunden ist, keine falschen Audiodaten
- **OBD2:** deaktivierte Skala, kein ELM327- oder Fahrzeugkommando
- **Drohne:** statisches Lage-/Telemetrie-Symbol, kein künstlicher Flugzustand und keine Befehle
- **System:** tatsächliche Heap-/PSRAM-Anteile sowie echte Heap-Verlaufskurve mit 72 Werten
- **PulsOx:** Herzsymbol und echte IR-Wellenform aus dem MAX30102, darunter Sensor, Finger, BPM und SpO2, inklusive Invalid-/Stale-Behandlung

Sämtliche Canvas-Elemente zeichnen über **die vorhandene GUIslice-Render-API** (gslc_DrawLine, gslc_DrawFillRect, gslc_DrawFillCircle). Es gibt nur **ein** ST7789-Displayobjekt; kein zusätzlicher TFT_eSPI-Treiber, kein zweiter UI-Task und keine parallelen Displayschreibzugriffe. Die vier Textstatuszeilen jedes Moduls bleiben über den bisherigen sicheren `gslc_ElemSetTxtStr`-Pfad aktuell.

### MAX30102-Anbindung aus hochgeladenem PulsOxMonitor

SparkFun MAX3010x 1.1.2, Sample-Rate 100 Hz / 4-fach-Mittelung = 25 effektive Samples pro Sekunde, Rot-/IR-FIFO, PBA-BPM, Maxim-SpO2 über 100 Samples mit 25 Samples Verschiebung, Finger-Schwelle IR=50.000. Der eigene FreeRTOS-Task läuft auf Core 0; Display/Touch bleiben auf dem Arduino/UI-Task. Er verwendet **keine LED-/Reset-Touch-Gesten**, die bestehende GUI stören könnten.

**Gleichzeitige I²C-Nutzung:** FT6336U 0x38 und MAX30102 0x57 teilen sich SDA2/SCL1. `SharedI2C` initialisiert den verifizierten Bus bei 400 kHz und stellt eine Mutex-Guard für sämtliche Touch-/Sensortransaktionen bereit. Der UI-Thread liest nur lock-geschützte Daten-Snapshots, niemals den Sensor über Wire. Ein fehlender Sensor soll die fünf anderen Module nicht blockieren. Sensor-FIFO-Zugriff, Reconnect und Touch müssen am realen FNK0086 geprüft werden.

**Sicherheit:** Kein Medizinprodukt. SpO2-/BPM-Werte dienen ausschließlich Demonstrationszwecken und dürfen nicht zur Diagnose oder für Behandlungsentscheidungen verwendet werden. Bei ungültigen oder überalterten Daten steht `--`, nicht ein erfundener Vitalwert.

### Lokaler Test (ohne Einfluss auf bestehenden Pi-Worktree)

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git fetch origin feature/guislice-pulsox-max30102
git worktree add --detach ../Freenove-DevBoard-Companion-pulsox \
  origin/feature/guislice-pulsox-max30102
cp ~/Projekte/Freenove-DevBoard-Companion-lvgl9-test/include/VerifiedDisplaySetup.h \
  ../Freenove-DevBoard-Companion-pulsox/include/
cd ../Freenove-DevBoard-Companion-pulsox
python3 -m unittest discover -s tests -v
pio run -c platformio.guislice-lgfx.ini -e fnk0086_lgfx
```

Vor einem Hardwaretest erst Build, USB-Gerät und I²C-Touchverhalten prüfen. **Nicht CI-Artefakte flashen** (synthetischer CFG1-Header). Die Webcam/Kamera-Integrationsdatei ist nur im neuen Branch entfernt; PR #5 und #6 bleiben erhalten.

## Raspberry Pi build, without disturbing the working LVGL project

Use a completely new Git worktree; do not perform a force reset and do not overwrite the original Pi checkout:

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git fetch origin experiment/guislice-lovyangfx
git worktree add --detach ../Freenove-DevBoard-Companion-guislice-lgfx \
  origin/experiment/guislice-lovyangfx

# Only copy a locally hardware-validated CFG1 setup (never use CFG2):
cp ~/Projekte/Freenove-DevBoard-Companion-lvgl9-test/include/VerifiedDisplaySetup.h \
   ../Freenove-DevBoard-Companion-guislice-lgfx/include/

cd ../Freenove-DevBoard-Companion-guislice-lgfx
python3 -m unittest discover -s tests -v
pio run -c platformio.guislice-lgfx.ini -e fnk0086_lgfx
```

**Do not flash automatically.** The display driver is new, and the current working LVGL 9 version is the safe rollback. After fetching the latest mode-3/BGR/40-MHz patch and building with the same real CFG1 header, confirm the attached board port again. Normal (optional) manual upload is:

```bash
pio device list
pio run -c platformio.guislice-lgfx.ini -e fnk0086_lgfx -t upload \
  --upload-port /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00
pio device monitor \
  -p /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00 \
  -b 115200
```

Expect startup text `FNK0086 GUIslice 0.17.2 + LovyanGFX 1.2.32 EXPERIMENT`. If display dimensions are not 320×240, GUIslice initialization fails and **the firmware must not be declared hardware-ready**. If touch buttons are offset, check GUIslice remapping before changing the proven `TouchTransform.h`.

## Acceptance gates

1. Python/host tests pass.
2. Full ESP32-S3 PlatformIO compile/link passes with the real CFG1 header.
3. On-board color bands / 320×240 placement match the LVGL validated reference.
4. All six tiles can be opened and returned from; no phantom touch or axis reversal.
5. A ten-minute interactive run, stable heap and PSRAM, no Guru Meditation errors.
6. Compare firmware size, RAM and GUI responsiveness to the last LVGL firmware.

**The Slint experimental work, LVGL PR #3 and user test worktrees are independent; nothing was merged or flashed automatically.**

Upstream:
- GUIslice: https://github.com/ImpulseAdventure/GUIslice/releases/tag/v0.17.2 (MIT).
- LovyanGFX: https://github.com/lovyan03/LovyanGFX (FreeBSD 2-clause).
- LovyanGFX compatibility header: https://github.com/lovyan03/LovyanGFX/blob/master/src/LGFX_TFT_eSPI.hpp.
