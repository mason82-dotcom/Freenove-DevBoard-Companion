#include "HermesStatus.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

namespace HermesStatus {
namespace {
const char* yesNo(bool value) { return value ? "OK" : "NEIN"; }
int toTenths(float n) { return static_cast<int>(n * 10.0f + (n >= 0 ? 0.5f : -0.5f)); }
int toHundredths(float n) { return static_cast<int>(n * 100.0f + (n >= 0 ? 0.5f : -0.5f)); }
}
void format(const Snapshot& s, uint32_t nowMs, char rows[4][52]) {
    // All output rows are strictly bounded, and invalid/stale sensor values
    // are never disguised as actual Pi data.
    const uint32_t ageMs = nowMs - s.lastSuccessMs; // unsigned rollover safe
    const bool fresh = s.phase == Phase::READY && s.haveSuccess
                    && ageMs <= 15000u && s.probeFresh;
    if (!fresh) {
        switch (s.phase) {
            case Phase::DISABLED:
                snprintf(rows[0], 52, "TLS: AUS (lokaler Modus)");
                break;
            case Phase::STARTING:
                snprintf(rows[0], 52, "TLS: startet...");
                break;
            case Phase::WIFI_DOWN:
                snprintf(rows[0], 52, "WLAN: nicht verbunden");
                break;
            case Phase::CLOCK_WAIT:
                snprintf(rows[0], 52, "TLS: warte auf gueltige Zeit");
                break;
            case Phase::CONFIG_ERROR:
                snprintf(rows[0], 52, "TLS: Konfiguration ungueltig");
                break;
            case Phase::HTTP_ERROR:
                snprintf(rows[0], 52, "HTTPS: Abruf fehlgeschlagen");
                break;
            case Phase::INVALID_RESPONSE:
                snprintf(rows[0], 52, "HTTPS: ungueltige Statusdaten");
                break;
            case Phase::TASK_FAILED:
                snprintf(rows[0], 52, "TLS: Netzwerk-Task fehlgeschlagen");
                break;
            case Phase::READY:
                snprintf(rows[0], 52, "TLS: Daten/Probe veraltet");
                break;
        }
        snprintf(rows[1], 52, "API/Dashboard: keine Livewerte");
        snprintf(rows[2], 52, "Gateway: Status nicht aktuell");
        snprintf(rows[3], 52, "Pi: keine aktuellen Messwerte");
        return;
    }
    snprintf(rows[0], 52, "TLS: OK | Alter: %lu s",
             static_cast<unsigned long>(ageMs / 1000u));
    snprintf(rows[1], 52, "API: %s | Dash: %s",
             yesNo(s.apiHealthy), yesNo(s.dashboardAvailable));
    snprintf(rows[2], 52, "Gateway: %s | Probe: frisch",
             s.gatewayKnown ? (s.gatewayRunning ? "AN" : "AUS") : "--");
    if (s.piTempValid && s.piRamValid && s.piLoadValid) {
        const int t = toTenths(s.piTempC);
        const int r = toTenths(s.piRamPct);
        const int l = toHundredths(s.piLoad);
        snprintf(rows[3], 52, "Pi: %d.%01dC %d.%01d%% L%d.%02d",
                 t / 10, abs(t % 10), r / 10, abs(r % 10),
                 l / 100, abs(l % 100));
    } else {
        snprintf(rows[3], 52, "Pi: Messwerte fehlen");
    }
}
}  // namespace HermesStatus
