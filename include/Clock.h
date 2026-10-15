#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Wall-clock time from NTP while the home WLAN is up (Fritzbox first, then
// the public pool), Europe/Berlin with daylight saving. The ESP32 keeps
// counting after the station switches to the OBD2 dongle, so logs written in
// the car carry real time as long as the board was started at home. After a
// cold start without internet the time stays unknown (never guessed).
namespace Clock {
constexpr const char* kTimeZone = "CET-1CEST,M3.5.0,M10.5.0/3";
// Anything before this is the unsynchronized boot epoch (1970).
constexpr time_t kPlausibleEpoch = 1735689600;  // 2025-01-01 00:00:00 UTC

void begin();    // once, after WifiLink::begin(); SNTP retries on its own
void service();  // UI task, ~1 Hz: logs the first synchronization
bool valid();
// Local time; false (and out untouched) while the time is unknown.
bool local(struct tm& out);

// --- Pure, host-testable helpers --------------------------------------------
inline bool plausible(time_t t) { return t >= kPlausibleEpoch; }
// "20:43", "--:--" if t is null.
void formatShort(const struct tm* t, char* out, size_t capacity);
// "14.10.2026 20:43:05", "nicht synchronisiert" if t is null.
void formatLong(const struct tm* t, char* out, size_t capacity);
// "2026-10-14 20:43:05" for logs, "" if t is null.
void formatIso(const struct tm* t, char* out, size_t capacity);
// "20:43:05" for log rows, "" if t is null.
void formatTime(const struct tm* t, char* out, size_t capacity);
}  // namespace Clock
