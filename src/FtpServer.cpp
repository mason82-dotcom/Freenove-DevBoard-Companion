#include "FtpServer.h"
#include "FtpProtocol.h"
#include "SdCard.h"
#include "WifiLink.h"
#include <Arduino.h>
#include <WiFi.h>
#include <atomic>
#include <memory>
#include <new>
#include <string.h>

#if __has_include("FtpLocalConfig.h")
#include "FtpLocalConfig.h"
#define FTP_LOCAL_CONFIG_PRESENT 1
#else
#define FTP_LOCAL_CONFIG_PRESENT 0
#endif

namespace {
using FtpServer::State;
using FtpServer::Status;

portMUX_TYPE statusLock = portMUX_INITIALIZER_UNLOCKED;
Status current;
TaskHandle_t worker = nullptr;
std::atomic<bool> wantEnabled{false};

void publish(const Status& s) {
    portENTER_CRITICAL(&statusLock);
    current = s;
    portEXIT_CRITICAL(&statusLock);
}

#if FTP_LOCAL_CONFIG_PRESENT
constexpr uint32_t kIdleOffMs = 10UL * 60UL * 1000UL;
constexpr uint32_t kControlIdleMs = 300000;
constexpr uint32_t kDataAcceptMs = 10000;
constexpr uint32_t kLockoutMs = 30000;
constexpr uint8_t kMaxFailedLogins = 3;
constexpr size_t kTransferBuffer = 4096;

uint32_t toHostOrder(const IPAddress& ip) {
    return (static_cast<uint32_t>(ip[0]) << 24) | (static_cast<uint32_t>(ip[1]) << 16) |
           (static_cast<uint32_t>(ip[2]) << 8) | ip[3];
}

void ipText(uint32_t ip, char* out, size_t capacity) {
    snprintf(out, capacity, "%u.%u.%u.%u", static_cast<unsigned>(ip >> 24),
             static_cast<unsigned>((ip >> 16) & 0xFF), static_cast<unsigned>((ip >> 8) & 0xFF),
             static_cast<unsigned>(ip & 0xFF));
}

NetworkServer control(FTP_PORT);
NetworkServer passive(FTP_PASV_PORT);
bool controlListening = false;
bool passiveListening = false;
uint8_t failedLogins = 0;
uint32_t lockoutUntilMs = 0;
uint32_t lastActivityMs = 0;
Status status;

struct Session {
    NetworkClient client;
    uint32_t clientIp = 0;
    bool userOk = false;
    bool loggedIn = false;
    char cwd[FtpProtocol::kMaxPath + 1] = "/";
    char renameFrom[FtpProtocol::kMaxPath + 1] = "";
};

void reply(Session& s, const char* text) {
    s.client.print(text);
}

void stopPassive() {
    if (passiveListening) passive.end();
    passiveListening = false;
}

void stopAll() {
    stopPassive();
    if (controlListening) control.end();
    controlListening = false;
}

bool stillWanted() {
    return wantEnabled.load() && WifiLink::connectedTo(WifiLink::Profile::Home);
}

// Accepts the passive data connection, but only from the logged-in client.
bool acceptData(Session& s, NetworkClient& data) {
    if (!passiveListening) return false;
    const uint32_t start = millis();
    while (static_cast<uint32_t>(millis() - start) < kDataAcceptMs && stillWanted()) {
        NetworkClient candidate = passive.accept();
        if (candidate) {
            if (toHostOrder(candidate.remoteIP()) == s.clientIp) {
                data = candidate;
                data.setNoDelay(true);
                return true;
            }
            candidate.stop();  // data-port hijack attempt from another host
            Serial.println("FTP: foreign data connection rejected");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

bool resolveArg(Session& s, const char* arg, char* path) {
    if (FtpProtocol::resolve(s.cwd, arg, path, FtpProtocol::kMaxPath + 1)) return true;
    reply(s, "553 Pfad ungueltig.\r\n");
    return false;
}

void list(Session& s, const char* arg, bool namesOnly) {
    // Clients send flags such as "-la"; they are ignored.
    if (arg[0] == '-') arg = "";
    char path[FtpProtocol::kMaxPath + 1];
    if (!resolveArg(s, arg, path)) return;
    File dir = SdCard::fs().open(path);
    if (!dir || !dir.isDirectory()) {
        reply(s, "550 Kein Verzeichnis.\r\n");
        return;
    }
    NetworkClient data;
    reply(s, "150 Verzeichnisliste folgt.\r\n");
    if (!acceptData(s, data)) {
        reply(s, "425 Keine Datenverbindung.\r\n");
        stopPassive();
        return;
    }
    char line[FtpProtocol::kMaxPath + 64];
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (namesOnly) snprintf(line, sizeof(line), "%s\r\n", f.name());
        else FtpProtocol::formatListLine(f.isDirectory(), f.size(), f.name(), line, sizeof(line));
        data.print(line);
    }
    data.stop();
    stopPassive();
    reply(s, "226 Liste gesendet.\r\n");
}

void retrieve(Session& s, const char* arg) {
    char path[FtpProtocol::kMaxPath + 1];
    if (!resolveArg(s, arg, path)) return;
    File f = SdCard::fs().open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        reply(s, "550 Datei nicht gefunden.\r\n");
        return;
    }
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[kTransferBuffer]);
    NetworkClient data;
    reply(s, "150 Sende Datei.\r\n");
    if (!buffer || !acceptData(s, data)) {
        reply(s, buffer ? "425 Keine Datenverbindung.\r\n" : "451 Kein Speicher.\r\n");
        stopPassive();
        return;
    }
    size_t total = 0;
    bool ok = true;
    for (;;) {
        const size_t n = f.read(buffer.get(), kTransferBuffer);
        if (n == 0) break;
        if (data.write(buffer.get(), n) != n || !stillWanted()) {
            ok = false;
            break;
        }
        total += n;
        lastActivityMs = millis();
    }
    data.stop();
    stopPassive();
    if (ok) ++status.downloads;
    Serial.printf("FTP: RETR %s (%u bytes)%s\n", path, static_cast<unsigned>(total),
                  ok ? "" : " aborted");
    reply(s, ok ? "226 Uebertragung abgeschlossen.\r\n" : "426 Uebertragung abgebrochen.\r\n");
}

void store(Session& s, const char* arg) {
    char path[FtpProtocol::kMaxPath + 1];
    if (!resolveArg(s, arg, path)) return;
    if (strcmp(path, "/") == 0) {
        reply(s, "553 Ungueltiger Dateiname.\r\n");
        return;
    }
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[kTransferBuffer]);
    NetworkClient data;
    reply(s, "150 Bereit zum Empfang.\r\n");
    if (!buffer || !acceptData(s, data)) {
        reply(s, buffer ? "425 Keine Datenverbindung.\r\n" : "451 Kein Speicher.\r\n");
        stopPassive();
        return;
    }
    File f = SdCard::fs().open(path, FILE_WRITE);
    if (!f) {
        data.stop();
        stopPassive();
        reply(s, "550 Datei kann nicht angelegt werden.\r\n");
        return;
    }
    size_t total = 0;
    bool ok = true;
    while (stillWanted() && (data.connected() || data.available() > 0)) {
        const int available = data.available();
        if (available <= 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        const int n = data.read(buffer.get(), available < static_cast<int>(kTransferBuffer)
                                                  ? available : static_cast<int>(kTransferBuffer));
        if (n <= 0) continue;
        if (f.write(buffer.get(), static_cast<size_t>(n)) != static_cast<size_t>(n)) {
            ok = false;  // card full or write error
            break;
        }
        total += static_cast<size_t>(n);
        lastActivityMs = millis();
    }
    if (!stillWanted()) ok = false;
    f.close();
    data.stop();
    stopPassive();
    if (ok) ++status.uploads;
    SdCard::refreshUsage();
    Serial.printf("FTP: STOR %s (%u bytes)%s\n", path, static_cast<unsigned>(total),
                  ok ? "" : " failed");
    reply(s, ok ? "226 Datei gespeichert.\r\n" : "451 Schreibfehler.\r\n");
}

void openPassive(Session& s, bool extended) {
    if (!passiveListening) {
        passive.begin();
        passiveListening = true;
    }
    char text[80];
    if (extended) snprintf(text, sizeof(text), "229 Entering Extended Passive Mode (|||%u|).\r\n",
                           static_cast<unsigned>(FTP_PASV_PORT));
    else FtpProtocol::formatPasv(toHostOrder(WiFi.localIP()), FTP_PASV_PORT, text, sizeof(text));
    reply(s, text);
}

// Returns false when the session must end.
bool handle(Session& s, char* line) {
    char verb[5];
    const char* arg = "";
    if (!FtpProtocol::parseCommand(line, verb, arg)) {
        reply(s, "500 Syntaxfehler.\r\n");
        return true;
    }
    const auto is = [&](const char* v) { return strcmp(verb, v) == 0; };
    if (is("QUIT")) {
        reply(s, "221 Tschuess.\r\n");
        return false;
    }
    if (is("USER")) {
        s.userOk = strcmp(arg, FTP_USER) == 0;
        s.loggedIn = false;
        reply(s, "331 Passwort erforderlich.\r\n");
        return true;
    }
    if (is("PASS")) {
        if (s.userOk && strcmp(arg, FTP_PASSWORD) == 0) {
            s.loggedIn = true;
            failedLogins = 0;
            char ip[16];
            ipText(s.clientIp, ip, sizeof(ip));
            Serial.printf("FTP: login ok from %s\n", ip);
            reply(s, "230 Angemeldet.\r\n");
            return true;
        }
        s.userOk = false;
        Serial.println("FTP: login failed");
        if (++failedLogins >= kMaxFailedLogins) {
            failedLogins = 0;
            lockoutUntilMs = millis() + kLockoutMs;
            reply(s, "421 Zu viele Fehlversuche.\r\n");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // slow down guessing
        reply(s, "530 Anmeldung fehlgeschlagen.\r\n");
        return true;
    }
    if (is("NOOP")) { reply(s, "200 OK.\r\n"); return true; }
    if (!s.loggedIn) { reply(s, "530 Bitte anmelden.\r\n"); return true; }
    if (!SdCard::mounted()) { reply(s, "550 Keine SD-Karte.\r\n"); return true; }

    char path[FtpProtocol::kMaxPath + 1];
    if (is("SYST")) reply(s, "215 UNIX Type: L8\r\n");
    else if (is("FEAT")) reply(s, "211-Features:\r\n PASV\r\n EPSV\r\n SIZE\r\n UTF8\r\n211 Ende\r\n");
    else if (is("OPTS")) reply(s, "200 OK.\r\n");
    else if (is("TYPE") || is("MODE") || is("STRU")) reply(s, "200 OK.\r\n");
    else if (is("PWD") || is("XPWD")) {
        char text[FtpProtocol::kMaxPath + 32];
        snprintf(text, sizeof(text), "257 \"%s\"\r\n", s.cwd);
        reply(s, text);
    } else if (is("CWD") || is("CDUP")) {
        if (!resolveArg(s, is("CDUP") ? ".." : arg, path)) return true;
        File d = SdCard::fs().open(path);
        if (d && d.isDirectory()) {
            strlcpy(s.cwd, path, sizeof(s.cwd));
            reply(s, "250 OK.\r\n");
        } else {
            reply(s, "550 Kein Verzeichnis.\r\n");
        }
    } else if (is("PASV")) openPassive(s, false);
    else if (is("EPSV")) openPassive(s, true);
    else if (is("PORT") || is("EPRT")) reply(s, "502 Nur Passivmodus.\r\n");
    else if (is("LIST")) list(s, arg, false);
    else if (is("NLST")) list(s, arg, true);
    else if (is("RETR")) retrieve(s, arg);
    else if (is("STOR")) store(s, arg);
    else if (is("SIZE")) {
        if (!resolveArg(s, arg, path)) return true;
        File f = SdCard::fs().open(path);
        char text[48];
        if (f && !f.isDirectory()) {
            snprintf(text, sizeof(text), "213 %u\r\n", static_cast<unsigned>(f.size()));
            reply(s, text);
        } else {
            reply(s, "550 Datei nicht gefunden.\r\n");
        }
    } else if (is("DELE")) {
        if (!resolveArg(s, arg, path)) return true;
        const bool ok = SdCard::fs().remove(path);
        if (ok) SdCard::refreshUsage();
        Serial.printf("FTP: DELE %s %s\n", path, ok ? "ok" : "failed");
        reply(s, ok ? "250 Geloescht.\r\n" : "550 Loeschen fehlgeschlagen.\r\n");
    } else if (is("MKD") || is("XMKD")) {
        if (!resolveArg(s, arg, path)) return true;
        reply(s, SdCard::fs().mkdir(path) ? "257 Verzeichnis angelegt.\r\n"
                                          : "550 Anlegen fehlgeschlagen.\r\n");
    } else if (is("RMD") || is("XRMD")) {
        if (!resolveArg(s, arg, path)) return true;
        const bool ok = strcmp(path, "/") != 0 && SdCard::fs().rmdir(path);
        reply(s, ok ? "250 Verzeichnis entfernt.\r\n" : "550 Entfernen fehlgeschlagen.\r\n");
    } else if (is("RNFR")) {
        if (!resolveArg(s, arg, path)) return true;
        if (strcmp(path, "/") != 0 && SdCard::fs().exists(path)) {
            strlcpy(s.renameFrom, path, sizeof(s.renameFrom));
            reply(s, "350 Ziel angeben.\r\n");
        } else {
            reply(s, "550 Nicht gefunden.\r\n");
        }
    } else if (is("RNTO")) {
        if (!resolveArg(s, arg, path)) return true;
        const bool ok = s.renameFrom[0] && strcmp(path, "/") != 0 &&
                        SdCard::fs().rename(s.renameFrom, path);
        s.renameFrom[0] = '\0';
        reply(s, ok ? "250 Umbenannt.\r\n" : "550 Umbenennen fehlgeschlagen.\r\n");
    } else {
        reply(s, "502 Nicht unterstuetzt.\r\n");
    }
    return true;
}

void runSession(NetworkClient client, uint32_t clientIp) {
    std::unique_ptr<Session> s(new (std::nothrow) Session());
    if (!s) {
        client.stop();
        return;
    }
    s->client = client;
    s->clientIp = clientIp;
    s->client.setNoDelay(true);
    status.state = State::Session;
    status.clientIp = clientIp;
    publish(status);
    reply(*s, "220 Freenove Companion FTP (SD-Karte)\r\n");
    char line[FtpProtocol::kMaxLine + 1];
    size_t length = 0;
    uint32_t lastCommandMs = millis();
    bool open = true;
    while (open && stillWanted() && s->client.connected()) {
        if (static_cast<uint32_t>(millis() - lastCommandMs) > kControlIdleMs) {
            reply(*s, "421 Zeitueberschreitung.\r\n");
            break;
        }
        int available = s->client.available();
        if (available <= 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        while (available-- > 0 && open) {
            const int c = s->client.read();
            if (c < 0) break;
            if (c == '\n') {
                line[length] = '\0';
                length = 0;
                lastCommandMs = lastActivityMs = millis();
                open = handle(*s, line);
            } else if (length < FtpProtocol::kMaxLine) {
                line[length++] = static_cast<char>(c);
            } else {
                reply(*s, "500 Zeile zu lang.\r\n");
                open = false;
            }
        }
    }
    stopPassive();
    s->client.stop();
    status.clientIp = 0;
    Serial.println("FTP: session closed");
}

void task(void*) {
    status.port = FTP_PORT;
    for (;;) {
        if (!wantEnabled.load()) {
            stopAll();
            status.state = State::Off;
            status.idleSecondsLeft = 0;
            publish(status);
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            lastActivityMs = millis();
            continue;
        }
        if (!SdCard::mounted() || !WifiLink::connectedTo(WifiLink::Profile::Home)) {
            stopAll();
            status.state = SdCard::mounted() ? State::NoWifi : State::NoCard;
            publish(status);
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
            continue;
        }
        if (static_cast<uint32_t>(millis() - lastActivityMs) >= kIdleOffMs) {
            wantEnabled.store(false);
            Serial.println("FTP: auto-off after 10 min idle");
            continue;
        }
        if (!controlListening) {
            control.begin();
            control.setNoDelay(true);
            controlListening = true;
            Serial.printf("FTP: listening on port %u\n", static_cast<unsigned>(FTP_PORT));
        }
        status.state = State::Listening;
        status.localIp = toHostOrder(WiFi.localIP());
        status.idleSecondsLeft = (kIdleOffMs - (millis() - lastActivityMs)) / 1000;
        publish(status);
        NetworkClient client = control.accept();
        if (client) {
            const uint32_t ip = toHostOrder(client.remoteIP());
            const uint32_t mask = toHostOrder(WiFi.subnetMask());
            if (!FtpProtocol::sameSubnet(ip, status.localIp, mask)) {
                client.stop();
                Serial.println("FTP: client outside local subnet rejected");
            } else if (static_cast<int32_t>(millis() - lockoutUntilMs) < 0) {
                client.print("421 Gesperrt, spaeter erneut versuchen.\r\n");
                client.stop();
            } else {
                lastActivityMs = millis();
                runSession(client, ip);
                lastActivityMs = millis();
            }
            continue;
        }
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
    }
}
#endif
}  // namespace

namespace FtpServer {
bool configured() {
#if FTP_LOCAL_CONFIG_PRESENT
    return FtpProtocol::credentialsAcceptable(FTP_USER, FTP_PASSWORD);
#else
    return false;
#endif
}

void begin() {
#if FTP_LOCAL_CONFIG_PRESENT
    if (worker != nullptr || !configured()) return;
    Status s;
    s.state = State::Off;
    publish(s);
    // Parked until switched on from the System page.
    if (xTaskCreatePinnedToCore(task, "ftp", 6144, nullptr, 1, &worker, 0) != pdPASS)
        worker = nullptr;
#endif
}

void setEnabled(bool on) {
    if (worker == nullptr) return;
    wantEnabled.store(on);
    xTaskNotifyGive(worker);
}

bool enabled() { return wantEnabled.load(); }

Status status() {
    portENTER_CRITICAL(&statusLock);
    const Status copy = current;
    portEXIT_CRITICAL(&statusLock);
    return copy;
}
}  // namespace FtpServer
