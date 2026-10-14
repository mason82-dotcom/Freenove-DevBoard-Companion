// Native host regression for the OBD2 CSV log format.
#include "Obd2Log.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace Obd2Log;

static int count(const char* text, char c) {
    int n = 0;
    for (; *text; ++text) n += *text == c;
    return n;
}

int main() {
    Obd2Status::Snapshot s;
    s.link = Obd2Status::Link::Live;
    std::strcpy(s.adapter, "ELM327 v1.5");
    std::strcpy(s.protocol, "CAN 11/500");

    // Header: metadata line, then 15 columns; adapter text cannot break the CSV.
    char header[400];
    size_t n = formatHeader(s, "C350 W204 M272", "2026-10-14 20:43:05", header, sizeof(header));
    assert(n == std::strlen(header));
    std::puts(header);
    assert(std::strstr(header, "# Freenove companion OBD2 log") == header);
    assert(std::strstr(header, "vehicle=C350 W204 M272; adapter=ELM327 v1.5; protocol=CAN 11/500; "
                               "started=2026-10-14 20:43:05;"));
    const char* columns = std::strchr(header, '\n') + 1;
    assert(std::strcmp(columns,
        "t_s,time,adapter_v,rpm,speed_kmh,coolant_c,intake_c,load_pct,throttle_pct,ecu_v,maf_gs,"
        "stft_b1_pct,ltft_b1_pct,stft_b2_pct,ltft_b2_pct\n") == 0);
    std::strcpy(s.adapter, "ELM,327\r\nX");
    formatHeader(s, nullptr, "", header, sizeof(header));
    assert(std::strstr(header, "vehicle=generic; adapter=ELM 327  X;"));
    assert(std::strstr(header, "started=unknown;"));
    assert(count(header, '\n') == 2);
    assert(formatHeader(s, nullptr, "", header, 40) == 0 && header[0] == 0);

    // Row: fresh values only; empty cells for stale/missing; fixed column count.
    s.values[Obd2Status::Rpm] = {true, 812.25f, 9000};
    s.values[Obd2Status::Speed] = {true, 0.0f, 9000};
    s.values[Obd2Status::Coolant] = {true, 88.0f, 3000};       // slow PID: 7 s is still fresh
    s.values[Obd2Status::Voltage] = {true, 14.12f, 9500};
    s.values[Obd2Status::ShortTrimB1] = {true, -2.34f, 9500};
    s.adapterVoltage = {true, 14.0f, 9000};
    char row[200];
    n = formatRow(s, 10000, 2500, "20:43:12", row, sizeof(row));
    std::puts(row);
    assert(n == std::strlen(row));
    assert(std::strcmp(row, "7.5,20:43:12,14.0,812,0,88,,,,14.12,,-2.3,,,\n") == 0);
    assert(count(row, ',') == 14);
    // RPM is stale after 3 s, slow PIDs after 15 s.
    formatRow(s, 12001, 2500, "", row, sizeof(row));
    assert(std::strcmp(row, "9.5,,14.0,,,88,,,,14.12,,-2.3,,,\n") == 0);
    formatRow(s, 18001, 2500, "", row, sizeof(row));
    assert(std::strcmp(row, "15.5,,14.0,,,,,,,14.12,,-2.3,,,\n") == 0);  // coolant 15.001 s old
    // Not live: ECU columns empty, adapter voltage (no ECU needed) stays.
    s.link = Obd2Status::Link::NoEcu;
    formatRow(s, 10000, 2500, "", row, sizeof(row));  // clock unknown: empty cell
    assert(std::strcmp(row, "7.5,,14.0,,,,,,,,,,,,\n") == 0);
    s.adapterVoltage.atMs = 0;  // stale adapter voltage
    formatRow(s, 20000, 2500, nullptr, row, sizeof(row));
    assert(std::strcmp(row, "17.5,,,,,,,,,,,,,,\n") == 0);
    assert(formatRow(s, 10000, 2500, "", row, 8) == 0);

    // File names.
    assert(numberFromName("0012.csv") == 12);
    assert(numberFromName("/obd2/9999.csv") == 9999);
    assert(numberFromName("12.csv") == 0 && numberFromName("00a1.csv") == 0);
    assert(numberFromName("0012.txt") == 0 && numberFromName(nullptr) == 0);

    // Footer.
    char footerText[64];
    Status st;
    footer(st, true, footerText, sizeof(footerText));
    assert(std::strcmp(footerText, "Nur Lesen | nur im Stillstand bedienen") == 0);
    st.state = State::Writing;
    st.number = 3;
    st.bytes = 12 * 1024 + 1;
    footer(st, true, footerText, sizeof(footerText));
    assert(std::strcmp(footerText, "Nur Lesen | Stillstand | Log 0003 13 KB") == 0);
    st.state = State::CardFull;
    footer(st, false, footerText, sizeof(footerText));
    assert(std::strcmp(footerText, "Nur Lesen | Labor | Log: SD voll") == 0);
    std::puts("obd2 log smoke: OK");
    return 0;
}
