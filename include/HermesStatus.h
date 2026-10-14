#pragma once
#include <stddef.h>
#include <stdint.h>

// Read-only Hermes bridge status, produced only on the Hermes worker task.
namespace HermesStatus {
// Data older than this is shown as stale and its values are hidden.
constexpr uint32_t kStaleMs = 15000;

struct Snapshot {
    bool configured = false;
    bool connected = false;
    // True once a response passed schema validation; the values below then
    // belong to the last successful poll at updatedAtMs (kept on later errors).
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
    // Result of the most recent attempt (HTTP status or negative client error).
    int httpCode = 0;
    // Consecutive failed attempts while Wi-Fi was connected; 0 after success.
    uint16_t failures = 0;
    int8_t rssi = 0;
};
void begin();
Snapshot read();
// Pure, host-testable page text (no Arduino or LVGL dependencies).
void format(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity);
}
