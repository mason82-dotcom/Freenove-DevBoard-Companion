#pragma once
#include <stdint.h>
// Host-only stand-in for the read-only ESP32 telemetry used by the six
// Companion diagnostics modules. NEVER used in the PlatformIO firmware.
class ESPClass {
public:
    unsigned getFreeHeap() const { return 335068u; }
    unsigned getFreePsram() const { return 8383268u; }
    unsigned getMinFreeHeap() const { return 329000u; }
    unsigned getCpuFreqMHz() const { return 240u; }
};
extern ESPClass ESP;
