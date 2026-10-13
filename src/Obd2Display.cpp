#include "Obd2Status.h"
#include <stdio.h>

namespace Obd2Status {
namespace {
const char* linkText(Link link) {
    switch (link) {
        case Link::NotConfigured: return "nicht konfiguriert";
        case Link::Idle: return "inaktiv";
        case Link::WaitingForWifi: return "wartet auf WLAN";
        case Link::Connecting: return "verbindet ...";
        case Link::Initializing: return "Adapter-Init ...";
        case Link::Searching: return "sucht Protokoll ...";
        case Link::Live: return "verbunden";
        case Link::Error: return "Fehler, neuer Versuch";
    }
    return "?";
}

// "--" for unsupported, missing or stale values; never show old data as live.
void value(char* out, size_t cap, const Snapshot& s, Pid pid, uint32_t now,
           const char* fmt) {
    const Value& v = s.values[pid];
    const bool fresh = s.link == Link::Live && v.valid &&
                       static_cast<uint32_t>(now - v.atMs) <= kValueStaleMs;
    if (s.supportKnown && !s.supported[pid]) snprintf(out, cap, "n/v");
    else if (fresh) snprintf(out, cap, fmt, static_cast<double>(v.value));
    else snprintf(out, cap, "--");
}
}  // namespace

void format(const Snapshot& s, uint32_t now, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    if (s.link == Link::NotConfigured) {
        snprintf(out, capacity, "ELM327: nicht konfiguriert\n(include/Obd2LocalConfig.h)");
        return;
    }
    char rpm[12], speed[12], coolant[12], intake[12], load[12], throttle[12], volt[12];
    value(rpm, sizeof(rpm), s, Rpm, now, "%.0f rpm");
    value(speed, sizeof(speed), s, Speed, now, "%.0f km/h");
    value(coolant, sizeof(coolant), s, Coolant, now, "%.0f C");
    value(intake, sizeof(intake), s, Intake, now, "%.0f C");
    value(load, sizeof(load), s, Load, now, "%.1f %%");
    value(throttle, sizeof(throttle), s, Throttle, now, "%.1f %%");
    value(volt, sizeof(volt), s, Voltage, now, "%.2f V");
    snprintf(out, capacity,
             "ELM327: %s%s%s\n"
             "Drehzahl: %s | Tempo: %s\n"
             "Kuehlmittel: %s | Ansaug: %s\n"
             "Last: %s | Drossel: %s\n"
             "Spannung: %s\n"
             "TX: %lu | Timeouts: %lu | Fehler: %lu",
             linkText(s.link), s.adapter[0] ? " | " : "", s.adapter,
             rpm, speed, coolant, intake, load, throttle, volt,
             static_cast<unsigned long>(s.tx), static_cast<unsigned long>(s.timeouts),
             static_cast<unsigned long>(s.errors));
}
}  // namespace Obd2Status
