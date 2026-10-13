# LVGL 8.3.11: Regressionstests des aktiven Builds (13.10.2026)

## Geltungsbereich

`platformio.ini` baut `src/` mit LVGL 8.3.11, TFT_eSPI 2.5.43 und SparkFun MAX3010x 1.1.2. Die sechs aktiven Module heißen Hermes, Webradio, OBD2, Drohne, System und PulsOx. `prototypes/guislice-lovyangfx/` und `docs/DEPENDENCY-MIGRATION-LVGL9.md` sind historische bzw. alternative Stände und werden von `pio run -e fnk0086` nicht als Anwendung gebaut. Die frühere LVGL-9-/Kamera-Beschreibung in `README.md` ist mit einem Hinweis auf diesen aktiven Buildpfad gekennzeichnet.

## Änderungen an den Tests

- `tests/test_companion_shell.py`: `Id::Camera` durch `Id::PulsOx` ersetzt; LVGL-9-API-Erwartungen durch LVGL-8-Registrierung, statische Lebensdauer von Draw-Buffer und Treiberstrukturen, RGB565-Puffer, sechs Kacheln, Navigation nach `lv_timer_handler`, Diagnosepfad und Startmodus ersetzt. Der Host-C++-Kompilierungstest für das Modulregister bleibt erhalten.
- `tests/test_lvgl_config.py`: Versionen und LVGL-8-Makros aus dem tatsächlich gebauten `platformio.ini` und `lv_conf.h` geprüft. Der Test deckt weiterhin Include-Pfad, CFG1-Bindung, Speicher-/Farbprofil und aktivierte bzw. deaktivierte Widgets ab.
- `tests/test_guislice_lgfx.py` wurde durch `tests/test_active_architecture.py` ersetzt. Die alten Prüfbereiche (sechs Module und Seiten, Livewerte, grafische Darstellung, PulsOx-Lebenszyklus, read-only-Grenzen, Touch, Display und CFG1) werden an den aktiven Dateien unter `src/` und `include/` geprüft. GUIslice-spezifische Aufrufe und LovyanGFX-Details werden nicht mehr als Firmware-Regressionskriterien behandelt.
- `tests/test_module_diagnostics_compile.py` und `tests/module_diagnostics_smoke.cpp` wurden durch `tests/test_pulsox_format_compile.py` und `tests/pulsox_format_smoke.cpp` ersetzt. Der native C++-Test kompiliert die echte Funktion `src/PulsOxDisplay.cpp` und prüft vier vollständige Anzeigezeilen für inaktiv, fehlende/frische/veraltete Daten, Fingerstatus sowie Puls- und SpO2-Grenzwerte. Der ältere Host-Test gegen die GUIslice-Adapter ist damit funktional für den aktiven Messwertpfad ersetzt.
- `tests/test_display_profile.py` und `tests/test_touch_transform.py` bleiben erhalten. Der erste schützt den CFG1-Import und die CFG2-Sperre; der zweite kompiliert die unveränderte Transformation mit den dokumentierten realen Touchpunkten.
- `README.md` trägt am Anfang einen Hinweis auf den aktiven LVGL-8-Build und diese Prüfdokumentation. Der darunterliegende historische LVGL-9-Text wurde nicht als aktuelle Buildbeschreibung umgeschrieben.

Es wurden keine Firmware-Quelldateien, Displayrotation, Touch-Transformation, CFG1-Header oder Sensoralgorithmen geändert.

## Grenzen

Die Python-Tests enthalten Quelltextverträge und native Host-C++-Tests. Sie führen LVGL, FreeRTOS, FT6336U und MAX30102 nicht auf dem ESP32-S3 aus. Ein erfolgreicher PlatformIO-Build prüft Übersetzung und Linken, aber keine Funktion auf realer Hardware. Die von `pio device list` erkannte USB-Seriell-ID belegt allein nicht die physische Boardidentität; es wurde kein Upload oder Flash vorgenommen.

`tests/test_display_profile.py` prüft weiterhin das bestehende CFG1-Importwerkzeug anhand künstlicher Header mit `TFT_WIDTH` und `TFT_HEIGHT`. Der lokal vorhandene importierte Freenove-Header definiert diese beiden Makros nicht aktiv. Ein erneuter Import dieses konkreten Headers mit dem derzeitigen Werkzeug wäre daher gesondert zu prüfen; an Werkzeug und Header wurde hier nichts geändert.

