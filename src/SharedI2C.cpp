#include "SharedI2C.h"
#include "BoardPins.h"
#include <Wire.h>

namespace {
SemaphoreHandle_t busMutex = nullptr;
bool busStarted = false;
}
namespace SharedI2C {
bool begin() {
    if (busStarted) return true;
    if (!busMutex) busMutex = xSemaphoreCreateMutex();
    if (!busMutex) return false;
    if (!Wire.begin(BoardPins::kTouchSda, BoardPins::kTouchScl, 400000)) return false;
    Wire.setTimeOut(20);
    busStarted = true;
    return true;
}
bool ready() { return busStarted && busMutex != nullptr; }
Guard::Guard(uint32_t timeoutMs) {
    if (ready() && xSemaphoreTake(busMutex, pdMS_TO_TICKS(timeoutMs)) == pdTRUE)
        acquired_ = true;
}
Guard::~Guard() {
    if (acquired_) xSemaphoreGive(busMutex);
}
}
