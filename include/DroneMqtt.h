#pragma once
#include <stddef.h>
#include <stdint.h>

// Minimal MQTT 3.1.1 codec for a subscribe-only client. Deliberately there is
// no PUBLISH encoder: the drone page cannot send anything to the broker except
// session control (CONNECT, SUBSCRIBE, PINGREQ, DISCONNECT).
namespace DroneMqtt {
constexpr uint8_t kConnack = 0x20;
constexpr uint8_t kPublish = 0x30;
constexpr uint8_t kSuback = 0x90;
constexpr uint8_t kPingresp = 0xD0;

// Each returns the packet length, or 0 if it does not fit / input is invalid.
size_t encodeConnect(uint8_t* out, size_t capacity, const char* clientId,
                     const char* user, const char* password, uint16_t keepAliveS);
size_t encodeSubscribe(uint8_t* out, size_t capacity, uint16_t packetId,
                       const char* topicFilter);  // QoS 0
size_t encodePingreq(uint8_t* out, size_t capacity);
size_t encodeDisconnect(uint8_t* out, size_t capacity);

// Remaining-length varint. Returns bytes consumed (1..4), 0 if more bytes are
// needed, -1 if malformed.
int decodeRemainingLength(const uint8_t* in, size_t available, uint32_t& value);

// CONNACK body (2 bytes): return code, 0 = accepted, 4/5 = bad credentials.
int connackCode(const uint8_t* body, size_t length);
// SUBACK body: true only if packetId matches and QoS 0 was granted.
bool subackGranted(const uint8_t* body, size_t length, uint16_t packetId);

// PUBLISH body (QoS 0 only). Points into body; topic is not terminated.
struct Message {
    const char* topic = nullptr;
    size_t topicLength = 0;
    const uint8_t* payload = nullptr;
    size_t payloadLength = 0;
};
bool parsePublish(uint8_t flags, const uint8_t* body, size_t length, Message& out);
}  // namespace DroneMqtt
