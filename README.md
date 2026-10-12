# Freenove-DevBoard-Companion

LVGL-9.6-basierte, **lokale und rein lesende** Companion-Oberfläche für das Freenove FNK0086 (ESP32-S3, 8 MB Flash und 8 MB PSRAM). Entwicklungsstand: `feature/deps-lvgl9`, siehe [PR #3](https://github.com/mason82-dotcom/Freenove-DevBoard-Companion/pull/3). `main` bleibt bis zur getrennten Integration unverändert.

**Unveränderliches Hardwareprofil:** `FNK0086A_2P8_CFG1_240x320_ST7789`; ausschließlich das auf realer Hardware bestätigte CFG1. Kein CFG2, keine automatische Umschaltung und kein Ersatzprofil.

## Abhängigkeiten und Hardware

| Bestandteil | Pin / Implementierung |
|---|---|
| LVGL | **9.6.0**, RGB565, 320 × 240, statischer 320 × 20 × 2 Byte Puffer |
| pioarduino | **55.03.39**, ESP32 Arduino 3.3.9 |
| TFT_eSPI | **2.5.43**, `setRotation(1)`, fixes CFG1 |
| Touch | FT6336U, I²C 0x38, SDA GPIO2, SCL GPIO1 |
| Touchtransformation | `screenX=min(rawY,319)`, `screenY=239-min(rawX,239)` |
| Display | ST7789, 320 × 240 Landscape |

## LVGL-Konfiguration: Extended global aktiv

`include/lv_conf.h` definiert **COMPANION_LVGL_PROFILE_EXTENDED=1** als einziges globales LVGL-9-Profil. Das PlatformIO-Buildsystem bindet diese Datei über `-DLV_CONF_INCLUDE_SIMPLE=1 -Iinclude` für **alle LVGL-Bibliotheksdateien und die Anwendung** ein; es gibt keine separate Einstellung pro Companion-Modul.

Aktiv sind: **Label, Button, Chart, Bar, Slider, Image, Flex, Grid**, RGB565-Software-Rendering und das Standard-Theme. Unbenutzte Widgets (beispielsweise Kalender, Keyboard, Textarea, Canvas, Menü und Tabellen) sowie Simple-/Mono-Themes und Observer sind explizit deaktiviert. Damit bleiben die für spätere Status-/Telemetrieoberflächen nötigen LVGL-Bausteine verfügbar, ohne sämtliche Standardwidgets mitzunehmen.

**Validierungsstand:** Die früheren Display-/Touch-Abnahmen beziehen sich auf die vorherige LVGL-9-Konfiguration. Der neue globale Extended-Zuschnitt benötigt einen frischen ESP32-S3-Build und kurzen Anzeige-/Touch-Smoke-Test auf dem FNK0086; noch keine Flash-/RAM-Ersparnis oder Hardwareabnahme der neuen Konfiguration behaupten. Das verifizierte CFG1-Profil wurde nicht verändert.

Die sechs Kacheln **Hermes, Webradio, OBD2, Drohne, System und Kamera** ermöglichen lokale Navigation. Nur das Systemmodul zeigt echte ESP32-Livewerte (Uptime, Heap, PSRAM). Alle anderen Module sind funktionslose Platzhalter. Es werden keine Netzwerkanfragen, Fahrzeugkommandos, Drohnenbefehle oder Kameraaufnahmen ausgeführt.

## Hardwareabnahme vom 12.10.2026

- Fünf-Punkte-Touch-Abnahme **5/5 PASS** mit der unveränderten CFG1-Ausrichtung.
- LVGL 9.6 auf dem Raspberry Pi 5 erfolgreich gebaut: `pio run -e fnk0086`, 664.910 Byte Flash (21,1 %), statischer RAM 103.124 Byte (31,5 %).
- Hardware-Diagnosestufen **0, 1, 2 bestanden**; `DIAG SPI ENTER` und `DIAG SPI EXIT` nachgewiesen.
- Foto belegt die vollständige Darstellung aller sechs Kacheln; Touch-Navigation funktioniert laut Nutzer und seriell für System, Kamera, Drohne und Webradio mit Rückkehr zu Home.
- Freier Heap **266.000 Byte**, PSRAM **8.383.524 Byte**, Loop-Stack-High-Watermark **3.672 Byte** im letzten eingereichten stabilen Log.
- **Hardware-Gesamttest: PASS auf ausdrückliche Nutzerbestätigung.** Die eingereichten seriellen Laufzeitprotokolle reichen bis `uptime=270102 ms` (ca. 230 Sekunden ab Aktivierung von Stufe 2); ein vollständiges zehnminütiges Log liegt nicht vor. Diese Unterscheidung bleibt dokumentiert.

Die Tests beziehen sich auf die manuell aktivierte Diagnose-Firmware. Der folgende **Auto-Start-Code ist noch nicht neu auf dem Pi gebaut/geflasht** und muss separat durch einen kurzen Boot-Test bestätigt werden.

Details: [docs/DEPENDENCY-MIGRATION-LVGL9.md](docs/DEPENDENCY-MIGRATION-LVGL9.md), [docs/TOUCH-LANDSCAPE.md](docs/TOUCH-LANDSCAPE.md).

## Startverhalten

Ab dem aktuellen Quellcode startet die GUI **ohne serielle Eingabe** direkt im normalen LVGL-/SPI-Betrieb (Stufe 2). Für einen gezielten Diagnose-Boot mit angehaltenem LVGL kann beim Kompilieren `-DCOMPANION_DIAGNOSTIC_BOOT=1` zusätzlich als Build-Flag gesetzt werden.

Die seriellen Kommandos bleiben auch bei normalem Boot verfügbar: `0` pausiert LVGL, `1` testet LVGL ohne TFT-SPI und `2` aktiviert LVGL mit TFT-SPI. Kommandos wirken nur auf die laufende Firmware, ändern keine Hardware-Konfiguration und werden nicht gespeichert.

## Raspberry Pi: neuen Stand isoliert bauen

Der Original-Checkout hat lokale, noch nicht nach GitHub übertragene Änderungen. **Keinen Reset, keinen Force-Pull und keine Überschreibung dieser Dateien.** Der bereits verwendete separate Testordner ist `~/Projekte/Freenove-DevBoard-Companion-lvgl9-test`:

```bash
cd ~/Projekte/Freenove-DevBoard-Companion-lvgl9-test
git status --short
git fetch origin feature/deps-lvgl9
# Nur bei sauberem Worktree:
git switch --detach origin/feature/deps-lvgl9
test -f include/VerifiedDisplaySetup.h
python3 -m unittest discover -s tests -v
pio run -e fnk0086
```

Die lokale, bewusst nicht versionierte `include/VerifiedDisplaySetup.h` enthält den tatsächlich verifizierten CFG1-TFT-Treiber. Falls sie fehlt, zunächst den bestehenden CFG1-Import prüfen; **niemals** CFG2 einsetzen.

Zum Hardware-Boot-Test nach erfolgreichem Build zuerst den Port mit `pio device list` und `ls -l /dev/serial/by-id/` prüfen. Wenn der Anschluss weiter eindeutig dem FNK0086 gehört:

```bash
pio run -e fnk0086 -t upload --upload-port \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00
pio device monitor -p \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00 \
  -b 115200
```

Erwartet wird `UI BOOT: stage 2 (normal LVGL/TFT operation)` und anschließend die sichtbare Oberfläche ohne Eingabe von `1` oder `2`.

## Struktur und Integration

- `src/CompanionUi.cpp`: LVGL-9-Display-/Touchanbindung, sechs Kacheln, sichere Navigationsverarbeitung außerhalb der Event-Callbacks.
- `src/BoardTouch.cpp`: I²C-Touch nur lesend.
- `include/TouchTransform.h`: hardwarevalidierte Landscape-Touchabbildung.
- `include/lv_conf.h`: projektspezifisches LVGL-9-RGB565-/Speicherprofil.
- `src/main.cpp`: normaler GUI-Autostart, freiwillige serielle Diagnose und Laufzeittelemetrie.
- `docs/LVGL-BOOT-CRASH.md`: Analyse des früheren LVGL-8-Dangling-Pointer-Problems.
- `tests/`: CFG1-Sperre, Touchmathematik, Modulregistrierung, LVGL-9-Integration und Startmodus.

**Entwicklungshistorie:** [PR #1](https://github.com/mason82-dotcom/Freenove-DevBoard-Companion/pull/1) (Touch), [PR #2](https://github.com/mason82-dotcom/Freenove-DevBoard-Companion/pull/2) (LVGL-8-Prototyp), [PR #3](https://github.com/mason82-dotcom/Freenove-DevBoard-Companion/pull/3) (LVGL-9-Migration). Diese Pull Requests sind voneinander abhängig. `main` nicht unabhängig von den gestapelten Änderungen überschreiben.

## Lizenz

Freenove-Originaldateien können zusätzliche Lizenzauflagen haben (unter anderem CC BY-NC-SA 3.0). Hersteller-Code vor einer späteren Übernahme lizenzrechtlich prüfen.
