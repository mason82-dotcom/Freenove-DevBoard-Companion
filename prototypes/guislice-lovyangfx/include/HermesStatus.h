#pragma once
#include <stdint.h>

// Pure-data bridge contract: the GUIslice task never performs Wi-Fi, TLS,
// HTTP or JSON parsing. Only a short non-blocking snapshot copy is allowed.
// This header intentionally has no Arduino/FreeRTOS dependencies.
namespace HermesStatus {
enum class Phase : uint8_t {
    DISABLED, STARTING, WIFI_DOWN, CLOCK_WAIT, CONFIG_ERROR,
    HTTP_ERROR, INVALID_RESPONSE, READY, TASK_FAILED
};
struct Snapshot {
    Phase phase = Phase::DISABLED;
    uint32_t lastSuccessMs = 0;
    uint32_t sequence = 0;
    int16_t httpCode = 0;
    bool haveSuccess = false;
    bool apiHealthy = false;
    bool dashboardAvailable = false;
    bool gatewayKnown = false;
    bool gatewayRunning = false;
    bool probeFresh = false;
    bool piTempValid = false;
    bool piRamValid = false;
    bool piLoadValid = false;
    float piTempC = 0;
    float piRamPct = 0;
    float piLoad = 0;
};

// Initializes an opt-in background TLS-only, GET-only poller.
// The default build never starts Wi-Fi or network tasks.
bool begin();
// Zero-wait GUI snapshot read: false if worker mutex is momentarily busy.
bool copy(Snapshot& out);
// Pure formatter: separate output from GUIslice-retained status strings.
void format(const Snapshot& s, uint32_t nowMs, char rows[4][52]);
}  // namespace HermesStatus
