#include "FtpProtocol.h"
#include <stdio.h>
#include <string.h>

namespace FtpProtocol {
namespace {
bool allowedChar(char c) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < 0x20 || u == 0x7F) return false;
    return strchr("\\:*?\"<>|", c) == nullptr;
}
}  // namespace

bool resolve(const char* cwd, const char* arg, char* out, size_t capacity) {
    if (out == nullptr || capacity < 2) return false;
    if (cwd == nullptr || cwd[0] != '/') cwd = "/";
    if (arg == nullptr) arg = "";
    // Segment start offsets in out, to pop on "..".
    size_t starts[64];
    size_t depth = 0;
    size_t len = 0;
    out[0] = '\0';
    const char* parts[2] = {arg[0] == '/' ? "" : cwd, arg};
    for (const char* part : parts) {
        const char* p = part;
        while (*p != '\0') {
            while (*p == '/') ++p;
            if (*p == '\0') break;
            const char* seg = p;
            while (*p != '\0' && *p != '/') {
                if (!allowedChar(*p)) return false;
                ++p;
            }
            const size_t n = static_cast<size_t>(p - seg);
            if (n == 1 && seg[0] == '.') continue;
            if (n == 2 && seg[0] == '.' && seg[1] == '.') {
                if (depth > 0) len = starts[--depth];  // clamp at "/"
                out[len] = '\0';
                continue;
            }
            if (depth >= sizeof(starts) / sizeof(starts[0])) return false;
            if (len + 1 + n > kMaxPath || len + 1 + n + 1 > capacity) return false;
            starts[depth++] = len;
            out[len++] = '/';
            memcpy(out + len, seg, n);
            len += n;
            out[len] = '\0';
        }
    }
    if (len == 0) {
        out[0] = '/';
        out[1] = '\0';
    }
    return true;
}

bool parseCommand(char* line, char verb[5], const char*& arg) {
    arg = "";
    if (line == nullptr) return false;
    size_t n = strlen(line);
    while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) line[--n] = '\0';
    if (n == 0 || n > kMaxLine) return false;
    size_t i = 0;
    for (; line[i] != '\0' && line[i] != ' '; ++i) {
        const char c = line[i];
        if (i >= 4) return false;
        if (c >= 'a' && c <= 'z') verb[i] = static_cast<char>(c - 'a' + 'A');
        else if (c >= 'A' && c <= 'Z') verb[i] = c;
        else return false;
    }
    if (i == 0) return false;
    verb[i] = '\0';
    if (line[i] == ' ') arg = line + i + 1;
    return true;
}

void formatPasv(uint32_t ip, uint16_t port, char* out, size_t capacity) {
    snprintf(out, capacity, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u).\r\n",
             static_cast<unsigned>((ip >> 24) & 0xFF), static_cast<unsigned>((ip >> 16) & 0xFF),
             static_cast<unsigned>((ip >> 8) & 0xFF), static_cast<unsigned>(ip & 0xFF),
             static_cast<unsigned>(port >> 8), static_cast<unsigned>(port & 0xFF));
}

void formatListLine(bool directory, uint64_t size, const char* name, char* out,
                    size_t capacity) {
    // No RTC: a fixed date keeps the format parseable for clients.
    snprintf(out, capacity, "%s 1 esp32 esp32 %12llu Jan 01 2026 %s\r\n",
             directory ? "drwxr-xr-x" : "-rw-r--r--",
             static_cast<unsigned long long>(directory ? 0 : size), name ? name : "");
}

bool credentialsAcceptable(const char* user, const char* password) {
    if (user == nullptr || password == nullptr) return false;
    if (strlen(user) == 0 || strlen(password) < 8) return false;
    if (strncmp(user, "YOUR_", 5) == 0 || strncmp(password, "YOUR_", 5) == 0) return false;
    return strcmp(password, "password") != 0 && strcmp(password, "12345678") != 0;
}
}  // namespace FtpProtocol
