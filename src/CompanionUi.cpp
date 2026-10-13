#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "BoardTouch.h"
#include "CompanionModules.h"
#include "CompanionUi.h"
#include "FtpServer.h"
#include "HermesStatus.h"
#include "SdCard.h"
#include "Obd2Status.h"
#include "PulsOxSensor.h"
#include "TouchTransform.h"
#include "WebradioPlayer.h"
#include "WebradioStations.h"
#include "WifiLink.h"

static_assert(LVGL_VERSION_MAJOR == 8 && LVGL_VERSION_MINOR == 3,
              "This shell requires LVGL 8.3.x");

// Normal firmware boots directly into the GUI. Rebuild with
// -DCOMPANION_DIAGNOSTIC_BOOT=1 to restore staged boot for fault isolation.
#ifndef COMPANION_DIAGNOSTIC_BOOT
#define COMPANION_DIAGNOSTIC_BOOT 0
#endif
static_assert(COMPANION_DIAGNOSTIC_BOOT == 0 || COMPANION_DIAGNOSTIC_BOOT == 1,
              "COMPANION_DIAGNOSTIC_BOOT must be 0 or 1");

namespace {
constexpr int kWidth = 320;
constexpr int kHeight = 240;
constexpr int kLinesPerFlush = 20;

TFT_eSPI screen;
static lv_disp_draw_buf_t lvDrawBuffer;
static lv_disp_drv_t lvDisplayDriver;
static lv_indev_drv_t lvPointerDriver;
// Partial RGB565 renderer: always in internal RAM and 4-byte aligned.
alignas(4) static lv_color_t drawPixels[kWidth * kLinesPerFlush];
lv_obj_t* statusLabel = nullptr;
uint32_t lastTick = 0;
uint32_t lastSystemRefresh = 0;
uint32_t lastPulsOxRefresh = 0;
lv_obj_t* pulsOxLabel = nullptr;
lv_obj_t* pulsOxChart = nullptr;
lv_chart_series_t* pulsOxSeries = nullptr;
uint32_t lastPulsOxWave = 0;
lv_obj_t* hermesLabel = nullptr;
uint32_t lastHermesRefresh = 0;
lv_obj_t* obd2Label = nullptr;
uint32_t lastObd2Refresh = 0;
lv_obj_t* ftpButtonLabel = nullptr;
lv_obj_t* radioStation = nullptr;
lv_obj_t* radioTitle = nullptr;
lv_obj_t* radioStatus = nullptr;
lv_obj_t* radioVolume = nullptr;
lv_obj_t* radioPlayLabel = nullptr;
uint32_t lastRadioRefresh = 0;
// IR trace: 100 samples at 25 Hz = 4 s; LVGL reads this array directly.
constexpr uint16_t kWavePoints = 100;
static lv_coord_t wavePoints[kWavePoints];
bool touchEnabled = false;
bool ready = false;
uint8_t diagStage = 0;  // Assigned in begin: normal 2, diagnostic 0.
uint32_t diagFlushCount = 0;

// 0 means Home. 1..6 correspond to CompanionModules::kModules.
uint8_t shownPage = 0;
uint8_t requestedPage = 0;
uint8_t moduleNumbers[CompanionModules::kModuleCount] = {1, 2, 3, 4, 5, 6};

// Pages are resolved through the module registry, not hard-coded numbers.
bool showing(CompanionModules::Id id) {
    return shownPage >= 1 && shownPage <= CompanionModules::kModuleCount &&
           CompanionModules::kModules[shownPage - 1].id == id;
}

lv_color_t color(uint32_t rgb) {
    return lv_color_hex(rgb);
}

void flushDisplay(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* colors) {
    ++diagFlushCount;
    if (diagFlushCount <= 3) {
        Serial.printf("DIAG FLUSH #%lu stage=%u area=%d,%d..%d,%d\n",
                      static_cast<unsigned long>(diagFlushCount),
                      static_cast<unsigned>(diagStage),
                      area->x1, area->y1, area->x2, area->y2);
    }
    // Stage 1 exercises LVGL's drawing pipeline without touching the TFT SPI bus.
    if (diagStage != 2) {
        lv_disp_flush_ready(disp);
        return;
    }
    if (area->x2 < 0 || area->y2 < 0 ||
        area->x1 >= kWidth || area->y1 >= kHeight) {
        lv_disp_flush_ready(disp);
        return;
    }
    // LVGL 8 renders RGB565 (LV_COLOR_DEPTH 16); swap to panel byte order here.
    const uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
    const uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
    if (diagFlushCount == 1) {
        Serial.printf("DIAG SPI ENTER pixels=%lu\n",
                      static_cast<unsigned long>(w * h));
        Serial.flush();
    }
    screen.startWrite();
    screen.setAddrWindow(area->x1, area->y1, w, h);
    screen.pushColors(reinterpret_cast<uint16_t*>(colors), w * h, true);
    screen.endWrite();
    if (diagFlushCount == 1) Serial.println("DIAG SPI EXIT");
    lv_disp_flush_ready(disp);
}

void readPointer(lv_indev_drv_t*, lv_indev_data_t* data) {
    uint16_t x = 0, y = 0;
    if (diagStage != 2 || !touchEnabled || !BoardTouch::readRaw(x, y)) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    const auto mapped = TouchTransform::rawToLandscape(x, y);
    data->point.x = mapped.x;
    data->point.y = mapped.y;
    data->state = LV_INDEV_STATE_PRESSED;
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
    pulsOxLabel = nullptr;
    pulsOxChart = nullptr;
    pulsOxSeries = nullptr;
    hermesLabel = nullptr;
    obd2Label = nullptr;
    radioStation = radioTitle = radioStatus = radioVolume = radioPlayLabel = nullptr;
    ftpButtonLabel = nullptr;
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

void sdLine(char* out, size_t cap) {
    const auto sd = SdCard::info();
    if (!sd.mounted) {
        snprintf(out, cap, "SD: keine Karte");
    } else if (!sd.usageKnown) {
        snprintf(out, cap, "SD: %s %.1f GB (Belegung ...)", SdCard::typeName(sd.type),
                 static_cast<double>(sd.totalBytes) / 1e9);
    } else {
        snprintf(out, cap, "SD: %.1f / %.1f GB frei",
                 static_cast<double>(sd.totalBytes - sd.usedBytes) / 1e9,
                 static_cast<double>(sd.totalBytes) / 1e9);
    }
}

void ftpLine(char* out, size_t cap) {
    const auto f = FtpServer::status();
    const uint32_t ip = f.localIp;
    switch (f.state) {
        case FtpServer::State::NotConfigured: snprintf(out, cap, "FTP: nicht konfiguriert"); break;
        case FtpServer::State::Off: snprintf(out, cap, "FTP: aus"); break;
        case FtpServer::State::NoCard: snprintf(out, cap, "FTP: wartet auf SD-Karte"); break;
        case FtpServer::State::NoWifi: snprintf(out, cap, "FTP: wartet auf Heim-WLAN"); break;
        case FtpServer::State::Listening:
            snprintf(out, cap, "FTP: %u.%u.%u.%u:%u (aus in %lu min)",
                     static_cast<unsigned>(ip >> 24), static_cast<unsigned>((ip >> 16) & 0xFF),
                     static_cast<unsigned>((ip >> 8) & 0xFF), static_cast<unsigned>(ip & 0xFF),
                     static_cast<unsigned>(f.port),
                     static_cast<unsigned long>((f.idleSecondsLeft + 59) / 60));
            break;
        case FtpServer::State::Session:
            snprintf(out, cap, "FTP: Client %u.%u.%u.%u verbunden",
                     static_cast<unsigned>(f.clientIp >> 24),
                     static_cast<unsigned>((f.clientIp >> 16) & 0xFF),
                     static_cast<unsigned>((f.clientIp >> 8) & 0xFF),
                     static_cast<unsigned>(f.clientIp & 0xFF));
            break;
    }
}

void updateSystemDetails() {
    if (statusLabel == nullptr || !showing(CompanionModules::Id::System)) return;
    const auto hermes = HermesStatus::read();
    char wifi[24], sd[48], ftp[48];
    if (!WifiLink::homeConfigured()) snprintf(wifi, sizeof(wifi), "nicht konfiguriert");
    else if (!WifiLink::connectedTo(WifiLink::Profile::Home)) snprintf(wifi, sizeof(wifi), "offline");
    else if (hermes.connected) snprintf(wifi, sizeof(wifi), "%d dBm", static_cast<int>(hermes.rssi));
    else snprintf(wifi, sizeof(wifi), "verbunden");
    sdLine(sd, sizeof(sd));
    ftpLine(ftp, sizeof(ftp));
    lv_label_set_text_fmt(statusLabel,
        "Uptime: %lu s | CPU: %u MHz\nHeap frei: %u B (min %u B)\nGroesster Block: %u B\n"
        "PSRAM frei: %u B\nWLAN: %s\n%s\n%s",
        static_cast<unsigned long>(millis() / 1000),
        static_cast<unsigned>(ESP.getCpuFreqMHz()),
        static_cast<unsigned>(ESP.getFreeHeap()),
        static_cast<unsigned>(ESP.getMinFreeHeap()),
        static_cast<unsigned>(ESP.getMaxAllocHeap()),
        static_cast<unsigned>(ESP.getFreePsram()), wifi, sd, ftp);
    if (ftpButtonLabel != nullptr)
        lv_label_set_text(ftpButtonLabel, !FtpServer::configured() ? "FTP: --"
                                          : FtpServer::enabled() ? "FTP: an" : "FTP: aus");
}

void onFtpToggle(lv_event_t*) {
    if (!FtpServer::configured()) return;
    FtpServer::setEnabled(!FtpServer::enabled());
    lastSystemRefresh = 0;  // show the new state on the next poll
}

void updateHermesDetails() {
    if (!hermesLabel || !showing(CompanionModules::Id::Hermes)) return;
    char text[224];
    HermesStatus::format(HermesStatus::read(), millis(), text, sizeof(text));
    lv_label_set_text(hermesLabel, text);
}

// Re-setting identical text would restart the scrolling title animation.
void setTextIfChanged(lv_obj_t* label, const char* text) {
    if (label != nullptr && strcmp(lv_label_get_text(label), text) != 0)
        lv_label_set_text(label, text);
}

void updateRadioDetails() {
    if (!radioStation || !showing(CompanionModules::Id::Webradio)) return;
    const Webradio::View v = Webradio::view();
    char line[64];
    snprintf(line, sizeof(line), "%u/%u  %s", static_cast<unsigned>(v.index + 1),
             static_cast<unsigned>(Webradio::kStationCount), v.name);
    setTextIfChanged(radioStation, line);
    setTextIfChanged(radioTitle, v.title[0] ? v.title : "-");
    Webradio::formatStatus(v.phase, v.attempt, v.bitrate, v.codec, line, sizeof(line));
    setTextIfChanged(radioStatus, line);
    snprintf(line, sizeof(line), "Lautstaerke: %d/%d", v.volume, RadioEngine::kMaxVolume);
    setTextIfChanged(radioVolume, line);
    setTextIfChanged(radioPlayLabel, v.wantsPlay ? LV_SYMBOL_STOP " Stop" : LV_SYMBOL_PLAY " Play");
}

void onRadioPrev(lv_event_t*) { Webradio::step(-1); }
void onRadioNext(lv_event_t*) { Webradio::step(1); }
void onRadioPlay(lv_event_t*) { Webradio::togglePlay(); }
void onRadioQuieter(lv_event_t*) { Webradio::changeVolume(-1); }
void onRadioLouder(lv_event_t*) { Webradio::changeVolume(1); }

void updateObd2Details() {
    if (!obd2Label || !showing(CompanionModules::Id::Obd2)) return;
    char text[320];
    Obd2Status::format(Obd2Status::read(), millis(), text, sizeof(text));
    lv_label_set_text(obd2Label, text);
}

void updatePulsOxWave() {
    if (!pulsOxChart || !pulsOxSeries) return;
    const auto sample = PulsOx::read();
    uint16_t raw[kWavePoints];
    // Without a detected finger the IR level is only ambient drift; never
    // magnify it into something that looks like a pulse.
    const uint16_t n = sample.sensorOk && sample.finger
                           ? PulsOx::readWave(raw, kWavePoints) : 0;
    if (n < PulsOx::kTraceAverage) {
        lv_chart_set_all_value(pulsOxChart, pulsOxSeries, LV_CHART_POINT_NONE);
        return;
    }
    const uint16_t pad = kWavePoints - n;
    for (uint16_t i = 0; i < pad; ++i) wavePoints[i] = LV_CHART_POINT_NONE;
    PulsOx::detrendTrace(raw, n, wavePoints + pad);
    lv_chart_refresh(pulsOxChart);
}

void showModule(uint8_t page) {
    makeScreenBlank();
    lv_obj_t* root = lv_scr_act();
    const auto& module = CompanionModules::kModules[page - 1];
    lv_obj_t* back = makeButton(root, 8, 7, 74, 28, "< Zurueck", 0x25415D);
    lv_obj_add_event_cb(back, onBackClicked, LV_EVENT_CLICKED, nullptr);
    if (module.id == CompanionModules::Id::Obd2 && Obd2Status::vehicleTitle()) {
        lv_label_set_text_fmt(newLabel(root, "", 95, 12, 0xF1F6FF), "%s | %s",
                              module.title, Obd2Status::vehicleTitle());
    } else {
        newLabel(root, module.title, 95, 12, 0xF1F6FF);
    }

    lv_obj_t* panel = lv_obj_create(root);
    lv_obj_set_pos(panel, 10, 48);
    lv_obj_set_size(panel, 300, 144);
    lv_obj_set_style_bg_color(panel, color(0x14283D), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, color(0x36516B), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Hermes has live connection state; do not show its static placeholder status.
    // PulsOx needs the full panel height: its first row is the live sensor state.
    // Panel content area (theme padding 10 px): about 278 x 122 px.
    if (module.id != CompanionModules::Id::PulsOx &&
        module.id != CompanionModules::Id::Obd2 &&
        module.id != CompanionModules::Id::Webradio &&
        module.id != CompanionModules::Id::System) {
        newLabel(panel, module.id == CompanionModules::Id::Hermes
                            ? "Hermes Live-Status" : module.detail,
                 9, 10, 0xCFDFED);
    }
    if (module.id == CompanionModules::Id::Hermes) {
        hermesLabel = newLabel(panel, "Hermes wird abgefragt ...", 9, 35, 0x81DDBC);
        updateHermesDetails();
    } else if (module.id == CompanionModules::Id::Webradio) {
        radioStation = newLabel(panel, "", 0, 0, 0xF1F6FF);
        lv_label_set_long_mode(radioStation, LV_LABEL_LONG_DOT);
        lv_obj_set_width(radioStation, 274);
        radioTitle = newLabel(panel, "", 0, 18, 0x81DDBC);
        lv_label_set_long_mode(radioTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_width(radioTitle, 274);
        radioStatus = newLabel(panel, "", 0, 36, 0x91A8C1);
        struct { int x, w; const char* text; lv_event_cb_t cb; } buttons[] = {
            {0, 48, LV_SYMBOL_PREV, onRadioPrev},
            {54, 84, LV_SYMBOL_PLAY " Play", onRadioPlay},
            {144, 48, LV_SYMBOL_NEXT, onRadioNext},
            {200, 36, "-", onRadioQuieter},
            {240, 36, "+", onRadioLouder},
        };
        for (const auto& b : buttons) {
            lv_obj_t* button = makeButton(panel, b.x, 56, b.w, 34, b.text, 0x25415D);
            lv_obj_add_event_cb(button, b.cb, LV_EVENT_CLICKED, nullptr);
            if (b.cb == onRadioPlay) radioPlayLabel = lv_obj_get_child(button, 0);
        }
        radioVolume = newLabel(panel, "", 0, 100, 0x91A8C1);
        updateRadioDetails();
    } else if (module.id == CompanionModules::Id::Obd2) {
        obd2Label = newLabel(panel, "ELM327 wird verbunden ...", 9, 0, 0x81DDBC);
        updateObd2Details();
    } else if (module.id == CompanionModules::Id::System) {
        statusLabel = newLabel(panel, "", 9, 0, 0x81DDBC);
        lv_obj_t* ftp = makeButton(root, 222, 7, 90, 28, "FTP: aus", 0x25415D);
        lv_obj_add_event_cb(ftp, onFtpToggle, LV_EVENT_CLICKED, nullptr);
        ftpButtonLabel = lv_obj_get_child(ftp, 0);
        updateSystemDetails();
    } else if (module.id == CompanionModules::Id::PulsOx) {
        pulsOxLabel = newLabel(panel, "MAX30102 wird gestartet ...", 9, 0, 0x81DDBC);
        pulsOxChart = lv_chart_create(panel);
        lv_obj_set_pos(pulsOxChart, 9, 62);
        lv_obj_set_size(pulsOxChart, 260, 38);
        lv_chart_set_type(pulsOxChart, LV_CHART_TYPE_LINE);
        lv_chart_set_point_count(pulsOxChart, kWavePoints);
        lv_chart_set_range(pulsOxChart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
        lv_chart_set_div_line_count(pulsOxChart, 0, 0);
        lv_obj_set_style_size(pulsOxChart, 0, LV_PART_INDICATOR);  // no point dots
        lv_obj_set_style_pad_all(pulsOxChart, 2, 0);
        lv_obj_set_style_bg_color(pulsOxChart, color(0x0B1220), 0);
        lv_obj_set_style_border_color(pulsOxChart, color(0x36516B), 0);
        pulsOxSeries = lv_chart_add_series(pulsOxChart, color(0xFF7A85),
                                           LV_CHART_AXIS_PRIMARY_Y);
        for (auto& point : wavePoints) point = LV_CHART_POINT_NONE;
        lv_chart_set_ext_y_array(pulsOxChart, pulsOxSeries, wavePoints);
        newLabel(panel, "Messwerte nur zur Orientierung", 9, 104, 0x91A8C1);
    } else {
        newLabel(panel, "Noch keine Verbindung / keine Befehle", 9, 51, 0x91A8C1);
    }
    if (module.id == CompanionModules::Id::Webradio) {
        newLabel(root, "Stream laeuft im Hintergrund weiter", 10, 212, 0x8EA8C1);
    } else if (module.id == CompanionModules::Id::Obd2) {
        // Seven data rows fill the panel; the read-only notice is the footer.
        newLabel(root, Obd2Status::vehicleProfile()
                           ? "Nur Lesen | nur im Stillstand bedienen"
                           : "Nur Lesen (Mode 01) | Laborprofil", 10, 212, 0x8EA8C1);
    } else {
        newLabel(root, "Nur lokale Anzeige | CFG1 320x240", 10, 212, 0x8EA8C1);
    }
}
} // namespace

namespace CompanionUi {

bool begin(bool touchPresent) {
    touchEnabled = touchPresent;
    const bool pulsOxReady = PulsOx::begin();
    Serial.printf("PulsOx worker: %s\n", pulsOxReady ? "READY" : "UNAVAILABLE");
    screen.init();
    screen.setRotation(1);  // fixed Freenove CFG1 only
    if (screen.width() != kWidth || screen.height() != kHeight) {
        Serial.printf("ERROR: TFT dimensions %dx%d (expected 320x240)\n",
                      screen.width(), screen.height());
        screen.fillScreen(TFT_RED);
        return false;
    }

    lv_init();

    lv_disp_draw_buf_init(&lvDrawBuffer, drawPixels, nullptr,
                          kWidth * kLinesPerFlush);
    lv_disp_drv_init(&lvDisplayDriver);
    lvDisplayDriver.hor_res = kWidth;
    lvDisplayDriver.ver_res = kHeight;
    lvDisplayDriver.flush_cb = flushDisplay;
    lvDisplayDriver.draw_buf = &lvDrawBuffer;
    lv_disp_t* display = lv_disp_drv_register(&lvDisplayDriver);
    if (!display) {
        Serial.println("ERROR: LVGL 8 display registration failed");
        return false;
    }

    lv_indev_drv_init(&lvPointerDriver);
    lvPointerDriver.type = LV_INDEV_TYPE_POINTER;
    lvPointerDriver.read_cb = readPointer;
    lv_indev_t* pointer = lv_indev_drv_register(&lvPointerDriver);
    if (!pointer) {
        Serial.println("ERROR: LVGL 8 input registration failed");
        return false;
    }

    Serial.printf("LVGL 8 display=%p indev=%p RGB565 buffer=%u bytes\n",
                  static_cast<void*>(display),
                  static_cast<void*>(pointer),
                  static_cast<unsigned>(sizeof(drawPixels)));
    WifiLink::begin();
    FtpServer::begin();
    Webradio::begin();
    HermesStatus::begin();
    Obd2Status::begin();
    lastTick = millis();
    lastSystemRefresh = lastTick;
    shownPage = 0;
    requestedPage = 0;
    showHome();
    ready = true;
    diagStage = COMPANION_DIAGNOSTIC_BOOT ? 0 : 2;
    diagFlushCount = 0;
    Serial.printf("LVGL shell ready: %dx%d, touch=%s, modules=%u\n",
                  kWidth, kHeight, touchEnabled ? "OK" : "MISSING",
                  static_cast<unsigned>(CompanionModules::kModuleCount));
    return true;
}

void setDiagnosticStage(uint8_t stage) {
    if (!ready || stage > 2) return;
    diagStage = stage;
    diagFlushCount = 0;
    lastTick = millis();
    lastSystemRefresh = lastTick;
    Serial.printf("DIAG STAGE %u: %s\n",
                  static_cast<unsigned>(stage),
                  stage == 0 ? "paused" :
                  stage == 1 ? "LVGL render; TFT SPI bypassed" :
                               "LVGL render; TFT SPI enabled");
    // A previous stage may have acknowledged flushes without actual TFT writes.
    // Force a complete repaint at stage 2 to make these writes observable.
    if (stage != 0) lv_obj_invalidate(lv_scr_act());
}

uint8_t diagnosticStage() {
    return diagStage;
}

void poll() {
    if (!ready || diagStage == 0) return;
    const uint32_t now = millis();
    const uint32_t elapsed = now - lastTick;
    if (elapsed > 0) {
        lv_tick_inc(elapsed);
        lastTick = now;
    }
    lv_timer_handler();

    if (requestedPage != shownPage) {
        if (showing(CompanionModules::Id::PulsOx)) PulsOx::requestActive(false);
        if (showing(CompanionModules::Id::Obd2)) Obd2Status::requestActive(false);
        shownPage = requestedPage;
        if (shownPage == 0) {
            Serial.println("UI NAV: Home");
            showHome();
        } else {
            Serial.printf("UI NAV: %s\n",
                          CompanionModules::kModules[shownPage - 1].title);
            showModule(shownPage);
            if (showing(CompanionModules::Id::PulsOx)) PulsOx::requestActive(true);
            if (showing(CompanionModules::Id::Obd2)) Obd2Status::requestActive(true);
        }
    }

    if (showing(CompanionModules::Id::PulsOx) && pulsOxLabel && now - lastPulsOxRefresh >= 500) {
        lastPulsOxRefresh = now;
        char rows[4][52] = {};
        PulsOx::format(PulsOx::read(), now, rows);
        lv_label_set_text_fmt(pulsOxLabel, "%s\n%s\n%s\n%s",
                              rows[0], rows[1], rows[2], rows[3]);
    }
    if (showing(CompanionModules::Id::PulsOx) && now - lastPulsOxWave >= 200) {
        lastPulsOxWave = now;
        updatePulsOxWave();
    }
    Webradio::service(now);
    if (showing(CompanionModules::Id::Webradio) && now - lastRadioRefresh >= 250) {
        lastRadioRefresh = now;
        updateRadioDetails();
    }
    if (showing(CompanionModules::Id::Obd2) && now - lastObd2Refresh >= 500) {
        lastObd2Refresh = now;
        updateObd2Details();
    }
    if (showing(CompanionModules::Id::Hermes) && now - lastHermesRefresh >= 1000) {
        lastHermesRefresh = now;
        updateHermesDetails();
    }
    if (showing(CompanionModules::Id::System) && now - lastSystemRefresh >= 1000) {
        lastSystemRefresh = now;
        updateSystemDetails();
    }
}
} // namespace CompanionUi
