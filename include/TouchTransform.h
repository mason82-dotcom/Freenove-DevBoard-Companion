#pragma once
#include <stdint.h>

// Freenove FNK0086: FT6336U raw portrait 240x320 -> ST7789 rotation(1) 320x240.
// Validated from the four physical corners (top-left, top-right, bottom-left,
// bottom-right), touched in that exact order on 2026-10-12.
// Do not change the CFG1 driver profile to correct touch orientation.
namespace TouchTransform {
struct Point {
    uint16_t x;
    uint16_t y;
};

constexpr uint16_t clampCoordinate(uint16_t value, uint16_t maximum) {
    return value <= maximum ? value : maximum;
}

constexpr Point rawToLandscape(uint16_t rawX, uint16_t rawY) {
    const uint16_t x = clampCoordinate(rawY, 319);
    const uint16_t boundedRawX = clampCoordinate(rawX, 239);
    return {x, static_cast<uint16_t>(239 - boundedRawX)};
}
// Integer-distance hit test: avoids floating point and remains usable on-device.
constexpr bool isNear(Point actual, Point expected, uint16_t radius = 40) {
    const int32_t dx = static_cast<int32_t>(actual.x) - expected.x;
    const int32_t dy = static_cast<int32_t>(actual.y) - expected.y;
    const int32_t limit = static_cast<int32_t>(radius);
    return dx * dx + dy * dy <= limit * limit;
}
}  // namespace TouchTransform
