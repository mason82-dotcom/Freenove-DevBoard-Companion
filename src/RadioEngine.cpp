#include "RadioEngine.h"
#include "BoardPins.h"
#include "WebradioText.h"
#include <Arduino.h>
#include <Audio.h>
#include <atomic>
#include <string.h>

namespace {
using RadioEngine::Event;
using RadioEngine::EventType;
using RadioEngine::State;

Audio audio;  // touched only by the engine task

enum class CommandType : uint8_t { Connect, Stop };
struct Command {
    CommandType type;
    uint32_t gen;
    char url[RadioEngine::kUrlLen];
};

constexpr UBaseType_t kQueueLen = 8;
constexpr uint32_t kConnectTimeoutMs = 15000;  // no audio within this -> failed
// Above the Arduino loop (LVGL, prio 1) so a long flush cannot starve the
// stream buffer; the library's decoder runs in its own task on core 0.
constexpr uint32_t kTaskStack = 8192;
constexpr UBaseType_t kTaskPriority = 2;
constexpr BaseType_t kTaskCore = 1;
// I2S DMA buffers must live in internal RAM. The library default (32 x 256
// frames x 8 bytes = 64 KB) left too little internal heap for the Hermes TLS
// handshake. 12 x 256 frames = 24 KB still holds ~70 ms at 44.1 kHz; network
// jitter is absorbed by the stream buffer in PSRAM, not by DMA.
constexpr uint16_t kDmaDescNum = 12;
constexpr uint16_t kDmaFrameNum = 256;

QueueHandle_t commands = nullptr;
QueueHandle_t events = nullptr;
std::atomic<uint8_t> engineState{static_cast<uint8_t>(State::Idle)};
std::atomic<uint32_t> engineBitrate{0};
std::atomic<int> wantedVolume{10};
portMUX_TYPE codecLock = portMUX_INITIALIZER_UNLOCKED;
char engineCodec[16] = "";

// Engine-task-only state.
bool active = false;
bool wasRunning = false;
uint32_t connectStartMs = 0;
uint32_t currentGen = 0;

void setState(State s) { engineState.store(static_cast<uint8_t>(s)); }

void post(EventType type, const char* text) {
    Event e;
    e.type = type;
    e.gen = currentGen;
    Webradio::utf8Copy(e.text, text ? text : "", sizeof(e.text));
    // Titles may be dropped when the UI lags; state changes wait briefly.
    const TickType_t wait = type == EventType::Title ? 0 : pdMS_TO_TICKS(50);
    if (xQueueSend(events, &e, wait) != pdTRUE)
        Serial.printf("Radio: event %d dropped (queue full)\n", static_cast<int>(type));
}

void setCodec(const char* name) {
    portENTER_CRITICAL(&codecLock);
    strlcpy(engineCodec, name ? name : "", sizeof(engineCodec));
    portEXIT_CRITICAL(&codecLock);
}

// ESP32-audioI2S 3.x callback; runs inside audio.loop()/connecttohost() on
// the engine task, so it only queues an event.
void onAudioEvent(Audio::msg_t m) {
    if (m.e == Audio::evt_streamtitle) post(EventType::Title, m.msg);
}

void streamEnd() {
    audio.stopSong();
    active = false;
    wasRunning = false;
    engineBitrate.store(0);
    setCodec("");
    setState(State::Idle);
}

void handle(const Command& cmd) {
    if (cmd.type == CommandType::Stop) {
        streamEnd();
        currentGen = cmd.gen;
        return;
    }
    // A newer command is already queued (rapid tapping): skip this one,
    // every connect may block for several seconds.
    if (uxQueueMessagesWaiting(commands) > 0) return;
    streamEnd();
    currentGen = cmd.gen;
    setState(State::Connecting);
    Serial.printf("Radio: connecting gen=%lu\n", static_cast<unsigned long>(cmd.gen));
    if (!audio.connecttohost(cmd.url)) {
        streamEnd();
        post(EventType::ConnectFailed, nullptr);
        return;
    }
    Serial.printf("Radio: engine stack_free=%u\n",
                  static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    active = true;
    wasRunning = false;
    connectStartMs = millis();
}

void task(void*) {
    int appliedVolume = -1;
    uint32_t lastInfoMs = 0;
    for (;;) {
        const int vol = wantedVolume.load();
        if (vol != appliedVolume) {
            audio.setVolume(static_cast<uint8_t>(vol));
            appliedVolume = vol;
        }
        // Idle: wait for commands (20 ms keeps volume responsive);
        // streaming: only peek so audio.loop() keeps the buffer filled.
        Command cmd;
        TickType_t wait = active ? 0 : pdMS_TO_TICKS(20);
        while (xQueueReceive(commands, &cmd, wait) == pdTRUE) {
            handle(cmd);
            wait = 0;
        }
        if (active) {
            audio.loop();
            const bool running = audio.isRunning();
            if (running && !wasRunning) {
                wasRunning = true;
                setState(State::Playing);
                post(EventType::Playing, nullptr);
            } else if (!running && wasRunning) {
                streamEnd();
                post(EventType::StreamLost, nullptr);
            } else if (!running &&
                       static_cast<uint32_t>(millis() - connectStartMs) > kConnectTimeoutMs) {
                streamEnd();
                post(EventType::ConnectFailed, nullptr);
            }
            if (active && static_cast<uint32_t>(millis() - lastInfoMs) > 500) {
                lastInfoMs = millis();
                engineBitrate.store(audio.getBitRate());
                setCodec(audio.getCodecname());
            }
        }
        vTaskDelay(1);
    }
}

void send(CommandType type, uint32_t gen, const char* url) {
    if (commands == nullptr) return;
    Command cmd;
    cmd.type = type;
    cmd.gen = gen;
    strlcpy(cmd.url, url ? url : "", sizeof(cmd.url));
    if (xQueueSend(commands, &cmd, 0) != pdTRUE) {
        // Engine is blocked in a connect: older commands are superseded.
        xQueueReset(commands);
        xQueueSend(commands, &cmd, 0);
    }
}
}  // namespace

namespace RadioEngine {
bool begin() {
    if (commands != nullptr) return true;
    commands = xQueueCreate(kQueueLen, sizeof(Command));
    events = xQueueCreate(kQueueLen, sizeof(Event));
    if (commands == nullptr || events == nullptr) {
        Serial.println("Radio: queue allocation failed");
        return false;
    }
    Audio::audio_info_callback = onAudioEvent;
    audio.setConnectionTimeout(3000, 5000);  // HTTP 3 s, HTTPS 5 s
    // Read by the I2S channel setup inside setPinout(): set before it.
    audio.settings.DMA_DESC_NUM = kDmaDescNum;
    audio.settings.DMA_FRAME_NUM = kDmaFrameNum;
    const bool pins = audio.setPinout(BoardPins::kAudioBclk, BoardPins::kAudioWs,
                                      BoardPins::kAudioData);
    if (xTaskCreatePinnedToCore(task, "radio", kTaskStack, nullptr, kTaskPriority,
                                nullptr, kTaskCore) != pdPASS) {
        Serial.println("Radio: engine task failed");
        return false;
    }
    return pins;
}

void connect(uint32_t gen, const char* url) { send(CommandType::Connect, gen, url); }
void stop(uint32_t gen) { send(CommandType::Stop, gen, nullptr); }

void setVolume(int v) {
    wantedVolume.store(v < 0 ? 0 : v > kMaxVolume ? kMaxVolume : v);
}
int volume() { return wantedVolume.load(); }

bool pollEvent(Event& out) {
    return events != nullptr && xQueueReceive(events, &out, 0) == pdTRUE;
}

State state() { return static_cast<State>(engineState.load()); }
uint32_t bitrate() { return engineBitrate.load(); }

void codec(char* out, size_t capacity) {
    portENTER_CRITICAL(&codecLock);
    strlcpy(out, engineCodec, capacity);
    portEXIT_CRITICAL(&codecLock);
}
}  // namespace RadioEngine
