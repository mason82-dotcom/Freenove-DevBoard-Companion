#pragma once
#include <stdint.h>

// Last station, volume and play state of the Webradio in NVS, so the radio
// continues where it was after a power cycle. Writes are debounced (the
// player calls save() only kSaveDelayMs after the last change) to spare the
// flash while stepping through stations.
namespace Webradio {
namespace Settings {
constexpr uint32_t kSaveDelayMs = 5000;

struct Values {
    uint8_t station = 0;
    uint8_t volume = 10;
    bool playing = false;
};

// Missing or out-of-range entries fall back to the defaults.
Values load(uint8_t stationCount, uint8_t maxVolume);
bool save(const Values& v);
}  // namespace Settings
}  // namespace Webradio
