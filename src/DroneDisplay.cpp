#include "DroneTelemetry.h"
#include <stdio.h>
#include <string.h>

namespace DroneTelemetry {
namespace {
const char* linkText(Link link) {
    switch (link) {
        case Link::NotConfigured: return "nicht konfiguriert";
        case Link::Idle: return "inaktiv";
        case Link::WaitingForWifi: return "wartet auf Heim-WLAN";
        case Link::Connecting: return "verbindet ...";
        case Link::Subscribed: return "verbunden";
        case Link::AuthFailed: return "Anmeldung abgelehnt";
        case Link::Error: return "nicht erreichbar, neuer Versuch";
    }
    return "?";
}

bool fresh(uint32_t atMs, uint32_t nowMs) {
    return static_cast<uint32_t>(nowMs - atMs) < kStaleMs;
}

// Last four characters: enough to tell devices apart, short enough to fit.
const char* tail(const char* sn) {
    const size_t n = strlen(sn);
    return n > 4 ? sn + n - 4 : sn;
}

void number(char* out, size_t cap, bool live, bool has, float value, const char* fmt) {
    if (live && has) snprintf(out, cap, fmt, static_cast<double>(value));
    else snprintf(out, cap, "--");
}

void integer(char* out, size_t cap, bool live, int value, const char* fmt) {
    if (live && value >= 0) snprintf(out, cap, fmt, value);
    else snprintf(out, cap, "--");
}
}  // namespace

const char* modelName(int payloadType) {
    switch (payloadType) {
        case 66: return "Mavic 3E";
        case 67: return "Mavic 3T";
        case 68: return "Mavic 3M";
        case 88: return "Matrice 4E";
        case 89: return "Matrice 4T";
        default: return "Drohne";
    }
}

// DJI Cloud API mode_code (DroneModeCodeEnum), ASCII for Montserrat 12.
const char* modeName(int modeCode) {
    static const char* const kNames[] = {
        "Bereit (am Boden)", "Startvorbereitung", "Startbereit", "Manueller Flug",
        "Automatischer Start", "Wegpunktflug", "Panorama", "ActiveTrack",
        "ADS-B-Ausweichen", "Rueckkehr (RTH)", "Automatische Landung", "Zwangslandung",
        "Landung (3 Rotoren)", "Firmware-Update", "Verbindung getrennt", "APAS",
        "Virtueller Joystick", "Live-Steuerung", "RTK-Start", "Standortpruefung", "POI",
    };
    if (modeCode < 0 || modeCode >= static_cast<int>(sizeof(kNames) / sizeof(kNames[0])))
        return "--";
    return kNames[modeCode];
}

bool inAir(int modeCode) {
    // 0-2 on the ground, 13 firmware update, 14 disconnected, -1 unknown.
    return (modeCode >= 3 && modeCode <= 12) || (modeCode >= 15 && modeCode <= 20);
}

// Fallback thresholds if the aircraft does not report its own.
constexpr int kReturnHomeFallbackPct = 25;
constexpr int kLandingFallbackPct = 15;
constexpr int kRemoteLowPct = 15;

Alert alert(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return Alert::None;
    out[0] = 0;
    if (s.link != Link::Subscribed) return Alert::None;
    const Aircraft& a = s.aircraft;
    const Remote& r = s.remote;
    const bool live = a.present && fresh(a.atMs, nowMs);
    const bool remoteLive = r.present && fresh(r.atMs, nowMs);
    Alert level = Alert::None;
    char text[48];
    // Checks run from most to least important; the first alert of the
    // highest level wins.
    auto raise = [&](Alert l) {
        if (l <= level) return;
        level = l;
        snprintf(out, capacity, "%s", text);
    };

    // Silence while airborne is the most serious: the display no longer
    // knows what the aircraft is doing.
    if (a.present && !live && a.flight.inAir) {
        snprintf(text, sizeof(text), "Drohne sendet nicht mehr (%lu s)",
                 static_cast<unsigned long>((nowMs - a.atMs) / 1000));
        raise(Alert::Critical);
    }
    if (live) {
        const int landing = a.landingPowerPct > 0 ? a.landingPowerPct : kLandingFallbackPct;
        const int rth = a.returnHomePowerPct > 0 ? a.returnHomePowerPct : kReturnHomeFallbackPct;
        if (a.modeCode == 11 || a.modeCode == 12) {
            snprintf(text, sizeof(text), "ZWANGSLANDUNG");
            raise(Alert::Critical);
        }
        if (a.batteryPct >= 0 && a.batteryPct <= landing) {
            snprintf(text, sizeof(text), "Akku %d %% - Landung noetig", a.batteryPct);
            raise(Alert::Critical);
        }
        if (a.modeCode == 9) {
            snprintf(text, sizeof(text), "Rueckkehr (RTH) aktiv");
            raise(Alert::Warning);
        }
        if (a.modeCode == 10) {
            snprintf(text, sizeof(text), "Automatische Landung");
            raise(Alert::Warning);
        }
        if (a.batteryPct >= 0 && a.batteryPct <= rth) {
            snprintf(text, sizeof(text), "Akku %d %% - Rueckkehr empfohlen", a.batteryPct);
            raise(Alert::Warning);
        }
    }
    if (remoteLive) {
        if (r.linkQuality >= 0 && r.linkQuality <= 1) {
            snprintf(text, sizeof(text), "Funk sehr schwach (%d/5)", r.linkQuality);
            raise(Alert::Critical);
        }
        if (r.linkQuality == 2) {
            snprintf(text, sizeof(text), "Funk schwach (2/5)");
            raise(Alert::Warning);
        }
        if (r.batteryPct >= 0 && r.batteryPct <= kRemoteLowPct) {
            snprintf(text, sizeof(text), "RC-Akku %d %%", r.batteryPct);
            raise(Alert::Warning);
        }
    }
    return level;
}

void format(const Snapshot& s, uint32_t nowMs, char* out, size_t capacity) {
    if (out == nullptr || capacity == 0) return;
    if (s.link == Link::NotConfigured) {
        snprintf(out, capacity, "AeroNexus: nicht konfiguriert\n(include/DroneLocalConfig.h)");
        return;
    }
    const Aircraft& a = s.aircraft;
    const bool connected = s.link == Link::Subscribed;
    const bool live = connected && a.present && fresh(a.atMs, nowMs);

    char header[48];
    if (!connected || !a.present) {
        snprintf(header, sizeof(header), "AeroNexus: %s", linkText(s.link));
    } else if (live) {
        snprintf(header, sizeof(header), "%s ..%s | live", modelName(a.payloadType), tail(a.sn));
    } else {
        snprintf(header, sizeof(header), "%s ..%s | offline (%lu s)", modelName(a.payloadType),
                 tail(a.sn), static_cast<unsigned long>((nowMs - a.atMs) / 1000));
    }

    char mode[32];
    if (live) snprintf(mode, sizeof(mode), "%s", modeName(a.modeCode));
    else if (connected && a.present) snprintf(mode, sizeof(mode), "keine Daten");
    else if (connected) snprintf(mode, sizeof(mode), "keine Drohne online");
    else snprintf(mode, sizeof(mode), "--");

    char battery[10], remain[16], height[14], distance[14], hSpeed[14], vSpeed[14];
    char gps[8], rtk[16];
    integer(battery, sizeof(battery), live, a.batteryPct, "%d %%");
    // DJI reports 0 on the ground: "--" instead of a misleading "0:00".
    if (live && a.remainFlightS > 0)
        snprintf(remain, sizeof(remain), "%d:%02d", a.remainFlightS / 60, a.remainFlightS % 60);
    else
        snprintf(remain, sizeof(remain), "--");
    number(height, sizeof(height), live, a.hasElevation, a.elevationM, "%.1f m");
    number(distance, sizeof(distance), live, a.hasHomeDistance, a.homeDistanceM, "%.0f m");
    number(hSpeed, sizeof(hSpeed), live, a.hasSpeed, a.horizontalMs, "%.1f m/s");
    number(vSpeed, sizeof(vSpeed), live, a.hasSpeed, a.verticalMs, "%+.1f m/s");
    integer(gps, sizeof(gps), live, a.gpsSats, "%d");
    if (live && a.rtkSats >= 0)
        snprintf(rtk, sizeof(rtk), "%d%s", a.rtkSats, a.rtkFixed == 2 ? " fix" : "");
    else
        snprintf(rtk, sizeof(rtk), "--");

    const Remote& r = s.remote;
    const bool remoteLive = connected && r.present && fresh(r.atMs, nowMs);
    char rcBattery[10], rcLink[8];
    integer(rcBattery, sizeof(rcBattery), remoteLive, r.batteryPct, "%d %%");
    integer(rcLink, sizeof(rcLink), remoteLive, r.linkQuality, "%d/5");

    // Current or last flight; kept while the aircraft stays selected. Short
    // form (max height / max distance): the long one overflowed the panel.
    char flight[48];
    const FlightStats& f = a.flight;
    if (connected && a.present && f.any) {
        const uint32_t seconds = f.airMs / 1000;
        snprintf(flight, sizeof(flight), "Flug: %lu:%02lu | Max: %.0f m / %.0f m",
                 static_cast<unsigned long>(seconds / 60), static_cast<unsigned long>(seconds % 60),
                 static_cast<double>(f.maxElevationM), static_cast<double>(f.maxHomeDistanceM));
    } else {
        snprintf(flight, sizeof(flight), "Flug: --");
    }

    snprintf(out, capacity,
             "%s\n"
             "Modus: %s\n"
             "Akku: %s | Restflug: %s\n"
             "Hoehe: %s | Entf.: %s\n"
             "Speed: %s | Vert.: %s\n"
             "GPS: %s Sat | RTK: %s\n"
             "RC%s%s: %s | Funk: %s\n"
             "%s",
             header, mode, battery, remain, height, distance, hSpeed, vSpeed, gps, rtk,
             remoteLive ? " .." : "", remoteLive ? tail(r.sn) : "", rcBattery, rcLink, flight);
}
}  // namespace DroneTelemetry
