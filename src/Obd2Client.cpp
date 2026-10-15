#include "Obd2Status.h"
#include "Obd2Policy.h"
#include "Obd2Log.h"
#include "Obd2Response.h"
#include "VehicleProfiles.h"
#include "WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <initializer_list>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#if __has_include("Obd2LocalConfig.h")
#include "Obd2LocalConfig.h"
#define OBD2_LOCAL_CONFIG_PRESENT 1
#else
#define OBD2_LOCAL_CONFIG_PRESENT 0
#endif

// Read-only ELM327 client over Wi-Fi/TCP. Lab profile: ELM327 simulator in the
// home WLAN. Vehicle profile (OBD2_PROFILE_VEHICLE 1): the dongle's own access
// point, selected via WifiLink only while the OBD2 page is open.
// Only Obd2Policy commands can be transmitted; see Obd2Policy.h.
namespace {
using Obd2Policy::Command;
using Obd2Status::Link;
using Obd2Status::Snapshot;

portMUX_TYPE snapshotLock = portMUX_INITIALIZER_UNLOCKED;
Snapshot current;
TaskHandle_t worker = nullptr;
std::atomic<bool> desiredActive{false};

void publish(const Snapshot& s) {
    portENTER_CRITICAL(&snapshotLock);
    current = s;
    portEXIT_CRITICAL(&snapshotLock);
}

#if OBD2_LOCAL_CONFIG_PRESENT
#if defined(OBD2_PROFILE_VEHICLE) && OBD2_PROFILE_VEHICLE
constexpr bool kVehicle = true;
#else
constexpr bool kVehicle = false;
#endif
constexpr WifiLink::Profile kWifiProfile =
    kVehicle ? WifiLink::Profile::Vehicle : WifiLink::Profile::Home;
constexpr uint32_t kNoEcuRetryMs = 5000;
constexpr uint32_t kConnectTimeoutMs = 3000;
constexpr uint32_t kResetTimeoutMs = 3000;
constexpr uint32_t kSetupTimeoutMs = 1500;
constexpr uint32_t kSearchTimeoutMs = 12000;  // first 0100 runs ATSP0 search
constexpr uint32_t kReadTimeoutMs = 2000;
constexpr uint32_t kRetryDelayMs = 3000;
constexpr uint8_t kMaxConsecutiveFailures = 5;
constexpr size_t kReplyCapacity = 512;

// Only the worker task touches these.
NetworkClient client;
char reply[kReplyCapacity];

constexpr Command kPidCommand[Obd2Status::kPidCount] = {
    Command::Rpm, Command::Speed, Command::Coolant, Command::Intake,
    Command::Load, Command::Throttle, Command::ModuleVoltage,
    Command::Maf, Command::ShortTrimB1, Command::LongTrimB1,
    Command::ShortTrimB2, Command::LongTrimB2};
static_assert(sizeof(kPidCommand) / sizeof(kPidCommand[0]) == Obd2Status::kPidCount,
              "every displayed value needs exactly one allowlisted command");

enum class Result : uint8_t { Ok, Timeout, Overflow, Closed, Aborted };

// Sends one allowlisted command and collects the reply up to the '>' prompt.
Result exchange(Command command, uint32_t timeoutMs, Snapshot& s) {
    const char* wire = Obd2Policy::wireCommand(command);
    if (wire == nullptr || !client.connected()) return Result::Closed;
    while (client.available() > 0) client.read();  // drop stale bytes
    client.print(wire);
    client.print('\r');
    ++s.tx;
    size_t length = 0;
    bool overflow = false;
    const uint32_t start = millis();
    while (static_cast<uint32_t>(millis() - start) < timeoutMs) {
        if (!desiredActive.load()) return Result::Aborted;
        int available = client.available();
        if (available <= 0) {
            if (!client.connected()) return Result::Closed;
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        while (available-- > 0) {
            const int c = client.read();
            if (c < 0) break;
            if (c == '>') {
                reply[length] = '\0';
                return overflow ? Result::Overflow : Result::Ok;
            }
            if (c == 0) continue;
            if (length + 1 < kReplyCapacity) reply[length++] = static_cast<char>(c);
            else overflow = true;
        }
    }
    reply[length] = '\0';
    return Result::Timeout;
}

bool countFailure(Result r, Snapshot& s) {
    if (r == Result::Timeout) ++s.timeouts;
    else if (r != Result::Ok && r != Result::Aborted) ++s.errors;
    return r == Result::Ok;
}

void setLink(Snapshot& s, Link link) {
    if (s.link == link) return;
    static const char* const names[] = {"not-configured", "idle", "wait-wifi", "connecting",
                                        "init", "searching", "live", "error", "no-ecu"};
    s.link = link;
    Serial.printf("OBD2: %s (tx=%lu timeouts=%lu errors=%lu)\n",
                  names[static_cast<uint8_t>(link)], static_cast<unsigned long>(s.tx),
                  static_cast<unsigned long>(s.timeouts), static_cast<unsigned long>(s.errors));
    publish(s);
}

// ATRV: answered by the adapter itself, so it also works while no ECU is
// awake (ignition off). Only a timeout is fatal (late reply, see pollOnce).
Result readAdapterVoltage(Snapshot& s) {
    const Result r = exchange(Command::AdapterVoltage, kSetupTimeoutMs, s);
    float volts = 0;
    if (r == Result::Ok && Obd2Response::adapterVoltage(reply, volts)) {
        s.adapterVoltage = {true, volts, millis()};
        publish(s);
    } else {
        countFailure(r == Result::Ok ? Result::Overflow : r, s);
    }
    return r;
}

bool initializeAdapter(Snapshot& s) {
    setLink(s, Link::Initializing);
    if (!countFailure(exchange(Command::AdapterReset, kResetTimeoutMs, s), s)) return false;
    Obd2Response::adapterId(reply, s.adapter, sizeof(s.adapter));
    // Raw ATZ reply (CR/LF as '|', non-printables as '.') for clone diagnosis:
    // serial log and CSV header (nobody reads the serial port in the car).
    size_t n = 0;
    for (const char* c = reply; *c && n + 1 < sizeof(s.atzRaw); ++c)
        s.atzRaw[n++] = (*c == '\r' || *c == '\n') ? '|' : (*c >= ' ' && *c < 127 && *c != ';') ? *c : '.';
    s.atzRaw[n] = '\0';
    Serial.printf("OBD2: ATZ reply \"%s\" (%u bytes) -> id \"%s\"\n", s.atzRaw,
                  static_cast<unsigned>(strlen(reply)), s.adapter);
    for (Command c : {Command::AdapterEchoOff, Command::AdapterLinesOff,
                      Command::AdapterSpacesOff, Command::AdapterHeadersOff,
                      Command::AdapterAutoProtocol}) {
        if (!countFailure(exchange(c, kSetupTimeoutMs, s), s)) return false;
        if (!Obd2Response::contains(reply, "OK")) {
            ++s.errors;
            return false;
        }
    }
    return readAdapterVoltage(s) != Result::Timeout;
}

bool discoverSupport(Snapshot& s) {
    setLink(s, Link::Searching);
    uint32_t mask00 = 0, mask20 = 0, mask40 = 0;
    if (!countFailure(exchange(Command::SupportedPids00, kSearchTimeoutMs, s), s)) return false;
    if (!Obd2Response::supportedBitmap(reply, 0x00, mask00)) {
        // Adapter answered, but no ECU did: ignition off, wrong protocol.
        if (Obd2Response::contains(reply, "UNABLE TO CONNECT") ||
            Obd2Response::contains(reply, "BUS INIT") ||
            Obd2Response::contains(reply, "NO DATA") ||
            Obd2Response::contains(reply, "CAN ERROR")) {
            setLink(s, Link::NoEcu);
        } else {
            ++s.errors;
        }
        return false;
    }
    if (mask00 & 1u) {
        if (countFailure(exchange(Command::SupportedPids20, kReadTimeoutMs, s), s))
            Obd2Response::supportedBitmap(reply, 0x20, mask20);
    }
    if (mask20 & 1u) {
        if (countFailure(exchange(Command::SupportedPids40, kReadTimeoutMs, s), s))
            Obd2Response::supportedBitmap(reply, 0x40, mask40);
    }
    for (uint8_t i = 0; i < Obd2Status::kPidCount; ++i) {
        const uint8_t pid = Obd2Policy::pidOf(kPidCommand[i]);
        const uint8_t base = pid > 0x40 ? 0x40 : pid > 0x20 ? 0x20 : 0x00;
        const uint32_t mask = base == 0x40 ? mask40 : base == 0x20 ? mask20 : mask00;
        s.supported[i] = Obd2Response::isSupported(mask, base, pid);
    }
    s.supportKnown = true;
    // Which protocol the automatic search found (diagnosis of the first test).
    if (countFailure(exchange(Command::AdapterProtocol, kSetupTimeoutMs, s), s)) {
        const char* name = Obd2Response::protocolName(reply);
        strncpy(s.protocol, name ? name : "?", sizeof(s.protocol) - 1);
    }
    Serial.printf("OBD2: %s, protocol %s, %.1f V\n", s.adapter[0] ? s.adapter : "adapter ?",
                  s.protocol[0] ? s.protocol : "?",
                  s.adapterVoltage.valid ? static_cast<double>(s.adapterVoltage.value) : 0.0);
    return true;
}

// Returns false when the session should be re-established.
bool pollOnce(Snapshot& s, uint8_t& slowIndex, uint8_t& failures) {
    if (slowIndex % 10 == 0) {
        const Result r = readAdapterVoltage(s);
        if (r == Result::Timeout || r == Result::Closed || r == Result::Aborted) return false;
    }
    // RPM and speed every cycle; the remaining PIDs round-robin.
    const uint8_t slow = static_cast<uint8_t>(Obd2Status::Coolant +
        slowIndex++ % (Obd2Status::kPidCount - Obd2Status::Coolant));
    for (uint8_t i : {static_cast<uint8_t>(Obd2Status::Rpm),
                      static_cast<uint8_t>(Obd2Status::Speed), slow}) {
        if (!desiredActive.load()) return false;
        if (!s.supported[i]) continue;
        const Command command = kPidCommand[i];
        const uint8_t pid = Obd2Policy::pidOf(command);
        const Result r = exchange(command, kReadTimeoutMs, s);
        if (r == Result::Closed || r == Result::Aborted) return false;
        if (r == Result::Timeout) {
            // No '>' prompt: a late reply could be paired with the next
            // command. Drop the TCP session instead of guessing.
            countFailure(r, s);
            return false;
        }
        uint8_t data[2] = {};
        float value = 0;
        if (r == Result::Ok &&
            Obd2Response::readPid(reply, pid, data, Obd2Response::dataBytes(pid)) &&
            Obd2Response::decodeValue(pid, data, value)) {
            s.values[i] = {true, value, millis()};
            failures = 0;
        } else {
            // NO DATA / malformed reply counts as an error, a timeout as timeout.
            countFailure(r == Result::Ok ? Result::Overflow : r, s);
            if (++failures >= kMaxConsecutiveFailures) return false;
        }
        publish(s);
    }
    return true;
}

void endSession(Snapshot& s, Link link) {
    client.stop();
    for (auto& v : s.values) v = Obd2Status::Value{};
    s.supportKnown = false;
    s.protocol[0] = '\0';  // the adapter voltage stays: it does not need an ECU
    // One log file per page visit: reconnects continue it, leaving ends it.
    if (link == Link::Idle) Obd2Log::close();
    setLink(s, link);
    publish(s);
    // Leaving the page hands the station back to the home WLAN (Hermes).
    if (link == Link::Idle && kVehicle) WifiLink::request(WifiLink::Profile::Home);
}

void task(void*) {
    Snapshot s;
    s.link = Link::Idle;
    publish(s);
    for (;;) {
        if (!desiredActive.load()) {
            if (s.link != Link::Idle) {
                endSession(s, Link::Idle);
                Serial.println("OBD2: session closed");
            }
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (s.link == Link::Idle) {
            s = Snapshot{};  // fresh counters and values for every visit
            s.link = Link::Idle;
            s.vehicleProfile = kVehicle;
        }
        WifiLink::request(kWifiProfile);
        if (!WifiLink::connectedTo(kWifiProfile)) {
            setLink(s, Link::WaitingForWifi);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
            continue;
        }
        setLink(s, Link::Connecting);
        if (!client.connect(OBD2_ELM_HOST, OBD2_ELM_PORT, kConnectTimeoutMs)) {
            ++s.errors;
            endSession(s, Link::Error);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kRetryDelayMs));
            continue;
        }
        client.setNoDelay(true);
        if (!initializeAdapter(s) || !discoverSupport(s)) {
            const bool noEcu = s.link == Link::NoEcu;
            endSession(s, !desiredActive.load() ? Link::Idle : noEcu ? Link::NoEcu : Link::Error);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(noEcu ? kNoEcuRetryMs : kRetryDelayMs));
            continue;
        }
        setLink(s, Link::Live);
        uint8_t slowIndex = 0, failures = 0;
        uint32_t lastLog = millis();
        uint32_t lastRow = millis() - Obd2Log::kRowIntervalMs;
        while (pollOnce(s, slowIndex, failures)) {
            if (static_cast<uint32_t>(millis() - lastRow) >= Obd2Log::kRowIntervalMs) {
                // Fixed 1 s grid; after a long stall resync instead of bursting.
                lastRow += Obd2Log::kRowIntervalMs;
                if (static_cast<uint32_t>(millis() - lastRow) >= Obd2Log::kRowIntervalMs)
                    lastRow = millis();
                Obd2Log::write(s, Obd2Status::vehicleTitle());
            }
            if (static_cast<uint32_t>(millis() - lastLog) >= 10000) {
                lastLog = millis();
                Serial.printf("OBD2: live tx=%lu timeouts=%lu errors=%lu stack_free=%u\n",
                              static_cast<unsigned long>(s.tx),
                              static_cast<unsigned long>(s.timeouts),
                              static_cast<unsigned long>(s.errors),
                              static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        endSession(s, desiredActive.load() ? Link::Error : Link::Idle);
        if (desiredActive.load()) ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(kRetryDelayMs));
    }
}
#endif
}  // namespace

namespace Obd2Status {
void begin() {
#if OBD2_LOCAL_CONFIG_PRESENT
    if (worker != nullptr) return;
    Snapshot s;
    s.link = Link::Idle;
    s.vehicleProfile = kVehicle;
    publish(s);
    // Created once and parked: no adapter traffic until the page is opened.
    // 7 KB: the SD log adds FATFS calls (measured 2.7 KB free with 6 KB).
    if (xTaskCreatePinnedToCore(task, "obd2", 7168, nullptr, 1, &worker, 0) != pdPASS)
        worker = nullptr;
#endif
}

bool vehicleProfile() {
#if OBD2_LOCAL_CONFIG_PRESENT
    return kVehicle;
#else
    return false;
#endif
}

const char* vehicleTitle() {
#if OBD2_LOCAL_CONFIG_PRESENT && defined(OBD2_VEHICLE)
    return VehicleProfiles::title(OBD2_VEHICLE);
#else
    return nullptr;
#endif
}

bool requestActive(bool active) {
    desiredActive.store(active);
    if (worker == nullptr) return false;
    xTaskNotifyGive(worker);
    return true;
}

Snapshot read() {
    Snapshot s;
    portENTER_CRITICAL(&snapshotLock);
    s = current;
    portEXIT_CRITICAL(&snapshotLock);
    return s;
}
}  // namespace Obd2Status