## Ergebnisse dieses Durchlaufs

- Vor der Migration: `python3 -m unittest discover -s tests -v` — 32 Tests, 35 Fehlschläge durch überholte Erwartungen und einen GUIslice-Prototyp-Test.
- Nach der Migration: `python3 -m unittest discover -s tests -v` — 31 Tests, alle bestanden.
- `pio device list`: `/dev/ttyACM0`, USB VID:PID `1A86:55D3`, Seriennummer `58FC003021`, Beschreibung `USB Single Serial`. `/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00` zeigt auf `/dev/ttyACM0`.
- `pio run -e fnk0086`: **SUCCESS** in 45,09 s; RAM 105.852 / 327.680 Byte (32,3 %), Flash 524.990 / 3.145.728 Byte (16,7 %). Die Kompilierung meldete Warnungen aus den Abhängigkeiten: TFT_eSPI hat keinen `TOUCH_CS` (der FT6336U wird stattdessen über `BoardTouch`/I²C gelesen); SparkFun MAX3010x definiert `I2C_BUFFER_LENGTH` erneut. Keine dieser Warnungen verhinderte den Build.


## Analyse- und Ausbaurunde (13.10.2026, Abend)

### Behobene Fehler

- **Zugangsdaten-Leck verhindert:** `include/HermesLocalConfig.h` (WLAN, Gerätetoken, CA) war weder versioniert noch ignoriert und wäre mit `git add -A` hochgeladen worden. Jetzt in `.gitignore`; ein Test sichert das ab.
- **Hermes-Worker konnte bis zu 120 s blockieren:** Der TLS-Handshake-Timeout von `NetworkClientSecure` ist in Arduino-ESP32 3.x standardmäßig 120 s. Jetzt `setHandshakeTimeout(8)` (Sekunden), Connect- und Lese-Timeout explizit je 4000 ms über `HTTPClient`. Das frühere `client->setTimeout(4)` wurde entfernt: Die Einheit ist in 3.x Millisekunden, der Wert wurde beim Verbinden ohnehin überschrieben.
- **Einzelner Fehlversuch löschte alle Hermes-Werte:** Der Worker hält jetzt einen persistenten Zustand. Werte des letzten gültigen Abrufs bleiben bis 15 s sichtbar, danach werden sie ausgeblendet („Daten veraltet“). HTTP-Fehler werden mit Zähler angezeigt.
- **Fehlende Diagnose:** Antworten ohne Content-Length (chunked) wurden stillschweigend verworfen; jetzt protokolliert.
- **Veraltete Kommentare** (LVGL 9.6 / 8.6) korrigiert.

### Optimierungen

- Seitenlogik in `CompanionUi.cpp` über `showing(CompanionModules::Id::…)` statt fester Seitennummern 1/5/6.
- Hermes-Log nur bei Änderung, Fehler oder etwa einmal pro Minute; enthält die Dauer von `http.end()` (`end_ms`) – ersetzt die vorübergehenden Diagnosezeilen davor/danach.
- Heartbeat standardmäßig alle 10 s (`COMPANION_HEARTBEAT_MS`).
- Hermes-Seitentext als reine Funktion `HermesStatus::format()` (`src/HermesDisplay.cpp`), nativ testbar.

### Weiterentwicklung der Module

- **PulsOx:** IR-Pulskurve als `lv_chart` (100 Punkte = 4 s, auto-skaliert, invertiert, Zugriff nur über `PulsOx::readWave()`, kein I²C aus der GUI).
- **System:** minimaler freier Heap, größter allokierbarer Block, CPU-Takt, WLAN-Pegel (aus dem Hermes-Snapshot, keine eigene Netzwerkfunktion in der GUI).
- **Hermes:** WLAN-Pegel, Alter der Daten, Fehlerzähler.

### Tests und Ergebnisse

