#include "WebradioSettings.h"
#include <Arduino.h>
#include <Preferences.h>

// Only this file touches NVS for the Webradio (namespace "webradio",
// three keys). The engine and player stay free of storage code.
namespace {
constexpr char kNamespace[] = "webradio";
}  // namespace

namespace Webradio {
namespace Settings {
Values load(uint8_t stationCount, uint8_t maxVolume) {
    Values v;
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) return v;  // read-only; absent on first boot
    const uint8_t station = prefs.getUChar("station", v.station);
    const uint8_t volume = prefs.getUChar("volume", v.volume);
    v.playing = prefs.getBool("playing", false);
    prefs.end();
    if (station < stationCount) v.station = station;
    if (volume <= maxVolume) v.volume = volume;
    return v;
}

bool save(const Values& v) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return false;
    // The player calls save() only when a value differs from the last save.
    const bool ok = prefs.putUChar("station", v.station) == 1 &&
                    prefs.putUChar("volume", v.volume) == 1 &&
                    prefs.putBool("playing", v.playing) == 1;
    prefs.end();
    Serial.printf("Webradio: saved station=%u volume=%u playing=%d%s\n",
                  static_cast<unsigned>(v.station + 1), static_cast<unsigned>(v.volume),
                  static_cast<int>(v.playing), ok ? "" : " (failed)");
    return ok;
}
}  // namespace Settings
}  // namespace Webradio
