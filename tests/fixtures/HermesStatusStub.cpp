#include "HermesStatus.h"
// Native host test: no WiFi, TLS, credentials or FreeRTOS ever run here.
namespace HermesStatus {
bool enabled() { return false; }
bool begin() { return true; }
bool copy(Snapshot& out) { out = Snapshot{}; return true; }
}
