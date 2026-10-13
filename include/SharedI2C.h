#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Serializes FT6336U and MAX30102 transactions on the shared Wire bus.
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
