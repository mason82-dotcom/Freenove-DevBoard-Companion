#include "Clock.h"
#include <stdio.h>

namespace Clock {
void formatShort(const struct tm* t, char* out, size_t capacity) {
    if (!out || capacity == 0) return;
    if (!t) snprintf(out, capacity, "--:--");
    else snprintf(out, capacity, "%02d:%02d", t->tm_hour, t->tm_min);
}

void formatLong(const struct tm* t, char* out, size_t capacity) {
    if (!out || capacity == 0) return;
    if (!t) {
        snprintf(out, capacity, "nicht synchronisiert");
        return;
    }
    snprintf(out, capacity, "%02d.%02d.%04d %02d:%02d:%02d", t->tm_mday, t->tm_mon + 1,
             t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec);
}

void formatIso(const struct tm* t, char* out, size_t capacity) {
    if (!out || capacity == 0) return;
    if (!t) {
        out[0] = 0;
        return;
    }
    snprintf(out, capacity, "%04d-%02d-%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1,
             t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

void formatTime(const struct tm* t, char* out, size_t capacity) {
    if (!out || capacity == 0) return;
    if (!t) {
        out[0] = 0;
        return;
    }
    snprintf(out, capacity, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}
}  // namespace Clock
