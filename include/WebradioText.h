#pragma once
#include <stddef.h>
#include <stdint.h>

// Pure, host-testable text helpers for the Webradio module.
namespace Webradio {
enum class Phase : uint8_t { Stopped, NoWifi, Connecting, Playing, Retrying, Failed };
constexpr uint8_t kMaxRetries = 3;

// strlcpy that never leaves half a UTF-8 sequence at the end (LVGL would
// render a replacement glyph or garbage).
void utf8Copy(char* dst, const char* src, size_t capacity);
// One status line, e.g. "Wiedergabe | 128 kbps MP3" or "Neustart 2/3 ...".
void formatStatus(Phase phase, uint8_t attempt, uint32_t bitrate, const char* codec,
                  char* out, size_t capacity);
}  // namespace Webradio
