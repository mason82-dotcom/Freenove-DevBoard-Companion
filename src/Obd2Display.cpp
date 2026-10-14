#include "Obd2Status.h"
#include <stdio.h>

namespace Obd2Status {
namespace {
const char* linkText(Link link, bool vehicle) {
    switch (link) {
        case Link::NotConfigured: return "nicht konfiguriert";
        case Link::Idle: return "inaktiv";
        case Link::WaitingForWifi: return vehicle ? "sucht Adapter-WLAN ..." : "wartet auf WLAN";
        case Link::Connecting: return "verbindet ...";
        case Link::Initializing: return "Adapter-Init ...";
        case Link::Searching: return "sucht Protokoll ...";
        case Link::Live: return "verbunden";
        case Link::Error: return "Fehler, neuer Versuch";
        case Link::NoEcu: return "kein Steuergeraet (Zuendung?)";
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
// Display-only cap so the counter row fits; the serial log keeps exact values.
void counter(char* out, size_t cap, uint32_t value, uint32_t limit) {
    if (value > limit) snprintf(out, cap, "%lu+", static_cast<unsigned long>(limit));
    else snprintf(out, cap, "%lu", static_cast<unsigned long>(value));
}
}  // namespace

void format(const Snapshot& s, uint32_t now, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    if (s.link == Link::NotConfigured) {
        snprintf(out, capacity, "ELM327: nicht konfiguriert\n(include/Obd2LocalConfig.h)");
        return;
    }
    char rpm[12], speed[12], coolant[12], intake[12], load[12], throttle[12], volt[12];
    char maf[14], st1[8], lt1[8], st2[8], lt2[8];
    value(rpm, sizeof(rpm), s, Rpm, now, "%.0f rpm");
    value(speed, sizeof(speed), s, Speed, now, "%.0f km/h");
    value(coolant, sizeof(coolant), s, Coolant, now, "%.0f C");
    value(intake, sizeof(intake), s, Intake, now, "%.0f C");
    value(load, sizeof(load), s, Load, now, "%.1f %%");
    value(throttle, sizeof(throttle), s, Throttle, now, "%.1f %%");
    value(volt, sizeof(volt), s, Voltage, now, "%.2f V");
    // Without a fresh ECU value show what the adapter measures itself (ATRV):
    // also with the ignition off, which tells the adapter link is fine.
    const Value& av = s.adapterVoltage;
    if (volt[0] == '-' && av.valid && s.link != Link::Idle &&
        static_cast<uint32_t>(now - av.atMs) <= kAdapterVoltageStaleMs)
        snprintf(volt, sizeof(volt), "%.1f V", static_cast<double>(av.value));
    value(maf, sizeof(maf), s, Maf, now, "%.1f g/s");
    char tx[8], timeouts[8], errors[8];
    counter(tx, sizeof(tx), s.tx, 99999);
    counter(timeouts, sizeof(timeouts), s.timeouts, 99);
    counter(errors, sizeof(errors), s.errors, 99);
    // Whole percent keeps the two-bank line within the panel width.
    value(st1, sizeof(st1), s, ShortTrimB1, now, "%+.0f");
    value(lt1, sizeof(lt1), s, LongTrimB1, now, "%+.0f");
    value(st2, sizeof(st2), s, ShortTrimB2, now, "%+.0f");
    value(lt2, sizeof(lt2), s, LongTrimB2, now, "%+.0f");
    // Live with a known protocol: adapter and protocol instead of "verbunden".
    char header[48];
    if (s.link == Link::Live && s.adapter[0] && s.protocol[0])
        snprintf(header, sizeof(header), "%s | %s", s.adapter, s.protocol);
    else
        snprintf(header, sizeof(header), "ELM327: %s%s%s", linkText(s.link, s.vehicleProfile),
                 s.link == Link::Live && s.adapter[0] ? " | " : "",
                 s.link == Link::Live ? s.adapter : "");
    snprintf(out, capacity,
             "%s\n"
             "Drehzahl: %s | Tempo: %s\n"
             "Kuehlmittel: %s | Ansaug: %s\n"
             "Last: %s | Drossel: %s\n"
             "Spannung: %s | MAF: %s\n"
             "Trim B1: %s/%s B2: %s/%s %%\n"
             "TX: %s | Timeouts: %s | Fehler: %s",
             header, rpm, speed, coolant, intake, load, throttle, volt, maf,
             st1, lt1, st2, lt2, tx, timeouts, errors);
}
}  // namespace Obd2Status
