#include "TlsMemory.h"
#include <esp_heap_caps.h>
#include <mbedtls/platform.h>
#include <stdlib.h>

namespace {
// PSRAM first; internal RAM only if PSRAM is exhausted. heap_caps memory is
// released with the regular free(), so blocks from either heap (and any
// allocated before begin()) are freed correctly.
void* tlsCalloc(size_t count, size_t size) {
    void* p = heap_caps_calloc(count, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p != nullptr ? p : heap_caps_calloc(count, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}
}  // namespace

namespace TlsMemory {
bool begin() {
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) == 0) return false;
    return mbedtls_platform_set_calloc_free(tlsCalloc, free) == 0;
}
}  // namespace TlsMemory
