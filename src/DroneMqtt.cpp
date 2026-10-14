#include "DroneMqtt.h"
#include <string.h>

namespace {
// Fixed header + remaining length; returns header size or 0.
size_t putHeader(uint8_t* out, size_t capacity, uint8_t type, uint32_t remaining) {
    if (remaining > 268435455u || capacity < 2) return 0;
    size_t n = 0;
    out[n++] = type;
    do {
        if (n >= capacity) return 0;
        uint8_t digit = remaining % 128;
        remaining /= 128;
        if (remaining > 0) digit |= 0x80;
        out[n++] = digit;
    } while (remaining > 0);
    return n;
}

bool putString(uint8_t* out, size_t capacity, size_t& n, const char* text) {
    const size_t length = strlen(text);
    if (length > 0xFFFF || n + 2 + length > capacity) return false;
    out[n++] = static_cast<uint8_t>(length >> 8);
    out[n++] = static_cast<uint8_t>(length & 0xFF);
    memcpy(out + n, text, length);
    n += length;
    return true;
}
}  // namespace

namespace DroneMqtt {
size_t encodeConnect(uint8_t* out, size_t capacity, const char* clientId,
                     const char* user, const char* password, uint16_t keepAliveS) {
    if (!out || !clientId || !user || !password || !*clientId || !*user) return 0;
    const uint32_t remaining = 10 + 2 + strlen(clientId) + 2 + strlen(user) +
                               2 + strlen(password);
    size_t n = putHeader(out, capacity, 0x10, remaining);
    if (n == 0 || n + remaining > capacity) return 0;
    static const uint8_t kVariable[] = {0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04,
                                        0xC2};  // user + password + clean session
    memcpy(out + n, kVariable, sizeof(kVariable));
    n += sizeof(kVariable);
    out[n++] = static_cast<uint8_t>(keepAliveS >> 8);
    out[n++] = static_cast<uint8_t>(keepAliveS & 0xFF);
    if (!putString(out, capacity, n, clientId) || !putString(out, capacity, n, user) ||
        !putString(out, capacity, n, password))
        return 0;
    return n;
}

size_t encodeSubscribe(uint8_t* out, size_t capacity, uint16_t packetId,
                       const char* topicFilter) {
    if (!out || !topicFilter || !*topicFilter || packetId == 0) return 0;
    const uint32_t remaining = 2 + 2 + strlen(topicFilter) + 1;
    size_t n = putHeader(out, capacity, 0x82, remaining);  // SUBSCRIBE, reserved flags 0010
    if (n == 0 || n + remaining > capacity) return 0;
    out[n++] = static_cast<uint8_t>(packetId >> 8);
    out[n++] = static_cast<uint8_t>(packetId & 0xFF);
    if (!putString(out, capacity, n, topicFilter)) return 0;
    out[n++] = 0x00;  // requested QoS 0
    return n;
}

size_t encodePingreq(uint8_t* out, size_t capacity) {
    if (!out || capacity < 2) return 0;
    out[0] = 0xC0;
    out[1] = 0x00;
    return 2;
}

size_t encodeDisconnect(uint8_t* out, size_t capacity) {
    if (!out || capacity < 2) return 0;
    out[0] = 0xE0;
    out[1] = 0x00;
    return 2;
}

int decodeRemainingLength(const uint8_t* in, size_t available, uint32_t& value) {
    value = 0;
    uint32_t multiplier = 1;
    for (size_t i = 0; i < 4; ++i) {
        if (i >= available) return 0;
        value += (in[i] & 0x7F) * multiplier;
        if ((in[i] & 0x80) == 0) {
            // Reject non-minimal encodings (e.g. 0x80 0x00).
            if (i > 0 && in[i] == 0) return -1;
            return static_cast<int>(i + 1);
        }
        multiplier *= 128;
    }
    return -1;
}

int connackCode(const uint8_t* body, size_t length) {
    if (!body || length != 2) return -1;
    return body[1];
}

bool subackGranted(const uint8_t* body, size_t length, uint16_t packetId) {
    if (!body || length != 3) return false;
    const uint16_t id = static_cast<uint16_t>(body[0] << 8 | body[1]);
    return id == packetId && body[2] == 0x00;
}

bool parsePublish(uint8_t flags, const uint8_t* body, size_t length, Message& out) {
    out = Message{};
    if (!body || (flags & 0x06) != 0) return false;  // QoS 0 only (we subscribe with 0)
    if (length < 2) return false;
    const size_t topicLength = static_cast<size_t>(body[0] << 8 | body[1]);
    if (topicLength == 0 || 2 + topicLength > length) return false;
    out.topic = reinterpret_cast<const char*>(body + 2);
    out.topicLength = topicLength;
    out.payload = body + 2 + topicLength;
    out.payloadLength = length - 2 - topicLength;
    return true;
}
}  // namespace DroneMqtt

