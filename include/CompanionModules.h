#pragma once
#include <stddef.h>
#include <stdint.h>

// The navigation model deliberately contains no network endpoints or actions.
// Modules stay placeholders until their dedicated read-only integration has
// been separately tested (active: Hermes status, Webradio, OBD2 read-only,
// drone telemetry read-only, System, PulsOx).
namespace CompanionModules {
enum class Id : uint8_t { Hermes, Webradio, Obd2, Drone, System, PulsOx, Kids, Spare };

struct Module {
    Id id;
    const char* title;
    const char* detail;
};

constexpr Module kModules[] = {
    {Id::Hermes,   "Hermes",   "Read-only Status ueber HTTPS"},
    {Id::Webradio, "Webradio", "Internetradio ueber I2S (44 Sender)"},
    {Id::Obd2,    "OBD2",     "Read-only ELM327 Live-Daten (Mode 01)"},
    {Id::Drone,   "Drohne",   "Read-only Telemetrie (AeroNexus)"},
    {Id::System,  "System",   "Lokale ESP32-Diagnose"},
    {Id::PulsOx,  "PulsOx",   "Status: MAX30102 noch nicht initialisiert"},
    // Placeholders: no integration yet (KidsGames_4in1 is its own firmware).
    {Id::Kids,    "Kids 4in1", "Lernspiele, noch nicht integriert"},
    {Id::Spare,   "Frei",     "Platzhalter fuer weiteres Projekt"},
};
constexpr size_t kModuleCount = sizeof(kModules) / sizeof(kModules[0]);
static_assert(kModuleCount == 8, "Companion home screen has eight tiles (4 x 2)");
} // namespace CompanionModules
