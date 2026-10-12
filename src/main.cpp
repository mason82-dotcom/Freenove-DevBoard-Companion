#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include "BoardPins.h"
#include "TouchTransform.h"

// Phase 0: fixed CFG1 display and FT6336U touch smoke test.
// Read-only local diagnostics, no Wi-Fi, vehicle, drone or Hermes control.
TFT_eSPI display;
static uint32_t lastTouchMs = 0;
static uint32_t lastHeartbeatMs = 0;
static bool touchAvailable = false;

static bool readTouch(uint16_t& x, uint16_t& y) {
    Wire.beginTransmission(BoardPins::kTouchAddress);
    Wire.write(static_cast<uint8_t>(0x02));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<uint8_t>(BoardPins::kTouchAddress), static_cast<uint8_t>(5)) != 5) return false;
    const uint8_t touches = Wire.read() & 0x0F;
    const uint8_t xh = Wire.read();
    const uint8_t xl = Wire.read();
    const uint8_t yh = Wire.read();
    const uint8_t yl = Wire.read();
    if (touches == 0 || touches > 2) return false;
    x = static_cast<uint16_t>(((xh & 0x0F) << 8) | xl);
    y = static_cast<uint16_t>(((yh & 0x0F) << 8) | yl);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("Freenove-DevBoard-Companion v0.1.0 / Phase 0");
    Wire.begin(BoardPins::kTouchSda, BoardPins::kTouchScl);
    Wire.beginTransmission(BoardPins::kTouchAddress);
    touchAvailable = Wire.endTransmission() == 0;
    Serial.printf("FT6336U probe: %s\n", touchAvailable ? "FOUND" : "MISSING");

    display.init();
    display.setRotation(1);  // Fixed landscape orientation, ST7789 CFG1 only
    display.fillScreen(TFT_BLACK);
    const int w = display.width();
    const int h = display.height();
    Serial.printf("TFT: %dx%d\n", w, h);
    const int band = h / 6;
    display.fillRect(0, 0, w / 3, band, TFT_RED);
    display.fillRect(w / 3, 0, w / 3, band, TFT_GREEN);
    display.fillRect((w * 2) / 3, 0, w - (w * 2) / 3, band, TFT_BLUE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.drawCentreString("Freenove", w / 2, band + 14, 2);
    display.drawCentreString("DevBoard Companion", w / 2, band + 40, 2);
    display.drawCentreString("Display / Touch Test", w / 2, band + 70, 2);
    display.drawCentreString("No network / no control", w / 2, band + 100, 1);
    display.drawCentreString(touchAvailable ? "Touch detected" : "Touch not detected", w / 2, band + 125, 2);
    Serial.println("RGB test: screen must show RED | GREEN | BLUE, left to right.");
}

void loop() {
    const uint32_t now = millis();
    if (touchAvailable && now - lastTouchMs >= 75) {
        lastTouchMs = now;
        uint16_t x = 0, y = 0;
        if (readTouch(x, y)) {
            const auto mapped = TouchTransform::rawToLandscape(x, y);
            Serial.printf("TOUCH raw x=%u y=%u mapped x=%u y=%u\n",
                          x, y, mapped.x, mapped.y);
            display.fillCircle(mapped.x, mapped.y, 3, TFT_YELLOW);
        }
    }
    if (now - lastHeartbeatMs >= 5000) {
        lastHeartbeatMs = now;
        Serial.printf("uptime=%lu ms free_heap=%u free_psram=%u\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()));
    }
    delay(10);
}
