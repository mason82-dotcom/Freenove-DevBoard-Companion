#include "ModuleDiagnostics.h"
#include <Arduino.h>
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void camera(Snapshot& out, uint32_t uptimeMs) {
    // Camera wiring and sensor power-up have NOT been verified on FNK0086.
    // Do not initialize camera GPIO/I2C or compete with display/FT6336U pins.
    (void)uptimeMs;
    snprintf(out.text[0], kTextCapacity, "Sensor: nicht initialisiert");
    snprintf(out.text[1], kTextCapacity, "Vorschau: aus");
    snprintf(out.text[2], kTextCapacity, "PSRAM frei: %u KiB",
             static_cast<unsigned>(ESP.getFreePsram() / 1024));
    snprintf(out.text[3], kTextCapacity, "Kamerapins: nicht geprueft");
}
}
