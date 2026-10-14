// Native host regression for the subscribe-only MQTT codec and the page text.
#include "DroneMqtt.h"
#include "DroneTelemetry.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace DroneMqtt;

int main() {
    uint8_t b[300];
    // CONNECT: fixed header, protocol "MQTT" level 4, flags user+password+clean.
    size_t n = encodeConnect(b, sizeof(b), "id", "companion", "secret123", 30);
    const uint8_t connect[] = {0x10, 36, 0, 4, 'M', 'Q', 'T', 'T', 4, 0xC2, 0, 30,
                               0, 2, 'i', 'd', 0, 9, 'c', 'o', 'm', 'p', 'a', 'n', 'i', 'o', 'n',
                               0, 9, 's', 'e', 'c', 'r', 'e', 't', '1', '2', '3'};
    assert(n == sizeof(connect) && std::memcmp(b, connect, n) == 0);
    assert(encodeConnect(b, 20, "id", "companion", "secret123", 30) == 0);  // too small
    assert(encodeConnect(b, sizeof(b), "", "u", "p", 30) == 0);

    // SUBSCRIBE: type 0x82 (reserved flags), packet id, filter, QoS 0.
    n = encodeSubscribe(b, sizeof(b), 1, "thing/product/+/osd");
    assert(n == 2 + 2 + 2 + 19 + 1 && b[0] == 0x82 && b[1] == 24 && b[3] == 1);
    assert(b[n - 1] == 0x00);
    assert(encodeSubscribe(b, sizeof(b), 0, "x") == 0);  // id 0 is invalid
    assert(encodePingreq(b, sizeof(b)) == 2 && b[0] == 0xC0 && b[1] == 0);
    assert(encodeDisconnect(b, sizeof(b)) == 2 && b[0] == 0xE0);

    // Remaining length: 1..4 bytes, incomplete, malformed.
    uint32_t v = 0;
    const uint8_t one[] = {0x7F}, two[] = {0x80, 0x01}, four[] = {0xFF, 0xFF, 0xFF, 0x7F};
    assert(decodeRemainingLength(one, 1, v) == 1 && v == 127);
    assert(decodeRemainingLength(two, 2, v) == 2 && v == 128);
    assert(decodeRemainingLength(four, 4, v) == 4 && v == 268435455u);
    assert(decodeRemainingLength(two, 1, v) == 0);
    const uint8_t five[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x01}, padded[] = {0x80, 0x00};
    assert(decodeRemainingLength(five, 5, v) == -1);
    assert(decodeRemainingLength(padded, 2, v) == -1);

    // CONNACK / SUBACK.
    const uint8_t ok[] = {0, 0}, badAuth[] = {0, 5};
    assert(connackCode(ok, 2) == 0 && connackCode(badAuth, 2) == 5 && connackCode(ok, 1) == -1);
    const uint8_t granted[] = {0, 1, 0}, refused[] = {0, 1, 0x80}, otherId[] = {0, 2, 0};
    assert(subackGranted(granted, 3, 1));
    assert(!subackGranted(refused, 3, 1));
    assert(!subackGranted(otherId, 3, 1));

    // PUBLISH body (QoS 0), topic bounds, QoS > 0 rejected.
    const uint8_t pub[] = {0, 5, 'a', '/', 'o', 's', 'd', '{', '}'};
    Message m;
    assert(parsePublish(0, pub, sizeof(pub), m) && m.topicLength == 5 && m.payloadLength == 2);
    assert(std::memcmp(m.topic, "a/osd", 5) == 0 && m.payload[0] == '{');
    assert(!parsePublish(0x02, pub, sizeof(pub), m));
    const uint8_t shortTopic[] = {0, 9, 'a'};
    assert(!parsePublish(0, shortTopic, sizeof(shortTopic), m));

    // Page text.
    using namespace DroneTelemetry;
    char text[320];
    Snapshot s;
    format(s, 0, text, sizeof(text));
    assert(std::strstr(text, "include/DroneLocalConfig.h"));
    s.link = Link::Subscribed;
    format(s, 1000, text, sizeof(text));
    assert(std::strstr(text, "AeroNexus: verbunden") && std::strstr(text, "keine Drohne online"));
    s.aircraft.present = true;
    std::strcpy(s.aircraft.sn, "1581F5BKD225A00A1B2");
    s.aircraft.payloadType = 67;
    s.aircraft.atMs = 10000;
    s.aircraft.modeCode = 3;
    s.aircraft.batteryPct = 87;
    s.aircraft.remainFlightS = 1390;
    s.aircraft.hasElevation = true;
    s.aircraft.elevationM = 35.24f;
    s.aircraft.hasHomeDistance = true;
    s.aircraft.homeDistanceM = 120.4f;
    s.aircraft.hasSpeed = true;
    s.aircraft.horizontalMs = 5.3f;
    s.aircraft.verticalMs = -0.2f;
    s.aircraft.gpsSats = 18;
    s.aircraft.rtkSats = 32;
    s.aircraft.rtkFixed = 2;
    s.remote.present = true;
    std::strcpy(s.remote.sn, "5YSZK1B00C99");
    s.remote.atMs = 10000;
    s.remote.batteryPct = 76;
    s.remote.linkQuality = 5;
    format(s, 11000, text, sizeof(text));
    std::puts(text);
    assert(std::strstr(text, "Mavic 3T ..A1B2 | live"));
    assert(std::strstr(text, "Modus: Manueller Flug"));
    assert(std::strstr(text, "Akku: 87 % | Restflug: 23:10"));
    assert(std::strstr(text, "Hoehe: 35.2 m | Entf.: 120 m"));
    assert(std::strstr(text, "Speed: 5.3 m/s | Vert.: -0.2 m/s"));
    assert(std::strstr(text, "GPS: 18 Sat | RTK: 32 fix"));
    assert(std::strstr(text, "RC ..0C99: 76 % | Funk: 5/5"));
    assert(std::strstr(text, "\nFlug: --"));  // no in-air message yet
    s.aircraft.flight.any = true;
    s.aircraft.flight.inAir = true;
    s.aircraft.flight.airMs = 192500;
    s.aircraft.flight.maxElevationM = 48.4f;
    s.aircraft.flight.maxHomeDistanceM = 310.2f;
    format(s, 11000, text, sizeof(text));
    assert(std::strstr(text, "\nFlug: 3:12 | Max: 48 m / 310 m"));

    // Alerts: none while all is well.
    char line[48];
    assert(alert(s, 11000, line, sizeof(line)) == Alert::None && line[0] == 0);
    // Battery against the aircraft's own thresholds, fallback 25/15 %.
    s.aircraft.batteryPct = 26;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::None);  // above the 25 % fallback
    s.aircraft.batteryPct = 25;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Warning);
    assert(std::strcmp(line, "Akku 25 % - Rueckkehr empfohlen") == 0);
    s.aircraft.returnHomePowerPct = 30;
    s.aircraft.batteryPct = 30;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Warning);
    s.aircraft.landingPowerPct = 18;
    s.aircraft.batteryPct = 18;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Critical);
    assert(std::strcmp(line, "Akku 18 % - Landung noetig") == 0);
    s.aircraft.batteryPct = 87;
    // Flight modes.
    s.aircraft.modeCode = 9;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Warning &&
           std::strcmp(line, "Rueckkehr (RTH) aktiv") == 0);
    s.aircraft.modeCode = 12;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Critical &&
           std::strcmp(line, "ZWANGSLANDUNG") == 0);
    s.aircraft.modeCode = 3;
    // Remote: link quality and battery; critical beats an earlier warning.
    s.remote.linkQuality = 2;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Warning &&
           std::strcmp(line, "Funk schwach (2/5)") == 0);
    s.aircraft.batteryPct = 20;  // warning (RTH 30 %) ...
    s.remote.linkQuality = 1;    // ... but the link is critical
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Critical &&
           std::strcmp(line, "Funk sehr schwach (1/5)") == 0);
    s.aircraft.batteryPct = 87;
    s.remote.linkQuality = 5;
    s.remote.batteryPct = 12;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::Warning &&
           std::strcmp(line, "RC-Akku 12 %") == 0);
    s.remote.batteryPct = 76;
    // Silence while airborne is critical; on the ground it is not.
    assert(alert(s, 10000 + kStaleMs + 4000, line, sizeof(line)) == Alert::Critical);
    assert(std::strcmp(line, "Drohne sendet nicht mehr (10 s)") == 0);
    s.aircraft.flight.inAir = false;
    assert(alert(s, 10000 + kStaleMs + 4000, line, sizeof(line)) == Alert::None);
    s.aircraft.flight.inAir = true;
    // No alerts without a broker session.
    s.link = Link::Error;
    assert(alert(s, 11000, line, sizeof(line)) == Alert::None);
    s.link = Link::Subscribed;
    assert(inAir(3) && inAir(9) && inAir(20) && !inAir(0) && !inAir(2) && !inAir(13) &&
           !inAir(14) && !inAir(-1));
    // Stale: never show old values as live.
    format(s, 10000 + kStaleMs + 2000, text, sizeof(text));
    assert(std::strstr(text, "| offline (8 s)") && std::strstr(text, "Akku: -- | Restflug: --"));
    assert(std::strstr(text, "Modus: keine Daten"));
    assert(std::strstr(text, "RC: -- | Funk: --"));
    s.aircraft.remainFlightS = 0;
    format(s, 11000, text, sizeof(text));
    assert(std::strstr(text, "Restflug: --"));
    s.aircraft.remainFlightS = 1390;
    // Link lost keeps nothing live either.
    s.link = Link::Error;
    format(s, 11000, text, sizeof(text));
    assert(std::strstr(text, "AeroNexus: nicht erreichbar") && std::strstr(text, "Hoehe: -- |"));
    assert(std::strcmp(modeName(9), "Rueckkehr (RTH)") == 0 && std::strcmp(modeName(99), "--") == 0);
    assert(std::strcmp(modelName(89), "Matrice 4T") == 0 && std::strcmp(modelName(-1), "Drohne") == 0);
    // Small buffers truncate safely.
    char tiny[8];
    format(s, 11000, tiny, sizeof(tiny));
    assert(std::strlen(tiny) == 7);
    std::puts("drone codec smoke: OK");
    return 0;
}
