#include <Arduino.h>
#include <SPI.h> // Explicit framework dependency required by GUIslice TFT backend.
#include <LovyanGFX.hpp> // Direct PlatformIO LDF dependency for the compatibility backend.
#include <GUIslice.h>
#include <GUIslice_drv.h>
#include <GUIslice_th.h>

#include "BoardTouch.h"
#include "CompanionModules.h"
#include "TouchTransform.h"

// EXPERIMENT: GUIslice GUI + LovyanGFX TFT_eSPI compatibility backend.
// The external GUIslice driver exclusively owns LovyanGFX's LGFX ST7789.
// Never instantiate another TFT_eSPI/LovyanGFX display object here.
// Read-only: no WiFi, OBD command, drone command, audio or camera operation.

namespace {
constexpr uint8_t kPages = 7;    // 0=Home; 1..6 = the existing Companion modules.
constexpr uint8_t kItemsPerPage = 12;
constexpr int16_t kFontBody = 0;
constexpr int16_t kHomeButtonBase = 10;
constexpr int16_t kBackButtonBase = 100;

gslc_tsGui gui;
gslc_tsDriver driver;
gslc_tsFont fonts[1];
gslc_tsPage pages[kPages];
gslc_tsElem elements[kPages][kItemsPerPage];
gslc_tsElemRef elementRefs[kPages][kItemsPerPage];
gslc_tsElemRef* statusUptime = nullptr;
gslc_tsElemRef* statusHeap = nullptr;
gslc_tsElemRef* statusPsram = nullptr;
char uptimeText[44] = "Uptime: -- s";
char heapText[44] = "Heap frei: -- B";
char psramText[44] = "PSRAM frei: -- B";

uint8_t currentPage = 0;
uint8_t requestedPage = 0;
uint32_t lastStatusMs = 0;
uint32_t lastHeartbeatMs = 0;
bool initialized = false;

static int16_t printDebug(char ch) {
    Serial.write(ch);
    return 0;
}

// GUIslice provides the driver integration point for external touch readers.
// Coordinates are ALREADY landscape-mapped; disable GUIslice remapping once
// it has initialized to avoid applying a second rotation.
class FnkTouchHandler : public TouchHandler {
public:
    void begin() override {
        const bool ok = BoardTouch::begin();
        Serial.printf("FT6336U probe: %s\n", ok ? "FOUND" : "MISSING");
    }

