#include "WebradioPlayer.h"
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
    return ready;
}

void service(uint32_t now) {
    if (!ready) return;
    RadioEngine::Event e;
    while (RadioEngine::pollEvent(e)) {
        if (e.gen != gen) continue;  // event of a previous stream
        switch (e.type) {
            case RadioEngine::EventType::Title: utf8Copy(title, e.text, sizeof(title)); break;
            case RadioEngine::EventType::Playing:
                phase = Phase::Playing;
                attempt = 0;
                break;
            case RadioEngine::EventType::ConnectFailed:
            case RadioEngine::EventType::StreamLost: onFailure(); break;
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
    if (wantsPlay) connectCurrent();
}

void changeVolume(int delta) { RadioEngine::setVolume(RadioEngine::volume() + delta); }

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
