#pragma once
#include <stdint.h>

// MAX30102 snapshot produced only on the sensor FreeRTOS task (core 0).
// No raw samples or other sensor configuration is transferred to the GUI.
namespace PulsOx {
struct Snapshot {
    bool running = false;
    bool sensorOk = false;
    bool finger = false;
    bool bpmValid = false;
    bool spo2Valid = false;
    int32_t bpm = 0;
    int32_t spo2 = 0;
    uint32_t lastSampleMs = 0;
    uint32_t lastBeatMs = 0;
    uint32_t beatCounter = 0;
};
// Starts one persistent worker, initially parked (sensor is OFF).
bool begin();
// Called ONLY from the GUIslice task when the PulsOx page is entered/left.
// Non-blocking: the worker exclusively handles MAX30102 I2C and shutdown.
bool requestActive(bool active);
// Snapshot stays empty while the page is inactive, even if the worker
// is still completing an in-flight I2C transaction.
Snapshot read();
// Copies the most recent chronological IR samples; read-only, no I2C.
uint16_t readWave(uint16_t* destination, uint16_t capacity);
void format(const Snapshot& sample, uint32_t nowMs, char rows[4][52]);
// Display-only trace: subtracts a centered 1 s moving average (removes the
// slow DC drift), inverts so systolic peaks point up and scales to 0..100.
// A flat window (span < kMinTraceSpan) is drawn as a centered line.
constexpr uint16_t kTraceAverage = 25;  // samples, 1 s at 25 Hz
constexpr int32_t kMinTraceSpan = 8;
void detrendTrace(const uint16_t* raw, uint16_t count, int16_t* out);
}
