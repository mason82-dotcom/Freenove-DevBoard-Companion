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

namespace {
// Decodes one code point at s (UTF-8, or one Latin-1 byte if the bytes are
// not a valid UTF-8 sequence); returns the number of bytes consumed.
size_t decode(const unsigned char* s, uint32_t& cp) {
    const unsigned char c = s[0];
    if (c < 0x80) {
        cp = c;
        return 1;
    }
    const size_t need = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 0;
    if (need > 0) {
        uint32_t value = c & (0x7F >> need);
        size_t i = 1;
        for (; i < need && (s[i] & 0xC0) == 0x80; ++i) value = (value << 6) | (s[i] & 0x3F);
        if (i == need && value >= 0x80) {
            cp = value;
            return need;
        }
    }
    cp = c;  // Latin-1 byte
    return 1;
}

// ASCII replacement for a code point ("" drops it, nullptr = unknown).
const char* ascii(uint32_t cp) {
    switch (cp) {
        case 0xE4: return "ae";
        case 0xF6: return "oe";
        case 0xFC: return "ue";
        case 0xC4: return "Ae";
        case 0xD6: return "Oe";
        case 0xDC: return "Ue";
        case 0xDF: return "ss";
        case 0xC6: return "AE";
        case 0xE6: return "ae";
        case 0xA0: return " ";       // no-break space
        case 0xAD: return "";        // soft hyphen
        case 0x2018: case 0x2019: case 0x201A: case 0xB4: return "'";
        case 0x201C: case 0x201D: case 0x201E: case 0xAB: case 0xBB: return "\"";
        case 0x2013: case 0x2014: case 0x2212: return "-";
        case 0x2026: return "...";
        case 0x20AC: return "EUR";
        default: break;
    }
    struct Range { uint32_t first, last; char letter; };
    static const Range kAccents[] = {
        {0xC0, 0xC5, 'A'}, {0xC7, 0xC7, 'C'}, {0xC8, 0xCB, 'E'}, {0xCC, 0xCF, 'I'},
        {0xD1, 0xD1, 'N'}, {0xD2, 0xD5, 'O'}, {0xD8, 0xD8, 'O'}, {0xD9, 0xDB, 'U'},
        {0xDD, 0xDD, 'Y'}, {0xE0, 0xE5, 'a'}, {0xE7, 0xE7, 'c'}, {0xE8, 0xEB, 'e'},
        {0xEC, 0xEF, 'i'}, {0xF1, 0xF1, 'n'}, {0xF2, 0xF5, 'o'}, {0xF8, 0xF8, 'o'},
        {0xF9, 0xFB, 'u'}, {0xFD, 0xFD, 'y'}, {0xFF, 0xFF, 'y'},
    };
    static char letter[2];
    for (const Range& r : kAccents) {
        if (cp >= r.first && cp <= r.last) {
            letter[0] = r.letter;
            letter[1] = 0;
            return letter;
        }
    }
    return nullptr;
}
}  // namespace

void toDisplayAscii(char* dst, const char* src, size_t capacity) {
    if (dst == nullptr || capacity == 0) return;
    if (src == nullptr) src = "";
    const unsigned char* s = reinterpret_cast<const unsigned char*>(src);
    size_t n = 0;
    while (*s && n + 1 < capacity) {
        uint32_t cp = 0;
        s += decode(s, cp);
        const char* rep = nullptr;
        char single[2] = {0, 0};
        if (cp >= 0x20 && cp < 0x7F) {
            single[0] = static_cast<char>(cp);
            rep = single;
        } else if (cp < 0x20 || cp == 0x7F) {
            rep = " ";  // control characters in ICY metadata
        } else {
            rep = ascii(cp);
            if (rep == nullptr) rep = "?";
        }
        const size_t len = strlen(rep);
        if (n + len >= capacity) break;  // never cut a replacement in half
        memcpy(dst + n, rep, len);
        n += len;
    }
    dst[n] = '\0';
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
