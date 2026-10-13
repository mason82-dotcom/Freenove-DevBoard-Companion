#include "ModuleDiagnostics.h"
#include "BoardPins.h"
#include <stdio.h>

namespace ModuleDiagnostics::Adapter {
void webradio(Snapshot& out, uint32_t uptimeMs) {
    // Show hardware pin allocation without starting I2S, writing SD media,
    // contacting stations or claiming an audio stream is playing.
    snprintf(out.text[0], kTextCapacity, "Audio: noch nicht gestartet");
    snprintf(out.text[1], kTextCapacity, "I2S B:%d W:%d D:%d",
             BoardPins::kAudioBclk, BoardPins::kAudioWs,
             BoardPins::kAudioData);
    snprintf(out.text[2], kTextCapacity, "Sender: keiner | SD: aus");
    snprintf(out.text[3], kTextCapacity, "Diagnose-Uptime: %lu s",
             static_cast<unsigned long>(uptimeMs / 1000));
}
}
