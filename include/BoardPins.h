#pragma once
// Freenove FNK0086 shield. TFT GPIO macros come only from VerifiedDisplaySetup.h (CFG1).
namespace BoardPins {
constexpr int kTouchSda = 2;
constexpr int kTouchScl = 1;
constexpr unsigned char kTouchAddress = 0x38;
constexpr int kAudioBclk = 42;
constexpr int kAudioWs = 14;
constexpr int kAudioData = 41;
constexpr int kSdCmd = 38;
constexpr int kSdClk = 39;
constexpr int kSdD0 = 40;
}
