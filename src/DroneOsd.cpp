#include "DroneTelemetry.h"
#include <ArduinoJson.h>
#include <initializer_list>
#include <stdlib.h>
#include <string.h>

// DJI Cloud API osd envelope: {"tid","bid","timestamp","gateway":<rc sn>,
// "data":{...}} on "thing/product/<device sn>/osd". Aircraft data carries
// mode_code; remote-controller data carries capacity_percent/wireless_link.
namespace {
using DroneTelemetry::Snapshot;

bool fresh(uint32_t atMs, uint32_t nowMs) {
    return static_cast<uint32_t>(nowMs - atMs) < DroneTelemetry::kStaleMs;
}

int intOr(JsonVariantConst v, int fallback) {
    return v.is<int>() || v.is<float>() ? v.as<int>() : fallback;
}

// "67-0-0" -> 67. Pilot 2 (RC Pro, seen 2026-10-14 with a Mavic 3M) reports
// the camera in cameras[]; the DJI demo model names payloads[].
int payloadType(JsonVariantConst list) {
    JsonVariantConst index = list[0]["payload_index"];
    if (!index.is<const char*>()) return -1;
    const char* text = index.as<const char*>();
    char* end = nullptr;
    const long type = strtol(text, &end, 10);
    return end != text && *end == '-' && type > 0 && type < 1000 ? static_cast<int>(type) : -1;
}

void applyAircraft(JsonVariantConst data, const char* sn, const char* gateway,
                   uint32_t nowMs, DroneTelemetry::Aircraft& a) {
    if (strcmp(a.sn, sn) != 0) a = DroneTelemetry::Aircraft{};  // new aircraft: no stale values
    a.present = true;
    strncpy(a.sn, sn, sizeof(a.sn) - 1);
    if (gateway) strncpy(a.gateway, gateway, sizeof(a.gateway) - 1);
    a.atMs = nowMs;
    a.modeCode = intOr(data["mode_code"], a.modeCode);
    int type = payloadType(data["cameras"]);
    if (type < 0) type = payloadType(data["payloads"]);
    if (type > 0) a.payloadType = type;
    JsonVariantConst battery = data["battery"];
    a.batteryPct = intOr(battery["capacity_percent"], a.batteryPct);
    a.remainFlightS = intOr(battery["remain_flight_time"], a.remainFlightS);
    if (data["elevation"].is<float>()) {
        a.hasElevation = true;
        a.elevationM = data["elevation"].as<float>();
    }
    if (data["home_distance"].is<float>()) {
        a.hasHomeDistance = true;
        a.homeDistanceM = data["home_distance"].as<float>();
    }
    if (data["horizontal_speed"].is<float>() && data["vertical_speed"].is<float>()) {
        a.hasSpeed = true;
        a.horizontalMs = data["horizontal_speed"].as<float>();
        a.verticalMs = data["vertical_speed"].as<float>();
    }
    JsonVariantConst position = data["position_state"];
    a.gpsSats = intOr(position["gps_number"], a.gpsSats);
    a.rtkSats = intOr(position["rtk_number"], a.rtkSats);
    a.rtkFixed = intOr(position["is_fixed"], a.rtkFixed);
}
}  // namespace

namespace DroneTelemetry {
bool serialFromTopic(const char* topic, char* out, size_t capacity) {
    static const char kPrefix[] = "thing/product/";
    static const char kSuffix[] = "/osd";
    if (!topic || !out || capacity == 0) return false;
    const size_t length = strlen(topic);
    const size_t prefix = sizeof(kPrefix) - 1, suffix = sizeof(kSuffix) - 1;
    if (length <= prefix + suffix || strncmp(topic, kPrefix, prefix) != 0 ||
        strcmp(topic + length - suffix, kSuffix) != 0)
        return false;
    const size_t snLength = length - prefix - suffix;
    if (snLength >= capacity) return false;
    for (size_t i = 0; i < snLength; ++i) {
        const char c = topic[prefix + i];
        const bool ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        if (!ok) return false;
    }
    memcpy(out, topic + prefix, snLength);
    out[snLength] = 0;
    return true;
}

bool applyOsd(const char* topic, const char* payload, size_t length,
              uint32_t nowMs, Snapshot& s) {
    char sn[sizeof(s.aircraft.sn)];
    if (!serialFromTopic(topic, sn, sizeof(sn)) || !payload) {
        ++s.ignored;
        return false;
    }
    // Keep only the fields the page shows: positions are never parsed, the
    // document stays small even for multi-kilobyte payload lists.
    JsonDocument filter;
    filter["gateway"] = true;
    JsonObject f = filter["data"].to<JsonObject>();
    for (const char* key : {"mode_code", "elevation", "home_distance", "horizontal_speed",
                            "vertical_speed", "capacity_percent"})
        f[key] = true;
    f["battery"]["capacity_percent"] = true;
    f["battery"]["remain_flight_time"] = true;
    f["position_state"]["gps_number"] = true;
    f["position_state"]["rtk_number"] = true;
    f["position_state"]["is_fixed"] = true;
    f["cameras"][0]["payload_index"] = true;
    f["payloads"][0]["payload_index"] = true;
    f["wireless_link"]["sdr_quality"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, payload, length, DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(8)) != DeserializationError::Ok) {
        ++s.ignored;
        return false;
    }
    JsonVariantConst data = doc["data"];
    if (!data.is<JsonObjectConst>()) {
        ++s.ignored;
        return false;
    }
    const char* gateway = doc["gateway"].is<const char*>() ? doc["gateway"].as<const char*>() : nullptr;

    if (!data["mode_code"].isNull()) {
        // Aircraft. Stick with the selected one while it is alive.
        Aircraft& a = s.aircraft;
        if (a.present && fresh(a.atMs, nowMs) && strcmp(a.sn, sn) != 0) {
            ++s.ignored;
            return false;
        }
        applyAircraft(data, sn, gateway, nowMs, a);
        ++s.messages;
        return true;
    }
    if (!data["capacity_percent"].isNull() || !data["wireless_link"].isNull()) {
        // Remote controller: the selected aircraft's gateway, or any remote
        // while no aircraft is selected ("controller on, aircraft off").
        Remote& r = s.remote;
        const bool selectedGateway = s.aircraft.present && strcmp(s.aircraft.gateway, sn) == 0;
        const bool noAircraft = !s.aircraft.present || !fresh(s.aircraft.atMs, nowMs);
        const bool sameRemote = r.present && strcmp(r.sn, sn) == 0;
        if (!selectedGateway && !(noAircraft && (sameRemote || !r.present || !fresh(r.atMs, nowMs)))) {
            ++s.ignored;
            return false;
        }
        if (!sameRemote) r = Remote{};
        r.present = true;
        strncpy(r.sn, sn, sizeof(r.sn) - 1);
        r.atMs = nowMs;
        r.batteryPct = intOr(data["capacity_percent"], r.batteryPct);
        r.linkQuality = intOr(data["wireless_link"]["sdr_quality"], r.linkQuality);
        ++s.messages;
        return true;
    }
    ++s.ignored;  // dock or unknown device type
    return false;
}
}  // namespace DroneTelemetry
