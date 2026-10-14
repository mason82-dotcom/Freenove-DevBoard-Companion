#pragma once
#include <stdint.h>

// Named vehicles for the OBD2 vehicle profile. A profile only labels the page
// and documents what was considered; it NEVER adds commands. All reads stay
// within Obd2Policy, and supported PIDs are taken from the ECU at runtime
// (0100/0120/0140), never assumed from this table.
namespace VehicleProfiles {
enum class Vehicle : uint8_t {
    Generic,
    // Mercedes-Benz C 350 (W204), model year 2008, M272 3.5 l V6 petrol.
    // Two cylinder banks -> fuel trims B1 and B2 (0106-0109) are meaningful.
    // Protocol expected ISO 15765-4 CAN (unverified); auto search (ATSP0) is
    // kept so a wrong guess can never block the connection.
    MercedesC350W204M272,
};

constexpr const char* title(Vehicle v) {
    return v == Vehicle::MercedesC350W204M272 ? "C350 W204 M272" : nullptr;
}
}  // namespace VehicleProfiles
