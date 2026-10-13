#include "ModuleDiagnostics.h"
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void obd2(Snapshot& out, uint32_t uptimeMs) {
    // The user's ELM327 is Wi-Fi based. Until explicit networking and a
    // read-only command allowlist are implemented, send ZERO AT or OBD frames.
    snprintf(out.text[0], kTextCapacity, "ELM327: nicht verbunden");
    snprintf(out.text[1], kTextCapacity, "PID-Werte: keine Daten");
    snprintf(out.text[2], kTextCapacity, "CAN/ELM TX: 0 (gesperrt)");
    snprintf(out.text[3], kTextCapacity, "Diagnose-Uptime: %lu s",
             static_cast<unsigned long>(uptimeMs / 1000));
}
}
