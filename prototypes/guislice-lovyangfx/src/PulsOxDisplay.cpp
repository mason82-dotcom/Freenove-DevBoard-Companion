#include "PulsOxSensor.h"
#include <stdio.h>

namespace PulsOx {
void format(const Snapshot& s, uint32_t nowMs, char rows[4][52]) {
    // Only recent sensor samples are eligible for display. Values are never
    // medical-grade and must not be used for diagnosis or treatment.
    const bool fresh = s.running && s.sensorOk
                    && s.lastSampleMs != 0
                    && static_cast<uint32_t>(nowMs - s.lastSampleMs) <= 2500u;
    if (!s.running) {
        snprintf(rows[0], 52, "MAX30102: nicht gestartet");
    } else if (!fresh) {
        snprintf(rows[0], 52, "MAX30102: fehlt / keine Daten");
    } else {
        snprintf(rows[0], 52, "MAX30102: Sensor OK");
    }
    snprintf(rows[1], 52, "Finger: %s", fresh && s.finger ? "erkannt" : "auflegen");
    if (fresh && s.finger && s.bpmValid && s.bpm > 20 && s.bpm < 255
        && static_cast<uint32_t>(nowMs - s.lastBeatMs) < 3000u) {
        snprintf(rows[2], 52, "Puls: %ld BPM (Demo)", static_cast<long>(s.bpm));
    } else {
        snprintf(rows[2], 52, "Puls: -- BPM (Demo)");
    }
    if (fresh && s.finger && s.spo2Valid && s.spo2 > 0 && s.spo2 <= 100) {
        snprintf(rows[3], 52, "SpO2: %ld %% (Demo)", static_cast<long>(s.spo2));
    } else {
        snprintf(rows[3], 52, "SpO2: -- %% (Demo)");
    }
}
}
