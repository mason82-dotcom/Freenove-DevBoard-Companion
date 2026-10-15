// Native host regression for the Hermes page (Hermes only) and the Pi lines
// of the System page.
#include "HermesStatus.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
char text[320];
void expect(const HermesStatus::Snapshot& s, uint32_t now, const char* clock, const char* wanted) {
    HermesStatus::format(s, now, clock, text, sizeof(text));
    if (std::strcmp(text, wanted) != 0) {
        std::printf("got:\n%s\nwanted:\n%s\n", text, wanted);
        assert(false);
    }
}
void expectPi(const HermesStatus::Snapshot& s, uint32_t now, const char* wanted) {
    HermesStatus::formatPi(s, now, text, sizeof(text));
    if (std::strcmp(text, wanted) != 0) {
        std::printf("got:\n%s\nwanted:\n%s\n", text, wanted);
        assert(false);
    }
}
}  // namespace

int main() {
    HermesStatus::Snapshot s{};
    expect(s, 0, "", "Hermes: Konfiguration fehlt\n(include/HermesLocalConfig.h)");

    s.configured = true;
    expect(s, 0, "", "Bridge: WLAN offline\nWLAN verbindet ...");
    expectPi(s, 0, "Pi: -- | RAM -- | Load --\nPi: Uptime -- | RAM --");

    s.connected = true;
    s.httpCode = -1;
    s.failures = 3;
    expect(s, 0, "", "Hermes: keine gueltigen Daten\nBridge: HTTP -1 (3x)");

    s.valid = true;
    s.fresh = true;
    s.apiReachable = true;
    s.apiHealthy = true;
    s.dashboardReachable = true;
    s.gatewayKnown = true;
    s.gatewayRunning = true;
    s.chatEnabled = true;
    std::strcpy(s.chatMode, "quick");
    std::strcpy(s.version, "0.21.6");
    s.probeAgeS = 6.5f;
    s.hasCpuTemp = true;
    s.cpuTempC = 48.25f;
    s.hasRam = true;
    s.ramUsedPct = 56.6f;
    s.hasLoad = true;
    s.load1 = 0.5f;
    s.hasRamTotal = true;
    s.ramTotalMb = 8062;
    s.hasUptime = true;
    s.uptimeS = 192130.6f;  // 2 d 05:22
    s.httpCode = 200;
    s.failures = 0;
    s.updatedAtMs = 1000;
    expect(s, 4000, "20:52:23",
           "Hermes v0.21.6 | Probe aktuell\nAPI: OK\nGateway: laeuft\nDashboard: OK\n"
           "Letzte Pruefung: 20:52:23 (vor 6 s)\nChat: AN (quick)\nBridge: OK | Stand 3 s");
    // Pi values belong to the System page, never to the Hermes page.
    assert(std::strstr(text, "Pi") == nullptr && std::strstr(text, "dBm") == nullptr);
    expectPi(s, 4000, "Pi: 48.2 C | RAM 57 % | Load 0.50\nPi: Uptime 2 d 05:22 | RAM 7.9 GB");

    // Without wall-clock time only the probe age is shown.
    s.apiHealthy = false;
    s.gatewayRunning = false;
    s.dashboardReachable = false;
    std::strcpy(s.dashboardReason, "http_error");
    s.dashboardHttp = 502;
    s.chatEnabled = false;
    s.chatMode[0] = 0;
    s.version[0] = 0;
    expect(s, 4000, "",
           "Hermes -- | Probe aktuell\nAPI: erreichbar, nicht gesund\nGateway: gestoppt\n"
           "Dashboard: http_error (HTTP 502)\nLetzte Pruefung: vor 6 s\nChat: AUS\n"
           "Bridge: OK | Stand 3 s");

    // A failed poll keeps the last values while they are recent; an old probe
    // of the bridge hides the Hermes health values.
    s.httpCode = -11;
    s.failures = 1;
    s.fresh = false;
    expect(s, 16000, "",
           "Hermes -- | Probe veraltet\nAPI: --\nGateway: --\nDashboard: --\n"
           "Letzte Pruefung: vor 6 s\nChat: AUS\nBridge: HTTP -11 (1x) | Stand 15 s");

    // Beyond kStaleMs no old values are displayed, also not the Pi values.
    expect(s, 16001, "", "Daten veraltet (vor 15 s)\nWerte ausgeblendet\nBridge: HTTP -11 (1x)");
    expectPi(s, 16001, "Pi: -- | RAM -- | Load --\nPi: Uptime -- | RAM --");
    s.connected = false;
    expect(s, 60000, "", "Daten veraltet (vor 59 s)\nWerte ausgeblendet\nBridge: WLAN offline");

    // millis() wrap-around keeps ages correct.
    s.connected = true;
    s.failures = 0;
    s.fresh = true;
    s.updatedAtMs = 0xFFFFF000u;
    HermesStatus::format(s, 0x00000400u, "", text, sizeof(text));
    assert(std::strstr(text, "Stand 5 s") != nullptr);

    // Truncation is safe for tiny buffers.
    char tiny[8];
    HermesStatus::format(s, 0x00000400u, "", tiny, sizeof(tiny));
    assert(std::strlen(tiny) == sizeof(tiny) - 1);
    HermesStatus::formatPi(s, 0x00000400u, tiny, sizeof(tiny));
    assert(std::strlen(tiny) == sizeof(tiny) - 1);
    HermesStatus::format(s, 0, "", nullptr, 0);
    std::puts("hermes format smoke: OK");
    return 0;
}
