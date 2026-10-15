#pragma once
#include "RadioEngine.h"
#include "WebradioText.h"
#include <stdint.h>

// Player logic on top of RadioEngine (no LVGL). All functions run on the
// LVGL/loop task only. Streaming continues while other pages are shown;
// only the Stop button (or exhausted retries) ends it.
namespace Webradio {
struct View {
    uint8_t index = 0;
    const char* name = "";
    char title[RadioEngine::kTextLen] = {};
    Phase phase = Phase::Stopped;
    uint8_t attempt = 0;
    uint32_t bitrate = 0;
    char codec[16] = {};
    int volume = 0;
    bool wantsPlay = false;
};

bool begin();
void service(uint32_t nowMs);  // call every loop: events, scheduled restarts
void togglePlay();
void step(int delta);          // previous/next station; reconnects if playing
void select(uint8_t index);    // station list: jump to index and play it
void changeVolume(int delta);
View view();
}  // namespace Webradio
