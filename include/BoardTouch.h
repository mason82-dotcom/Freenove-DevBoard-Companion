#pragma once
#include <stdint.h>

// FT6336U capacitive I2C touch. Reads only; never writes configuration.
namespace BoardTouch {
bool begin();
bool available();
bool readRaw(uint16_t& x, uint16_t& y);
}