- Neu: `tests/test_hermes_status.py` (gitignore, TLS-Grenzen, keine schreibenden HTTP-Methoden, persistenter Zustand) und `tests/hermes_format_smoke.cpp` (nativer Formatter-Test inkl. Veraltungsgrenze, `millis()`-Überlauf und Kürzung).
- `tests/test_active_architecture.py`: Seiten-Verträge auf Modul-IDs umgestellt; Chart-Vertrag ergänzt.
- `python3 -m unittest discover -s tests -p 'test_*.py'`: **37 Tests, alle bestanden.**
- `pio run -e fnk0086`: **SUCCESS**, RAM 130.288 B (39,8 %), Flash 1.259.523 B (40,0 %); zusätzlich ohne `HermesLocalConfig.h` (CI-Pfad) erfolgreich gebaut.
- Gerät: `/dev/ttyACM0`, VID:PID `1A86:55D3`, SER `58FC003021` (`/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00`). **Nicht geflasht**, keine Hardwareprüfung dieser Änderungen. Zu prüfen auf dem Gerät: Layout der PulsOx-Kurve und der fünfzeiligen Hermes-/System-Texte, Hermes-Verhalten bei abgeschaltetem Server (Worker darf höchstens ca. 12 s pro Abruf blockieren).

## Hardware-Nachprüfung (13.10.2026, spät)

Geflasht auf `/dev/ttyACM0` (SER `58FC003021`) nach Freigabe durch den Nutzer; Upload per Hash verifiziert.

- **TLS-Logzeilen:** Nach vollständiger Antwort schloss `http.end()` die vom Server bereits beendete TLS-Verbindung über `available()` und protokollierte mbedTLS `-76` („failed available check“). Jetzt wird nach vollständig gelesenem Body zuerst still `client->stop()` aufgerufen; unvollständige oder fehlerhafte Übertragungen melden weiterhin normal. 40 s Log: keine Fehlerzeilen, `end_ms` 1–2 ms.
- **Displayfotos (Nutzer):** System- und Hermes-Seite zeigen alle fünf Zeilen vollständig im Panel; PulsOx-Layout (Zeilen, Kurve, Hinweis) passt.
- **PulsOx-Kurve korrigiert:** Ohne Finger hat die Auto-Skalierung die langsame IR-Drift als große Rampe dargestellt. Die Kurve wird jetzt nur bei erkanntem Finger gezeichnet und vorher mit einem zentrierten gleitenden 1-s-Mittelwert hochpassgefiltert (`PulsOx::detrendTrace()`, nativ getestet: Rampe bleibt flach, Pulse auf Drift erreichen volle Skala).
- Tests: 38 bestanden. Build: SUCCESS (RAM 39,8 %, Flash 40,0 %). Mit aufgelegtem Finger vom Nutzer per Foto bestätigt: „Finger: erkannt“, regelmäßige Pulskurve ohne Driftrampe (ca. 5 Spitzen in 4 s). Nach kurzer Anlaufzeit (≥ 2 Schläge bzw. 4-s-SpO₂-Fenster) erscheinen Puls- und SpO₂-Werte (Nutzerbestätigung). Werte bleiben Demonstration, kein Medizinprodukt.


## OBD2 read-only Modul (13.10.2026)

