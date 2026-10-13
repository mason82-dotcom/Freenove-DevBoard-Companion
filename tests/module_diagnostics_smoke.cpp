#include "ModuleDiagnostics.h"
#include "PulsOxSensor.h"
#include <Arduino.h>
#include <cassert>
#include <cstring>

ESPClass ESP;

int main() {
    using namespace ModuleDiagnostics;
    using CompanionModules::Id;
    static_assert(CompanionModules::kModuleCount == 6, "Six modules");
    const Id ids[] = {Id::Hermes, Id::Webradio, Id::Obd2,
                      Id::Drone, Id::System, Id::PulseOx};

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
        } else if (id != Id::PulseOx) {
            assert(std::strstr(early.text[3], "1 s") != nullptr);
            assert(std::strstr(later.text[3], "3 s") != nullptr);
        }
    }

    PulsOx::Snapshot ox{};
    char pulseLines[4][52] = {};
    PulsOx::format(ox, 1000, pulseLines);
    assert(std::strstr(pulseLines[0], "nicht gestartet") != nullptr);
    assert(std::strstr(pulseLines[2], "--") != nullptr);
    ox.running=true;ox.sensorOk=true;ox.finger=true;
    ox.bpm=72;ox.bpmValid=true;ox.lastBeatMs=1000;
    ox.spo2=98;ox.spo2Valid=true;ox.lastSampleMs=1000;
    PulsOx::format(ox, 1100, pulseLines);
    assert(std::strstr(pulseLines[2], "72 BPM") != nullptr);
    assert(std::strstr(pulseLines[3], "98 %") != nullptr);
    PulsOx::format(ox, 5000, pulseLines);
    assert(std::strstr(pulseLines[2], "--") != nullptr);
    assert(std::strstr(pulseLines[3], "--") != nullptr);

    Snapshot invalid{};
    assert(!capture(static_cast<Id>(255), invalid, 0));
    return 0;
}
