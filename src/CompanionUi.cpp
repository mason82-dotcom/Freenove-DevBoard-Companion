#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "BoardTouch.h"
#include "CompanionModules.h"
#include "CompanionUi.h"
#include "TouchTransform.h"

static_assert(LV_VERSION_MAJOR == 8 && LV_VERSION_MINOR == 3,
              "This shell requires LVGL 8.3.x");

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kLinesPerFlush = 20;

TFT_eSPI screen;
lv_disp_draw_buf_t drawBuffer;
lv_color_t drawPixels[kWidth * kLinesPerFlush];
lv_obj_t* statusLabel = nullptr;
uint32_t lastTick = 0;
uint32_t lastSystemRefresh = 0;
bool touchEnabled = false;
bool ready = false;

// 0 means Home. 1..6 correspond to CompanionModules::kModules.
uint8_t shownPage = 0;
uint8_t requestedPage = 0;
uint8_t moduleNumbers[CompanionModules::kModuleCount] = {1, 2, 3, 4, 5, 6};

lv_color_t color(uint32_t rgb) {
    return lv_color_hex(rgb);
}

void flushDisplay(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* colors) {
    if (area->x2 < 0 || area->y2 < 0 ||
        area->x1 >= kWidth || area->y1 >= kHeight) {
        lv_disp_flush_ready(drv);
        return;
    }
    // LVGL 8.3 supplies RGB565 because include/lv_conf.h sets LV_COLOR_DEPTH=16.
    const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    screen.startWrite();
    screen.setAddrWindow(area->x1, area->y1, w, h);
    screen.pushColors(reinterpret_cast<uint16_t*>(colors), w * h, true);
    screen.endWrite();
    lv_disp_flush_ready(drv);
}

void readPointer(lv_indev_drv_t*, lv_indev_data_t* data) {
    uint16_t x = 0, y = 0;
    if (!touchEnabled || !BoardTouch::readRaw(x, y)) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }
    const auto mapped = TouchTransform::rawToLandscape(x, y);
    data->point.x = mapped.x;
    data->point.y = mapped.y;
    data->state = LV_INDEV_STATE_PR;
}

lv_obj_t* newLabel(lv_obj_t* parent, const char* text,
                   int x, int y, uint32_t rgb) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, color(rgb), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

void makeScreenBlank() {
    lv_obj_t* root = lv_scr_act();
    // Only invoked outside LVGL event callbacks, after lv_timer_handler returns.
    lv_obj_clean(root);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_bg_color(root, color(0x0B1220), 0);
    statusLabel = nullptr;
}

void onTileClicked(lv_event_t* event) {
    auto* number = static_cast<uint8_t*>(lv_event_get_user_data(event));
    if (number != nullptr && *number >= 1 &&
        *number <= CompanionModules::kModuleCount) {
        requestedPage = *number;
    }
}

void onBackClicked(lv_event_t*) {
    requestedPage = 0;
}

lv_obj_t* makeButton(lv_obj_t* parent, int x, int y, int w, int h,
                     const char* title, uint32_t background) {
    lv_obj_t* button = lv_btn_create(parent);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, w, h);
    lv_obj_set_style_bg_color(button, color(background), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, color(0x36516B), 0);
    lv_obj_set_style_radius(button, 8, 0);
    lv_obj_set_style_shadow_width(button, 0, 0);

    lv_obj_t* label = lv_label_create(button);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_color(label, color(0xF2F6FF), 0);
    lv_obj_center(label);
    return button;
}

void showHome() {
    makeScreenBlank();
    lv_obj_t* root = lv_scr_act();

    newLabel(root, "FREENOVE COMPANION", 10, 10, 0xE9F3FF);
    newLabel(root, "CFG1", 274, 10, 0x80D4FF);

    for (size_t i = 0; i < CompanionModules::kModuleCount; ++i) {
        const int col = static_cast<int>(i % 3);
        const int row = static_cast<int>(i / 3);
        lv_obj_t* tile = makeButton(root,
            8 + col * 104, 42 + row * 79, 96, 70,
            CompanionModules::kModules[i].title, 0x1A304B);
        lv_obj_add_event_cb(tile, onTileClicked, LV_EVENT_CLICKED, &moduleNumbers[i]);
    }

    newLabel(root, touchEnabled ? "Touch OK" : "Touch nicht erkannt", 9, 216,
             touchEnabled ? 0x80D8B0 : 0xFF9889);
    newLabel(root, "Nur Anzeige", 226, 216, 0x92A8C0);
}