    THPoint getPoint() override {
        uint16_t rawX = 0;
        uint16_t rawY = 0;
        if (!BoardTouch::readRaw(rawX, rawY)) return THPoint(0, 0, 0);
        const auto p = TouchTransform::rawToLandscape(rawX, rawY);
        return THPoint(p.x, p.y, 1);
    }
};

FnkTouchHandler fnkTouch;

gslc_tsElemRef* addText(int16_t page, gslc_tsRect rect,
                        char* text, uint8_t maxLen = 0) {
    auto* ref = gslc_ElemCreateTxt(&gui, GSLC_ID_AUTO, page, rect,
                                   text, maxLen, kFontBody);
    if (ref) {
        gslc_ElemSetFillEn(&gui, ref, false);
        gslc_ElemSetFrameEn(&gui, ref, false);
        gslc_ElemSetTxtCol(&gui, ref, GSLC_COL_WHITE);
    }
    return ref;
}

bool onButton(void*, void* ref, gslc_teTouch touch, int16_t, int16_t) {
    if (touch != GSLC_TOUCH_UP_IN) return true;
    const int16_t id = gslc_ElemGetId(&gui,
                        static_cast<gslc_tsElemRef*>(ref));
    if (id >= kHomeButtonBase &&
        id < kHomeButtonBase + static_cast<int16_t>(CompanionModules::kModuleCount)) {
        requestedPage = static_cast<uint8_t>(id - kHomeButtonBase + 1);
    } else if (id >= kBackButtonBase &&
               id < kBackButtonBase + static_cast<int16_t>(CompanionModules::kModuleCount)) {
        requestedPage = 0;
    }
    return true;
}

bool addButton(int16_t page, int16_t id, gslc_tsRect rect, const char* label) {
    // A zero-length string buffer means read-only, constant storage.
    auto* ref = gslc_ElemCreateBtnTxt(
        &gui, id, page, rect, const_cast<char*>(label), 0, kFontBody, &onButton);
    if (!ref) return false;
    gslc_ElemSetCol(&gui, ref, GSLC_COL_BLUE_DK2,
                    GSLC_COL_BLUE_DK4, GSLC_COL_BLUE_DK1);
    gslc_ElemSetTxtCol(&gui, ref, GSLC_COL_WHITE);
    return true;
}

void updateSystem() {
    snprintf(uptimeText, sizeof(uptimeText), "Uptime: %lu s",
             static_cast<unsigned long>(millis() / 1000));
    snprintf(heapText, sizeof(heapText), "Heap frei: %u B",
             static_cast<unsigned>(ESP.getFreeHeap()));
    snprintf(psramText, sizeof(psramText), "PSRAM frei: %u B",
             static_cast<unsigned>(ESP.getFreePsram()));
    if (statusUptime) gslc_ElemSetTxtStr(&gui, statusUptime, uptimeText);
    if (statusHeap) gslc_ElemSetTxtStr(&gui, statusHeap, heapText);
    if (statusPsram) gslc_ElemSetTxtStr(&gui, statusPsram, psramText);
}

bool buildGui() {
    if (!gslc_Init(&gui, &driver, pages, kPages, fonts, 1)) return false;
    gslc_SetTouchRemapEn(&gui, false);
    if (!gslc_FontSet(&gui, kFontBody, GSLC_FONTREF_PTR, nullptr, 1)) return false;
    gslc_SetBkgndColor(&gui, GSLC_COL_BLACK);

    if (gui.nDispW != 320 || gui.nDispH != 240) {
        Serial.printf("ERROR: GUI display is %dx%d, required 320x240\n",
                      gui.nDispW, gui.nDispH);
        return false;
    }

    for (uint8_t i = 0; i < kPages; ++i) {
        gslc_PageAdd(&gui, i, elements[i], kItemsPerPage,
                     elementRefs[i], kItemsPerPage);
    }

    if (!addText(0, {8, 8, 250, 25}, (char*)"FREENOVE COMPANION")) return false;
    if (!addText(0, {270, 8, 45, 25}, (char*)"CFG1")) return false;
    for (size_t i = 0; i < CompanionModules::kModuleCount; ++i) {
        const int16_t x = static_cast<int16_t>(8 + (i % 3) * 104);
        const int16_t y = static_cast<int16_t>(42 + (i / 3) * 79);
        if (!addButton(0, kHomeButtonBase + i, {x, y, 96, 70},
                       CompanionModules::kModules[i].title)) return false;
    }
    if (!addText(0, {8, 218, 160, 18}, (char*)"GUIslice + LovyanGFX")) return false;

    for (size_t i = 0; i < CompanionModules::kModuleCount; ++i) {
        const int16_t page = static_cast<int16_t>(i + 1);
        if (!addButton(page, kBackButtonBase + i, {8, 5, 78, 30},
                       "< Zurueck")) return false;
        if (!addText(page, {94, 6, 212, 25},
                     const_cast<char*>(CompanionModules::kModules[i].title))) return false;
        if (!addText(page, {12, 50, 294, 27},
                     const_cast<char*>(CompanionModules::kModules[i].detail))) return false;
        if (CompanionModules::kModules[i].id == CompanionModules::Id::System) {
            statusUptime = addText(page, {12, 90, 294, 20},
                                   uptimeText, sizeof(uptimeText));
            statusHeap = addText(page, {12, 116, 294, 20},
                                 heapText, sizeof(heapText));
            statusPsram = addText(page, {12, 142, 294, 20},
                                  psramText, sizeof(psramText));
            if (!statusUptime || !statusHeap || !statusPsram) return false;
        } else {
            if (!addText(page, {12, 112, 294, 32},
                         (char*)"Nur Anzeige / keine Befehle")) return false;
        }
        if (!addText(page, {10, 211, 300, 20},
                     (char*)"Nur lokale Anzeige | CFG1")) return false;
    }

    gslc_SetPageCur(&gui, 0);
    updateSystem();
    return true;
}
}  // namespace

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println("FNK0086 GUIslice 0.17.2 + LovyanGFX 1.2.32 EXPERIMENT");
    Serial.printf("PSRAM at boot: %u bytes free\n",
                  static_cast<unsigned>(ESP.getFreePsram()));
    gslc_InitDebug(printDebug);
    gslc_InitTouchHandler(&fnkTouch);
    initialized = buildGui();
    Serial.printf("GUI BOOT: %s, display=320x240, modules=%u\n",
                  initialized ? "OK" : "FAIL",
                  static_cast<unsigned>(CompanionModules::kModuleCount));
}

void loop() {
    if (initialized) {
        gslc_Update(&gui);

        // Defer page changes until after GUIslice has finished event processing.
        if (requestedPage != currentPage && requestedPage < kPages) {
            currentPage = requestedPage;
            gslc_SetPageCur(&gui, currentPage);
            Serial.printf("UI NAV: %s\n", currentPage == 0 ? "Home" :
                 CompanionModules::kModules[currentPage - 1].title);
        }
        const uint32_t now = millis();
        if (currentPage == 5 && now - lastStatusMs >= 1000) {
            lastStatusMs = now;
            updateSystem();
        }
    }
    const uint32_t now = millis();
    if (now - lastHeartbeatMs >= 2000) {
        lastHeartbeatMs = now;
        Serial.printf("GUI heartbeat: uptime=%lu heap=%u psram=%u\n",
                      static_cast<unsigned long>(now),
                      static_cast<unsigned>(ESP.getFreeHeap()),
                      static_cast<unsigned>(ESP.getFreePsram()));
    }
    delay(5);
}
