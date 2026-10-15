// Native host regression for the clock text formats.
#include "Clock.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
    struct tm t = {};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 9;
    t.tm_mday = 4;
    t.tm_hour = 7;
    t.tm_min = 3;
    t.tm_sec = 5;
    char out[32];
    Clock::formatShort(&t, out, sizeof(out));
    assert(std::strcmp(out, "07:03") == 0);
    Clock::formatShort(nullptr, out, sizeof(out));
    assert(std::strcmp(out, "--:--") == 0);
    Clock::formatLong(&t, out, sizeof(out));
    assert(std::strcmp(out, "04.10.2026 07:03:05") == 0);
    Clock::formatLong(nullptr, out, sizeof(out));
    assert(std::strcmp(out, "nicht synchronisiert") == 0);
    Clock::formatIso(&t, out, sizeof(out));
    assert(std::strcmp(out, "2026-10-04 07:03:05") == 0);
    Clock::formatIso(nullptr, out, sizeof(out));
    assert(out[0] == 0);
    Clock::formatTime(&t, out, sizeof(out));
    assert(std::strcmp(out, "07:03:05") == 0);
    Clock::formatTime(nullptr, out, sizeof(out));
    assert(out[0] == 0);
    assert(!Clock::plausible(0) && !Clock::plausible(1735689599) && Clock::plausible(1735689600));
    std::puts("clock smoke: OK");
    return 0;
}