void updateSystemDetails() {
    if (statusLabel == nullptr || shownPage != 5) return;
    lv_label_set_text_fmt(statusLabel,
        "Uptime: %lu s\nHeap frei: %u B\nPSRAM frei: %u B",
        static_cast<unsigned long>(millis() / 1000),
        static_cast<unsigned>(ESP.getFreeHeap()),
        static_cast<unsigned>(ESP.getFreePsram()));
}

void showModule(uint8_t page) {
    makeScreenBlank();
    lv_obj_t* root = lv_scr_act();
    const auto& module = CompanionModules::kModules[page - 1];
    lv_obj_t* back = makeButton(root, 8, 7, 74, 28, "< Zurueck", 0x25415D);
    lv_obj_add_event_cb(back, onBackClicked, LV_EVENT_CLICKED, nullptr);
    newLabel(root, module.title, 95, 12, 0xF1F6FF);

    lv_obj_t* panel = lv_obj_create(root);
    lv_obj_set_pos(panel, 10, 48);
    lv_obj_set_size(panel, 300, 144);
    lv_obj_set_style_bg_color(panel, color(0x14283D), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, color(0x36516B), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    newLabel(panel, module.detail, 9, 10, 0xCFDFED);
    if (module.id == CompanionModules::Id::System) {
        statusLabel = newLabel(panel, "", 9, 42, 0x81DDBC);
        updateSystemDetails();
    } else {
        newLabel(panel, "Noch keine Verbindung / keine Befehle", 9, 51, 0x91A8C1);
    }
    newLabel(root, "Nur lokale Anzeige | CFG1 320x240", 10, 212, 0x8EA8C1);
}
} // namespace

namespace CompanionUi {

bool begin(bool touchPresent) {
    touchEnabled = touchPresent;
    screen.init();
    screen.setRotation(1);  // fixed Freenove CFG1 only
    if (screen.width() != kWidth || screen.height() != kHeight) {
        Serial.printf("ERROR: TFT dimensions %dx%d (expected 320x240)\n",
                      screen.width(), screen.height());
        screen.fillScreen(TFT_RED);
        return false;
    }

    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, drawPixels, nullptr, kWidth * kLinesPerFlush);
    lv_disp_drv_t displayDriver;
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = kWidth;
    displayDriver.ver_res = kHeight;
    displayDriver.flush_cb = flushDisplay;
    displayDriver.draw_buf = &drawBuffer;
    if (lv_disp_drv_register(&displayDriver) == nullptr) {
        Serial.println("ERROR: LVGL display registration failed");
        return false;
    }

    lv_indev_drv_t inputDriver;
    lv_indev_drv_init(&inputDriver);
    inputDriver.type = LV_INDEV_TYPE_POINTER;
    inputDriver.read_cb = readPointer;
    if (lv_indev_drv_register(&inputDriver) == nullptr) {
        Serial.println("ERROR: LVGL input registration failed");
        return false;
    }

    lastTick = millis();
    lastSystemRefresh = lastTick;
    shownPage = 0;
    requestedPage = 0;
    showHome();
    ready = true;
    Serial.printf("LVGL shell ready: %dx%d, touch=%s, modules=%u\n",
                  kWidth, kHeight, touchEnabled ? "OK" : "MISSING",
                  static_cast<unsigned>(CompanionModules::kModuleCount));
    return true;
}

void poll() {
    if (!ready) return;
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastTick;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        lastTick = now;
    }
    lv_timer_handler();

    if (requestedPage != shownPage) {
        shownPage = requestedPage;
        if (shownPage == 0) {
            Serial.println("UI NAV: Home");
            showHome();
        } else {
            Serial.printf("UI NAV: %s\n",
                          CompanionModules::kModules[shownPage - 1].title);
            showModule(shownPage);
        }
    }

    if (shownPage == 5 && now - lastSystemRefresh >= 1000) {
        lastSystemRefresh = now;
        updateSystemDetails();
    }
}
} // namespace CompanionUi
