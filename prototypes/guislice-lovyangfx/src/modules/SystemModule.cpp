#include "ModuleDiagnostics.h"
#include <Arduino.h>
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void system(Snapshot& out, uint32_t uptimeMs) {
    // Actual ESP32 telemetry, sampled on demand without network activity.
    snprintf(out.text[0], kTextCapacity, "Uptime: %lu s",
             static_cast<unsigned long>(uptimeMs / 1000));
    snprintf(out.text[1], kTextCapacity, "Heap frei: %u B",
             static_cast<unsigned>(ESP.getFreeHeap()));
    snprintf(out.text[2], kTextCapacity, "PSRAM frei: %u B",
             static_cast<unsigned>(ESP.getFreePsram()));
    snprintf(out.text[3], kTextCapacity, "CPU: %u MHz | MinHeap: %u",
             static_cast<unsigned>(ESP.getCpuFreqMHz()),
             static_cast<unsigned>(ESP.getMinFreeHeap()));
}
}
