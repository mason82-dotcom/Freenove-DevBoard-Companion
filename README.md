# Freenove-DevBoard-Companion

Companion-Dashboard für das Freenove ESP32-S3 FNK0086 mit **LVGL 8.3.11**, **TFT_eSPI 2.5.43** und festem, hardwareverifiziertem **CFG1-Displayprofil**. Aktiver Entwicklungsbranch: `feature/companion-lvgl8-rebuild`.

> **Aktueller Stand (13.10.2026):** Der aktive PlatformIO-Build enthält sechs Module: **Hermes, Webradio, OBD2, Drohne, System und PulsOx**. Die ältere LVGL-9-/Kamera-Architektur gehört nicht zu diesem Build. Die Tests und deren Grenzen sind in [docs/LVGL8-TEST-REGRESSION.md](docs/LVGL8-TEST-REGRESSION.md) beschrieben.

## Funktionsumfang

| Modul | Aktueller Stand |
| --- | --- |
| Hermes | Read-only HTTPS-Status (CA-verifiziert, Token-Header, nur GET). Letzter gültiger Stand bleibt bei Fehlern sichtbar und wird nach 15 s als veraltet ausgeblendet. Zeigt WLAN-Pegel bzw. HTTP-Fehler mit Zähler |
| Webradio | Navigierbare Platzhalterseite; Audio noch nicht aktiv |
| OBD2 | Read-only ELM327 über WLAN/TCP (Laborprofil): Drehzahl, Tempo, Kühlmittel, Ansaugluft, Last, Drossel, Spannung; feste Allowlist (Adapter-Setup + Mode 01), keine Fehlercodes, kein Löschen. Sitzung nur bei geöffneter Seite. Am Board gegen den Laborsimulator geprüft (2.699 Befehle, 0 Policy-Verletzungen); noch nicht am Fahrzeug |
| Drohne | Navigierbare Platzhalterseite; keine Telemetrieverbindung |
| System | Lokale Livewerte: Uptime, freier/minimaler Heap, größter Block, PSRAM, CPU-Takt, WLAN-Pegel |
| PulsOx | MAX30102-Sensoranbindung mit eigenem Worker, Start/Stopp beim Öffnen/Schließen, Fingerstatus, Puls, SpO₂ und auto-skalierte IR-Pulskurve (4 s) |

**Sicherheit:** Die Platzhaltermodule senden keine Netzwerk-, Fahrzeug- oder Drohnenbefehle. Hermes liest nur (`GET /v1/status`), niemals mit `setInsecure()`. Zugangsdaten stehen ausschließlich in `include/HermesLocalConfig.h` (per `.gitignore` ausgeschlossen; Vorlage: `HermesLocalConfig.example.h`). PulsOx ist ein **Demonstrationsmodul**, kein Medizinprodukt; Messwerte sind nicht für Diagnose oder Behandlung geeignet.

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

## OBD2-Labortest

`include/Obd2LocalConfig.example.h` nach `include/Obd2LocalConfig.h` kopieren (git-ignoriert) und Host/Port eintragen. Das WLAN stellt der Hermes-Worker bereit. Simulator auf dem Pi:

```bash
python3 tools/elm327_lab_sim.py --bind <Pi-LAN-IP> --port 35000
```

Der Simulator protokolliert jeden empfangenen Befehl und meldet alles außerhalb der Allowlist als `POLICY VIOLATION`. Ein echter WLAN-ELM327 spannt ein eigenes Netz auf (meist `192.168.0.10`) und braucht ein eigenes Fahrzeugprofil, das noch nicht umgesetzt ist.

## Entwicklungshinweise

- Aktiver Anwendungscode: `src/` und `include/`
- Aktiver Build: `platformio.ini`, Umgebung `fnk0086`
- Prototypen unter `prototypes/` und LVGL-9-Dokumente sind nicht die aktive Firmware.
- Änderungen an Sensor-Worker, Touchabbildung und CFG1 nur mit begründeter, gesonderter Validierung vornehmen.
- Serielles Heartbeat-Intervall: `-DCOMPANION_HEARTBEAT_MS=2000` für Absturzanalysen (Standard 10 s).
- Bestehende lokale Änderungen vor einem Pull oder Branchwechsel prüfen; **keine Force-Resets**.

## Lizenz

Freenove-Originaldateien können zusätzliche Lizenzauflagen enthalten (unter anderem CC BY-NC-SA 3.0). Herkunft und Lizenzbedingungen vor einer Weiterverwendung prüfen.
