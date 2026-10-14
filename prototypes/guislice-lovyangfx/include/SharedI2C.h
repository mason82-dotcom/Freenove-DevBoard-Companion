#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Shared FNK0086 Wire bus: FT6336U at 0x38 and MAX30102 at 0x57.
// No I2C operation in either driver is permitted without holding this lock.
namespace SharedI2C {
bool begin();
bool ready();
class Guard {
public:
    explicit Guard(uint32_t timeoutMs = 30);
    ~Guard();
    explicit operator bool() const { return acquired_; }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
private:
    bool acquired_ = false;
};
}
