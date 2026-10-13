// Native host regression for the OBD2 allowlist, reply parser and formatter.
#include "Obd2Policy.h"
#include "Obd2Response.h"
#include "Obd2Status.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

using namespace Obd2Response;

int main() {
    // Policy: every command is "AT..." setup or exactly "01PP".
    for (unsigned i = 0; i < static_cast<unsigned>(Obd2Policy::Command::CommandCount); ++i) {
        const auto c = static_cast<Obd2Policy::Command>(i);
        const char* w = Obd2Policy::wireCommand(c);
        assert(w != nullptr);
        if (Obd2Policy::isVehicleRead(c)) assert(std::strlen(w) == 4 && w[0] == '0' && w[1] == '1');
        else assert(w[0] == 'A' && w[1] == 'T');
    }
    static_assert(!Obd2Policy::allowedVehicleWire("04"), "clear DTC rejected");
    static_assert(!Obd2Policy::allowedVehicleWire("0300"), "only service 01");
    static_assert(!Obd2Policy::allowedVehicleWire("010c"), "uppercase only");
    static_assert(!Obd2Policy::allowedVehicleWire("010C0"), "no suffix");
    static_assert(!Obd2Policy::allowedAdapterWire("ATSH7E0"), "no header set");
    static_assert(!Obd2Policy::allowedAdapterWire("ATMA"), "no monitor all");

    // Parser: echo, SEARCHING, spaces, multiple ECUs, prompt.
    uint8_t d[4] = {};
    assert(readPid("SEARCHING...\r41 0C 13 48\r\r", 0x0C, d, 2) && d[0] == 0x13 && d[1] == 0x48);
    assert(readPid("410D2A", 0x0D, d, 1) && d[0] == 0x2A);
    assert(!readPid("010C\r", 0x0C, d, 2));               // echo is not data
    assert(!readPid("41 0C 13", 0x0C, d, 2));             // too short
    assert(!readPid("41 0C 1 348", 0x0C, d, 2));          // split byte
    assert(!readPid("7E8 04 41 0C 13 48", 0x0C, d, 2));   // header not guessed
    assert(!readPid("41 0D 2A", 0x0C, d, 1));             // other PID
    assert(classify("NO DATA\r", 0x0C) == Kind::NoData);
    assert(classify("?\r", 0x0C) == Kind::Error);
    assert(classify("UNABLE TO CONNECT\r", 0x0C) == Kind::Error);
    assert(classify("41 0C 13 48\r", 0x0C) == Kind::Data);

    uint32_t mask = 0;
    assert(supportedBitmap("SEARCHING...\r41 00 18 1A 80 01\r", 0x00, mask));
    assert(mask == 0x181A8001u);
    for (uint8_t pid : {0x04, 0x05, 0x0C, 0x0D, 0x0F, 0x11, 0x20}) assert(isSupported(mask, 0, pid));
    for (uint8_t pid : {0x01, 0x0B, 0x10, 0x21, 0x00}) assert(!isSupported(mask, 0, pid));
    assert(supportedBitmap("41 00 80 00 00 00\r41 00 00 00 00 01\r", 0x00, mask) && mask == 0x80000001u);
    assert(supportedBitmap("41 40 40 00 00 00", 0x40, mask) && isSupported(mask, 0x40, 0x42));
    assert(!supportedBitmap("NO DATA", 0x00, mask) && mask == 0);

    float v = 0;
    const uint8_t rpm[] = {0x13, 0x48};
    assert(decodeValue(0x0C, rpm, v) && v == 1234.0f);
    const uint8_t volt[] = {0x34, 0xBC};
    assert(decodeValue(0x42, volt, v) && std::fabs(v - 13.5f) < 1e-4f);
    const uint8_t cool[] = {0x82};
    assert(decodeValue(0x05, cool, v) && v == 90.0f);
    const uint8_t pct[] = {0xFF};
    assert(decodeValue(0x11, pct, v) && v == 100.0f);
    assert(!decodeValue(0x01, pct, v));
    assert(dataBytes(0x0C) == 2 && dataBytes(0x42) == 2 && dataBytes(0x0D) == 1);

    // Adapter id: vendor suffix dropped, bounded length.
    char id[24];
    assert(adapterId("ATZ\r\rELM327 v1.5 (LAB SIM)\r\r", id, sizeof(id)) &&
           std::strcmp(id, "ELM327 v1.5") == 0);
    assert(adapterId("ELM327 v2.1\r", id, sizeof(id)) && std::strcmp(id, "ELM327 v2.1") == 0);
    assert(adapterId("ELM327 v1.5 extremely long clone name", id, sizeof(id)) &&
           std::strlen(id) <= kAdapterIdMax);
    assert(adapterId("ATE0 echo\rELM327 v1.4b\r", id, sizeof(id)) &&
           std::strcmp(id, "ELM327 v1.4b") == 0);              // earlier 'E' ignored
    assert(!adapterId("OK\r", id, sizeof(id)) && id[0] == '\0');
    assert(!adapterId("ELM32", id, sizeof(id)));
    char small[4];
    assert(adapterId("ELM327 v1.5", small, sizeof(small)) && std::strcmp(small, "ELM") == 0);

    // Formatter.
    char text[256];
    Obd2Status::Snapshot s;
    Obd2Status::format(s, 0, text, sizeof(text));
    assert(std::strcmp(text, "ELM327: nicht konfiguriert\n(include/Obd2LocalConfig.h)") == 0);

    s.link = Obd2Status::Link::Live;
    std::strcpy(s.adapter, "ELM327 v1.5");
    s.supportKnown = true;
    for (auto& f : s.supported) f = true;
    s.supported[Obd2Status::Throttle] = false;
    s.values[Obd2Status::Rpm] = {true, 1234.0f, 1000};
    s.values[Obd2Status::Speed] = {true, 42.0f, 1000};
    s.values[Obd2Status::Coolant] = {true, 90.0f, 1000};
    s.values[Obd2Status::Load] = {true, 29.8f, 1000};
    s.values[Obd2Status::Voltage] = {true, 13.5f, 1000};
    s.tx = 175;
    Obd2Status::format(s, 6000, text, sizeof(text));
    const char* live =
        "ELM327: verbunden | ELM327 v1.5\n"
        "Drehzahl: 1234 rpm | Tempo: 42 km/h\n"
        "Kuehlmittel: 90 C | Ansaug: --\n"
        "Last: 29.8 % | Drossel: n/v\n"
        "Spannung: 13.50 V\n"
        "TX: 175 | Timeouts: 0 | Fehler: 0";
    if (std::strcmp(text, live) != 0) { std::printf("%s\n", text); assert(false); }

    Obd2Status::format(s, 6001, text, sizeof(text));  // stale -> dashes
    assert(std::strstr(text, "Drehzahl: -- | Tempo: --") != nullptr);
    s.link = Obd2Status::Link::Error;                  // never live values on error
    Obd2Status::format(s, 1000, text, sizeof(text));
    assert(std::strstr(text, "ELM327: Fehler, neuer Versuch") == text);
    assert(std::strstr(text, "Drehzahl: --") != nullptr);
    // Adapter id only next to a live link; no-ECU and vehicle Wi-Fi texts.
    s.link = Obd2Status::Link::NoEcu;
    Obd2Status::format(s, 1000, text, sizeof(text));
    assert(std::strncmp(text, "ELM327: kein Steuergeraet (Zuendung?)\n", 38) == 0);
    s.link = Obd2Status::Link::WaitingForWifi;
    s.vehicleProfile = true;
    Obd2Status::format(s, 1000, text, sizeof(text));
    assert(std::strncmp(text, "ELM327: sucht Adapter-WLAN ...\n", 31) == 0);
    s.vehicleProfile = false;
    Obd2Status::format(s, 1000, text, sizeof(text));
    assert(std::strncmp(text, "ELM327: wartet auf WLAN\n", 24) == 0);
    for (const char* line = text; *line;) {  // every line fits the 278 px panel
        const char* end = std::strchr(line, '\n');
        const size_t len = end ? static_cast<size_t>(end - line) : std::strlen(line);
        assert(len <= 40);
        line = end ? end + 1 : line + len;
    }
    char tiny[6];
    Obd2Status::format(s, 0, tiny, sizeof(tiny));
    assert(std::strlen(tiny) == 5);
    return 0;
}
