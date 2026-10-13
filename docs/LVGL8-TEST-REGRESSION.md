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
