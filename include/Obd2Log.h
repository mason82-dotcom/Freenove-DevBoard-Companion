#pragma once
#include "Obd2Status.h"
#include <stddef.h>
#include <stdint.h>

// CSV log of the OBD2 live values on the SD card: /obd2/NNNN.csv, one file
// per visit of the OBD2 page, created at the first live value. One row per
// second; stale or unsupported values stay empty (never repeated as live).
// Written only by the OBD2 worker; the UI reads status() for the footer.
namespace Obd2Log {
constexpr uint32_t kRowIntervalMs = 1000;
constexpr uint32_t kFlushEveryRows = 10;       // car USB power can vanish at any time
constexpr uint64_t kMinFreeBytes = 64ull << 20;  // stop before the card is full
constexpr const char* kDirectory = "/obd2";

enum class State : uint8_t { Off, Writing, NoCard, CardFull, WriteError };

struct Status {
    State state = State::Off;
    uint16_t number = 0;   // current or last file number
    uint32_t bytes = 0;
    uint32_t rows = 0;
};

// Worker only. Appends a row (opens the next file first if needed).
void write(const Obd2Status::Snapshot& s, const char* vehicleTitle);
// Worker only. Ends the file when the page is left.
void close();
Status status();

// --- Pure, host-testable helpers (no Arduino) -------------------------------
// "# ..." line naming vehicle, adapter and protocol, then the column header.
size_t formatHeader(const Obd2Status::Snapshot& s, const char* vehicleTitle,
                    char* out, size_t capacity);
size_t formatRow(const Obd2Status::Snapshot& s, uint32_t nowMs, uint32_t startMs,
                 char* out, size_t capacity);
// File number from "0012.csv" (0 if the name is not a log file).
uint16_t numberFromName(const char* name);
void footer(const Status& st, bool vehicle, char* out, size_t capacity);
}  // namespace Obd2Log
