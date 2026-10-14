// Native host regression for osd parsing and automatic aircraft selection
// (needs ArduinoJson on the include path).
#include "DroneTelemetry.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace DroneTelemetry;

namespace {
bool apply(const char* topic, const char* json, uint32_t now, Snapshot& s) {
    return applyOsd(topic, json, std::strlen(json), now, s);
}

const char* kAircraft =
    R"({"tid":"t","bid":"b","timestamp":1,"gateway":"RC0001","data":{
        "mode_code":3,"elevation":35.2,"height":512.7,"latitude":48.1,"longitude":9.2,
        "home_distance":120,"horizontal_speed":5.3,"vertical_speed":-0.2,
        "battery":{"capacity_percent":87,"remain_flight_time":1390,"batteries":[{"capacity_percent":87}]},
        "position_state":{"gps_number":18,"rtk_number":32,"is_fixed":2,"quality":5},
        "payloads":[{"payload_index":"67-0-0","gimbal_pitch":-30.0}]}})";
const char* kRemote =
    R"({"gateway":"RC0001","data":{"capacity_percent":76,"latitude":48.1,
        "wireless_link":{"sdr_quality":5,"sdr_link_state":true}}})";
const char* kOtherAircraft =
    R"({"gateway":"RC0002","data":{"mode_code":0,"elevation":0,
        "payloads":[{"payload_index":"89-0-0"}]}})";
const char* kOtherRemote = R"({"gateway":"RC0002","data":{"capacity_percent":40}})";
// Shape reported by Pilot 2 on an RC Pro with a Mavic 3M (2026-10-14): the
// camera is in cameras[] and as a "68-0-0" key, there is no payloads[] list.
const char* kPilot2Aircraft =
    R"({"gateway":"RC0003","data":{"68-0-0":{"payload_index":"68-0-0","gimbal_pitch":0},
        "cameras":[{"camera_mode":0,"payload_index":"68-0-0"}],"mode_code":0,"elevation":0,
        "home_distance":0,"horizontal_speed":0,"vertical_speed":0,
        "battery":{"capacity_percent":64,"remain_flight_time":0}}})";
}  // namespace

int main() {
    char sn[24];
    assert(serialFromTopic("thing/product/1581F5BKD225A00A1B2/osd", sn, sizeof(sn)));
    assert(std::strcmp(sn, "1581F5BKD225A00A1B2") == 0);
    assert(!serialFromTopic("thing/product/AB/state", sn, sizeof(sn)));
    assert(!serialFromTopic("thing/product//osd", sn, sizeof(sn)));
    assert(!serialFromTopic("thing/product/A+B/osd", sn, sizeof(sn)));
    assert(!serialFromTopic("sys/product/AB/osd", sn, sizeof(sn)));
    assert(!serialFromTopic("thing/product/0123456789012345678901234/osd", sn, sizeof(sn)));

    Snapshot s;
    s.link = Link::Subscribed;
    // A remote first: shown while no aircraft is selected.
    assert(apply("thing/product/RC0001/osd", kRemote, 1000, s));
    assert(s.remote.present && s.remote.batteryPct == 76 && s.remote.linkQuality == 5);
    assert(!s.aircraft.present);

    assert(apply("thing/product/AC0001/osd", kAircraft, 2000, s));
    const Aircraft& a = s.aircraft;
    assert(a.present && std::strcmp(a.sn, "AC0001") == 0 && std::strcmp(a.gateway, "RC0001") == 0);
    assert(a.payloadType == 67 && a.modeCode == 3 && a.batteryPct == 87 && a.remainFlightS == 1390);
    assert(a.hasElevation && a.elevationM > 35.1f && a.elevationM < 35.3f);
    assert(a.hasHomeDistance && a.homeDistanceM == 120.0f);  // integer JSON value
    assert(a.hasSpeed && a.verticalMs < -0.1f);
    assert(a.gpsSats == 18 && a.rtkSats == 32 && a.rtkFixed == 2);

    // A second aircraft and its remote are ignored while the first one reports.
    const uint32_t ignored = s.ignored;
    assert(!apply("thing/product/AC0002/osd", kOtherAircraft, 3000, s));
    assert(!apply("thing/product/RC0002/osd", kOtherRemote, 3000, s));
    assert(s.ignored == ignored + 2 && std::strcmp(s.aircraft.sn, "AC0001") == 0);
    assert(s.remote.batteryPct == 76);

    // After kStaleMs of silence the next reporting aircraft takes over, with
    // no values carried over from the previous one.
    const uint32_t later = 2000 + kStaleMs + 1;
    assert(apply("thing/product/AC0002/osd", kOtherAircraft, later, s));
    assert(std::strcmp(s.aircraft.sn, "AC0002") == 0 && s.aircraft.payloadType == 89);
    assert(s.aircraft.batteryPct == -1 && !s.aircraft.hasHomeDistance && s.aircraft.modeCode == 0);
    assert(apply("thing/product/RC0002/osd", kOtherRemote, later + 10, s));
    assert(std::strcmp(s.remote.sn, "RC0002") == 0 && s.remote.batteryPct == 40);
    assert(s.remote.linkQuality == -1);

    // Malformed, foreign and dock messages are counted, not applied.
    const uint32_t before = s.ignored;
    assert(!apply("thing/product/AC0002/osd", "{not json", later + 20, s));
    assert(!apply("thing/product/AC0002/osd", R"({"data":[]})", later + 20, s));
    assert(!apply("thing/product/DOCK01/osd", R"({"data":{"mode_code_dock":1}})", later + 20, s));
    assert(!apply("thing/product/AC0002/state", kAircraft, later + 20, s));
    assert(s.ignored == before + 4);
    // Pilot 2 shape: model from cameras[], once the previous aircraft is silent.
    Snapshot p;
    p.link = Link::Subscribed;
    assert(apply("thing/product/AC0003/osd", kPilot2Aircraft, 100, p));
    assert(p.aircraft.payloadType == 68 && p.aircraft.batteryPct == 64 && p.aircraft.modeCode == 0);
    std::puts("drone osd smoke: OK");
    return 0;
}
