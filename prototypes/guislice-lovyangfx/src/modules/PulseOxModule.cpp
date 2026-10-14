#include "ModuleDiagnostics.h"
#include "PulsOxSensor.h"

namespace ModuleDiagnostics::Adapter {
void pulseOx(Snapshot& out, uint32_t uptimeMs) {
    // Live values come from a short cross-core snapshot, NEVER a direct
    // I2C/Wire read on the GUIslice thread. Invalid metrics show "--".
    PulsOx::format(PulsOx::read(), uptimeMs, out.text);
}
}
