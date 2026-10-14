# Freenove-DevBoard-Companion

Companion-Dashboard für das Freenove ESP32-S3 FNK0086 mit **LVGL 8.3.11**, **TFT_eSPI 2.5.43** und festem, hardwareverifiziertem **CFG1-Displayprofil**. Aktiver Entwicklungsbranch: `feature/companion-lvgl8-rebuild`.

> **Aktueller Stand (13.10.2026):** Der aktive PlatformIO-Build enthält sechs Module: **Hermes, Webradio, OBD2, Drohne, System und PulsOx**. Die ältere LVGL-9-/Kamera-Architektur gehört nicht zu diesem Build. Die Tests und deren Grenzen sind in [docs/LVGL8-TEST-REGRESSION.md](docs/LVGL8-TEST-REGRESSION.md) beschrieben.

## Funktionsumfang

| Modul | Aktueller Stand |
| --- | --- |
| Hermes | Read-only HTTPS-Status (CA-verifiziert, Token-Header, nur GET). Letzter gültiger Stand bleibt bei Fehlern sichtbar und wird nach 15 s als veraltet ausgeblendet. Zeigt WLAN-Pegel bzw. HTTP-Fehler mit Zähler |
| Webradio | Internetradio über I²S (BCLK 42, LRC 14, DOUT 41) mit ESP32-audioI2S 3.4.7; Audio-Engine aus dem eigenständigen ESP32-S3-Webradio-Projekt portiert. 44 feste Sender, Play/Stop, Sender vor/zurück, Lautstärke, Titel (ICY), Bitrate/Codec, bis zu 3 automatische Neustarts. Läuft im Hintergrund weiter. Kein Webinterface, keine NVS-Speicherung, keine Aufnahme |
| OBD2 | Read-only ELM327 über WLAN/TCP (Laborprofil): Drehzahl, Tempo, Kühlmittel, Ansaugluft, Last, Drossel, Spannung; feste Allowlist (Adapter-Setup + Mode 01), keine Fehlercodes, kein Löschen. Sitzung nur bei geöffneter Seite. Am Board gegen den Laborsimulator geprüft (2.699 Befehle, 0 Policy-Verletzungen); noch nicht am Fahrzeug |
| Drohne | Read-only Telemetrie von AeroNexus (DJI Cloud API `thing/product/+/osd` per MQTT, Konto `companion`, das laut Broker-ACL nur abonnieren darf). Folgt automatisch dem Fluggerät, das gerade sendet: Modell, Flugmodus, Akku/Restflugzeit, Höhe, Entfernung, Geschwindigkeit, GPS/RTK sowie Akku und Funkqualität der Fernsteuerung. Eigener MQTT-Client ohne Sendefunktion, keine Positionen, Sitzung nur bei geöffneter Seite. Konfiguration: `include/DroneLocalConfig.h` (Vorlage `DroneLocalConfig.example.h`), Laborsimulator `tools/drone_osd_sim.py` |
| System | Lokale Livewerte: Uptime, freier/minimaler Heap, größter Block, PSRAM, CPU-Takt, WLAN-Pegel |
| PulsOx | MAX30102-Sensoranbindung mit eigenem Worker, Start/Stopp beim Öffnen/Schließen, Fingerstatus, Puls, SpO₂ und auto-skalierte IR-Pulskurve (4 s) |

**Sicherheit:** Kein Modul sendet Fahrzeug- oder Drohnenbefehle; die Drohnenseite kann technisch nichts an den Broker senden. Hermes liest nur (`GET /v1/status`), niemals mit `setInsecure()`. Zugangsdaten stehen ausschließlich in `include/HermesLocalConfig.h` (per `.gitignore` ausgeschlossen; Vorlage: `HermesLocalConfig.example.h`). PulsOx ist ein **Demonstrationsmodul**, kein Medizinprodukt; Messwerte sind nicht für Diagnose oder Behandlung geeignet.

## Hardwareprofil

- Board: Freenove FNK0086 / ESP32-S3, 8 MB Flash und PSRAM
- Display: ST7789, **320 × 240 Landscape**, `screen.setRotation(1)`
- Displaytreiber: ausschließlich `include/VerifiedDisplaySetup.h` (**CFG1**, keine automatische CFG2-Umschaltung)
- Touch: FT6336U, I²C-Adresse `0x38`, SDA GPIO2, SCL GPIO1
- PulsOx: MAX30102 über gemeinsam abgesicherten I²C-Bus
- Framework: pioarduino ESP32 Arduino; maßgebliche Versionen stehen in `platformio.ini`

Die bestätigte Touch-Transformation ist in `include/TouchTransform.h` implementiert. Display-Pins und Rotation nicht ohne erneute Hardwareprüfung ändern.

## Bauen und prüfen

Auf dem Raspberry Pi im Checkout dieses Branches:

```bash
python3 -m unittest discover -s tests -p 'test_*.py' -v
pio run -e fnk0086
```

Die GitHub-Actions-Datei [lvgl8-regression.yml](.github/workflows/lvgl8-regression.yml) führt beide Prüfungen für Pushes auf den Entwicklungsbranch und für Pull Requests aus. **CI baut die Firmware, flasht jedoch keine Hardware.**

