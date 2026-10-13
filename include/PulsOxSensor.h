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
}
