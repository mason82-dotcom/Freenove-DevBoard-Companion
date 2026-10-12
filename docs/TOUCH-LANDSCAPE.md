# FNK0086 landscape-touch hardware evidence

**Board:** Freenove FNK0086. **Only permitted display setup:** `FNK0086A_2P8_CFG1_240x320_ST7789`.

On 2026-10-12, the operator explicitly confirmed that the four physical corners were touched **in this order**:
1. Top left: raw (234, 27)
2. Top right: raw (226, 318)
3. Bottom left: raw (0, 46)
4. Bottom right: raw (0, 287)

The measured touch sensor is oriented in portrait coordinates. With ST7789 `setRotation(1)`, the intended screen is **320x240**. The mapping is:

```text
screen_x = clamp(raw_y, 0, 319)
screen_y = 239 - clamp(raw_x, 0, 239)
```

The values map approximately to (27, 5), (318, 13), (46, 239), (287, 239). Measured physical corners are not precisely at logical pixel (0,0) because of bezel/finger offsets. Do **not** introduce interpolation, axis reversal, or calibration offsets before the on-device four-corner and center-point verification.

`include/TouchTransform.h` contains the pure, testable transformation. `src/main.cpp` on this feature branch logs raw and mapped coordinates. Host tests verify the exact four measured points, two logical limits and input clamping.

## Raspberry Pi: safe integration with existing local work

The operator's Pi checkout has uncommitted changes to six files. **Never run `git reset --hard`, `git checkout -- .`, or a pull that would overwrite local edits.** Review and commit/stash those files separately (and keep `include/VerifiedDisplaySetup.h` private), then integrate this feature branch with a normal merge after conflicts are reviewed. Do not blindly copy this feature branch's `src/main.cpp` over the Pi's changed version.

After integration, run:
```bash
python3 -m unittest discover -s tests -v
pio run -e fnk0086
pio device list
```

Upload only after the operator approves the target port. Verify `320x240` is logged, raw/mapped coordinates match the four physical corners and touch dots land beneath a fingertip. Do not claim this feature branch is hardware validated merely because the prior portrait smoke test passed.
