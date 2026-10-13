#pragma once
#include <stddef.h>

// Fixed station list taken from the standalone ESP32-S3-Webradio project
// (default_stations[], all reachable when checked there on 2026-10-10).
// Read-only in the companion: no web editor, no NVS writes.
namespace Webradio {
struct Station {
    const char* name;  // <= 40 chars
    const char* url;   // http(s), no spaces, < 257 chars
};

constexpr Station kStations[] = {
    {"Querfunk", "http://mp3.querfunk.de/qfhi"},
    {"dinamo.fm deep", "http://channels.dinamo.fm/deep-mp3"},
    {"SSL Techno", "http://stream.sunshine-live.de/techno/mp3-192/stream.sunshine-live.de/"},
    {"Intense Radio - We love Dance", "http://secure.live-streams.nl/flac.ogg"},
    {"TranceBase.FM", "http://listen.trancebase.fm/tunein-aac-hd-pls"},
    {"Deep Radio", "http://stream.deep.radio/hd"},
    {"Hardcore Radio.NL", "http://shoutcast1.hardcoreradio.nl/"},
    {"Ibiza Global Radio", "http://ibizaglobalradio.streaming-pro.com:8024/"},
    {"Orbital", "http://centova.radios.pt:8401/;listen.pls"},
    {"Costa del Mar - Deep House", "http://stream.cdm-chillout.com:8012/stream-mp3-Deep"},
    {"electroradio.fm", "http://n05.radiojar.com/fkyxx4q35k0uv"},
    {"Psyndora Psytrance", "http://cast.magicstreams.gr:9111/stream"},
    {"psyradio * fm - progressive", "http://streamer.psyradio.org:8010/;listen.mp3"},
    {"Uzic Minimal", "http://uzic.ice.infomaniak.ch/uzic-128.aac"},
    {"DnBRadio.com", "http://ildnb1.dnbradio.com:8000/dnbradio_main.mp3"},
    {"brokenbeats - Atmospheric DnB", "https://brokenbeats.net/stream/aac"},
    {"Technolovers Hypertechno", "http://stream.technolovers.fm/hypertechno"},
    {"Back2Noize - Hardstyle", "https://securestreams4.autopo.st:1558/Back2Noize"},
    {"ROminimal.club", "https://r4.rominimal.club/listen/rominimal.club/radio2.m4a"},
    {"Goanight - Goa Trance", "http://goanight.stream.laut.fm/goanight"},
    {"1.FM Deep House Radio", "http://strm112.1.fm/deephouse_mobile_mp3"},
    {"1.FM Techno & Deep House", "http://strm112.1.fm/deeptech_mobile_mp3"},
    {"1.FM BPM Psytrance", "http://strm112.1.fm/psytrance_mobile_mp3"},
    {"Technolovers Deep House", "http://stream.technolovers.fm/deep-house"},
    {"Technolovers Minimal", "http://stream.technolovers.fm/minimal"},
    {"rautemusik Techno", "http://streams.rautemusik.fm/techno/mp3-192/"},
    {"rautemusik Trance", "http://trance-high.rautemusik.fm/"},
    {"Dutch Delite DnB", "http://radio.dutchdelite.nl:8000/dnb"},
    {"Extreme Deep House", "https://whsh4u-panel.com/proxy/yfryujzw/stream"},
    {"FRISKY", "http://stream2.friskyradio.com/frisky_mp3_hi"},
    {"Dance Wave!", "https://dancewave.online/dance.mp3"},
    // Electronic
    {"Hirschmilch Electronic", "http://hirschmilch.de:7000/electronic.mp3"},
    {"laut.fm Electronic", "http://stream.laut.fm/electronic"},
    {"SomaFM cliqhop idm", "http://ice1.somafm.com/cliqhop-128-mp3"},
    {"SomaFM Space Station Soma", "http://ice1.somafm.com/spacestation-128-mp3"},
    {"Nightride FM - Synthwave", "http://stream.nightride.fm/nightride.mp3"},
    // Minimal
    {"Record Minimal Tech", "http://radiorecord.hostingradio.ru/mini96.aacp"},
    {"laut.fm Minimal", "http://stream.laut.fm/minimal"},
    // Psytrance
    {"Hirschmilch Psytrance", "http://hirschmilch.de:7000/psytrance.mp3"},
    {"Hirschmilch Progressive", "http://hirschmilch.de:7000/progressive.mp3"},
    {"Technolovers Psytrance", "http://stream.technolovers.fm/psytrance"},
    {"Record Goa/Psy", "http://radiorecord.hostingradio.ru/goa96.aacp"},
    {"Psyndora Chillout", "http://cast.magicstreams.gr:9125/stream"},
    {"psyradio * fm - chillout", "http://streamer.psyradio.org:8020/;listen.mp3"},
};
constexpr size_t kStationCount = sizeof(kStations) / sizeof(kStations[0]);
static_assert(kStationCount > 0 && kStationCount <= 255, "station index fits uint8_t");
}  // namespace Webradio
