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

void detrendTrace(const uint16_t* raw, uint16_t n, int16_t* out) {
    if (!raw || !out || n == 0) return;
    constexpr int32_t kMaxPoints = 256;
    int32_t ac[kMaxPoints];
    if (n > kMaxPoints) n = kMaxPoints;
    int32_t lo = 0, hi = 0;
    for (uint16_t i = 0; i < n; ++i) {
        const int32_t half = kTraceAverage / 2;
        const int32_t from = static_cast<int32_t>(i) - half < 0 ? 0 : i - half;
        const int32_t to = static_cast<int32_t>(i) + half >= n ? n - 1 : i + half;
        int32_t sum = 0;
        for (int32_t k = from; k <= to; ++k) sum += raw[k];
        const int32_t baseline = sum / (to - from + 1);
        ac[i] = baseline - static_cast<int32_t>(raw[i]);  // inverted PPG
        if (i == 0 || ac[i] < lo) lo = ac[i];
        if (i == 0 || ac[i] > hi) hi = ac[i];
    }
    const int32_t span = hi - lo;
    for (uint16_t i = 0; i < n; ++i) {
        out[i] = span < kMinTraceSpan ? 50
                 : static_cast<int16_t>(((ac[i] - lo) * 100) / span);
    }
}
}
