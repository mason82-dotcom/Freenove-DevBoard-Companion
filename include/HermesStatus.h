#pragma once
#include <stdint.h>

namespace HermesStatus {
struct Snapshot {
    bool configured = false;
    bool connected = false;
    bool valid = false;
    bool fresh = false;
    bool apiReachable = false;
    bool apiHealthy = false;
    bool dashboardReachable = false;
    bool gatewayKnown = false;
    bool gatewayRunning = false;
    bool chatEnabled = false;
    float cpuTempC = 0;
    float ramUsedPct = 0;
    float load1 = 0;
    float probeAgeS = 0;
    bool hasCpuTemp = false;
    bool hasRam = false;
    bool hasLoad = false;
    uint32_t updatedAtMs = 0;
    int httpCode = 0;
};
void begin();
Snapshot read();
}
