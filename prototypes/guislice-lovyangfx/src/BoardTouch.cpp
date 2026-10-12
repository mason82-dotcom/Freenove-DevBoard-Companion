// Isolated prototype copy of the verified, read-only FT6336U reader.
#include <Wire.h>
#include "BoardPins.h"
#include "BoardTouch.h"

namespace {
bool found = false;
}

namespace BoardTouch {

bool begin() {
    Wire.begin(BoardPins::kTouchSda, BoardPins::kTouchScl);
    Wire.setTimeOut(20);
    Wire.beginTransmission(BoardPins::kTouchAddress);
    found = Wire.endTransmission() == 0;
    return found;
}

bool available() {
    return found;
}

bool readRaw(uint16_t& x, uint16_t& y) {
    if (!found) return false;

    // TD_STATUS(0x02), P1_XH, P1_XL, P1_YH, P1_YL.
    Wire.beginTransmission(BoardPins::kTouchAddress);
    Wire.write(static_cast<uint8_t>(0x02));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<uint8_t>(BoardPins::kTouchAddress),
                         static_cast<uint8_t>(5)) != 5) return false;

    const uint8_t count = Wire.read() & 0x0F;
    const uint8_t xh = Wire.read();
    const uint8_t xl = Wire.read();
    const uint8_t yh = Wire.read();
    const uint8_t yl = Wire.read();
    if (count != 1) return false;

    x = static_cast<uint16_t>(((xh & 0x0F) << 8) | xl);
    y = static_cast<uint16_t>(((yh & 0x0F) << 8) | yl);
    return true;
}
}
