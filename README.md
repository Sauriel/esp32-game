# ESP32 Gameduino 2 Game

Ein kleines 2D-Spielprojekt für einen **ESP32** mit **Gameduino 2 (FT800)** und einem **PlayStation-4-Controller** über Bluetooth. Das Projekt ist noch ein Prototyp: Aktuell lässt sich ein Rechteck mit dem linken Analogstick über den Bildschirm bewegen.

Entwickelt wird mit **C++**, **PlatformIO**, **ESP-IDF**, **Arduino als ESP-IDF-Komponente** und **Bluepad32**.

## Aktueller Stand

- Gameduino-2-Display über SPI ansteuern (480 × 272 Pixel)
- DualShock-4-Controller über Bluepad32 verbinden und Eingaben auslesen
- Rechteck mit dem linken Analogstick bewegen und innerhalb des Bildschirms halten
- Spiellogik, Eingabeverarbeitung und Displayinitialisierung in getrennten Modulen

**Noch nicht umgesetzt:** eigentliche Spielmechaniken, Sprites, Karten, Audio oder ein zeitbasiertes Update mit Delta-Time.

## Hardware

- ESP32 Dev Module (klassischer ESP32)
- Gameduino 2 mit FT800
- PlayStation-4-Controller (DualShock 4)
- Verbindungsleitungen und passende Stromversorgung

### Verkabelung

| Gameduino 2 | ESP32 | Funktion |
| --- | --- | --- |
| 5V | 5V | Versorgung |
| GND | GND | Gemeinsame Masse |
| D13 / SCK | GPIO18 | SPI-Takt |
| D12 / MISO | GPIO19 | SPI-Daten vom Display |
| D11 / MOSI | GPIO23 | SPI-Daten zum Display |
| D8 / GPU CS | GPIO5 | FT800-Chip-Select |
| D9 / SD CS | GPIO27 | SD-Karten-Chip-Select |

Der SD-CS wird beim Start auf **HIGH** gesetzt, damit die SD-Karte den SPI-Bus nicht stört. Die SD-Karten-Funktionen des Gameduino sind noch nicht implementiert.

**Achtung:** ESP32-GPIOs sind nicht 5-V-tolerant. Zusätzliche Signale des Shields nur anschließen, wenn deren Pegel zum ESP32 passen.

## Software und Abhängigkeiten

Die aktuell verwendete Kombination:

| Komponente | Version |
| --- | --- |
| ESP-IDF | 5.4.2 |
| Arduino-ESP32 | 3.2.1 |
| Bluepad32 | 4.2.0 |
| BTstack | 1.6.2 |
| PlatformIO-Plattform (pioarduino) | 54.03.21 |

`components/gameduino2/` und `components/bluepad32_arduino/` enthalten die für dieses Projekt verwendeten, angepassten Bibliotheken.

Die umfangreichen Fremdkomponenten `arduino`, `bluepad32`, `btstack`, `cmd_nvs` und `cmd_system` werden **nicht** im Git-Repository gespeichert. Das Script `setup-deps.sh` ist dafür vorgesehen, diese Komponenten nach einem Clone herunterzuladen, ohne vorhandene Verzeichnisse zu überschreiben.

## Einrichten und bauen

Voraussetzungen: **Git**, **PlatformIO Core** (`pio`) und eine funktionierende ESP32-Toolchain-Installation. Der erste Build lädt je nach Umgebung zusätzliche Werkzeuge herunter.

```bash
# Repository klonen und in das Projekt wechseln
# (Repository-URL von der GitHub-Projektseite verwenden)

# Externe ESP-IDF-Komponenten installieren
bash setup-deps.sh

# Firmware bauen
pio run -e esp32dev

# Firmware auf den ESP32 übertragen
pio run -e esp32dev -t upload

# Seriellen Monitor öffnen
pio device monitor -e esp32dev
```

**USB-Port beachten:** `platformio.ini` enthält derzeit einen gerätespezifischen `/dev/serial/by-id/...`-Pfad. Auf einem anderen Rechner muss dieser gegebenenfalls angepasst werden, beispielsweise auf `/dev/ttyUSB0`.

**Stand der Anleitung:** Der Build wurde mit den lokal installierten Abhängigkeiten erfolgreich getestet. Ein vollständiger Build aus einem frischen Clone mit `setup-deps.sh` muss noch verifiziert werden.

## Projektstruktur

```text
.
├── components/
│   ├── bluepad32_arduino/   # Angepasste Bluepad32-/Arduino-Integration
│   └── gameduino2/         # Angepasste Gameduino-2-Bibliothek
├── main/
│   ├── display/            # Initialisierung von SPI und Gameduino
│   ├── game/               # Spielzustand, Bewegung und Darstellung
│   ├── input/              # Bluetooth-Controller und Eingabestatus
│   ├── main.c              # Bluepad32-/ESP-IDF-Einstiegspunkt
│   └── main.cpp            # setup() und loop()
├── patches/                # Ergänzende Patches aus der Entwicklung
├── CMakeLists.txt
├── platformio.ini
├── sdkconfig.defaults
└── setup-deps.sh           # Installation externer Komponenten
```

Die zusätzlich installierten Fremdkomponenten erscheinen ebenfalls unter `components/`, sind aber durch `.gitignore` vom Repository ausgeschlossen.

## Bedienung

1. ESP32 mit dem Gameduino 2 verbinden und einschalten.
2. DualShock 4 per Bluetooth mit Bluepad32 koppeln.
3. Das orange Rechteck mit dem **linken Analogstick** bewegen.

Die Darstellung verwendet aktuell eine einfache Testgrafik und ein festes `delay(100)` in der Hauptschleife.

## Geplante nächste Schritte

- Hauptschleife mit geregelter Framerate und Delta-Time
- Spielfigur und Sprite-Darstellung
- Tilemap, Kollisionen und grundlegende 2D-Spielmechaniken
- Weitere Controller-Aktionen

## Hinweise

Dieses Repository ist ein Lern- und Entwicklungsprojekt, kein fertiges Spiel. Es nutzt Drittanbieter-Komponenten mit eigenen Lizenzen; Details stehen in den jeweiligen Projekten. Für dieses Projekt gilt die Lizenz in [`LICENSE`](LICENSE).