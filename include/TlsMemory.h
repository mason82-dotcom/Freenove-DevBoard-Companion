#pragma once

// Routes every mbedTLS allocation (Hermes HTTPS and the Webradio's HTTPS
// streams) to PSRAM. The framework is built with internal-only mbedTLS
// memory, where two concurrent TLS sessions do not fit next to the I2S DMA
// buffers. Call once at boot, before the first TLS connection.
namespace TlsMemory {
bool begin();
}
