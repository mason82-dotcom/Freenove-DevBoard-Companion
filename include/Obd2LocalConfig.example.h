#pragma once
// Copy to include/Obd2LocalConfig.h (git-ignored) to enable the OBD2 module.
// Read-only in both profiles: only the Obd2Policy allowlist is ever sent.

// --- LAB PROFILE (default) ---------------------------------------------------
// ELM327 TCP simulator in the home WLAN (Wi-Fi from HermesLocalConfig.h):
//   python3 tools/elm327_lab_sim.py --bind <Pi-LAN-IP> --port 35000
#define OBD2_PROFILE_VEHICLE 0
#define OBD2_ELM_HOST "192.168.178.63"
#define OBD2_ELM_PORT 35000

// --- VEHICLE PROFILE ---------------------------------------------------------
// Real Wi-Fi ELM327 dongle with its own access point. While the OBD2 page is
// open the ESP32 joins the dongle network (Hermes pauses); leaving the page
// returns to the home WLAN. First tests only with the vehicle at standstill.
// Replace the lab block above with:
// #define OBD2_PROFILE_VEHICLE 1
// #define OBD2_WIFI_SSID "WiFi_OBDII"   // SSID printed on / shown by the dongle
// #define OBD2_WIFI_PASSWORD ""          // most dongles are open networks
// #define OBD2_ELM_HOST "192.168.0.10"
// #define OBD2_ELM_PORT 35000
// Optional vehicle label (see include/VehicleProfiles.h), e.g. the owner's
// Mercedes C 350 W204 (2008, M272):
// #define OBD2_VEHICLE VehicleProfiles::Vehicle::MercedesC350W204M272
