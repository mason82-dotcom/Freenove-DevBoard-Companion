#pragma once
// Copy to include/HermesLocalConfig.h and fill in locally.
// NEVER commit the populated file or share credentials in logs.
// This example is deliberately nonfunctional until configured.
#define HERMES_WIFI_SSID "YOUR_WIFI_SSID"
#define HERMES_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define HERMES_DEVICE_TOKEN "YOUR_DEVICE_TOKEN_AT_LEAST_24_CHARS"
// TLS server IP must appear in the server certificate's subjectAltName (IP).
#define HERMES_SERVER_HOST "192.168.178.63"
#define HERMES_SERVER_PORT 8766
// Paste the PEM-encoded issuing CA certificate, not the server private key.
// Use a raw string so PEM newlines remain intact.
#define HERMES_CA_PEM R"PEM(-----BEGIN CERTIFICATE-----
REPLACE_WITH_YOUR_LOCAL_CA_CERTIFICATE
-----END CERTIFICATE-----)PEM"
