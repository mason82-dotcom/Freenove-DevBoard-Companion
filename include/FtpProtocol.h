#pragma once
#include <stddef.h>
#include <stdint.h>

// Pure, host-testable parts of the local FTP server (no Arduino, no sockets).
namespace FtpProtocol {
constexpr size_t kMaxPath = 255;
constexpr size_t kMaxLine = 512;

// Resolves arg against cwd into an absolute, normalized SD path ("/a/b").
// ".." never climbs above "/" (sandbox); "." and empty segments vanish.
// Rejects control characters and FAT-invalid characters (\ : * ? " < > |)
// and results longer than kMaxPath. out is always "/"-rooted on success.
bool resolve(const char* cwd, const char* arg, char* out, size_t capacity);

// Splits "VERB arg" in place: verb upper-cased (max 4 letters + NUL),
// arg points after the single separating space (may be empty).
// Returns false for empty, oversized or non-letter verbs.
bool parseCommand(char* line, char verb[5], const char*& arg);

// "227 Entering Passive Mode (a,b,c,d,p1,p2)." for an IPv4 in host order
// (first octet in the most significant byte).
void formatPasv(uint32_t ip, uint16_t port, char* out, size_t capacity);

// One Unix-style LIST line understood by common clients, CRLF terminated.
void formatListLine(bool directory, uint64_t size, const char* name, char* out,
                    size_t capacity);

// Only clients in the device's own IPv4 subnet may connect.
constexpr bool sameSubnet(uint32_t a, uint32_t b, uint32_t mask) {
    return mask != 0 && (a & mask) == (b & mask);
}

// Credentials from the local config must be real, not template values.
bool credentialsAcceptable(const char* user, const char* password);
}  // namespace FtpProtocol
