#include "ModuleDiagnostics.h"
// Arduino-ESP32 declares DISABLED as a macro; this must not corrupt
// HermesStatus enum declarations in a host-only compilation.
#define DISABLED 0x00
#include "HermesStatus.h"
#undef DISABLED
#include <Arduino.h>
#include <cassert>
#include <cstring>

ESPClass ESP;

int main() {
    using namespace ModuleDiagnostics;
    using CompanionModules::Id;
    static_assert(CompanionModules::kModuleCount == 6, "Six modules");
    const Id ids[] = {Id::Hermes, Id::Webradio, Id::Obd2,
                      Id::Drone, Id::System, Id::Camera};

    for (auto id : ids) {
        Snapshot early{};
        Snapshot later{};
        assert(capture(id, early, 1000));
        assert(capture(id, later, 3000));
        for (unsigned r = 0; r < kRows; ++r) {
            assert(std::strlen(early.text[r]) > 0);
            assert(std::strlen(early.text[r]) < kTextCapacity);
            assert(std::strlen(later.text[r]) > 0);
            assert(std::strlen(later.text[r]) < kTextCapacity);
        }
        if (id == Id::System) {
            assert(std::strcmp(early.text[0], "Uptime: 1 s") == 0);
            assert(std::strcmp(later.text[0], "Uptime: 3 s") == 0);
            assert(std::strstr(later.text[1], "335068") != nullptr);
            assert(std::strstr(later.text[2], "8383268") != nullptr);
        } else if (id != Id::Camera && id != Id::Hermes) {
            assert(std::strstr(early.text[3], "1 s") != nullptr);
            assert(std::strstr(later.text[3], "3 s") != nullptr);
        }
    }

    // TLS status view requires a fresh successful probe. No stale fallbacks
    // or fake Pi measurements may be displayed after a failed request.
    HermesStatus::Snapshot telemetry{};
    char rows[4][52] = {};
    HermesStatus::format(telemetry, 1000, rows);
    assert(std::strstr(rows[0], "AUS") != nullptr);
    assert(std::strstr(rows[1], "keine Livewerte") != nullptr);
    telemetry.phase = HermesStatus::Phase::READY;
    telemetry.haveSuccess = true;
    telemetry.probeFresh = true;
    telemetry.lastSuccessMs = 1000;
    telemetry.apiHealthy = true;
    telemetry.dashboardAvailable = true;
    telemetry.gatewayKnown = true;
    telemetry.gatewayRunning = true;
    telemetry.piTempValid = telemetry.piRamValid = telemetry.piLoadValid = true;
    telemetry.piTempC = 51.8f;
    telemetry.piRamPct = 62.2f;
    telemetry.piLoad = 0.68f;
    HermesStatus::format(telemetry, 3000, rows);
    assert(std::strstr(rows[0], "TLS: OK") != nullptr);
    assert(std::strstr(rows[1], "API: OK") != nullptr);
    assert(std::strstr(rows[2], "Gateway: AN") != nullptr);
    assert(std::strstr(rows[3], "51.8C") != nullptr);
    assert(std::strstr(rows[3], "62.2%") != nullptr);
    assert(std::strstr(rows[3], "L0.68") != nullptr);

    // Data older than 15s are explicitly invalid even after 200 OK.
    HermesStatus::format(telemetry, 19000, rows);
    assert(std::strstr(rows[0], "veraltet") != nullptr);
    assert(std::strstr(rows[1], "keine Livewerte") != nullptr);
    telemetry.phase = HermesStatus::Phase::HTTP_ERROR;
    HermesStatus::format(telemetry, 2000, rows);
    assert(std::strstr(rows[0], "fehlgeschlagen") != nullptr);
    assert(std::strstr(rows[3], "keine aktuellen") != nullptr);
    telemetry.phase = HermesStatus::Phase::READY;
    telemetry.probeFresh = false;
    HermesStatus::format(telemetry, 2000, rows);
    assert(std::strstr(rows[0], "veraltet") != nullptr);
    telemetry.probeFresh = true;
    telemetry.piRamValid = false;
    HermesStatus::format(telemetry, 2000, rows);
    assert(std::strstr(rows[3], "Messwerte fehlen") != nullptr);

    Snapshot invalid{};
    assert(!capture(static_cast<Id>(255), invalid, 0));
    return 0;
}
