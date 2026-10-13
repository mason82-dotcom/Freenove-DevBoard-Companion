// Native host regression for the active MAX30102 display formatter.
#include "PulsOxSensor.h"
#include <cassert>
#include <cstring>

int main() {
    PulsOx::Snapshot sample{};
    char rows[4][52] = {};
    auto check = [&](const char* state, const char* finger,
                     const char* bpm, const char* spo2) {
        assert(std::strcmp(rows[0], state) == 0);
        assert(std::strcmp(rows[1], finger) == 0);
        assert(std::strcmp(rows[2], bpm) == 0);
        assert(std::strcmp(rows[3], spo2) == 0);
        for (const auto& row : rows) assert(std::strlen(row) < sizeof(row));
    };
    const char* noFinger = "Finger: auflegen";
    const char* noBpm = "Puls: -- BPM (Demo)";
    const char* noSpo2 = "SpO2: -- % (Demo)";

    PulsOx::format(sample, 1000, rows);
    check("MAX30102: nicht gestartet", noFinger, noBpm, noSpo2);

    sample.running = true;
    PulsOx::format(sample, 1000, rows);
    check("MAX30102: fehlt / keine Daten", noFinger, noBpm, noSpo2);

    sample.sensorOk = true;
    sample.finger = true;
    sample.bpmValid = true;
    sample.spo2Valid = true;
    sample.bpm = 72;
    sample.spo2 = 98;
    sample.lastSampleMs = 1000;
    sample.lastBeatMs = 1000;
    PulsOx::format(sample, 1100, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt",
          "Puls: 72 BPM (Demo)", "SpO2: 98 % (Demo)");

    PulsOx::format(sample, 3500, rows);  // Freshness boundary is inclusive.
    check("MAX30102: Sensor OK", "Finger: erkannt",
          "Puls: 72 BPM (Demo)", "SpO2: 98 % (Demo)");
    PulsOx::format(sample, 3501, rows);
    check("MAX30102: fehlt / keine Daten", noFinger, noBpm, noSpo2);

    sample.lastSampleMs = 10000;
    sample.lastBeatMs = 7000;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt", noBpm,
          "SpO2: 98 % (Demo)");
    sample.lastBeatMs = 7001;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt",
          "Puls: 72 BPM (Demo)", "SpO2: 98 % (Demo)");

    sample.bpm = 20;
    sample.spo2 = 101;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt", noBpm, noSpo2);
    sample.bpm = 254;
    sample.spo2 = 100;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt",
          "Puls: 254 BPM (Demo)", "SpO2: 100 % (Demo)");
    sample.bpm = 255;
    sample.spo2 = 0;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", "Finger: erkannt", noBpm, noSpo2);

    sample.finger = false;
    sample.bpm = 72;
    sample.spo2 = 98;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: Sensor OK", noFinger, noBpm, noSpo2);

    sample.running = false;
    PulsOx::format(sample, 10000, rows);
    check("MAX30102: nicht gestartet", noFinger, noBpm, noSpo2);
    return 0;
}
