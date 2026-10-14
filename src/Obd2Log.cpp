#include "Obd2Log.h"
#include "SdCard.h"
#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>

// SD side of the OBD2 log. Only the OBD2 worker calls write()/close(); the
// FATFS of ESP-IDF is reentrant, so the FTP server may run at the same time.
// Rows are collected in PSRAM and written as one block every
// kFlushEveryRows rows: fewer FAT updates, less card wear, shorter stalls of
// the worker, and at most kFlushEveryRows seconds lost if the power vanishes.
namespace {
portMUX_TYPE statusLock = portMUX_INITIALIZER_UNLOCKED;
Obd2Log::Status current;

constexpr size_t kTextCapacity = 400;  // header line, or one row
constexpr size_t kPendingCapacity = 4096;  // > kFlushEveryRows rows of <= 200 bytes

// Worker-only state. One PSRAM block per log file: text scratch + pending rows.
File file;
char* text = nullptr;
char* pending = nullptr;
size_t pendingBytes = 0;
uint32_t pendingRows = 0;
uint32_t startMs = 0;
bool givenUp = false;  // a failure ends logging until the page is left

void setStatus(const Obd2Log::Status& st) {
    portENTER_CRITICAL(&statusLock);
    current = st;
    portEXIT_CRITICAL(&statusLock);
}

void release() {
    if (file) file.close();
    heap_caps_free(text);  // pending lives in the same allocation
    text = pending = nullptr;
    pendingBytes = 0;
    pendingRows = 0;
}

void fail(Obd2Log::Status& st, Obd2Log::State state) {
    release();
    st.state = state;
    setStatus(st);
    givenUp = true;
    Serial.printf("OBD2 log: stopped (%s)\n", state == Obd2Log::State::NoCard ? "no SD card"
                                              : state == Obd2Log::State::CardFull ? "SD card full"
                                                                                  : "write error");
}

// Writes the collected rows as one block and syncs the FAT entry.
bool commit() {
    if (pendingBytes == 0) return true;
    const bool ok = file.write(reinterpret_cast<const uint8_t*>(pending), pendingBytes) == pendingBytes;
    file.flush();
    pendingBytes = 0;
    pendingRows = 0;
    return ok;
}

uint16_t nextNumber(fs::FS& fs) {
    uint16_t highest = 0;
    File dir = fs.open(Obd2Log::kDirectory);
    if (!dir || !dir.isDirectory()) return 1;
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
        const uint16_t n = entry.isDirectory() ? 0 : Obd2Log::numberFromName(entry.name());
        if (n > highest) highest = n;
    }
    return highest >= 9999 ? 0 : static_cast<uint16_t>(highest + 1);
}

bool open(Obd2Log::Status& st, const Obd2Status::Snapshot& s, const char* vehicle) {
    if (!SdCard::mounted()) {
        fail(st, Obd2Log::State::NoCard);
        return false;
    }
    const SdCard::Info info = SdCard::info();
    if (info.usageKnown && info.totalBytes - info.usedBytes < Obd2Log::kMinFreeBytes) {
        fail(st, Obd2Log::State::CardFull);
        return false;
    }
    text = static_cast<char*>(heap_caps_malloc(kTextCapacity + kPendingCapacity,
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (text == nullptr) {
        fail(st, Obd2Log::State::WriteError);
        return false;
    }
    pending = text + kTextCapacity;
    fs::FS& fs = SdCard::fs();
    if (!fs.exists(Obd2Log::kDirectory) && !fs.mkdir(Obd2Log::kDirectory)) {
        fail(st, Obd2Log::State::WriteError);
        return false;
    }
    const uint16_t number = nextNumber(fs);
    if (number == 0) {  // 9999 files: let the user clean up via FTP
        fail(st, Obd2Log::State::CardFull);
        return false;
    }
    char path[24];
    snprintf(path, sizeof(path), "%s/%04u.csv", Obd2Log::kDirectory, static_cast<unsigned>(number));
    file = fs.open(path, FILE_WRITE);
    const size_t n = Obd2Log::formatHeader(s, vehicle, text, kTextCapacity);
    if (!file || n == 0 || file.write(reinterpret_cast<const uint8_t*>(text), n) != n) {
        fail(st, Obd2Log::State::WriteError);
        return false;
    }
    file.flush();  // the file exists on the card from the first second
    st = Obd2Log::Status{};
    st.state = Obd2Log::State::Writing;
    st.number = number;
    st.bytes = n;
    startMs = millis();
    setStatus(st);
    Serial.printf("OBD2 log: %s\n", path);
    return true;
}
}  // namespace

namespace Obd2Log {
void write(const Obd2Status::Snapshot& s, const char* vehicleTitle) {
    if (givenUp) return;
    Status st = status();
    if (!file && !open(st, s, vehicleTitle)) return;
    const size_t n = formatRow(s, millis(), startMs, text, kTextCapacity);
    if (n == 0 || (pendingBytes + n > kPendingCapacity && !commit())) {
        fail(st, State::WriteError);
        return;
    }
    memcpy(pending + pendingBytes, text, n);
    pendingBytes += n;
    if (++pendingRows >= kFlushEveryRows && !commit()) {
        fail(st, State::WriteError);
        return;
    }
    st.bytes += n;
    ++st.rows;
    setStatus(st);
}

void close() {
    Status st = status();
    if (file) {
        const bool ok = commit();
        release();
        Serial.printf("OBD2 log: %04u.csv closed, %lu rows, %lu bytes%s\n",
                      static_cast<unsigned>(st.number), static_cast<unsigned long>(st.rows),
                      static_cast<unsigned long>(st.bytes), ok ? "" : " (last block failed)");
        SdCard::refreshUsage();
    } else {
        release();
    }
    givenUp = false;
    st.state = State::Off;
    setStatus(st);
}

Status status() {
    portENTER_CRITICAL(&statusLock);
    const Status copy = current;
    portEXIT_CRITICAL(&statusLock);
    return copy;
}
}  // namespace Obd2Log
