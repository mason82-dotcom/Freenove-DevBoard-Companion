#pragma once
// Copy to include/Obd2LocalConfig.h (git-ignored) to enable the OBD2 module.
// LAB PROFILE: ELM327 TCP simulator in the home WLAN, e.g. on the Pi:
//   python3 tools/elm327_lab_sim.py --bind <Pi-LAN-IP> --port 35000
// The Wi-Fi association is provided by the Hermes worker (HermesLocalConfig.h).
// A real Wi-Fi ELM327 dongle runs its own access point (often 192.168.0.10)
// and needs a separate vehicle profile; it is NOT supported by this lab build.
#define OBD2_ELM_HOST "192.168.178.63"
#define OBD2_ELM_PORT 35000
