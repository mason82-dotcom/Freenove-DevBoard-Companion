#pragma once
#include <stdint.h>

// Minimal local FTP server for the global SD card (see FtpLocalConfig.example.h).
namespace FtpServer {
enum class State : uint8_t { NotConfigured, Off, NoCard, NoWifi, Listening, Session };

struct Status {
    State state = State::NotConfigured;
    uint32_t localIp = 0;      // host order, first octet most significant
    uint32_t clientIp = 0;
    uint16_t port = 0;
    uint32_t idleSecondsLeft = 0;
    uint32_t uploads = 0;
    uint32_t downloads = 0;
};

void begin();
bool configured();
// UI: switch on/off (non-blocking). Off closes any session immediately.
void setEnabled(bool enabled);
bool enabled();
Status status();
}  // namespace FtpServer
