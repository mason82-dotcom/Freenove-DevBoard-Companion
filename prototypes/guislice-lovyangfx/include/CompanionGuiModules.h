#pragma once
#include <stddef.h>
#include <stdint.h>

// The navigation model deliberately contains no network endpoints or actions.
// Individual modules remain disabled placeholders until their dedicated
// read-only integration has been separately tested.
namespace CompanionModules {
enum class Id : uint8_t { Hermes, Webradio, Obd2, Drone, System, PulseOx };

struct Module {
    Id id;
    const char* title;
    const char* detail;
};

constexpr Module kModules[] = {
    {Id::Hermes,   "Hermes",   "Status: noch nicht verbunden"},
    {Id::Webradio, "Webradio", "Status: Audiomodul noch nicht aktiv"},
    {Id::Obd2,    "OBD2",     "Status: read-only ELM327 noch nicht aktiv"},
    {Id::Drone,   "Drohne",   "Status: Telemetrie noch nicht verbunden"},
    {Id::System,  "System",   "Lokale ESP32-Diagnose"},
    {Id::PulseOx, "PulsOx", "MAX30102 | Puls & SpO2 (Demo)"},
};
constexpr size_t kModuleCount = sizeof(kModules) / sizeof(kModules[0]);
static_assert(kModuleCount == 6, "Companion home screen must have six modules");
} // namespace CompanionModules
