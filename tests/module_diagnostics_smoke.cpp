#include "ModuleDiagnostics.h"
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
        } else if (id != Id::Camera) {
            assert(std::strstr(early.text[3], "1 s") != nullptr);
            assert(std::strstr(later.text[3], "3 s") != nullptr);
        }
    }

    Snapshot invalid{};
    assert(!capture(static_cast<Id>(255), invalid, 0));
    return 0;
}
