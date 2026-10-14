#pragma once
#include <stddef.h>
#include <stdint.h>

// Read-only drone telemetry from the AeroNexus EMQX broker (DJI Cloud API
// "thing/product/<sn>/osd"). The worker subscribes only while the Drohne page
// is open; it has no code path that publishes (see DroneMqtt.h), and the
// broker account "companion" may only subscribe to osd topics.
namespace DroneTelemetry {
// An aircraft or remote without a new osd message for this long is offline.
constexpr uint32_t kStaleMs = 6000;

enum class Link : uint8_t {
    NotConfigured, Idle, WaitingForWifi, Connecting, Subscribed, AuthFailed, Error
};

struct Aircraft {
    bool present = false;
    char sn[24] = {};
    char gateway[24] = {};    // serial of the remote controller it is attached to
    int payloadType = -1;     // camera type from payload_index, e.g. 67 = M3T
    uint32_t atMs = 0;
    int modeCode = -1;        // DJI mode_code, -1 unknown
    int batteryPct = -1;
    int remainFlightS = -1;
    bool hasElevation = false;
    float elevationM = 0;     // relative to the take-off point
    bool hasHomeDistance = false;
    float homeDistanceM = 0;
    bool hasSpeed = false;
    float horizontalMs = 0;
    float verticalMs = 0;
    int gpsSats = -1;
    int rtkSats = -1;
    int rtkFixed = -1;        // 0 not, 1 fixing, 2 fixed, 3 failed
};

struct Remote {
    bool present = false;
    char sn[24] = {};
    uint32_t atMs = 0;
    int batteryPct = -1;
    int linkQuality = -1;     // wireless_link.sdr_quality 0..5
};

struct Snapshot {
    Link link = Link::NotConfigured;
    Aircraft aircraft;
    Remote remote;
    uint32_t messages = 0;    // osd messages accepted this session
    uint32_t ignored = 0;     // other devices, oversized or malformed messages
};

void begin();
// UI only: start/stop the subscription when the page is entered/left.
bool requestActive(bool active);
Snapshot read();

// --- Pure, host-testable helpers (no Arduino) -------------------------------
// Applies one osd message to the snapshot. The first aircraft heard stays
// selected until it has been silent for kStaleMs; then the next one that
// reports takes over ("automatically the device that is online").
// Returns false if the message was ignored.
bool applyOsd(const char* topic, const char* payload, size_t length,
              uint32_t nowMs, Snapshot& s);
// Device serial from "thing/product/<sn>/osd"; false for any other topic.
bool serialFromTopic(const char* topic, char* out, size_t capacity);
const char* modelName(int payloadType);
const char* modeName(int modeCode);
void format(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity);
}  // namespace DroneTelemetry
