// Native host regression for the Webradio text helpers and station table.
#include "WebradioStations.h"
#include "WebradioText.h"
#include <cassert>
#include <cstring>

using namespace Webradio;

int main() {
    char out[64];
    utf8Copy(out, "Techno", sizeof(out));
    assert(std::strcmp(out, "Techno") == 0);
    char small[6];
    utf8Copy(small, "abcdefgh", sizeof(small));
    assert(std::strcmp(small, "abcde") == 0);
    // "abc" + U+00E4 (2 bytes): a 5-byte buffer must not keep half of it.
    char cut[5];
    utf8Copy(cut, "abc\xC3\xA4z", sizeof(cut));
    assert(std::strcmp(cut, "abc") == 0);
    char fits[6];
    utf8Copy(fits, "abc\xC3\xA4z", sizeof(fits));
    assert(std::strcmp(fits, "abc\xC3\xA4") == 0);
    char euro[5];  // U+20AC is 3 bytes; only 2 fit after "ab"
    utf8Copy(euro, "ab\xE2\x82\xAC", sizeof(euro));
    assert(std::strcmp(euro, "ab") == 0);
    utf8Copy(out, nullptr, sizeof(out));
    assert(out[0] == '\0');

    formatStatus(Phase::Playing, 0, 128000, "MP3", out, sizeof(out));
    assert(std::strcmp(out, "Wiedergabe | 128 kbps MP3") == 0);
    formatStatus(Phase::Playing, 0, 96000, "", out, sizeof(out));
    assert(std::strcmp(out, "Wiedergabe | 96 kbps") == 0);
    formatStatus(Phase::Playing, 0, 0, "MP3", out, sizeof(out));
    assert(std::strcmp(out, "Wiedergabe") == 0);
    formatStatus(Phase::Retrying, 2, 0, nullptr, out, sizeof(out));
    assert(std::strcmp(out, "Neustart 2/3 ...") == 0);
    formatStatus(Phase::NoWifi, 0, 0, nullptr, out, sizeof(out));
    assert(std::strcmp(out, "Kein WLAN") == 0);
    formatStatus(Phase::Failed, 0, 0, nullptr, out, sizeof(out));
    assert(std::strcmp(out, "Stream nicht erreichbar") == 0);

    for (const auto& st : kStations) {
        assert(st.name && std::strlen(st.name) > 0 && std::strlen(st.name) <= 40);
        assert(std::strncmp(st.url, "http://", 7) == 0 || std::strncmp(st.url, "https://", 8) == 0);
        assert(std::strlen(st.url) < 257);
        for (const char* c = st.url; *c; ++c) assert(*c > 0x20 && *c < 0x7F);
    }
    return 0;
}
