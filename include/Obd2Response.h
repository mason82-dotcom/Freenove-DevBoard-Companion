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

// Copies the "ELM327 vX.Y" identification from an ATZ reply, without any
// vendor suffix in parentheses, truncated to fit one display line.
constexpr size_t kAdapterIdMax = 15;
inline bool adapterId(const char* reply, char* out, size_t capacity) {
    if (!out || capacity == 0) return false;
    out[0] = '\0';
    if (!reply) return false;
    const char* id = nullptr;
    static const char kTag[] = "ELM327";
    for (size_t i = 0; reply[i] != '\0' && i < 512 && id == nullptr; ++i) {
        size_t k = 0;
        while (kTag[k] != '\0' && reply[i + k] == kTag[k]) ++k;
        if (kTag[k] == '\0') id = reply + i;
    }
    if (!id) return false;
    size_t n = 0;
    while (id[n] && id[n] != '\r' && id[n] != '\n' && id[n] != '(' &&
           n < kAdapterIdMax && n + 1 < capacity) {
        out[n] = id[n];
        ++n;
    }
    while (n > 0 && out[n - 1] == ' ') --n;
    out[n] = '\0';
    return n > 0;
}

// ATRV reply "12.6V" (clones: "12.6 V", leading CR). Plausible 0..40 V only.
inline bool adapterVoltage(const char* reply, float& volts) {
    if (!reply) return false;
    size_t i = 0;
    while (reply[i] == '\r' || reply[i] == '\n' || reply[i] == ' ') ++i;
    float value = 0;
    int digits = 0, decimals = -1;
    for (; reply[i] != '\0' && i < 32; ++i) {
        const char c = reply[i];
        if (c >= '0' && c <= '9') {
            value = value * 10 + (c - '0');
            ++digits;
            if (decimals >= 0) ++decimals;
        } else if (c == '.' && decimals < 0 && digits > 0) {
            decimals = 0;
        } else {
            break;
        }
    }
    while (reply[i] == ' ') ++i;
    if (digits == 0 || decimals == 0 || (reply[i] != 'V' && reply[i] != 'v')) return false;
    for (int d = 0; d < decimals; ++d) value /= 10;
    if (value > 40.0f) return false;
    volts = value;
    return true;
}

// ATDPN reply "A6" (automatic, protocol 6) or "6". Short display names.
inline const char* protocolName(const char* reply) {
    if (!reply) return nullptr;
    size_t i = 0;
    while (reply[i] == '\r' || reply[i] == '\n' || reply[i] == ' ') ++i;
    if (reply[i] == 'A' && reply[i + 1] != '\0' && reply[i + 1] != '\r') ++i;
    const char c = reply[i];
    const char next = reply[i + 1];
    if (next != '\0' && next != '\r' && next != '\n' && next != ' ') return nullptr;
    switch (c) {
        case '1': return "J1850 PWM";
        case '2': return "J1850 VPW";
        case '3': return "ISO 9141";
        case '4': return "KWP 5 Baud";
        case '5': return "KWP schnell";
        case '6': return "CAN 11/500";
        case '7': return "CAN 29/500";
        case '8': return "CAN 11/250";
        case '9': return "CAN 29/250";
        case 'A': return "J1939";
        default: return nullptr;
    }
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
    return pid == 0x0C || pid == 0x10 || pid == 0x42 ? 2 : 1;
}
inline bool decodeValue(uint8_t pid, const uint8_t* d, float& value) {
    switch (pid) {
        case 0x04: case 0x11: value = d[0] * 100.0f / 255.0f; return true;  // %
        case 0x05: case 0x0F: value = static_cast<float>(d[0]) - 40.0f; return true;  // C
        case 0x0C: value = ((d[0] << 8) | d[1]) / 4.0f; return true;  // rpm
        case 0x0D: value = d[0]; return true;  // km/h
        case 0x06: case 0x07: case 0x08: case 0x09:  // fuel trim %, -100..+99.2
            value = (static_cast<float>(d[0]) - 128.0f) * 100.0f / 128.0f; return true;
        case 0x10: value = ((d[0] << 8) | d[1]) / 100.0f; return true;  // MAF g/s
        case 0x42: value = ((d[0] << 8) | d[1]) / 1000.0f; return true;  // V
        default: return false;
    }
}
}  // namespace Obd2Response
