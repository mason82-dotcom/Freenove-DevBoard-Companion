#pragma once
#include <stdint.h>

// Closed, compile-time audited allowlist for the read-only OBD2 module.
// There is deliberately no API that sends a caller-supplied string: the
// worker can only transmit wireCommand(Command). Writing services
// (04 clear DTC, 08 actuator tests, UDS 1x/2x/3x), ATSH/ATMA and DTC
// reads are not part of this module. A write mode would be separate firmware.
namespace Obd2Policy {
enum class Command : uint8_t {
    // Adapter setup: configures only the ELM327, never the vehicle ECU.
    AdapterReset, AdapterEchoOff, AdapterLinesOff, AdapterSpacesOff,
    AdapterHeadersOff, AdapterAutoProtocol,
    // Adapter queries: answered by the ELM327 itself, nothing reaches the
    // vehicle bus. ATRV measures OBD pin 16 (works with the ignition off),
    // ATDPN names the protocol the automatic search selected.
    AdapterVoltage, AdapterProtocol,
    // Vehicle reads: SAE J1979 service 01 (current data) only.
    SupportedPids00, SupportedPids20, SupportedPids40,
    Load, Coolant, Rpm, Speed, Intake, Throttle, ModuleVoltage,
    ShortTrimB1, LongTrimB1, ShortTrimB2, LongTrimB2, Maf,
    CommandCount  // sentinel, not a command
};

constexpr bool isVehicleRead(Command c) {
    return c >= Command::SupportedPids00 && c < Command::CommandCount;
}

constexpr const char* wireCommand(Command c) {
    switch (c) {
        case Command::AdapterReset: return "ATZ";
        case Command::AdapterEchoOff: return "ATE0";
        case Command::AdapterLinesOff: return "ATL0";
        case Command::AdapterSpacesOff: return "ATS0";
        case Command::AdapterHeadersOff: return "ATH0";
        case Command::AdapterAutoProtocol: return "ATSP0";
        case Command::AdapterVoltage: return "ATRV";
        case Command::AdapterProtocol: return "ATDPN";
        case Command::SupportedPids00: return "0100";
        case Command::SupportedPids20: return "0120";
        case Command::SupportedPids40: return "0140";
        case Command::Load: return "0104";
        case Command::Coolant: return "0105";
        case Command::Rpm: return "010C";
        case Command::Speed: return "010D";
        case Command::Intake: return "010F";
        case Command::Throttle: return "0111";
        case Command::ModuleVoltage: return "0142";
        case Command::ShortTrimB1: return "0106";
        case Command::LongTrimB1: return "0107";
        case Command::ShortTrimB2: return "0108";
        case Command::LongTrimB2: return "0109";
        case Command::Maf: return "0110";
        default: return nullptr;
    }
}

// Mode 01 PID byte of a vehicle read (0 for adapter commands).
constexpr uint8_t pidOf(Command c) {
    return !isVehicleRead(c) ? 0 : static_cast<uint8_t>(
        ((wireCommand(c)[2] >= 'A' ? wireCommand(c)[2] - 'A' + 10 : wireCommand(c)[2] - '0') << 4) |
        (wireCommand(c)[3] >= 'A' ? wireCommand(c)[3] - 'A' + 10 : wireCommand(c)[3] - '0'));
}

constexpr bool sameText(const char* a, const char* b) {
    return *a == *b && (*a == '\0' || sameText(a + 1, b + 1));
}
constexpr bool upperHex4(const char* w) {
    for (int i = 0; i < 4; ++i)
        if (!((w[i] >= '0' && w[i] <= '9') || (w[i] >= 'A' && w[i] <= 'F'))) return false;
    return w[4] == '\0';
}
constexpr bool allowedAdapterWire(const char* w) {
    return sameText(w, "ATZ") || sameText(w, "ATE0") || sameText(w, "ATL0") ||
           sameText(w, "ATS0") || sameText(w, "ATH0") || sameText(w, "ATSP0") ||
           sameText(w, "ATRV") || sameText(w, "ATDPN");
}
// Exactly "01PP": service 01, one PID, uppercase hex, nothing appended.
constexpr bool allowedVehicleWire(const char* w) {
    return w[0] == '0' && w[1] == '1' && upperHex4(w);
}
constexpr bool allowed(Command c) {
    return wireCommand(c) != nullptr &&
           (isVehicleRead(c) ? allowedVehicleWire(wireCommand(c))
                             : allowedAdapterWire(wireCommand(c)));
}
constexpr bool everyCommandAllowed(unsigned i = 0) {
    return i >= static_cast<unsigned>(Command::CommandCount) ||
           (allowed(static_cast<Command>(i)) && everyCommandAllowed(i + 1));
}
static_assert(everyCommandAllowed(), "Obd2Policy: command outside read-only shapes");
static_assert(wireCommand(Command::CommandCount) == nullptr, "sentinel has no wire form");
static_assert(pidOf(Command::Rpm) == 0x0C && pidOf(Command::ModuleVoltage) == 0x42,
              "PID derivation");
}  // namespace Obd2Policy
