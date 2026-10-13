#pragma once
#include <stddef.h>
#include <stdint.h>

// Strict ELM327 text-reply decoder for service 01 (no Arduino, no heap).
// Rejects echoes, "SEARCHING...", headers and malformed lines rather than
// guessing; the same code is compiled and tested on the host.
namespace Obd2Response {
enum class Kind : uint8_t { Data, NoData, Error };

inline int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// One line of hex byte pairs (spaces optional). False for anything else.
inline bool decodeLine(const char* text, size_t length, uint8_t* out, size_t& size) {
    size = 0;
    if (!text || length == 0 || length > 128) return false;
    int high = -1;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == ' ' || text[i] == '\t') {
            if (high >= 0) return false;  // split byte
            continue;
        }
        const int n = nibble(text[i]);
        if (n < 0) return false;
        if (high < 0) {
            high = n;
        } else {
            if (size >= 32) return false;
            out[size++] = static_cast<uint8_t>((high << 4) | n);
            high = -1;
        }
    }
    return high < 0 && size > 0;
}

// Calls visit(bytes, size) for every well-formed line; bounded to 512 chars.
template <class Visitor>
inline void forEachLine(const char* reply, Visitor visit) {
    if (!reply) return;
    const char* begin = reply;
    for (size_t i = 0;; ++i) {
        const char c = reply[i];
        if (i >= 512 || c == '\0' || c == '\r' || c == '\n' || c == '>') {
            uint8_t bytes[32];
            size_t size = 0;
            if (decodeLine(begin, static_cast<size_t>(reply + i - begin), bytes, size))
                visit(bytes, size);
            if (i >= 512 || c == '\0') return;
            begin = reply + i + 1;
        }
    }
}

inline bool contains(const char* text, const char* word) {
    if (!text) return false;
    for (size_t i = 0; text[i] != '\0' && i < 512; ++i) {
        size_t k = 0;
        while (word[k] != '\0' && text[i + k] == word[k]) ++k;
        if (word[k] == '\0') return true;
    }
    return false;
}

// Mode 01 reply "41 PP A [B]" for pid; first matching ECU line wins.
inline bool readPid(const char* reply, uint8_t pid, uint8_t* out, size_t count) {
    if (!out || count == 0 || count > 4) return false;
    bool matched = false;
    forEachLine(reply, [&](const uint8_t* b, size_t n) {
        if (matched || n < 2 + count || b[0] != 0x41 || b[1] != pid) return;
        for (size_t i = 0; i < count; ++i) out[i] = b[2 + i];
        matched = true;
    });
    return matched;
}

inline Kind classify(const char* reply, uint8_t pid) {
    uint8_t scratch[4];
    if (readPid(reply, pid, scratch, 1)) return Kind::Data;
    if (contains(reply, "NO DATA")) return Kind::NoData;
    return Kind::Error;  // ?, UNABLE TO CONNECT, CAN ERROR, STOPPED, garbage
}

// Supported-PID bitmap of base 0x00/0x20/0x40, OR-combined across ECUs.
inline bool supportedBitmap(const char* reply, uint8_t base, uint32_t& mask) {
    mask = 0;
    bool any = false;
    forEachLine(reply, [&](const uint8_t* bytes, size_t n) {
        if (n < 6 || bytes[0] != 0x41 || bytes[1] != base) return;
        mask |= (static_cast<uint32_t>(bytes[2]) << 24) | (static_cast<uint32_t>(bytes[3]) << 16) |
                (static_cast<uint32_t>(bytes[4]) << 8) | bytes[5];
        any = true;
    });
    return any;
}

// True if pid (1..0x60) is flagged in the bitmap of its 0x20 block.
inline bool isSupported(uint32_t mask, uint8_t base, uint8_t pid) {
    if (pid <= base || pid > base + 0x20) return false;
    return (mask >> (32 - (pid - base))) & 1u;
}

// Number of data bytes and SAE J1979 scaling for the module's PIDs.
inline size_t dataBytes(uint8_t pid) {
    return pid == 0x0C || pid == 0x42 ? 2 : 1;
}
inline bool decodeValue(uint8_t pid, const uint8_t* d, float& value) {
    switch (pid) {
        case 0x04: case 0x11: value = d[0] * 100.0f / 255.0f; return true;  // %
        case 0x05: case 0x0F: value = static_cast<float>(d[0]) - 40.0f; return true;  // C
        case 0x0C: value = ((d[0] << 8) | d[1]) / 4.0f; return true;  // rpm
        case 0x0D: value = d[0]; return true;  // km/h
        case 0x42: value = ((d[0] << 8) | d[1]) / 1000.0f; return true;  // V
        default: return false;
    }
}
}  // namespace Obd2Response
