// Native host regression for the Hermes page formatter.
#include "HermesStatus.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
char text[224];
void expect(const HermesStatus::Snapshot& s, uint32_t now, const char* wanted) {
    HermesStatus::format(s, now, text, sizeof(text));
    if (std::strcmp(text, wanted) != 0) {
        std::printf("got:\n%s\nwanted:\n%s\n", text, wanted);
        assert(false);
    }
}
}  // namespace

int main() {
    HermesStatus::Snapshot s{};
    expect(s, 0, "Hermes: Konfiguration fehlt\n(include/HermesLocalConfig.h)");

    s.configured = true;
    expect(s, 0, "WLAN: offline\nWLAN verbindet ...");

    s.connected = true;
    s.httpCode = -1;
    s.failures = 3;
    expect(s, 0, "Hermes: keine gueltigen Daten\nHTTP-Fehler: -1 (3x)");

    s.valid = true;
    s.fresh = true;
    s.apiHealthy = true;
    s.dashboardReachable = true;
    s.gatewayKnown = true;
    s.gatewayRunning = true;
    s.hasCpuTemp = true;
    s.cpuTempC = 48.25f;
    s.hasLoad = true;
    s.load1 = 0.5f;
    s.httpCode = 200;
    s.failures = 0;
    s.rssi = -61;
    s.updatedAtMs = 1000;
    expect(s, 4000,
           "API: OK | Probe: aktuell\nGateway: AN | Dashboard: OK\n"
           "Pi CPU: 48.2 C | RAM: --\nLoad: 0.50 | Chat: AUS\nWLAN: -61 dBm | Stand: 3 s");

    // A failed poll keeps the last values while they are still recent.
    s.httpCode = -11;
    s.failures = 1;
    s.fresh = false;
    expect(s, 16000,
           "API: NICHT OK | Probe: veraltet\nGateway: -- | Dashboard: nicht OK\n"
           "Pi CPU: 48.2 C | RAM: --\nLoad: 0.50 | Chat: AUS\nHTTP-Fehler: -11 (1x) | Stand: 15 s");

    // Beyond kStaleMs no old health values are displayed.
    expect(s, 16001, "Daten veraltet (vor 15 s)\nWerte ausgeblendet\nHTTP-Fehler: -11 (1x)");
    s.connected = false;
    expect(s, 60000, "Daten veraltet (vor 59 s)\nWerte ausgeblendet\nWLAN: offline");

    // millis() wrap-around keeps ages correct.
    s.connected = true;
    s.failures = 0;
    s.fresh = true;
    s.updatedAtMs = 0xFFFFF000u;
    HermesStatus::format(s, 0x00000400u, text, sizeof(text));
    assert(std::strstr(text, "Stand: 5 s") != nullptr);

    // Truncation is safe for tiny buffers.
    char tiny[8];
    HermesStatus::format(s, 0x00000400u, tiny, sizeof(tiny));
    assert(std::strlen(tiny) == sizeof(tiny) - 1);
    HermesStatus::format(s, 0, nullptr, 0);
    return 0;
}
