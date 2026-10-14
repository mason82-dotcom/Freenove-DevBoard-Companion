#include "DroneTelemetry.h"
#include "DroneMqtt.h"
#include "WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#if __has_include("DroneLocalConfig.h")
#include "DroneLocalConfig.h"
#define DRONE_LOCAL_CONFIG_PRESENT 1
#else
#define DRONE_LOCAL_CONFIG_PRESENT 0
#endif

// Subscribe-only MQTT session to the AeroNexus broker while the Drohne page is
// open. Read-only by construction: DroneMqtt has no PUBLISH encoder, and the
// broker account "companion" may only subscribe to thing/product/+/osd.
// Positions are never parsed or logged (flight sites stay private).
namespace {
using DroneTelemetry::Link;
using DroneTelemetry::Snapshot;

portMUX_TYPE snapshotLock = portMUX_INITIALIZER_UNLOCKED;
Snapshot current;
TaskHandle_t worker = nullptr;
std::atomic<bool> desiredActive{false};

void store(const Snapshot& s) {
    portENTER_CRITICAL(&snapshotLock);
    current = s;
    portEXIT_CRITICAL(&snapshotLock);
}

#if DRONE_LOCAL_CONFIG_PRESENT
constexpr uint16_t kKeepAliveS = 30;
constexpr uint32_t kPingEveryMs = 15000;
constexpr uint32_t kBrokerSilentMs = 2 * kKeepAliveS * 1000;  // not even a PINGRESP
constexpr uint32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kHandshakeTimeoutMs = 4000;
constexpr uint32_t kPacketTimeoutMs = 5000;  // rest of a packet after its first byte
constexpr uint32_t kRetryDelayMs = 5000;
constexpr uint32_t kAuthRetryDelayMs = 60000;  // do not hammer the broker with bad credentials
constexpr size_t kMaxPacket = 32 * 1024;       // multi-kilobyte osd; larger ones are skipped
constexpr uint16_t kSubscribeId = 1;
constexpr char kTopicFilter[] = "thing/product/+/osd";

// Only the worker task touches these.
NetworkClient client;
uint8_t* packet = nullptr;  // kMaxPacket in PSRAM while a session runs

enum class ReadResult : uint8_t { Packet, None, Skipped, Closed };

bool readExact(uint8_t* out, size_t n, uint32_t deadline) {
    size_t got = 0;
    while (got < n) {
        const int available = client.available();
        if (available > 0) {
            const size_t want = n - got < static_cast<size_t>(available) ? n - got
                                                                          : static_cast<size_t>(available);
            const int r = client.read(out + got, want);
            if (r > 0) got += static_cast<size_t>(r);
            continue;
        }
        if (!client.connected()) return false;
        if (static_cast<int32_t>(millis() - deadline) >= 0) return false;
        vTaskDelay(1);
    }
    return true;
}

bool discard(size_t n, uint32_t deadline) {
    uint8_t scratch[256];
    while (n > 0) {
        const size_t chunk = n < sizeof(scratch) ? n : sizeof(scratch);
        if (!readExact(scratch, chunk, deadline)) return false;
        n -= chunk;
    }
    return true;
}

// Waits up to waitMs for a packet to start, then reads it completely.
ReadResult readPacket(uint8_t& header, size_t& length, uint32_t waitMs) {
    const uint32_t start = millis();
    while (client.available() == 0) {
        if (!client.connected()) return ReadResult::Closed;
        if (static_cast<uint32_t>(millis() - start) >= waitMs) return ReadResult::None;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    const uint32_t deadline = millis() + kPacketTimeoutMs;
    if (!readExact(&header, 1, deadline)) return ReadResult::Closed;
    uint8_t lengthBytes[4];
    uint32_t remaining = 0;
    for (size_t i = 0;; ++i) {
        if (i == sizeof(lengthBytes) || !readExact(&lengthBytes[i], 1, deadline))
            return ReadResult::Closed;
        const int used = DroneMqtt::decodeRemainingLength(lengthBytes, i + 1, remaining);
        if (used > 0) break;
        if (used < 0) return ReadResult::Closed;  // malformed stream: drop the session
    }
    if (remaining > kMaxPacket) {
        return discard(remaining, deadline + kPacketTimeoutMs) ? ReadResult::Skipped
                                                               : ReadResult::Closed;
    }
    if (!readExact(packet, remaining, deadline)) return ReadResult::Closed;
    length = remaining;
    return ReadResult::Packet;
}

bool send(const uint8_t* bytes, size_t n) {
    return n > 0 && client.write(bytes, n) == n;
}

void setLink(Snapshot& s, Link link) {
    if (s.link == link) return;
    s.link = link;
    store(s);
    static const char* const kNames[] = {"not configured", "idle", "waiting for wifi",
                                         "connecting", "subscribed", "auth failed", "error"};
    Serial.printf("Drone: %s (messages=%lu ignored=%lu)\n", kNames[static_cast<int>(link)],
                  static_cast<unsigned long>(s.messages), static_cast<unsigned long>(s.ignored));
}

// MQTT CONNECT + SUBSCRIBE. Returns Subscribed, AuthFailed or Error.
Link handshake() {
    uint8_t out[256];
    char clientId[32];
    const uint64_t mac = ESP.getEfuseMac();
    snprintf(clientId, sizeof(clientId), "fnk0086-companion-%06lx",
             static_cast<unsigned long>((mac >> 24) & 0xFFFFFF));
    if (!send(out, DroneMqtt::encodeConnect(out, sizeof(out), clientId, DRONE_MQTT_USER,
                                            DRONE_MQTT_PASSWORD, kKeepAliveS)))
        return Link::Error;
    uint8_t header = 0;
    size_t length = 0;
    if (readPacket(header, length, kHandshakeTimeoutMs) != ReadResult::Packet ||
        header != DroneMqtt::kConnack)
        return Link::Error;
    const int code = DroneMqtt::connackCode(packet, length);
    if (code == 4 || code == 5) return Link::AuthFailed;  // bad credentials / not authorized
    if (code != 0) return Link::Error;
    if (!send(out, DroneMqtt::encodeSubscribe(out, sizeof(out), kSubscribeId, kTopicFilter)))
        return Link::Error;
    // Telemetry may already arrive before the SUBACK; wait for the SUBACK only.
    const uint32_t deadline = millis() + kHandshakeTimeoutMs;
    while (static_cast<int32_t>(millis() - deadline) < 0) {
        const ReadResult r = readPacket(header, length, 200);
        if (r == ReadResult::Closed) return Link::Error;
        if (r != ReadResult::Packet || header != DroneMqtt::kSuback) continue;
        // 0x80 = refused by the broker ACL: same remedy as wrong credentials.
        return DroneMqtt::subackGranted(packet, length, kSubscribeId) ? Link::Subscribed
                                                                       : Link::AuthFailed;
    }
    return Link::Error;
}

void handlePublish(uint8_t header, size_t length, Snapshot& s) {
    DroneMqtt::Message m;
    char topic[96];
    if (!DroneMqtt::parsePublish(header & 0x0F, packet, length, m) ||
        m.topicLength >= sizeof(topic)) {
        ++s.ignored;
        return;
    }
    memcpy(topic, m.topic, m.topicLength);
    topic[m.topicLength] = 0;
    char before[sizeof(s.aircraft.sn)];
    strncpy(before, s.aircraft.sn, sizeof(before));
    if (DroneTelemetry::applyOsd(topic, reinterpret_cast<const char*>(m.payload),
                                 m.payloadLength, millis(), s) &&
        strcmp(before, s.aircraft.sn) != 0) {
        const size_t n = strlen(s.aircraft.sn);
        Serial.printf("Drone: aircraft ..%s (%s)\n", n > 4 ? s.aircraft.sn + n - 4 : s.aircraft.sn,
                      DroneTelemetry::modelName(s.aircraft.payloadType));
    }
    store(s);
}

// Runs one subscribed session until the page closes or the link breaks.
Link session(Snapshot& s) {
    uint8_t out[4];
    uint32_t lastSent = millis(), lastHeard = millis(), lastLog = millis();
    while (desiredActive.load() && WifiLink::connectedTo(WifiLink::Profile::Home)) {
        const uint32_t now = millis();
        if (now - lastSent >= kPingEveryMs) {
            if (!send(out, DroneMqtt::encodePingreq(out, sizeof(out)))) return Link::Error;
            lastSent = now;
        }
        if (now - lastHeard >= kBrokerSilentMs) return Link::Error;
        if (now - lastLog >= 10000) {
            lastLog = now;
            Serial.printf("Drone: live messages=%lu ignored=%lu stack_free=%u\n",
                          static_cast<unsigned long>(s.messages),
                          static_cast<unsigned long>(s.ignored),
                          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
        uint8_t header = 0;
        size_t length = 0;
        switch (readPacket(header, length, 200)) {
            case ReadResult::Closed: return Link::Error;
            case ReadResult::None: continue;
            case ReadResult::Skipped:
                lastHeard = millis();
                ++s.ignored;
                store(s);
                continue;
            case ReadResult::Packet:
                lastHeard = millis();
                if ((header & 0xF0) == DroneMqtt::kPublish) handlePublish(header, length, s);
                continue;  // PINGRESP and anything else: only proof of life
        }
    }
    return desiredActive.load() ? Link::WaitingForWifi : Link::Idle;
}

void closeSession() {
    if (client.connected()) {
        uint8_t out[2];
        send(out, DroneMqtt::encodeDisconnect(out, sizeof(out)));
    }
    client.stop();
    heap_caps_free(packet);
    packet = nullptr;
}

void task(void*) {
    Snapshot s;
    s.link = Link::Idle;
    store(s);
    for (;;) {
        if (!desiredActive.load()) {
            if (s.link != Link::Idle) {
                closeSession();
                setLink(s, Link::Idle);
                Serial.println("Drone: session closed");
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (s.link == Link::Idle) {
            // Fresh counters for every visit; the last aircraft and its flight
            // statistics stay (shown as offline until it reports again).
            const DroneTelemetry::Aircraft aircraft = s.aircraft;
            const DroneTelemetry::Remote remote = s.remote;
            s = Snapshot{};
            s.link = Link::Idle;
            s.aircraft = aircraft;
            s.remote = remote;
        }
        WifiLink::request(WifiLink::Profile::Home);
        if (!WifiLink::connectedTo(WifiLink::Profile::Home)) {
            setLink(s, Link::WaitingForWifi);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
            continue;
        }
        if (packet == nullptr)
            packet = static_cast<uint8_t*>(heap_caps_malloc(kMaxPacket, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        setLink(s, Link::Connecting);
        Link result = Link::Error;
        if (packet != nullptr && client.connect(DRONE_MQTT_HOST, DRONE_MQTT_PORT, kConnectTimeoutMs)) {
            client.setNoDelay(true);
            result = handshake();
            if (result == Link::Subscribed) {
                setLink(s, Link::Subscribed);
                result = session(s);
            }
        }
        closeSession();
        if (!desiredActive.load()) continue;  // page left: becomes Idle above
        setLink(s, result);  // session() never returns Subscribed
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(result == Link::AuthFailed ? kAuthRetryDelayMs
                                                                            : kRetryDelayMs));
    }
}
#endif
}  // namespace

namespace DroneTelemetry {
void begin() {
#if DRONE_LOCAL_CONFIG_PRESENT
    if (worker != nullptr) return;
    Snapshot s;
    s.link = Link::Idle;
    store(s);
    // Created once and parked: no broker traffic until the page is opened.
    if (xTaskCreatePinnedToCore(task, "drone", 6144, nullptr, 1, &worker, 0) != pdPASS)
        worker = nullptr;
#endif
}

bool requestActive(bool active) {
    if (worker == nullptr) return false;
    desiredActive.store(active);
    xTaskNotifyGive(worker);
    return true;
}

Snapshot read() {
    portENTER_CRITICAL(&snapshotLock);
    const Snapshot copy = current;
    portEXIT_CRITICAL(&snapshotLock);
    return copy;
}
}  // namespace DroneTelemetry
