#pragma once
// Copy to include/DroneLocalConfig.h (git-ignored) to enable the Drohne page.
// Read-only telemetry from the AeroNexus EMQX broker (DJI Cloud API osd).
// The account must be the AeroNexus "companion" user: its broker ACL allows
// only subscribing to thing/product/+/osd (AeroNexus telemetry/emqx/acl.conf).
// Never use the backend (superuser) or a pilot account here.
// Plain MQTT inside the home WLAN; the session runs only while the page is open.
#define DRONE_MQTT_HOST "192.168.178.63"   // AeroNexus SERVER_HOST
#define DRONE_MQTT_PORT 1884               // EMQX MQTT port of AeroNexus edge
#define DRONE_MQTT_USER "companion"
#define DRONE_MQTT_PASSWORD "YOUR_COMPANION_PASSWORD"   // MQTT_COMPANION_PASSWORD in AeroNexus edge/.env
