#include "WebradioPlayer.h"
#include "WebradioSettings.h"
#include "WebradioStations.h"
#include "WifiLink.h"
#include <Arduino.h>
#include <string.h>

namespace {
using Webradio::Phase;

constexpr uint32_t kRetryDelayMs = 3000;

uint8_t stationIndex = 0;
bool wantsPlay = false;
uint32_t gen = 0;
Phase phase = Phase::Stopped;
uint8_t attempt = 0;
bool retryPending = false;
uint32_t retryAtMs = 0;
char title[RadioEngine::kTextLen] = {};
bool ready = false;
// Persistence: user actions mark the state dirty; service() saves it once it
// has been unchanged for kSaveDelayMs, and only if it differs from NVS.
Webradio::Settings::Values saved;
bool dirty = false;
uint32_t dirtyAtMs = 0;

void markDirty() {
    dirty = true;
    dirtyAtMs = millis();
}

Webradio::Settings::Values currentSettings() {
    Webradio::Settings::Values v;
    v.station = stationIndex;
    v.volume = static_cast<uint8_t>(RadioEngine::volume());
    v.playing = wantsPlay;
    return v;
}

bool wifiReady() { return WifiLink::connectedTo(WifiLink::Profile::Home); }

void connectCurrent() {
    retryPending = false;
    title[0] = '\0';
    if (!wifiReady()) {
        phase = Phase::NoWifi;
        retryPending = wantsPlay;  // start as soon as the home WLAN is back
        retryAtMs = millis() + kRetryDelayMs;
        return;
    }
    ++gen;
    phase = attempt > 0 ? Phase::Retrying : Phase::Connecting;
    Serial.printf("Webradio: %s (%u/%u)\n", Webradio::kStations[stationIndex].name,
                  static_cast<unsigned>(stationIndex + 1),
                  static_cast<unsigned>(Webradio::kStationCount));
    RadioEngine::connect(gen, Webradio::kStations[stationIndex].url);
}

void onFailure() {
    if (!wantsPlay) return;
    if (attempt < Webradio::kMaxRetries) {
        ++attempt;
        phase = Phase::Retrying;
        retryPending = true;
        retryAtMs = millis() + kRetryDelayMs;
    } else {
        wantsPlay = false;
        phase = Phase::Failed;
        Serial.println("Webradio: retries exhausted");
    }
}
}  // namespace

namespace Webradio {
bool begin() {
    ready = RadioEngine::begin();
    Serial.printf("Webradio engine: %s, %u stations\n", ready ? "READY" : "FAILED",
                  static_cast<unsigned>(kStationCount));
    if (!ready) return false;
    // Continue where the radio was: station, volume and, if it was playing,
    // playback (connectCurrent waits for the home WLAN on its own).
    saved = Settings::load(static_cast<uint8_t>(kStationCount),
                           static_cast<uint8_t>(RadioEngine::kMaxVolume));
    stationIndex = saved.station;
    RadioEngine::setVolume(saved.volume);
    Serial.printf("Webradio: restored station %u, volume %u%s\n",
                  static_cast<unsigned>(stationIndex + 1), static_cast<unsigned>(saved.volume),
                  saved.playing ? ", resuming playback" : "");
    if (saved.playing) {
        wantsPlay = true;
        connectCurrent();
    }
    return true;
}

void service(uint32_t now) {
    if (!ready) return;
    RadioEngine::Event e;
    while (RadioEngine::pollEvent(e)) {
        if (e.gen != gen) continue;  // event of a previous stream
        switch (e.type) {
            case RadioEngine::EventType::Title: toDisplayAscii(title, e.text, sizeof(title)); break;
            case RadioEngine::EventType::Playing:
                phase = Phase::Playing;
                attempt = 0;
                break;
            case RadioEngine::EventType::ConnectFailed:
            case RadioEngine::EventType::StreamLost: onFailure(); break;
        }
    }
    // Signed: 'now' is taken at the start of the UI loop, markDirty() may run
    // later in the same loop, so now - dirtyAtMs can be slightly negative.
    if (dirty && static_cast<int32_t>(now - dirtyAtMs) >= static_cast<int32_t>(Settings::kSaveDelayMs)) {
        dirty = false;
        const Settings::Values v = currentSettings();
        if (v.station != saved.station || v.volume != saved.volume || v.playing != saved.playing) {
            if (Settings::save(v)) saved = v;
        }
    }
    if (retryPending && static_cast<int32_t>(now - retryAtMs) >= 0) {
        if (wifiReady()) connectCurrent();
        else {
            phase = Phase::NoWifi;
            retryAtMs = now + kRetryDelayMs;
        }
    }
}

void togglePlay() {
    if (!ready) return;
    attempt = 0;
    markDirty();
    if (wantsPlay) {
        wantsPlay = false;
        retryPending = false;
        ++gen;
        RadioEngine::stop(gen);
        phase = Phase::Stopped;
        title[0] = '\0';
    } else {
        wantsPlay = true;
        connectCurrent();
    }
}

void step(int delta) {
    const int count = static_cast<int>(kStationCount);
    stationIndex = static_cast<uint8_t>(((stationIndex + delta) % count + count) % count);
    attempt = 0;
    title[0] = '\0';
    markDirty();
    if (wantsPlay) connectCurrent();
}

void changeVolume(int delta) {
    RadioEngine::setVolume(RadioEngine::volume() + delta);
    markDirty();
}

View view() {
    View v;
    v.index = stationIndex;
    v.name = kStations[stationIndex].name;
    strlcpy(v.title, title, sizeof(v.title));
    v.phase = phase;
    v.attempt = attempt;
    v.bitrate = phase == Phase::Playing ? RadioEngine::bitrate() : 0;
    RadioEngine::codec(v.codec, sizeof(v.codec));
    v.volume = RadioEngine::volume();
    v.wantsPlay = wantsPlay;
    return v;
}
}  // namespace Webradio
