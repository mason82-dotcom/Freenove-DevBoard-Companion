#include "ModuleDiagnostics.h"
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void hermes(Snapshot& out, uint32_t uptimeMs) {
    // Hermes-Agent endpoint and credentials are NOT configured in firmware.
    // No network connection or HTTP probes are started automatically.
    snprintf(out.text[0], kTextCapacity, "Agent: nicht verbunden");
    snprintf(out.text[1], kTextCapacity, "Gateway: nicht eingerichtet");
    snprintf(out.text[2], kTextCapacity, "Daten: keine Antwort");
    snprintf(out.text[3], kTextCapacity, "Diagnose-Uptime: %lu s",
             static_cast<unsigned long>(uptimeMs / 1000));
}
}