- `include/Obd2Policy.h`: geschlossene Allowlist, beim Kompilieren geprüft. Adapter: `ATZ ATE0 ATL0 ATS0 ATH0 ATSP0`; Fahrzeug: nur exakt `01PP` (`0100 0120 0140 0104 0105 010C 010D 010F 0111 0142`). Keine Mode 03/04/07/08/0A, kein `ATSH`/`ATMA`, keine frei eingebbaren Befehle.
- `include/Obd2Response.h`: strenger Parser (Echo, `SEARCHING...`, Header, geteilte Bytes, `NO DATA`, `?` werden nicht als Daten akzeptiert); SAE-J1979-Skalierung.
- `src/Obd2Client.cpp`: Worker auf Core 0, nur bei geöffneter OBD2-Seite aktiv; Antwortpuffer 512 Byte, Timeouts je Befehl (Suche 12 s, sonst 2 s), Neuverbindung nach 5 Fehlern in Folge. Sendet ausschließlich `wireCommand()` + CR.
- `src/Obd2Display.cpp`: Seitentext; Werte älter als 5 s oder ohne Live-Verbindung als `--`, nicht unterstützte PIDs als `n/v`.
- `tools/elm327_lab_sim.py`: eigener Laborsimulator mit Befehlsprotokoll und Policy-Verletzungszähler.
- Tests: `tests/test_obd2.py` + `tests/obd2_smoke.cpp` (Allowlist, Parser, Bitmap, Skalierung, Formatter, Simulator). **43 Tests bestanden.** Build mit und ohne `Obd2LocalConfig.h`: **SUCCESS** (RAM 40,0 %, Flash 40,2 %).
- **Labortest am Board (13.10.2026):** Nach Nutzerfreigabe geflasht auf `/dev/ttyACM0` (SER `58FC003021`); `tools/elm327_lab_sim.py` auf `192.168.178.63:35000`, 180 s. Drei Besuche der OBD2-Seite (öffnen, zurück, erneut öffnen):
  - Simulator: **2.699 Befehle, 0 Policy-Verletzungen**; empfangen wurden ausschließlich `ATZ ATE0 ATL0 ATS0 ATH0 ATSP0 0100 0120 0140 0104 0105 010C 010D 010F 0111 0142`.
  - Board-Zähler je Sitzung stimmen exakt mit dem Simulator überein (158 / 536 / 2.005 Befehle); 0 Timeouts. Sitzung 1 meldete `errors=1` ohne verlorene Antwort (tx = Simulatorzählung), vermutlich ein erster Verbindungsversuch direkt nach dem Booten; der Mitschnitt begann zu spät für einen Nachweis. Sitzung 2 und 3: 0 Fehler.
  - Sitzung endet beim Verlassen der Seite (`OBD2: idle`, TCP getrennt); beim nächsten Öffnen vollständige Neuinitialisierung.
  - Simulator beendet: Board wechselt zu `error`, neuer Versuch alle ~3 s, kein Absturz, Heap stabil (~152 KB). Worker-Stack: 3.712 von 6.144 Byte frei.
  - Foto der OBD2-Seite (Nutzer, zweiter Simulatorlauf): alle Werte live (859 rpm, 32 km/h, 90 C, 30 C, 29,8 %, 12,5 %, 13,50 V; TX 75, 0 Timeouts, 0 Fehler), Read-only-Hinweis sichtbar. Die erste Zeile `ELM327 v1.5 (LAB SIM)` reichte bis an den Panelrand: Die Adapterkennung wird jetzt ohne Klammerzusatz übernommen und auf 15 Zeichen begrenzt (`Obd2Response::adapterId()`, nativ getestet, inkl. Echo mit früherem „E“). Nach Freigabe geflasht und per Foto bestätigt: erste Zeile `ELM327: verbunden | ELM327 v1.5` mit Abstand zum Panelrand; Livewerte 1400 rpm, 46 km/h, TX 183, 0 Timeouts, 0 Fehler. Simulatorlauf: 347 Befehle, 0 Policy-Verletzungen; Beenden per Ctrl+C sauber, kein Traceback.
  - Simulator: beendet sich jetzt bei Ctrl+C/SIGTERM sauber (vorher nur per SIGTERM, ohne Zusammenfassung) und protokolliert keinen Traceback mehr, wenn das Board die Verbindung beim Verlassen der Seite zurücksetzt.
  - **Kein Fahrzeugtest**, kein echter ELM327-Dongle.


## OBD2-Fahrzeugprofil (13.10.2026)

- `include/WifiLink.h`, `src/WifiLink.cpp`: einzige Stelle mit `WiFi.begin()`/`WiFi.disconnect()`. Profile *Heim* (Hermes, OBD2-Labor) und *Fahrzeug* (Dongle-AP). `WiFi.persistent(false)`: Profilwechsel schreiben keine Zugangsdaten in den NVS-Flash. Das Profil wird vor dem Trennen umgestellt, damit kein Worker der alten Verbindung vertraut.
- Hermes fragt nur bei verbundenem Heimprofil ab; im Fahrzeugprofil verlässt kein Request (und kein Token) das Gerät.
- OBD2-Worker: wählt beim Öffnen der Seite das Profil, kehrt beim Verlassen ins Heim-WLAN zurück.
- **Sicherheitskorrektur:** Ein Timeout ohne `>`-Prompt beendet jetzt die TCP-Sitzung (vorher lief sie weiter, eine verspätete Antwort hätte dem nächsten Befehl derselben PID zugeordnet werden können).
- Neuer Zustand `NoEcu` bei `UNABLE TO CONNECT`/`BUS INIT`/`NO DATA`/`CAN ERROR` auf `0100` („kein Steuergeraet (Zuendung?)“), neuer Versuch nach 5 s. Adapterkennung nur noch bei Live-Verbindung in Zeile 1, damit lange Zustandstexte passen.
- Tests: 45 bestanden (u. a. WLAN-Einzelbesitz, Timeout-Abbruch, NoEcu-Texte, Zeilenlänge ≤ 40 Zeichen). Builds: Labor, Fahrzeug (`OBD2_PROFILE_VEHICLE 1`) und ohne lokale Konfiguration jeweils **SUCCESS**.
- **Noch nicht auf dem Board geprüft**; kein echter Dongle, kein Fahrzeug.


