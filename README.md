# Freenove-DevBoard-Companion

Modularer Companion Hub für **Freenove FNK0086 (ESP32-S3, 8 MB Flash, 8 MB PSRAM)**.

## v0.1.0 / Phase 0 — CFG1-Hardwareprüfung

**Einzig erlaubtes Displayprofil:** `FNK0086A_2P8_CFG1_240x320_ST7789`.
Kein CFG2, keine automatische Profilerkennung, kein Fallback.

Dieser Entwicklungsstand enthält eine lokale ST7789-RGB-Diagnose, FT6336U-I²C-Touchprobe und Heap-/PSRAM-Ausgabe auf der seriellen Konsole. **Noch keine LVGL-Oberfläche**, Netzwerkanbindung, Audiofunktion oder externe Steuerung.

## Raspberry Pi 5 — Initialisierung

```bash
cd ~/Projekte
git clone https://github.com/mason82-dotcom/Freenove-DevBoard-Companion.git
cd Freenove-DevBoard-Companion
codex --version
python3 -m unittest discover -s tests -v
```

Falls das Repository bereits vorhanden ist, den Ordner **nicht überschreiben**, sondern dort `git status` prüfen und nur ohne lokale Konflikte `git pull --ff-only` ausführen.

### CFG1-Setup importieren

Das für die reale Freenove-Hardware funktionierende `FNK0086A_2P8_CFG1_240x320_ST7789.h` aus der Freenove-Bibliothek wird lokal importiert:

```bash
find ~/Projekte ~/.platformio ~/Arduino -name 'FNK0086A_2P8_CFG1_240x320_ST7789.h' 2>/dev/null
python3 tools/import_verified_display_setup.py --source /PFAD/ZUR/FNK0086A_2P8_CFG1_240x320_ST7789.h
```

**Keinen Beispielpfad unverändert ausführen**. Falls die Datei fehlt, muss das offiziell funktionierende Freenove-Setup zuerst bereitgestellt werden. `include/VerifiedDisplaySetup.h` ist lokal und gitignored.

### Build ohne Flash

```bash
pio run -e fnk0086
pio device list
ls -l /dev/serial/by-id/
```

Der Build wurde hier **nicht auf dem Raspberry Pi ausgeführt**. Erst nach tatsächlicher Identifikation des Zielgeräts und Sichtung des Builds kann ein Upload manuell freigegeben werden.

### Codex lokal starten

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
codex
```

Codex liest `AGENTS.md` automatisch als Projektarbeitsregeln. Er soll zuerst die Tests, den vorhandenen Code und die reale CFG1-Konfiguration untersuchen, dann den Build und seine Fehler bearbeiten. **Kein automatisches Flashen.**

## Geplante Module

1. LVGL App-Shell und Touchnavigation
2. Hermes-Agent und Pi-Status (bereits bestehende TLS-Bridge nutzen, keine direkten Hermes-API-Schlüssel)
3. Webradio (I²S, SD_MMC)
4. OBD2 per ELM327-WLAN (nur freigegebene Leseoperationen)
5. FH-Clone/SkyHub-Telemetrie (FC0/read-only)
6. Kamera und Diagnose

Quellcode von `OpenNextion_Hermes_Dashboard`, `Webradio_v2`, `OpenNextion_ELM327_Interface`, `FH-Clone` und `SkyHub` darf zunächst nur als Referenz gelesen werden.

### Hinweis zu Herstellerquellen

Die Freenove-Originaldateien stehen unter CC BY-NC-SA 3.0. Vor Übernahme in veröffentlichte oder kommerzielle Anwendungen ist die Lizenz zu berücksichtigen.

**Status:** Initiales GitHub-Gerüst vorhanden. Pi-Checkout, echter Firmware-Build, Port-Identifikation und Flash stehen noch aus.
