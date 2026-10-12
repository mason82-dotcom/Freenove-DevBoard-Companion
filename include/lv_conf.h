/**
 * LVGL 9.6.0 configuration for FNK0086, fixed Freenove ST7789 CFG1.
 * Included from every LVGL translation unit via LV_CONF_INCLUDE_SIMPLE / -Iinclude.
 *
 * Only RGB565 SW rendering is required for our TFT_eSPI partial-flush driver.
 * Do not change the verified Freenove display pin setup or touch orientation.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (64U * 1024U)

#define LV_USE_OS LV_OS_NONE

#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_RGB565
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4
#define LV_USE_DRAW_SW 1
#define LV_USE_LOG 0

/* Keep necessary default widgets explicit across configuration migrations. */
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1

#endif /* LV_CONF_H */
