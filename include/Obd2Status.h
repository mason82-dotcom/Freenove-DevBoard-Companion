#pragma once
#include <stddef.h>
#include <stdint.h>

// Read-only ELM327 (Wi-Fi/TCP) live data, produced only on the OBD2 worker.
// The worker talks to the adapter only while the OBD2 page is open.
namespace Obd2Status {
// ATRV is refreshed every few seconds, also between NoEcu retries.
constexpr uint32_t kAdapterVoltageStaleMs = 15000;

enum class Link : uint8_t {
    NotConfigured, Idle, WaitingForWifi, Connecting, Initializing, Searching, Live, Error,
    NoEcu  // adapter OK, but no ECU answered (ignition off / wrong protocol)
};

struct Value {
    bool valid = false;
    float value = 0;
    uint32_t atMs = 0;
};

enum Pid : uint8_t {
    Rpm, Speed, Coolant, Intake, Load, Throttle, Voltage,
    Maf, ShortTrimB1, LongTrimB1, ShortTrimB2, LongTrimB2, kPidCount
};

// Values older than this are displayed as "--" / logged empty. RPM and speed
// are read every cycle; the other PIDs round-robin, which on the real W204
// ECU (2026-10-14) took 4-6 s per value, so 5 s for all left gaps.
constexpr uint32_t kFastStaleMs = 3000;
constexpr uint32_t kSlowStaleMs = 15000;
constexpr uint32_t staleAfterMs(Pid pid) {
    return pid == Rpm || pid == Speed ? kFastStaleMs : kSlowStaleMs;
}

struct Snapshot {
    Link link = Link::NotConfigured;
    bool vehicleProfile = false;  // true: dongle access point, false: lab/home WLAN
    char adapter[24] = {};  // ATZ identification, e.g. "ELM327 v1.5"
    char atzRaw[48] = {};   // raw ATZ reply, CR/LF as '|', others '.', for clone diagnosis
    char protocol[16] = {}; // ATDPN after the automatic search, e.g. "CAN 11/500"
    Value adapterVoltage;   // ATRV: on-board voltage at OBD pin 16, no ECU needed
    Value values[kPidCount];
    bool supported[kPidCount] = {};
    bool supportKnown = false;
    uint32_t tx = 0;        // commands sent this session
    uint32_t timeouts = 0;
    uint32_t errors = 0;    // ?, NO DATA, malformed or oversized replies
};

void begin();
// UI only: start/stop the session when the page is entered/left. Non-blocking.
bool requestActive(bool active);
Snapshot read();
// True if this build uses the ELM327 dongle's own access point.
bool vehicleProfile();
// Short vehicle title for the page header, or nullptr (generic profile).
const char* vehicleTitle();
// Pure, host-testable page text (no Arduino/LVGL).
void format(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity);
}  // namespace Obd2Status
