#pragma once
#include <stdint.h>

// Single owner of the ESP32 station interface. Exactly one profile is
// associated at a time:
//   Home    - home WLAN from HermesLocalConfig.h (Hermes, OBD2 lab profile)
//   Vehicle - the ELM327 dongle's own access point (Obd2LocalConfig.h,
//             OBD2_PROFILE_VEHICLE 1); used only while the OBD2 page is open.
// Credentials are never written to NVS (WiFi.persistent(false)).
namespace WifiLink {
enum class Profile : uint8_t { None, Home, Vehicle };

void begin();
bool homeConfigured();
bool vehicleConfigured();
// Switches the station to the given profile (no-op if already selected).
// Call from worker tasks only; never from the LVGL thread.
bool request(Profile profile);
Profile selected();
// True only if the selected profile is p and the station is associated.
bool connectedTo(Profile p);
}  // namespace WifiLink
