#pragma once
#include <FS.h>
#include <stdint.h>

// Global SD card (SD_MMC, 1-bit: CLK 39, CMD 38, D0 40), mounted once at
// boot and shared by all modules through fs(). FATFS in ESP-IDF is
// reentrant, so tasks may open files concurrently.
namespace SdCard {
struct Info {
    bool mounted = false;
    uint8_t type = 0;          // CARD_NONE/MMC/SD/SDHC (sdcard_type_t)
    uint64_t totalBytes = 0;
    uint64_t usedBytes = 0;
    bool usageKnown = false;   // usedBytes is computed in the background
};

bool begin();          // mount; call once from setup()
bool mounted();
fs::FS& fs();          // only valid while mounted()
Info info();
// Recompute used space in the background (e.g. after FTP uploads). usedBytes()
// scans the FAT and can take seconds on large cards - never on the UI task.
void refreshUsage();
const char* typeName(uint8_t type);
}  // namespace SdCard
