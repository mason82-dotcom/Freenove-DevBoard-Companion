# Freenove-DevBoard-Companion

Modularer Companion Hub für das **Freenove FNK0086 (ESP32-S3, 8 MB Flash und 8 MB PSRAM)**.

**Einzig erlaubtes Displayprofil:** `FNK0086A_2P8_CFG1_240x320_ST7789`. CFG2, automatische Profilauswahl und Display-Fallbacks sind ausdrücklich ausgeschlossen.

## v0.2.0 – LVGL-Companion-Shell (Entwicklungsbranch)

Dieser Branch baut auf dem **hardwarevalidierten** Landscape-Touch-Test auf: 12.10.2026, 5/5 Touchziele bestanden, PSRAM `8382796` Byte frei. Die neue Oberfläche selbst ist **noch nicht auf dem FNK0086 getestet**.

Die LVGL-8.3.11-Shell bietet einen 320×240-Startbildschirm mit sechs Schaltflächen: Hermes, Webradio, OBD2, Drohne, System und Kamera. Ein Klick öffnet eine Moduldetailseite mit Zurück-Navigation. Nur **System** zeigt tatsächlich lokale Livewerte (Uptime, freien Heap und freien PSRAM) an. Die übrigen Module sind ausdrücklich inaktive Platzhalter – keine WLAN-, Fahrzeug-, Drohnen- oder Kamerasteuerung.

Technik: ST7789/TFT_eSPI mit festem `setRotation(1)`, FT6336U/I²C über `BoardTouch`, zentrale feste `TouchTransform::rawToLandscape`-Abbildung, synchroner LVGL-8-Tick und teilweise gepufferter RGB565-SPI-Flush (`320×20×2` Byte). LVGL-Konfiguration: `include/lv_conf.h`, `-DLV_CONF_INCLUDE_SIMPLE=1`, `-Iinclude`.

## Raspberry Pi: isolierter Test, ohne lokale Daten zu verlieren

Der ursprüngliche Pi-Checkout hat eigene, noch nicht nach GitHub übertragene Änderungen. **Nicht überschreiben oder zurücksetzen.** Der Test erfolgt daher in einem weiteren Git-Worktree:

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git status --short
git fetch origin feature/lvgl-companion-shell
git worktree add --detach ../Freenove-DevBoard-Companion-lvgl-test \
  origin/feature/lvgl-companion-shell
cp include/VerifiedDisplaySetup.h \
  ../Freenove-DevBoard-Companion-lvgl-test/include/

cd ../Freenove-DevBoard-Companion-lvgl-test
python3 -m unittest discover -s tests -v
pio run -e fnk0086
```

Die `VerifiedDisplaySetup.h` muss zuvor aus dem **tatsächlich funktionierenden CFG1-Header** importiert worden sein. Sie wird bewusst nicht in Git versioniert. Wenn sie fehlt, nicht durch CFG2 ersetzen. Siehe `tools/import_verified_display_setup.py`.

Erst nach erfolgreichem Build das angeschlossene Ziel prüfen:

```bash
pio device list
ls -l /dev/serial/by-id/
```

Der zuletzt bestätigte Port war `/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00`; vor einem erneuten Upload überprüfen, dass es weiterhin das FNK0086 ist.

### Geplante Hardwareabnahme v0.2.0

1. TFT zeigt `FREENOVE COMPANION` sowie alle sechs Kacheln an.
2. Alle sechs Kacheln per Touch ansteuerbar; jede Seite lässt sich mit `< Zurueck` verlassen.
3. Systemseite zeigt Uptime, Heap und PSRAM live an.
4. Serielle Ausgabe meldet `LVGL shell ready: 320x240, touch=OK, modules=6`.
5. Für einen 10-Minuten-Lauf keine Abstürze oder unerwarteten starken Heap-Verluste.

Die zwei Minuten langen Einzeltests oder ein erfolgreicher Build ersetzen keine Hardwareabnahme. Keine automatische Flash-Ausführung über GitHub.

## Versionierung und Architektur

- `src/main.cpp`: Setup, LVGL-Aufruf und serielle Systemdiagnose.
- `src/BoardTouch.cpp`: FT6336U-I²C-Abfrage (nur lesen).
- `src/CompanionUi.cpp`: LVGL-Displaytreiber, Navigation, Seiten und Systemstatus.
- `include/CompanionModules.h`: sechs Moduldefinitionen.
- `include/TouchTransform.h`: durch 5/5 Hardware-Punkte validierte Touch-Rotation.
- `include/lv_conf.h`: verbindliche LVGL-8-Konfiguration.
- `docs/TOUCH-LANDSCAPE.md`: ursprüngliche Messwerte und bestandene Hardwareabnahme.

Andere Projekte wie Webradio_v2, OpenNextion_Hermes_Dashboard, OpenNextion_ELM327_Interface, FH-Clone und SkyHub bleiben unverändert. Eine spätere Anbindung darf erst nach gesonderter Prüfung und ohne unbeabsichtigte Steuerbefehle erfolgen.

## Lizenz

Originaldateien von Freenove können zusätzlichen Lizenzbedingungen unterliegen (unter anderem CC BY-NC-SA 3.0). Vor einer Übernahme von Hersteller-Code müssen die Bedingungen geprüft werden.
