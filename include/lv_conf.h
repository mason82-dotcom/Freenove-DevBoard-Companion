/**
 * LVGL 9.6.0 project-wide Extended profile for FNK0086.
 *
 * This header is included from ALL LVGL library translation units and the
 * application through -DLV_CONF_INCLUDE_SIMPLE=1 -Iinclude in platformio.ini.
 * Single active profile: Extended (no per-module fallback or runtime switch).
 *
 * Hardware is the verified Freenove ST7789 CFG1 ONLY; this configuration does
 * not change TFT_eSPI display wiring, rotation or FT6336U touch calibration.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define COMPANION_LVGL_PROFILE_EXTENDED 1

/* Memory, platform and display core: keep the hardware-validated settings. */
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

/* Extended: current shell + future Hermes and telemetry/status UIs. */
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_CHART 1
#define LV_USE_BAR 1
#define LV_USE_SLIDER 1
#define LV_USE_IMAGE 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* Retain the theme used by the existing button/label appearance. */
#define LV_USE_THEME_DEFAULT 1
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

/*
 * Unused widgets intentionally disabled for a predictable project-wide
 * footprint. The Extended list above is the only enabled widget set.
 * Re-enable a widget deliberately when a future module genuinely needs it.
 */
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_ARCLABEL 0
#define LV_USE_BARCODE 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* Not used by the current shell; can be enabled later without touching CFG1. */
#define LV_USE_OBSERVER 0

#endif /* LV_CONF_H */