Zuletzt bestätigter GitHub-Actions-Lauf: [38450901771](https://github.com/mason82-dotcom/Freenove-DevBoard-Companion/actions/runs/38450901771) für Commit `b947db9` (**success**). Der letzte lokale Durchlauf (Hermes/PulsOx/System-Ausbau) umfasste **37 bestandene Tests** und einen erfolgreichen PlatformIO-Build (RAM 39,8 %, Flash 40,0 %), sowohl mit als auch ohne lokale Hermes-Konfiguration. Diese Werte sind Momentaufnahmen, keine Garantie für spätere Commits.

## Dokumentation und Testgrenzen

- [LVGL-8-Testmigration und Ergebnisse](docs/LVGL8-TEST-REGRESSION.md)
- [Touch-Ausrichtung](docs/TOUCH-LANDSCAPE.md)
- [Frühere LVGL-9-Abhängigkeitsmigration](docs/DEPENDENCY-MIGRATION-LVGL9.md) — **historisch, nicht aktiver Build**
- [LVGL-Boot-/Crash-Analyse](docs/LVGL-BOOT-CRASH.md)

Frühere Hardwareprüfungen und Diagnoseprotokolle sind Teil der Projektgeschichte; die automatisierten LVGL-8-Tests sind **keine erneute physische Hardwareabnahme**. Hardwaretests wurden bereits im Projektverlauf durchgeführt. Neue Firmwareänderungen sollten bei Bedarf gezielt und nicht pauschal erneut geprüft werden.

## SD-Karte und FTP

Die SD-Karte (SD_MMC, 1 Bit: CLK 39, CMD 38, D0 40) wird beim Start eingebunden und steht allen Modulen über `SdCard::fs()` zur Verfügung; sie wird nie automatisch formatiert. Die System-Seite zeigt Typ und freien Platz.

Optionaler FTP-Server für die SD-Karte: `include/FtpLocalConfig.example.h` nach `include/FtpLocalConfig.h` kopieren (git-ignoriert), Benutzer und Passwort (mind. 8 Zeichen) setzen. **FTP ist unverschlüsselt.** Der Server ist nach dem Start aus und wird über die Taste „FTP“ auf der System-Seite eingeschaltet; er läuft nur im Heim-WLAN, nimmt nur Clients aus dem eigenen Subnetz an (ein Client, nur Passivmodus, Port 21 + 50000), sperrt nach 3 Fehlversuchen für 30 s, bleibt auf die SD-Karte beschränkt und schaltet sich nach 10 min ohne Aktivität ab. Beispiel: `ftp <Board-IP>` oder FileZilla mit „Nur einfaches FTP (unsicher)“ und Passivmodus.

## OBD2-Labortest

`include/Obd2LocalConfig.example.h` nach `include/Obd2LocalConfig.h` kopieren (git-ignoriert) und Host/Port eintragen. Das WLAN stellt der Hermes-Worker bereit. Simulator auf dem Pi:

```bash
python3 tools/elm327_lab_sim.py --bind <Pi-LAN-IP> --port 35000
```

Der Simulator protokolliert jeden empfangenen Befehl und meldet alles außerhalb der Allowlist als `POLICY VIOLATION`. ### Fahrzeugprofil (echter WLAN-ELM327)

In `include/Obd2LocalConfig.h` den Laborblock durch den Fahrzeugblock aus der Vorlage ersetzen (`OBD2_PROFILE_VEHICLE 1`, SSID/Passwort des Dongles, meist `192.168.0.10:35000`). Beim Öffnen der OBD2-Seite wechselt `WifiLink` ins Dongle-WLAN, Hermes pausiert; beim Verlassen kehrt das Board ins Heim-WLAN zurück. Zugangsdaten werden nicht in den Flash (NVS) geschrieben. Antwortet kein Steuergerät (Zündung aus), zeigt die Seite „kein Steuergeraet (Zuendung?)“. **Erste Tests nur im Stillstand**; das Fahrzeugprofil ist noch nicht an einem echten Dongle geprüft. Für den Mercedes C 350 W204 (2008, M272) gibt es ein benanntes Profil mit Kraftstoffkorrektur beider Zylinderbänke und Luftmasse (`include/VehicleProfiles.h`).

## Entwicklungshinweise

- Aktiver Anwendungscode: `src/` und `include/`
- Aktiver Build: `platformio.ini`, Umgebung `fnk0086`
- Prototypen unter `prototypes/` und LVGL-9-Dokumente sind nicht die aktive Firmware.
- Änderungen an Sensor-Worker, Touchabbildung und CFG1 nur mit begründeter, gesonderter Validierung vornehmen.
- Serielles Heartbeat-Intervall: `-DCOMPANION_HEARTBEAT_MS=2000` für Absturzanalysen (Standard 10 s).
- Bestehende lokale Änderungen vor einem Pull oder Branchwechsel prüfen; **keine Force-Resets**.

## Lizenz

Freenove-Originaldateien können zusätzliche Lizenzauflagen enthalten (unter anderem CC BY-NC-SA 3.0). Herkunft und Lizenzbedingungen vor einer Weiterverwendung prüfen.