## Fahrzeugprofil Mercedes C 350 W204 (2008, M272) (13.10.2026)

- `include/VehicleProfiles.h`: benanntes Profil `MercedesC350W204M272`; es beschriftet nur die Seite („OBD2 | C350 W204 M272“) und fügt **keine Befehle** hinzu. Auswahl lokal per `#define OBD2_VEHICLE VehicleProfiles::Vehicle::MercedesC350W204M272` im Fahrzeugblock von `Obd2LocalConfig.h`.
- Fahrzeugbezogene Erweiterung der Allowlist (weiterhin nur exakt `01PP`): Kraftstoffkorrektur kurz/lang **Bank 1 und Bank 2** (`0106`–`0109`, sinnvoll beim V6 M272) und Luftmasse (`0110`). Skalierung: Trim `(A−128)·100/128 %`, MAF `(256A+B)/100 g/s`.
- Protokoll: erwartet ISO 15765-4 CAN, **nicht verifiziert**; `ATSP0` (automatische Suche) bleibt, damit eine falsche Annahme die Verbindung nicht blockiert. Welche PIDs das Steuergerät liefert, wird zur Laufzeit per `0100/0120/0140` ermittelt; nicht gelieferte Werte erscheinen als `n/v`.
- Seite: 7 Datenzeilen (u. a. „Spannung | MAF“, „Trim B1: kurz/lang B2: kurz/lang %“); der Read-only-Hinweis steht in der Fußzeile. Zähleranzeige begrenzt (TX `99999+`, Timeouts/Fehler `99+`), Worst-Case-Test: jede Zeile ≤ 40 Zeichen.
- Praxis: Dongle bei abgestelltem Fahrzeug abziehen (Dauerstromverbrauch). Erste Tests nur im Stillstand mit Zündung an.
- Tests: 45 bestanden. Builds Labor, Fahrzeug, Fahrzeug+C350 und ohne Konfiguration: **SUCCESS**. **Nicht am Fahrzeug geprüft**; Dongle-SSID noch unbekannt.


## Webradio-Modul (13.10.2026)

- Quelle: hochgeladenes Projekt `ESP32-S3-Webradio` (nur gelesen, in isoliertem Ordner entpackt). Portiert: `radio_engine` → `src/RadioEngine.cpp` (eigener Audio-Task auf Core 1, Prio 2; Befehls-/Ereignis-Queues mit Generationsnummer; 15-s-Verbindungs-Timeout; HTTP 3 s/HTTPS 5 s), Neustartlogik → `src/WebradioPlayer.cpp` (3 Versuche im 3-s-Abstand, wartet auf das Heim-WLAN), Senderliste (44 Sender) → `include/WebradioStations.h`.
- Nicht portiert: Webinterface (Port 80), OTA, SD-Aufnahme, Wecker, Equalizer, NVS-Einstellungen, Verstärker-SD-Pin (im Original erzeugte `AMP_SD_PIN -1` die `IO 255`-Fehler im Boot-Log).
- Bibliothek: `ESP32-audioI2S` per Git-Tag `3.4.7` gepinnt. `esp_dsp.h` stammt aus dem Arduino-3.3.9-Framework (`libespressif__esp-dsp.a`); der eigene esp-dsp-Shim des Originalprojekts ist hier nicht nötig. Zusätzliche Framework-Bibliotheken (FS, SD, SD_MMC, FFat, SPI) und `-I…/FS/src` nur, weil `Audio.h` deren Header einbindet.
- Seite: Sender (n/44), Titel als Lauftext (Text nur bei Änderung gesetzt, sonst springt die Animation), Status („Wiedergabe | 128 kbps MP3“), Tasten ◀ / Play-Stop / ▶ / − / +, Lautstärke 0–21. Montserrat 12 enthält keine Umlaute; Sonderzeichen in Titeln werden nicht dargestellt.
- Tests: 50 bestanden (`tests/test_webradio.py`, `tests/webradio_smoke.cpp`: UTF-8-Kürzung, Statustexte, Senderliste; Engine ohne LVGL, UI ohne Audio-Objekt, keine Server-/NVS-/SD-Zugriffe). Build mit und ohne lokale Konfiguration **SUCCESS** (RAM 43,9 %, Flash 72,2 %).
- **Nicht auf dem Board geprüft**, kein Audiotest.
