#include <Arduino.h>
#include <SPI.h> // Explicit framework dependency required by GUIslice TFT backend.
#include <LovyanGFX.hpp> // Direct PlatformIO LDF dependency for the compatibility backend.
#include <GUIslice.h>
#include <GUIslice_drv.h>
#include <GUIslice_th.h>

#include "BoardTouch.h"
#include "CompanionGuiModules.h"
#include "ModuleDiagnostics.h"
#include "ModuleGraphics.h"
#include "PulsOxSensor.h"
#include "TouchTransform.h"

// EXPERIMENT: GUIslice GUI + LovyanGFX TFT_eSPI compatibility backend.
// The external GUIslice driver exclusively owns LovyanGFX's LGFX ST7789.
// Never instantiate another TFT_eSPI/LovyanGFX display object here.
// Read-only: no WiFi, OBD command, drone command, audio or camera operation.

namespace {
constexpr uint8_t kPages = 7;    // 0=Home; 1..6 = the existing Companion modules.
constexpr uint8_t kRadioPage = 2;
constexpr uint8_t kPulsOxPage = 6;  // Only the last module uses MAX30102.
constexpr uint8_t kItemsPerPage = 12;
constexpr uint8_t kStatusRows = ModuleDiagnostics::kRows;
constexpr int16_t kFontBody = 0;
constexpr int16_t kHomeButtonBase = 10;
constexpr int16_t kBackButtonBase = 100;
constexpr int16_t kRadioButtonBase = 200;
char radioMessage[64] = "Audio aus | Steuerung noch nicht verbunden";
gslc_tsElemRef* radioMessageRef = nullptr;

gslc_tsGui gui;
gslc_tsDriver driver;
gslc_tsFont fonts[1];
gslc_tsPage pages[kPages];
gslc_tsElem elements[kPages][kItemsPerPage];
gslc_tsElemRef elementRefs[kPages][kItemsPerPage];
// GUIslice elements keep pointers to these persistent writable buffers.
// Always use gslc_ElemSetTxtStr() with a *separate* snapshot for redraw.
gslc_tsElemRef* statusRefs[CompanionModules::kModuleCount][kStatusRows] = {};
gslc_tsElemRef* canvasRefs[CompanionModules::kModuleCount] = {};
char statusText[CompanionModules::kModuleCount][kStatusRows]
               [ModuleDiagnostics::kTextCapacity] = {};

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
        // Fill the text element background on redraw so shorter uptime/
        // memory strings do not leave glyph fragments from older values.
        gslc_ElemSetFillEn(&gui, ref, true);
        gslc_ElemSetFrameEn(&gui, ref, false);
        gslc_ElemSetCol(&gui, ref, GSLC_COL_BLACK,
                        GSLC_COL_BLACK, GSLC_COL_BLACK);
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
    } else if (id >= kRadioButtonBase && id < kRadioButtonBase + 4) {
        // Controls are real touch targets; audio is deliberately not simulated.
        static const char* actions[] = {"Sender zurueck", "Play/Pause",
                                         "Sender weiter", "Lautstaerke"};
        const uint8_t action = static_cast<uint8_t>(id - kRadioButtonBase);
        char nextMessage[sizeof(radioMessage)];
        snprintf(nextMessage, sizeof(nextMessage),
                 "%s: Audio noch nicht verbunden", actions[action]);
        if (radioMessageRef) gslc_ElemSetTxtStr(&gui, radioMessageRef, nextMessage);
        Serial.printf("WEBRADIO TOUCH: %s (no audio action)\\n", actions[action]);
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

void updateModule(uint8_t moduleIndex, uint32_t nowMs) {
    if (moduleIndex >= CompanionModules::kModuleCount) return;
    ModuleDiagnostics::Snapshot next{};
    if (!ModuleDiagnostics::capture(CompanionModules::kModules[moduleIndex].id,
                                    next, nowMs)) return;
    // A GUIslice element compares the existing retained buffer with the new
    // string before copying and scheduling a redraw. Never format directly
    // into statusText[][], or updates will silently stop.
    for (uint8_t row = 0; row < kStatusRows; ++row) {
        auto* ref = statusRefs[moduleIndex][row];
        if (ref) gslc_ElemSetTxtStr(&gui, ref, next.text[row]);
    }
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
        if (page == kRadioPage) {
            // Dedicated landscape player: reserve the whole page for radio.
            if (!addText(page, {12, 42, 294, 20},
                         (char*)"WEBRADIO | Player offline")) return false;
            canvasRefs[i] = gslc_ElemCreateBox(&gui, GSLC_ID_AUTO, page,
                                               {12, 65, 294, 97});
            if (!canvasRefs[i]) return false;
            ModuleGraphics::bind(i, canvasRefs[i]);
            gslc_ElemSetDrawFunc(&gui, canvasRefs[i], &ModuleGraphics::draw);
            const char* labels[] = {"<<", "PLAY", ">>", "VOL"};
            for (uint8_t j = 0; j < 4; ++j) {
                if (!addButton(page, kRadioButtonBase + j,
                               {static_cast<int16_t>(12 + j * 75), 169, 69, 36},
                               labels[j])) return false;
            }
            radioMessageRef = addText(page, {12, 213, 294, 20},
                                      radioMessage, sizeof(radioMessage));
            if (!radioMessageRef) return false;
            continue;
        }
        if (!addText(page, {12, 42, 294, 20},
                     const_cast<char*>(CompanionModules::kModules[i].detail))) return false;
        // Each module owns a native GUIslice graphics canvas.
        canvasRefs[i] = gslc_ElemCreateBox(&gui, GSLC_ID_AUTO,
                                             page, i == 5 ? gslc_tsRect{12, 65, 74, 84}
                                                          : gslc_tsRect{12, 65, 294, 84});
        if (!canvasRefs[i]) return false;
        ModuleGraphics::bind(i, canvasRefs[i]);
        gslc_ElemSetDrawFunc(&gui, canvasRefs[i], &ModuleGraphics::draw);
        if (i == 5) {
            // Static heart and dynamic waveform are independent elements.
            gslc_ElemSetRect(&gui, canvasRefs[i], {12, 65, 74, 84});
            auto* wave = gslc_ElemCreateBox(&gui, GSLC_ID_AUTO,
                                             page, {86, 65, 220, 84});
            if (!wave) return false;
            ModuleGraphics::bindPulseWave(wave);
            gslc_ElemSetDrawFunc(&gui, wave, &ModuleGraphics::draw);
        }
        // Live values remain in independent, persistent GUIslice text rows.
        // The callback re-renders the canvas without another TFT driver.
        for (uint8_t row = 0; row < kStatusRows; ++row) {
            const int16_t y = static_cast<int16_t>(155 + row * 18);
            statusRefs[i][row] = addText(
                page, {12, y, 294, 17}, statusText[i][row],
                sizeof(statusText[i][row]));
            if (!statusRefs[i][row]) return false;
        }
        // No redundant footer: the four diagnostic rows need the screen.
    }

    for (uint8_t i = 0; i < CompanionModules::kModuleCount; ++i) {
        updateModule(i, millis());  // Populate each page before first draw.
    }
    gslc_SetPageCur(&gui, 0);
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
    // SharedI2C is established by FT6336U begin() during GUI init.
    // Sensor failure never prevents the five other pages from working.
    if (initialized) {
        Serial.printf("PulsOx worker: %s\n",
                      PulsOx::begin() ? "STARTED" : "OFFLINE");
    }
    Serial.printf("GUI BOOT: %s, display=320x240, modules=%u\n",
                  initialized ? "OK" : "FAIL",
                  static_cast<unsigned>(CompanionModules::kModuleCount));
}

