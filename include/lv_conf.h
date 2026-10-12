/**
 * LVGL 8.3.11 project-local configuration — Freenove FNK0086.
 * Only the fixed CFG1 ST7789 hardware profile is supported.
 *
 * Include this file from all LVGL translation units via
 * -DLV_CONF_INCLUDE_SIMPLE=1 -Iinclude (see platformio.ini).
 *
 * Keep LVGL out of the touch smoke-test logic until hardware acceptance.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Keep memory use bounded on ESP32-S3 internal RAM. PSRAM is separate. */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)

/* A future GUI loop must explicitly call lv_tick_inc() and lv_timer_handler(). */
#define LV_TICK_CUSTOM 0
#define LV_USE_LOG 0

#endif /* LV_CONF_H */
