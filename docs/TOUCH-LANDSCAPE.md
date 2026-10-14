# FNK0086: fünfpunktiger Landscape-Touch-Test (CFG1)

## Hardwareabnahme 12.10.2026 – PASS

Der Benutzer hat die Firmware auf dem Raspberry Pi 5 erfolgreich mit PlatformIO 55.03.32 gebaut (`[SUCCESS] Took 54.51 seconds`, Flash 388851 Byte, 12.4 %, statischer RAM 22824 Byte, 7.0 %).

Anschließend wurden **alle fünf Touchziele auf dem realen FNK0086 erfolgreich getroffen**, protokolliert über `/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00`:

| Ziel | Roh X/Y | Display X/Y | Ergebnis |
|---|---|---|---|
| 1 oben links | 217/27 | 27/22 | PASS |
| 2 oben rechts | 198/292 | 292/41 | PASS |
| 3 unten links | 6/24 | 24/233 | PASS |
| 4 unten rechts | 8/299 | 299/231 | PASS |
| 5 Mitte | 116/165 | 165/123 | PASS |

Letzte Monitorzeile: `HARDWARE TOUCH TEST PASSED (5/5)`.

`free_heap=345556` und `free_psram=8382796` blieben von 15 bis 30 Sekunden stabil. **Damit ist die feste Touch-Rotation für den jetzigen Aufbau hardwarevalidiert.** Der Test liefert keinen Langzeitstabilitäts- oder Belastungsnachweis für spätere Module.


## Hardware-Messwerte und feste Transformation

**Nur Freenove FNK0086 CFG1**: `FNK0086A_2P8_CFG1_240x320_ST7789`.
Am 12.10.2026 wurde die Berührungsreihenfolge ausdrücklich bestätigt:

| Reihenfolge | Physische Ecke | Roh-X | Roh-Y | Logisches X | Logisches Y |
|---:|---|---:|---:|---:|---:|
| 1 | oben links | 234 | 27 | 27 | 5 |
| 2 | oben rechts | 226 | 318 | 318 | 13 |
| 3 | unten links | 0 | 46 | 46 | 239 |
| 4 | unten rechts | 0 | 287 | 287 | 239 |

Ziel ist **320 × 240** mit `display.setRotation(1)`. Die feste Abbildung:

```text
x = min(rawY, 319)
y = 239 - min(rawX, 239)
```

Die Messwerte sind physische Berührungen, nicht mathematische Eckpixel. Die Koordinaten sprechen für diese Abbildung, aber **die gedrehte Firmware wurde noch nicht auf der Hardware geprüft**.

`include/TouchTransform.h` enthält die einfache, rein mathematische Transformation und den Treffer-Test. Der neue Test fordert auf dem Display nacheinander **oben links, oben rechts, unten links, unten rechts, Mitte**. Ein Tastendruck zählt nur einmal, bis der Finger abgehoben wird. Treffer sind im Umkreis von 40 Pixeln um die Markierung gültig. Die mit einer anderen Hardwareausrichtung verwendbare Touch-Transformation wird nicht stillschweigend geändert.

Erwartete Monitor-Ausgabe am Schluss:

```text
TFT: 320x240; target landscape 320x240
TARGET 1/5 PASS (TOP LEFT)
TARGET 2/5 PASS (TOP RIGHT)
TARGET 3/5 PASS (BOTTOM LEFT)
TARGET 4/5 PASS (BOTTOM RIGHT)
TARGET 5/5 PASS (CENTRE)
HARDWARE TOUCH TEST PASSED (5/5)
```

Bei einem falschen Treffer erscheint `TARGET ... MISS`. Wenn die Punkte nicht unter dem Finger liegen, zuerst Displayausrichtung und die tatsächlichen Berührungspositionen dokumentieren; keine blind erratenen Offsets einführen.

## Pi: separater Arbeitsordner, ohne lokale Änderungen zu verlieren

Das ursprüngliche Pi-Projekt hat lokal geänderte Dateien. Dieser Test muss **in einem separaten Git-Worktree** stattfinden; keinen Force-Pull, Reset oder unkontrolliertes Cherry-Pick im bisherigen Arbeitsordner ausführen.

```bash
cd ~/Projekte/Freenove-DevBoard-Companion
git status --short
git fetch origin feature/fnk0086-landscape-touch
git worktree add --detach ../Freenove-DevBoard-Companion-touch-test \
  origin/feature/fnk0086-landscape-touch

# CFG1 wurde lokal aus den freigegebenen Freenove-Dateien importiert.
# Wird vom Repository bewusst nicht versioniert.
test -f include/VerifiedDisplaySetup.h
cp include/VerifiedDisplaySetup.h \
  ../Freenove-DevBoard-Companion-touch-test/include/VerifiedDisplaySetup.h

cd ../Freenove-DevBoard-Companion-touch-test
python3 -m unittest discover -s tests -v
pio run -e fnk0086
```

**Stopp bei einem Fehler**. Niemals direkt mit `git reset --hard` oder `git clean -fd` versuchen, lokale Änderungen zu reparieren. Der Original-Checkout und seine lokalen Verbesserungen bleiben in Ruhe.

Für einen etwaigen Hardware-Upload `pio device list` und `ls -l /dev/serial/by-id` erneut prüfen. Auf dem Pi wurde zuletzt `/dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00` gemeldet. Nur verwenden, **wenn dieser Anschluss weiterhin das tatsächlich angeschlossene FNK0086 ist**.

```bash
# Nur nach eindeutigem Portabgleich und erfolgreichem Build:
pio run -e fnk0086 -t upload --upload-port \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00

pio device monitor -p \
  /dev/serial/by-id/usb-1a86_USB_Single_Serial_58FC003021-if00 \
  -b 115200
```

Nach dem Test den seriellen Monitor mit `Ctrl+C` verlassen. Erst wenn 5/5 Treffer und stabile Heap-/PSRAM-Werte vorliegen, den Entwurf für die LVGL-Oberfläche angehen. Dieser Pull Request bleibt bis zur tatsächlichen Hardwareabnahme im Entwurfszustand.

## Nach Abnahme: Änderungen integrieren

Die lokalen Änderungen aus dem ursprünglichen Checkout sind weiterhin nicht in GitHub. Zuerst den Diff der sechs Dateien prüfen und separat committen oder anderweitig sichern. Anschließend die Feature-Änderungen mit normalen Git-Merge-Werkzeugen konfliktbewusst integrieren. **Nicht** die aus GitHub stammende alte `src/main.cpp` blind über den lokalen, weiterentwickelten Code kopieren.
