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
    assert(dataBytes(0x10) == 2 && dataBytes(0x06) == 1);
    const uint8_t trimZero[] = {0x80}, trimLean[] = {0x8A}, trimMin[] = {0x00};
    assert(decodeValue(0x06, trimZero, v) && v == 0.0f);
    assert(decodeValue(0x07, trimLean, v) && std::fabs(v - 7.8125f) < 1e-4f);
    assert(decodeValue(0x09, trimMin, v) && v == -100.0f);
    const uint8_t maf[] = {0x01, 0xF4};
    assert(decodeValue(0x10, maf, v) && v == 5.0f);

    // ATRV / ATDPN (adapter queries, clone spellings).
    float volts = 0;
    assert(adapterVoltage("12.6V", volts) && std::fabs(volts - 12.6f) < 1e-4f);
    assert(adapterVoltage("\r14.12 V\r", volts) && std::fabs(volts - 14.12f) < 1e-4f);
    assert(adapterVoltage("9v", volts) && volts == 9.0f);
    assert(!adapterVoltage("?", volts) && !adapterVoltage("12.V", volts));
    assert(!adapterVoltage("V", volts) && !adapterVoltage("99.9V", volts));
    assert(!adapterVoltage("12.6", volts));  // unit required
    assert(std::strcmp(protocolName("A6"), "CAN 11/500") == 0);
    assert(std::strcmp(protocolName("\r7\r"), "CAN 29/500") == 0);
    assert(std::strcmp(protocolName("A"), "J1939") == 0);
    assert(protocolName("A0") == nullptr && protocolName("?") == nullptr);
    assert(protocolName("A66") == nullptr);

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
    // Clones without "ELM327": first content line, never echo/OK/?.
    assert(adapterId("ELM32", id, sizeof(id)) && std::strcmp(id, "ELM32") == 0);
    assert(adapterId("ATZ\r\rOBDII v1.5\r\r", id, sizeof(id)) && std::strcmp(id, "OBDII v1.5") == 0);
    assert(adapterId("\r\rV-LINK\r", id, sizeof(id)) && std::strcmp(id, "V-LINK") == 0);
    assert(!adapterId("ATZ\r\r", id, sizeof(id)) && id[0] == '\0');
    assert(!adapterId("?\r", id, sizeof(id)) && !adapterId("", id, sizeof(id)));
    assert(adapterId("OBD\x01X", id, sizeof(id)) && std::strcmp(id, "OBD?X") == 0);
    char small[4];
    assert(adapterId("ELM327 v1.5", small, sizeof(small)) && std::strcmp(small, "ELM") == 0);

    // Formatter.
    char text[320];
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
    s.values[Obd2Status::Maf] = {true, 5.0f, 1000};
    s.values[Obd2Status::ShortTrimB1] = {true, 0.78125f, 1000};
    s.values[Obd2Status::LongTrimB1] = {true, 7.8125f, 1000};
    s.values[Obd2Status::ShortTrimB2] = {true, -2.3f, 1000};
    s.supported[Obd2Status::LongTrimB2] = false;
    s.tx = 175;
    Obd2Status::format(s, 3000, text, sizeof(text));
    const char* live =
        "ELM327: verbunden | ELM327 v1.5\n"
        "Drehzahl: 1234 rpm | Tempo: 42 km/h\n"
        "Kuehlmittel: 90 C | Ansaug: --\n"
        "Last: 29.8 % | Drossel: n/v\n"
        "Spannung: 13.50 V | MAF: 5.0 g/s\n"
        "Trim B1: +1/+8 B2: -2/n/v %\n"
        "TX: 175 | Timeouts: 0 | Fehler: 0";
    if (std::strcmp(text, live) != 0) { std::printf("%s\n", text); assert(false); }

    // Known protocol replaces "verbunden"; a fresh ECU voltage beats ATRV.
    std::strcpy(s.protocol, "CAN 11/500");
    s.adapterVoltage = {true, 13.9f, 1000};
    Obd2Status::format(s, 6000, text, sizeof(text));
    assert(std::strstr(text, "ELM327 v1.5 | CAN 11/500\n") == text);
    assert(std::strstr(text, "Spannung: 13.50 V |") != nullptr);
    s.protocol[0] = '\0';
    // Ignition off: no ECU, but the adapter still measures pin 16.
    s.link = Obd2Status::Link::NoEcu;
    Obd2Status::format(s, 2000, text, sizeof(text));
    assert(std::strstr(text, "Spannung: 13.9 V |") != nullptr);
    Obd2Status::format(s, 1000 + Obd2Status::kAdapterVoltageStaleMs + 1, text, sizeof(text));
    assert(std::strstr(text, "Spannung: -- |") != nullptr);
    s.adapterVoltage = {};
    s.link = Obd2Status::Link::Live;

    Obd2Status::format(s, 4001, text, sizeof(text));  // RPM/speed stale after 3 s
    assert(std::strstr(text, "Drehzahl: -- | Tempo: --") != nullptr);
    assert(std::strstr(text, "Kuehlmittel: 90 C") != nullptr);  // slow PID: 15 s
    Obd2Status::format(s, 16001, text, sizeof(text));
    assert(std::strstr(text, "Kuehlmittel: -- |") != nullptr);
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
    // Worst case: every value at its widest encodable magnitude.
    s.link = Obd2Status::Link::Live;
    for (auto& f : s.supported) f = true;
    const float widest[Obd2Status::kPidCount] = {
        16383.75f, 255, 215, -40, 100, 100, 65.535f, 655.35f, -100, -100, -100, -100};
    for (uint8_t i = 0; i < Obd2Status::kPidCount; ++i) s.values[i] = {true, widest[i], 1000};
    s.tx = s.timeouts = s.errors = 4294967295u;
    Obd2Status::format(s, 1000, text, sizeof(text));
    assert(std::strlen(text) + 1 < sizeof(text));
    assert(std::strstr(text, "TX: 99999+ | Timeouts: 99+ | Fehler: 99+") != nullptr);
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
