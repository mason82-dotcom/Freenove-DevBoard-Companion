// Native host regression for the FTP protocol helpers (sandbox first).
#include "FtpProtocol.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace FtpProtocol;

static std::string R(const char* cwd, const char* arg) {
    char out[kMaxPath + 1];
    if (!resolve(cwd, arg, out, sizeof(out))) return "<rejected>";
    return out;
}

int main() {
    // Normalization.
    assert(R("/", "") == "/");
    assert(R("/music", "") == "/music");
    assert(R("/", "music") == "/music");
    assert(R("/music", "rock/a.mp3") == "/music/rock/a.mp3");
    assert(R("/music", "/docs") == "/docs");
    assert(R("/a/b", "..") == "/a");
    assert(R("/a/b", "./c//d/") == "/a/b/c/d");
    assert(R("/", "Musik mit Leerzeichen.mp3") == "/Musik mit Leerzeichen.mp3");
    // Sandbox: ".." can never leave the SD root.
    assert(R("/", "..") == "/");
    assert(R("/", "../../etc/passwd") == "/etc/passwd");
    assert(R("/a", "../../../..") == "/");
    assert(R("/a/b", "../../../x") == "/x");
    assert(R("bogus", "x") == "/x");                 // invalid cwd treated as root
    // Rejected characters.
    assert(R("/", "a\\b") == "<rejected>");
    assert(R("/", "a:b") == "<rejected>");
    assert(R("/", "a*") == "<rejected>");
    assert(R("/", "a\rb") == "<rejected>");
    assert(R("/", "a\nDELE x") == "<rejected>");
    // Length limits.
    std::string longName(kMaxPath, 'x');
    assert(R("/", longName.c_str()) == "<rejected>");
    char tiny[4];
    assert(!resolve("/", "abcdef", tiny, sizeof(tiny)));

    // Command parsing.
    char line[64];
    char verb[5];
    const char* arg = nullptr;
    std::strcpy(line, "retr music/a b.mp3\r\n");
    assert(parseCommand(line, verb, arg) && std::strcmp(verb, "RETR") == 0 &&
           std::strcmp(arg, "music/a b.mp3") == 0);
    std::strcpy(line, "PWD\r\n");
    assert(parseCommand(line, verb, arg) && std::strcmp(verb, "PWD") == 0 && *arg == '\0');
    std::strcpy(line, "TOOLONG x");
    assert(!parseCommand(line, verb, arg));
    std::strcpy(line, "R3TR x");
    assert(!parseCommand(line, verb, arg));
    std::strcpy(line, "\r\n");
    assert(!parseCommand(line, verb, arg));

    // Replies.
    char out[128];
    formatPasv(0xC0A8B239u, 50000, out, sizeof(out));  // 192.168.178.57:50000
    assert(std::strcmp(out, "227 Entering Passive Mode (192,168,178,57,195,80).\r\n") == 0);
    formatListLine(false, 1234, "a.mp3", out, sizeof(out));
    assert(std::strcmp(out, "-rw-r--r-- 1 esp32 esp32         1234 Jan 01 2026 a.mp3\r\n") == 0);
    formatListLine(true, 999, "music", out, sizeof(out));
    assert(std::strncmp(out, "drwxr-xr-x", 10) == 0 && std::strstr(out, " 0 Jan") != nullptr);

    // Subnet and credentials.
    static_assert(sameSubnet(0xC0A8B239u, 0xC0A8B201u, 0xFFFFFF00u), "same /24");
    static_assert(!sameSubnet(0xC0A8B239u, 0xC0A80001u, 0xFFFFFF00u), "other /24");
    static_assert(!sameSubnet(1, 1, 0), "no mask, no access");
    assert(credentialsAcceptable("mason", "s3cure-pass"));
    assert(!credentialsAcceptable("YOUR_FTP_USER", "s3cure-pass"));
    assert(!credentialsAcceptable("mason", "short"));
    assert(!credentialsAcceptable("mason", "password"));
    assert(!credentialsAcceptable("", "s3cure-pass"));
    return 0;
}
