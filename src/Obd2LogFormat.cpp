#include "Obd2Log.h"
#include <stdio.h>
#include <string.h>

namespace Obd2Log {
namespace {
using Obd2Status::Pid;

// Column order of the CSV (after t_s).
struct Column {
    Pid pid;
    const char* name;
    const char* format;
};
constexpr Column kColumns[] = {
    {Obd2Status::Rpm, "rpm", "%.0f"},
    {Obd2Status::Speed, "speed_kmh", "%.0f"},
    {Obd2Status::Coolant, "coolant_c", "%.0f"},
    {Obd2Status::Intake, "intake_c", "%.0f"},
    {Obd2Status::Load, "load_pct", "%.1f"},
    {Obd2Status::Throttle, "throttle_pct", "%.1f"},
    {Obd2Status::Voltage, "ecu_v", "%.2f"},
    {Obd2Status::Maf, "maf_gs", "%.2f"},
    {Obd2Status::ShortTrimB1, "stft_b1_pct", "%.1f"},
    {Obd2Status::LongTrimB1, "ltft_b1_pct", "%.1f"},
    {Obd2Status::ShortTrimB2, "stft_b2_pct", "%.1f"},
    {Obd2Status::LongTrimB2, "ltft_b2_pct", "%.1f"},
};
static_assert(sizeof(kColumns) / sizeof(kColumns[0]) == Obd2Status::kPidCount,
              "every polled value has exactly one CSV column");

// Appends with bounds; returns false once the buffer is full.
bool append(char* out, size_t capacity, size_t& n, const char* text) {
    const size_t len = strlen(text);
    if (n + len >= capacity) return false;
    memcpy(out + n, text, len + 1);
    n += len;
    return true;
}

// The adapter id and protocol come from the dongle; keep the CSV well formed.
void clean(char* dst, size_t cap, const char* src) {
    size_t n = 0;
    for (; src && *src && n + 1 < cap; ++src)
        dst[n++] = (*src == ',' || *src == '\r' || *src == '\n' || *src < ' ') ? ' ' : *src;
    dst[n] = 0;
}
}  // namespace

size_t formatHeader(const Obd2Status::Snapshot& s, const char* vehicleTitle,
                    const char* started, char* out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    char adapter[24], protocol[16], vehicle[32];
    clean(adapter, sizeof(adapter), s.adapter[0] ? s.adapter : "?");
    clean(protocol, sizeof(protocol), s.protocol[0] ? s.protocol : "?");
    clean(vehicle, sizeof(vehicle), vehicleTitle ? vehicleTitle : "generic");
    int n = snprintf(out, capacity,
                     "# Freenove companion OBD2 log (read-only Mode 01); vehicle=%s; adapter=%s; "
                     "protocol=%s; started=%s; t_s=seconds since log start, "
                     "empty=stale or unsupported\n"
                     "t_s,time,adapter_v",
                     vehicle, adapter, protocol, started && *started ? started : "unknown");
    if (n < 0 || static_cast<size_t>(n) >= capacity) {
        out[0] = 0;
        return 0;
    }
    size_t len = static_cast<size_t>(n);
    for (const Column& c : kColumns) {
        if (!append(out, capacity, len, ",") || !append(out, capacity, len, c.name)) {
            out[0] = 0;
            return 0;
        }
    }
    if (!append(out, capacity, len, "\n")) {
        out[0] = 0;
        return 0;
    }
    return len;
}

size_t formatRow(const Obd2Status::Snapshot& s, uint32_t nowMs, uint32_t startMs,
                 const char* clockText, char* out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    char cell[16];
    size_t len = 0;
    out[0] = 0;
    snprintf(cell, sizeof(cell), "%.1f", static_cast<double>(nowMs - startMs) / 1000.0);
    if (!append(out, capacity, len, cell) || !append(out, capacity, len, ",")) return 0;
    if (clockText && !append(out, capacity, len, clockText)) return 0;
    if (!append(out, capacity, len, ",")) return 0;
    const auto& av = s.adapterVoltage;
    if (av.valid && static_cast<uint32_t>(nowMs - av.atMs) <= Obd2Status::kAdapterVoltageStaleMs) {
        snprintf(cell, sizeof(cell), "%.1f", static_cast<double>(av.value));
        if (!append(out, capacity, len, cell)) return 0;
    }
    const bool live = s.link == Obd2Status::Link::Live;
    for (const Column& c : kColumns) {
        if (!append(out, capacity, len, ",")) return 0;
        const auto& v = s.values[c.pid];
        if (live && v.valid && static_cast<uint32_t>(nowMs - v.atMs) <= Obd2Status::kValueStaleMs) {
            snprintf(cell, sizeof(cell), c.format, static_cast<double>(v.value));
            if (!append(out, capacity, len, cell)) return 0;
        }
    }
    if (!append(out, capacity, len, "\n")) return 0;
    return len;
}

uint16_t numberFromName(const char* name) {
    if (!name) return 0;
    const char* base = strrchr(name, '/');
    base = base ? base + 1 : name;
    if (strlen(base) != 8 || strcmp(base + 4, ".csv") != 0) return 0;
    unsigned value = 0;
    for (int i = 0; i < 4; ++i) {
        if (base[i] < '0' || base[i] > '9') return 0;
        value = value * 10 + static_cast<unsigned>(base[i] - '0');
    }
    return static_cast<uint16_t>(value);
}

void footer(const Status& st, bool vehicle, char* out, size_t capacity) {
    if (!out || capacity == 0) return;
    const char* mode = vehicle ? "Nur Lesen | Stillstand" : "Nur Lesen | Labor";
    switch (st.state) {
        case State::Writing:
            snprintf(out, capacity, "%s | Log %04u %lu KB", mode, static_cast<unsigned>(st.number),
                     static_cast<unsigned long>((st.bytes + 1023) / 1024));
            break;
        case State::NoCard: snprintf(out, capacity, "%s | Log: keine SD", mode); break;
        case State::CardFull: snprintf(out, capacity, "%s | Log: SD voll", mode); break;
        case State::WriteError: snprintf(out, capacity, "%s | Log: Schreibfehler", mode); break;
        case State::Off:
        default:
            snprintf(out, capacity, "%s", vehicle ? "Nur Lesen | nur im Stillstand bedienen"
                                                  : "Nur Lesen (Mode 01) | Laborprofil");
            break;
    }
}
}  // namespace Obd2Log
