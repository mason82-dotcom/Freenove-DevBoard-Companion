#include "WebradioText.h"
#include <stdio.h>
#include <string.h>

namespace Webradio {
void utf8Copy(char* dst, const char* src, size_t capacity) {
    if (dst == nullptr || capacity == 0) return;
    if (src == nullptr) src = "";
    size_t n = 0;
    while (src[n] != '\0' && n + 1 < capacity) {
        dst[n] = src[n];
        ++n;
    }
    dst[n] = '\0';
    if (src[n] == '\0') return;  // fitted completely
    // Truncated: drop a trailing incomplete multi-byte sequence.
    size_t i = n;
    while (i > 0 && (static_cast<unsigned char>(dst[i - 1]) & 0xC0) == 0x80) --i;
    if (i == 0) {
        dst[0] = '\0';
        return;
    }
    const unsigned char lead = static_cast<unsigned char>(dst[i - 1]);
    const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (n - (i - 1) < need) dst[i - 1] = '\0';
}

void formatStatus(Phase phase, uint8_t attempt, uint32_t bitrate, const char* codec,
                  char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    switch (phase) {
        case Phase::Stopped: snprintf(out, capacity, "Gestoppt"); break;
        case Phase::NoWifi: snprintf(out, capacity, "Kein WLAN"); break;
        case Phase::Connecting: snprintf(out, capacity, "Verbinde ..."); break;
        case Phase::Retrying:
            snprintf(out, capacity, "Neustart %u/%u ...", static_cast<unsigned>(attempt),
                     static_cast<unsigned>(kMaxRetries));
            break;
        case Phase::Failed: snprintf(out, capacity, "Stream nicht erreichbar"); break;
        case Phase::Playing:
            if (bitrate >= 1000)
                snprintf(out, capacity, "Wiedergabe | %lu kbps%s%s",
                         static_cast<unsigned long>(bitrate / 1000),
                         codec && codec[0] ? " " : "", codec ? codec : "");
            else
                snprintf(out, capacity, "Wiedergabe");
            break;
    }
}
}  // namespace Webradio
