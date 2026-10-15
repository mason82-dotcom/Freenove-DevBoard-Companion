#include "Clock.h"
#include <Arduino.h>

namespace {
bool started = false;
bool announced = false;
}  // namespace

namespace Clock {
void begin() {
    if (started) return;
    started = true;
    // fritz.box answers NTP on most Fritzbox setups; the pool is the fallback.
    // Lookups simply fail while the station is in the OBD2 dongle network.
    configTzTime(kTimeZone, "fritz.box", "de.pool.ntp.org", "pool.ntp.org");
}

void service() {
    if (announced || !valid()) return;
    announced = true;
    struct tm t;
    char text[24];
    if (local(t)) {
        formatIso(&t, text, sizeof(text));
        Serial.printf("Clock: synchronized %s\n", text);
    }
}

bool valid() {
    return started && plausible(time(nullptr));
}

bool local(struct tm& out) {
    if (!valid()) return false;
    const time_t now = time(nullptr);
    return localtime_r(&now, &out) != nullptr;
}
}  // namespace Clock
