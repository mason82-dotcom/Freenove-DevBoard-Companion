#include "ModuleDiagnostics.h"
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void drone(Snapshot& out, uint32_t uptimeMs) {
    // Display only. No motor, mission, RTK-setting or flight commands.
    snprintf(out.text[0], kTextCapacity, "Gateway: nicht verbunden");
    snprintf(out.text[1], kTextCapacity, "Telemetrie: keine Daten");
    snprintf(out.text[2], kTextCapacity, "Flugkommandos: gesperrt");
    snprintf(out.text[3], kTextCapacity, "Diagnose-Uptime: %lu s",
             static_cast<unsigned long>(uptimeMs / 1000));
}
}
