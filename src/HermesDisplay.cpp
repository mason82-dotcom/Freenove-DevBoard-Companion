#include "HermesStatus.h"
#include <stdio.h>

namespace HermesStatus {
namespace {
const char* onOff(bool known, bool value) {
    return !known ? "--" : value ? "AN" : "AUS";
}

void metric(char* out, size_t capacity, bool present, const char* fmt, float value) {
    if (present) snprintf(out, capacity, fmt, static_cast<double>(value));
    else snprintf(out, capacity, "--");
}
}  // namespace

void format(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    char link[40];
    if (!s.connected) snprintf(link, sizeof(link), "WLAN: offline");
    else if (s.failures > 0)
        snprintf(link, sizeof(link), "HTTP-Fehler: %d (%ux)", s.httpCode,
                 static_cast<unsigned>(s.failures));
    else snprintf(link, sizeof(link), "WLAN: %d dBm", static_cast<int>(s.rssi));

    if (!s.configured) {
        snprintf(out, capacity, "Hermes: Konfiguration fehlt\n(include/HermesLocalConfig.h)");
        return;
    }
    if (!s.valid) {
        snprintf(out, capacity, s.connected ? "Hermes: keine gueltigen Daten\n%s"
                                            : "%s\nWLAN verbindet ...", link);
        return;
    }
    const uint32_t ageS = static_cast<uint32_t>(nowMs - s.updatedAtMs) / 1000u;
    if (static_cast<uint32_t>(nowMs - s.updatedAtMs) > kStaleMs) {
        // Never present old health values as current.
        snprintf(out, capacity, "Daten veraltet (vor %lu s)\nWerte ausgeblendet\n%s",
                 static_cast<unsigned long>(ageS), link);
        return;
    }
    char cpu[16], ram[16], load[16];
    metric(cpu, sizeof(cpu), s.hasCpuTemp, "%.1f C", s.cpuTempC);
    metric(ram, sizeof(ram), s.hasRam, "%.1f %%", s.ramUsedPct);
    metric(load, sizeof(load), s.hasLoad, "%.2f", s.load1);
    snprintf(out, capacity,
             "API: %s | Probe: %s\nGateway: %s | Dashboard: %s\n"
             "Pi CPU: %s | RAM: %s\nLoad: %s | Chat: %s\n%s | Stand: %lu s",
             s.fresh && s.apiHealthy ? "OK" : "NICHT OK",
             s.fresh ? "aktuell" : "veraltet",
             onOff(s.fresh && s.gatewayKnown, s.gatewayRunning),
             s.fresh && s.dashboardReachable ? "OK" : "nicht OK",
             cpu, ram, load, s.chatEnabled ? "AN" : "AUS",
             link, static_cast<unsigned long>(ageS));
}
}  // namespace HermesStatus
