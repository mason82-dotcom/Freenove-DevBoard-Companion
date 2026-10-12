#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include "BoardPins.h"
#include "TouchTransform.h"

// Phase 0: isolated, read-only FNK0086 CFG1 hardware acceptance test.
// No Wi-Fi, no audio, no Hermes, no OBD or drone write/control operations.
TFT_eSPI display;

namespace {
using TouchTransform::Point;
constexpr Point kTargets[] = {
    {24, 24},    // 1: top-left
    {296, 24},   // 2: top-right
    {24, 216},   // 3: bottom-left
    {296, 216},  // 4: bottom-right
    {160, 120}   // 5: centre
};
constexpr const char* kTargetNames[] = {
    "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM RIGHT", "CENTRE"
};
constexpr unsigned kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);
static_assert(kTargetCount == 5, "Expected four corners and centre");

uint32_t lastTouchMs = 0;
uint32_t lastHeartbeatMs = 0;
bool touchAvailable = false;
bool touchDown = false;
bool displayReady = false;
unsigned targetIndex = 0;

// FT6336U TD_STATUS at 0x02, followed by first touch XH/XL/YH/YL.
bool readTouch(uint16_t& x, uint16_t& y) {
    Wire.beginTransmission(BoardPins::kTouchAddress);
    Wire.write(static_cast<uint8_t>(0x02));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<uint8_t>(BoardPins::kTouchAddress),
                         static_cast<uint8_t>(5)) != 5) return false;
    const uint8_t touches = Wire.read() & 0x0F;
    const uint8_t xh = Wire.read();
    const uint8_t xl = Wire.read();
    const uint8_t yh = Wire.read();
    const uint8_t yl = Wire.read();
    if (touches != 1) return false;
    x = static_cast<uint16_t>(((xh & 0x0F) << 8) | xl);
    y = static_cast<uint16_t>(((yh & 0x0F) << 8) | yl);
    return true;
}

void drawTargets() {
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("FREENOVE COMPANION", 160, 44, 2);
    display.drawCentreString("CFG1 | 320 x 240", 160, 63, 1);
    if (!touchAvailable) {
        display.setTextColor(TFT_RED, TFT_BLACK);
        display.drawCentreString("FT6336U NOT FOUND", 160, 96, 2);
        return;
    }
    if (targetIndex == kTargetCount) {
        display.setTextColor(TFT_GREEN, TFT_BLACK);
        display.drawCentreString("TOUCH TEST PASSED", 160, 92, 2);
        display.setTextColor(TFT_WHITE, TFT_BLACK);
        display.drawCentreString("5/5 | All targets OK", 160, 168, 2);
    } else {
        char message[48];
        snprintf(message, sizeof(message), "%u/%u: TAP %s", targetIndex + 1,
                 kTargetCount, kTargetNames[targetIndex]);
        display.drawCentreString(message, 160, 91, 2);
        display.drawCentreString("Lift finger between targets", 160, 170, 1);
    }
    for (unsigned i = 0; i < kTargetCount; ++i) {
        uint16_t color = TFT_DARKGREY;
        if (i < targetIndex) color = TFT_GREEN;
        else if (i == targetIndex) color = TFT_YELLOW;
        display.drawCircle(kTargets[i].x, kTargets[i].y, 13, color);
        display.fillCircle(kTargets[i].x, kTargets[i].y, 5, color);
    }
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("Freenove-DevBoard-Companion / CFG1 5-point touch test");
    Wire.begin(BoardPins::kTouchSda, BoardPins::kTouchScl);
    Wire.beginTransmission(BoardPins::kTouchAddress);
    touchAvailable = Wire.endTransmission() == 0;
    Serial.printf("FT6336U probe: %s\n", touchAvailable ? "FOUND" : "MISSING");

    display.init();
    display.setRotation(1);
    displayReady = (display.width() == 320 && display.height() == 240);
    Serial.printf("TFT: %dx%d; target landscape 320x240\n",
                  display.width(), display.height());
    if (!displayReady) {
        Serial.println("ERROR: display rotation/configuration mismatch");
        display.fillScreen(TFT_RED);
        return;
    }
    drawTargets();
    Serial.println("Tap 1 top-left, 2 top-right, 3 bottom-left, 4 bottom-right, 5 centre.");
}

void loop() {
    const uint32_t now = millis();
    if (displayReady && touchAvailable && now - lastTouchMs >= 75) {
        lastTouchMs = now;
        uint16_t rawX = 0, rawY = 0;
        const bool pressed = readTouch(rawX, rawY);
        if (!pressed) touchDown = false;
        else if (!touchDown) {
            touchDown = true;
            const Point mapped = TouchTransform::rawToLandscape(rawX, rawY);
            Serial.printf("TOUCH raw x=%u y=%u mapped x=%u y=%u\n",
                          rawX, rawY, mapped.x, mapped.y);
            if (targetIndex < kTargetCount &&
                TouchTransform::isNear(mapped, kTargets[targetIndex])) {
                Serial.printf("TARGET %u/%u PASS (%s)\n",
                              targetIndex + 1, kTargetCount, kTargetNames[targetIndex]);
                ++targetIndex;
                drawTargets();
                if (targetIndex == kTargetCount) {
                    Serial.println("HARDWARE TOUCH TEST PASSED (5/5)");
                }
            } else if (targetIndex < kTargetCount) {
                Serial.printf("TARGET %u/%u MISS: %s\n",
                              targetIndex + 1, kTargetCount, kTargetNames[targetIndex]);
                display.fillCircle(mapped.x, mapped.y, 3, TFT_CYAN);
            }
        }
    }
    if (now - lastHeartbeatMs >= 5000) {
        lastHeartbeatMs = now;
        Serial.printf("uptime=%lu ms free_heap=%u free_psram=%u touch_targets=%u/%u\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()),
                      targetIndex, kTargetCount);
    }
    delay(10);
}
