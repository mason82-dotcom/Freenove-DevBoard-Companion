#pragma once
#include <stddef.h>
#include <stdint.h>

// The navigation model deliberately contains no network endpoints or actions.
// Modules stay placeholders until their dedicated read-only integration has
// been separately tested (active: Hermes status, Webradio, OBD2 read-only, System, PulsOx).
namespace CompanionModules {
enum class Id : uint8_t { Hermes, Webradio, Obd2, Drone, System, PulsOx };

struct Module {
    Id id;
    const char* title;
    const char* detail;
};

constexpr Module kModules[] = {
    {Id::Hermes,   "Hermes",   "Read-only Status ueber HTTPS"},
    {Id::Webradio, "Webradio", "Internetradio ueber I2S (44 Sender)"},
    {Id::Obd2,    "OBD2",     "Read-only ELM327 Live-Daten (Mode 01)"},
    {Id::Drone,   "Drohne",   "Status: Telemetrie noch nicht verbunden"},
    {Id::System,  "System",   "Lokale ESP32-Diagnose"},
    {Id::PulsOx,  "PulsOx",   "Status: MAX30102 noch nicht initialisiert"},
};
constexpr size_t kModuleCount = sizeof(kModules) / sizeof(kModules[0]);
static_assert(kModuleCount == 6, "Companion home screen must have six modules");
} // namespace CompanionModules
