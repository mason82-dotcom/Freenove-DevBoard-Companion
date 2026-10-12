# GUIslice + LovyanGFX (FNK0086 experiment)

**Status: source-only experimental port; build and hardware validation pending.** This is an isolated prototype. It does **not** change the LVGL 9 firmware, default `platformio.ini`, stable touch calibration, the current Raspberry Pi working tree, or any external system.

## Hardware and libraries

- **GUIslice 0.17.2:** six home buttons, seven pages, back navigation and read-only System page.
- **LovyanGFX 1.2.32:** ONLY ST7789 graphics driver, 240×320 physical panel, rotated to 320×240 by GUIslice's `GSLC_ROTATE 1`.
- **GUIslice TFT_eSPI adapter:** the existing upstream GUIslice `DRV_DISP_TFT_ESPI` backend is connected to LovyanGFX's upstream `LGFX_TFT_eSPI.hpp` compatibility layer using the project's `include/TFT_eSPI.h` shim. **The Bodmer TFT_eSPI package is NOT installed or linked.** This path is experimental and must be compiled to assess actual API compatibility.
- **FT6336U:** the already validated read-only `BoardTouch` I²C handler, via GUIslice `DRV_TOUCH_HANDLER`. It passes the existing `TouchTransform::rawToLandscape` positions with `gslc_SetTouchRemapEn(false)` to avoid double rotation.
- **CFG1 only:** the LovyanGFX panel class reads locally imported `include/VerifiedDisplaySetup.h` macros; requires ST7789, size 240×320, MOSI20, SCLK21, DC0. No default CFG2 and no board autodetect. Other ST7789 settings (color order, inversion, backlight control) need on-board validation.
- **ESP32-S3:** pioarduino 55.03.39, 8MB Flash/PSRAM, same partitions file as LVGL.

The prototype is read-only. Hermes, Webradio, OBD2, Drohne and Kamera are placeholder pages with **no network requests or device commands**. System shows only locally reported ESP32 data.

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

**Do not flash automatically.** The display driver is new, and the current working LVGL 9 version is the safe rollback. Only after a successful native build, review the log and confirm the attached board port. Normal (optional) manual upload is:

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
