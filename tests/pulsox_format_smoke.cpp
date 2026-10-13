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

    // Trace: a pure ramp (drift without finger pulse) must not fill the chart.
    uint16_t raw[100];
    int16_t trace[100];
    for (uint16_t i = 0; i < 100; ++i) raw[i] = static_cast<uint16_t>(3000 + i * 5);
    PulsOx::detrendTrace(raw, 100, trace);
    for (uint16_t i = 13; i < 87; ++i) assert(trace[i] == 50);  // interior flat

    // Pulse on top of a strong drift: valleys in IR become peaks, full scale.
    for (uint16_t i = 0; i < 100; ++i) {
        const bool beat = i % 25 == 12;
        raw[i] = static_cast<uint16_t>(3000 + i * 5 - (beat ? 200 : 0));
    }
    PulsOx::detrendTrace(raw, 100, trace);
    int16_t lo = 100, hi = 0;
    for (auto v : trace) { assert(v >= 0 && v <= 100); lo = v < lo ? v : lo; hi = v > hi ? v : hi; }
    assert(lo == 0 && hi == 100);
    for (uint16_t i = 12; i < 100; i += 25) assert(trace[i] == 100);

    // Flat or tiny input stays centered; null input is ignored.
    for (auto& v : raw) v = 4000;
    PulsOx::detrendTrace(raw, 100, trace);
    for (auto v : trace) assert(v == 50);
    PulsOx::detrendTrace(nullptr, 100, trace);
    return 0;
}
