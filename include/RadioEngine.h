#pragma once
#include <stddef.h>
#include <stdint.h>

// Audio engine ported from the standalone ESP32-S3-Webradio project.
// One persistent task owns the ESP32-audioI2S object (connect, loop, stop);
// it never calls LVGL. The UI talks to it only through queues:
//   UI -> engine: connect/stop commands, volume as "last value wins"
//   engine -> UI: events stamped with the generation of their connect request,
//                 so events of a previous stream can be discarded.
// Not ported: SD recorder, tone control, amplifier SD pin, NVS settings.
namespace RadioEngine {
constexpr size_t kTextLen = 160;  // stream title in events (UTF-8 safe)
constexpr size_t kUrlLen = 257;
constexpr int kMaxVolume = 21;

enum class State : uint8_t { Idle, Connecting, Playing };
enum class EventType : uint8_t { Playing, ConnectFailed, StreamLost, Title };

struct Event {
    EventType type;
    uint32_t gen;
    char text[kTextLen];
};

bool begin();  // I2S + engine task; call once
void connect(uint32_t gen, const char* url);
void stop(uint32_t gen);
void setVolume(int volume);  // clamped 0..kMaxVolume
int volume();
bool pollEvent(Event& out);  // false: no further events
State state();
uint32_t bitrate();  // bit/s, 0 = unknown
void codec(char* out, size_t capacity);
}  // namespace RadioEngine