void loop() {
    if (initialized) {
        gslc_Update(&gui);
        const uint32_t now = millis();

        // Do not switch pages from within a GUIslice touch callback.
        if (requestedPage != currentPage && requestedPage < kPages) {
            // A Back press requests MAX30102 shutdown before switching page.
            // Never power down a sensor from a GUIslice touch callback:
            // the worker handles its I2C transaction under SharedI2C.
            const uint8_t nextPage = requestedPage;
            if (currentPage == kPulsOxPage && nextPage != kPulsOxPage) {
                PulsOx::requestActive(false);
                Serial.println("UI PulsOx: STOP REQUESTED");
            } else if (currentPage != kPulsOxPage && nextPage == kPulsOxPage) {
                Serial.printf("UI PulsOx: %s\n",
                              PulsOx::requestActive(true) ? "START REQUESTED"
                                                          : "WORKER UNAVAILABLE");
            }
            currentPage = nextPage;
            if (currentPage == kPulsOxPage) ModuleGraphics::resetPulseHeart();
            if (currentPage > 0) updateModule(currentPage - 1, now);
            gslc_SetPageCur(&gui, currentPage);
            lastStatusMs = now;
            if (currentPage > 0) ModuleGraphics::refresh(&gui, currentPage - 1);
            Serial.printf("UI NAV: %s\n", currentPage == 0 ? "Home" :
                 CompanionModules::kModules[currentPage - 1].title);
        }
        if (currentPage > 0 && now - lastStatusMs >= 1000) {
            lastStatusMs = now;
            updateModule(currentPage - 1, now);
            if (currentPage == 5) ModuleGraphics::sampleSystem(ESP.getFreeHeap());
            ModuleGraphics::refresh(&gui, currentPage - 1);
        }
        // Pulse waveform: 4 FPS reduces ST7789 full-canvas redraw flicker.
        // No direct Wire access on the UI task.
        static uint32_t lastPulseCanvasMs = 0;
        if (currentPage == 6 && now - lastPulseCanvasMs >= 250) {
            lastPulseCanvasMs = now;
            ModuleGraphics::refreshPulse(&gui);
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
