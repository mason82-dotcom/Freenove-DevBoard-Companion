#include "HermesStatus.h"
#include <stdio.h>

namespace HermesStatus {
namespace {
void metric(char* out, size_t capacity, bool present, const char* fmt, float value) {
    if (present) snprintf(out, capacity, fmt, static_cast<double>(value));
    else snprintf(out, capacity, "--");
}

// Connection from this board to the Hermes bridge on the Pi.
void bridgeLine(const Snapshot& s, char* out, size_t capacity) {
    if (!s.connected) snprintf(out, capacity, "Bridge: WLAN offline");
    else if (s.failures > 0)
        snprintf(out, capacity, "Bridge: HTTP %d (%ux)", s.httpCode,
                 static_cast<unsigned>(s.failures));
    else snprintf(out, capacity, "Bridge: OK");
}

bool current(const Snapshot& s, uint32_t nowMs) {
    return s.valid && static_cast<uint32_t>(nowMs - s.updatedAtMs) <= kStaleMs;
}
}  // namespace

void format(const Snapshot& s, uint32_t nowMs, const char* lastProbeClock,
            char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    if (!s.configured) {
        snprintf(out, capacity, "Hermes: Konfiguration fehlt\n(include/HermesLocalConfig.h)");
        return;
    }
    char bridge[40];
    bridgeLine(s, bridge, sizeof(bridge));
    if (!s.valid) {
        snprintf(out, capacity, s.connected ? "Hermes: keine gueltigen Daten\n%s"
                                            : "%s\nWLAN verbindet ...", bridge);
        return;
    }
    const uint32_t ageS = static_cast<uint32_t>(nowMs - s.updatedAtMs) / 1000u;
    if (!current(s, nowMs)) {
        // Never present old health values as current.
        snprintf(out, capacity, "Daten veraltet (vor %lu s)\nWerte ausgeblendet\n%s",
                 static_cast<unsigned long>(ageS), bridge);
        return;
    }
    // The bridge's own probe of Hermes may be old even if the bridge answered.
    const bool probe = s.fresh;
    const char* api = !probe ? "--" : s.apiHealthy ? "OK"
                    : s.apiReachable ? "erreichbar, nicht gesund" : "nicht erreichbar";
    const char* gateway = !probe || !s.gatewayKnown ? "--" : s.gatewayRunning ? "laeuft" : "gestoppt";
    char dashboard[40];
    if (!probe) snprintf(dashboard, sizeof(dashboard), "--");
    else if (s.dashboardReachable) snprintf(dashboard, sizeof(dashboard), "OK");
    else if (s.dashboardHttp > 0)
        snprintf(dashboard, sizeof(dashboard), "%s (HTTP %d)",
                 s.dashboardReason[0] ? s.dashboardReason : "Fehler", s.dashboardHttp);
    else snprintf(dashboard, sizeof(dashboard), "%s", s.dashboardReason[0] ? s.dashboardReason : "nicht OK");
    char lastProbe[40];
    if (lastProbeClock && lastProbeClock[0])
        snprintf(lastProbe, sizeof(lastProbe), "%s (vor %.0f s)", lastProbeClock,
                 static_cast<double>(s.probeAgeS));
    else
        snprintf(lastProbe, sizeof(lastProbe), "vor %.0f s", static_cast<double>(s.probeAgeS));
    snprintf(out, capacity,
             "Hermes %s%s | Probe %s\n"
             "API: %s\n"
             "Gateway: %s\n"
             "Dashboard: %s\n"
             "Letzte Pruefung: %s\n"
             "Chat: %s%s%s%s\n"
             "%s | Stand %lu s",
             s.version[0] ? "v" : "", s.version[0] ? s.version : "--",
             probe ? "aktuell" : "veraltet", api, gateway, dashboard, lastProbe,
             s.chatEnabled ? "AN" : "AUS", s.chatMode[0] ? " (" : "", s.chatMode,
             s.chatMode[0] ? ")" : "", bridge, static_cast<unsigned long>(ageS));
}

void formatPi(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    const bool now = current(s, nowMs);
    char cpu[12], ram[12], load[12], total[12], uptime[32];
    metric(cpu, sizeof(cpu), now && s.hasCpuTemp, "%.1f C", s.cpuTempC);
    metric(ram, sizeof(ram), now && s.hasRam, "%.0f %%", s.ramUsedPct);
    metric(load, sizeof(load), now && s.hasLoad, "%.2f", s.load1);
    metric(total, sizeof(total), now && s.hasRamTotal, "%.1f GB", s.ramTotalMb / 1024.0f);
    if (now && s.hasUptime) {
        const unsigned long minutes = static_cast<unsigned long>(s.uptimeS / 60.0f);
        snprintf(uptime, sizeof(uptime), "%lu d %02lu:%02lu", minutes / 1440, minutes / 60 % 24,
                 minutes % 60);
    } else {
        snprintf(uptime, sizeof(uptime), "--");
    }
    snprintf(out, capacity, "Pi: %s | RAM %s | Load %s\nPi: Uptime %s | RAM %s", cpu, ram, load,
             uptime, total);
}
}  // namespace HermesStatus
