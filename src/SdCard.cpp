#include "SdCard.h"
#include "BoardPins.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
portMUX_TYPE infoLock = portMUX_INITIALIZER_UNLOCKED;
SdCard::Info current;
TaskHandle_t usageWorker = nullptr;

void usageTask(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t start = millis();
        const uint64_t used = SD_MMC.usedBytes();
        portENTER_CRITICAL(&infoLock);
        current.usedBytes = used;
        current.usageKnown = true;
        portEXIT_CRITICAL(&infoLock);
        Serial.printf("SD: used=%llu MB (scan %lu ms)\n",
                      static_cast<unsigned long long>(used >> 20),
                      static_cast<unsigned long>(millis() - start));
    }
}
}  // namespace

namespace SdCard {
bool begin() {
    if (current.mounted) return true;
    if (!SD_MMC.setPins(BoardPins::kSdClk, BoardPins::kSdCmd, BoardPins::kSdD0)) {
        Serial.println("SD: pin setup failed");
        return false;
    }
    // 1-bit mode, no auto-format: a foreign card is never erased.
    if (!SD_MMC.begin("/sdcard", true, false)) {
        Serial.println("SD: no card / mount failed");
        return false;
    }
    Info info;
    info.mounted = true;
    info.type = static_cast<uint8_t>(SD_MMC.cardType());
    info.totalBytes = SD_MMC.totalBytes();
    portENTER_CRITICAL(&infoLock);
    current = info;
    portEXIT_CRITICAL(&infoLock);
    Serial.printf("SD: %s, %llu MB\n", typeName(info.type),
                  static_cast<unsigned long long>(info.totalBytes >> 20));
    if (xTaskCreatePinnedToCore(usageTask, "sd-usage", 3072, nullptr, 0, &usageWorker, 0) == pdPASS)
        refreshUsage();
    return true;
}

bool mounted() { return info().mounted; }
fs::FS& fs() { return SD_MMC; }

Info info() {
    portENTER_CRITICAL(&infoLock);
    const Info copy = current;
    portEXIT_CRITICAL(&infoLock);
    return copy;
}

void refreshUsage() {
    if (usageWorker != nullptr) xTaskNotifyGive(usageWorker);
}

const char* typeName(uint8_t type) {
    switch (type) {
        case CARD_MMC: return "MMC";
        case CARD_SD: return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default: return "unbekannt";
    }
}
}  // namespace SdCard
