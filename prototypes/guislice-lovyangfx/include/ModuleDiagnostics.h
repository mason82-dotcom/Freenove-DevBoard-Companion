#pragma once
#include <stdint.h>
#include "CompanionModules.h"

// Read-only presentation contract for the six Companion pages.
// No backend is allowed to change vehicle, drone or radio state merely
// because the user opens a status page. All text is copied into GUI-owned
// stable buffers by GUIslice (never modify those buffers in place).
namespace ModuleDiagnostics {
constexpr uint8_t kRows = 4;
constexpr uint8_t kTextCapacity = 52; // Fits 320x240 at GUIslice font 1.
struct Snapshot {
    char text[kRows][kTextCapacity];
};

bool capture(CompanionModules::Id id, Snapshot& out, uint32_t uptimeMs);

namespace Adapter {
    void hermes(Snapshot& out, uint32_t uptimeMs);
    void webradio(Snapshot& out, uint32_t uptimeMs);
    void obd2(Snapshot& out, uint32_t uptimeMs);
    void drone(Snapshot& out, uint32_t uptimeMs);
    void system(Snapshot& out, uint32_t uptimeMs);
    void pulseOx(Snapshot& out, uint32_t uptimeMs);
}
} // namespace ModuleDiagnostics
