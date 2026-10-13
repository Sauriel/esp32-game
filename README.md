# ESP32 Gameduino 2 Game

Ein kleines 2D-Spielprojekt für einen **ESP32** mit **Gameduino 2 (FT800)** und einem **PlayStation-4-Controller** über Bluetooth. Das Projekt ist noch ein Prototyp: Die Testszene enthält zwei unabhängig steuerbare Blöcke und eine Anzeige für Tastendrücke.

Entwickelt wird mit **C++**, **PlatformIO**, **ESP-IDF**, **Arduino als ESP-IDF-Komponente** und **Bluepad32**.

## Aktueller Stand

- Gameduino-2-Display über SPI ansteuern (480 × 272 Pixel)
- DualShock-4-Controller über Bluepad32 verbinden und Eingaben auslesen
- Zwei Blöcke mit linkem und rechtem Analogstick bewegen und innerhalb des Spielbereichs halten
- Tastendrücke oben mittig für eine Sekunde anzeigen (z. B. `X (Cross) Pressed`)
- Spiellogik, Eingabeverarbeitung und Displayinitialisierung in getrennten Modulen
- Zeitbasierte Bewegung mit Delta-Time und einer Zielrate von maximal 60 Frames/s
- FPS-Anzeige oben rechts, gemessen über vollständige Frameintervalle und einmal pro Sekunde aktualisiert

**Noch nicht umgesetzt:** eigentliche Spielmechaniken, Sprites, Karten oder Audio. Die neue Hauptschleife muss noch auf der Hardware gemessen werden.

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

### Fehlende Zertifikats-Assembly beim Build

Die Arduino-Komponentenabhängigkeiten binden unter anderem ESP Insights und
RainMaker ein. Deren Zertifikate werden von ESP-IDF über `target_add_binary_data()`
in Assembly-Dateien umgewandelt. PlatformIO 54.03.21 übernimmt diese generierten
Quellen, führt deren CMake-Erzeugungsregeln aber nicht automatisch aus. Dadurch kann
der Build mit `Source .../https_server.crt.S not found` abbrechen; der doppelte
`.pio/build/esp32dev`-Pfad betrifft den daraus abgeleiteten Objektpfad.

Der Projekt-Hook in `cmake/EmbedData.cmake` erzeugt bereits vorhandene einzubettende
Dateien zusätzlich bei der CMake-Konfiguration mit dem originalen ESP-IDF-Konverter.
Die ursprünglichen Buildregeln und Symbolnamen bleiben erhalten; Fremdkomponenten
werden nicht geändert. Generierte Eingaben wie das mbedTLS-Zertifikatsbündel behalten
ihren eigenen Erzeugungsweg. `main` deklariert außerdem `gameduino2` ausdrücklich
als Abhängigkeit, damit dessen Header und Bibliothek korrekt eingebunden werden.

Nach Übernahme der Korrektur bei Bedarf die Build-Artefakte neu erzeugen:

```bash
pio run -e esp32dev -t clean
pio run -e esp32dev
```

Prüfstand der Korrektur: CMake-Konfiguration erfolgreich; die vier Assemblydateien
für Insights/RainMaker wurden erzeugt und direkt mit der ESP32-Toolchain kompiliert.
Ein früherer Build-Versuch des Agenten scheiterte am separaten
SCons-Installationsfehler `No module named 'SCons.Tool.FortranCommon'`.
Laut Benutzer funktioniert der Build über den VSCode-Build-Button ohne Probleme.
Weitere Builds führt der Benutzer in VSCode aus.

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
│   ├── timing/             # Delta-Time, FPS-Messung und Framebegrenzung
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
3. Den orangefarbenen Block mit dem **linken Analogstick** bewegen.
4. Den blauen Block mit dem **rechten Analogstick** bewegen.
5. Eine Taste drücken: Oben mittig erscheint ihr Name mit `Pressed` für eine Sekunde.
   Halten verlängert die Anzeige nicht; ein neuer Tastendruck ersetzt sie. Bei mehreren
   gleichzeitigen Tastendrücken wird der erste Eintrag aus der Tabelle in `Game.cpp` angezeigt.

Die Blöcke sind jeweils 20 × 20 Pixel groß und starten getrennt voneinander.
Die oberen 32 Pixel bleiben für den Text frei; beide Blöcke werden darunter an den
Bildschirmrändern begrenzt. Ohne Controller bleiben sie stehen und die Anzeige lautet
`Controller disconnected`. Die Szene verwendet FT800-Rechtecke und die eingebaute
Schrift, ohne neue Bitmap-Assets oder RAM_G-Belegung. Der zusätzliche statische
Spielzustand und die feste Tabelle benötigen keine Heap-Allokationen. Darstellung,
Bewegung beider Sticks und alle Tasten müssen noch auf der Hardware geprüft werden.
Ein temporärer Host-Test mit Display-Testdouble prüfte unabhängige Bewegung,
Randbegrenzung, Anzeigedauer (auch beim Uhrüberlauf) und Disconnect erfolgreich.
`Game.cpp` wurde direkt mit der ESP32-Toolchain und dem zusätzlichen Include-Pfad
zur projektlokalen Gameduino-Bibliothek kompiliert. Der damalige vollständige
Build-Versuch des Agenten scheiterte am oben beschriebenen SCons-Fehler;
der Benutzer bestätigt inzwischen einen funktionierenden VSCode-Build.

Die Darstellung verwendet aktuell eine einfache Testgrafik. Die Hauptschleife misst
mit `micros()` die Zeit zwischen Frame-Anfängen, einschließlich Render- und Wartezeit.
`Game::update()` erhält Sekunden; beide Blöcke bewegen sich bei voller Auslenkung
mit 80 Pixeln/s pro Achse. Die alte zusätzliche Multiplikation mit 8 Pixeln pro
Update wurde entfernt. Das entspricht ungefähr dem Tempo der früheren Schleife
mit `delay(100)`; der Parameter `moveSpeedPixelsPerSecond` in `Game.cpp` legt das
gewünschte Tempo unabhängig von der Framerate fest.

Die Schleife wartet nur die verbleibende Framezeit, auf ganze Millisekunden
aufgerundet, und gibt bei überschrittener Framezeit mit `delay(1)` anderen Tasks
Rechenzeit. 60 FPS sind ein Zielwert, keine gemessene Bildrate. Delta-Time wird
bei 50 ms begrenzt, um große Positionssprünge nach Hängern zu vermeiden. Bei
dauerhaft längeren Frames geht dadurch Bewegungszeit verloren; die Begrenzung
ist kein Ersatz für das Beheben langsamer Frames. Ein Fixed-Timestep ist noch
nicht umgesetzt.

Delta-Time, Framebegrenzung und FPS-Messung liegen in `main/timing/FrameTiming.cpp`.
`main.cpp` koordiniert nur Eingabe, Spielupdate, Rendering und Timing. Die bisherige
periodische serielle Schleifen-/Heap-Diagnose wurde entfernt.

Oben rechts zeigt `FPS: 59.8` beispielsweise die gemessene Spiel-Framerate mit einer
Nachkommastelle. Vor dem ersten vollständigen Messfenster steht dort `FPS: --`.
Die Berechnung teilt die Anzahl vollständiger Frameintervalle durch deren tatsächlich
vergangene Zeit (mindestens eine Sekunde pro Messfenster), einschließlich Eingabe,
Rendering, FIFO-Wartezeit und Framebegrenzung. Das 50-ms-Limit gilt nur für die
Bewegung und verfälscht die FPS-Messung nicht. Die Anzeige misst die Rate neu
bereitgestellter Spielbilder, nicht die elektrische Bildwiederholrate des Displays.

`GD.swap()` übergibt Kommandos, ohne auf deren Abarbeitung zu warten. Deshalb folgt
in `Game::render()` weiterhin `GD.finish()`, damit keine Warteschlange alter Bilder
eine sichtbare Eingabeverzögerung aufbaut. Diese Wartezeit ist im nächsten
Frameintervall enthalten. Die Anzeige nutzt die eingebaute FT800-Schrift und
benötigt keine zusätzlichen RAM_G-Assets oder Heap-Allokationen. Das Timing hält
wenige feste Zähler; der Textpuffer belegt 20 Byte Stack
und ergänzt einen Textbefehl pro Frame in der weiterhin kleinen Testszene.
Layout, FPS mit/ohne Controller, Verhalten bei langsamen Frames und
Disconnect/Reconnect sind noch auf echter Hardware zu prüfen.

Prüfstand der FPS-Erweiterung: Ein temporärer Host-Test prüfte die Messung bei
50 und 10 FPS, das unabhängige 50-ms-Delta-Time-Limit, den `micros()`-Überlauf und
die Wartezeit erfolgreich. `main.cpp`, `Game.cpp` und `FrameTiming.cpp` wurden
mit dem installierten ESP32-Compiler und den Projekt-Headern erfolgreich kompiliert.
Der vollständige `pio run -e esp32dev` scheiterte weiterhin im Build-Werkzeug an
`No module named 'SCons.Tool.FortranCommon'`; ein vollständiger Firmware-Build
und die Hardwareprüfung der Anzeige stehen aus.

Prüfstand der Delta-Time-Korrektur: Ein temporärer Host-Test prüft gleiche
Bewegungsstrecken bei 30/60 Updates/s und wechselnden Zeitschritten sowie
unabhängige Sticks, Randbegrenzung und Disconnect. `main.cpp` und `Game.cpp`
wurden vor der Ergänzung von `GD.finish()` direkt mit der vorhandenen ESP32-Toolchain
erfolgreich kompiliert. Der Benutzer bestätigt einen funktionierenden VSCode-Build.
Die anschließende FIFO-Synchronisierung wurde anhand des lokalen Treibers geprüft;
ein erneuter Build durch den Agenten wurde auf Benutzerwunsch nicht ausgeführt.
Auf der Hardware sind insbesondere die gemeldete Eingabeverzögerung von 1–2 Sekunden,
Bewegung bei kleinen Stickauslenkungen, FPS mit/ohne Controller, alle Tasten
sowie Disconnect/Reconnect zu prüfen.

### Controller-Snapshot

`GameInput::update()` fragt Bluepad32 ab und erstellt einen Eingabe-Snapshot.
`getInput()` gibt diesen unverändert zurück, auch bei mehrfachem Lesen im selben Update.
Der erste verbundene Gamepad-Slot liefert die Eingaben; `connected` zeigt dessen Verbindungsstatus.

| Felder in `InputState` | Eingabe |
| --- | --- |
| `moveX`, `moveY` | Linker Stick |
| `lookX`, `lookY` | Rechter Stick |
| `leftTrigger`, `rightTrigger` | Analoge L2-/R2-Werte von 0 bis 1 |
| `cross`, `circle`, `square`, `triangle` | Kreuz, Kreis, Quadrat, Dreieck |
| `dpadUp`, `dpadDown`, `dpadLeft`, `dpadRight` | Steuerkreuz, einschließlich gleichzeitig gedrückter Richtungen |
| `l1`, `r1`, `l2`, `r2`, `l3`, `r3` | Schulter-, Trigger- und Stick-Tasten |
| `share`, `options`, `ps` | Share, Options, PS |

Jede digitale Taste hat `held` (gehalten), `pressed` (gerade gedrückt) und
`released` (gerade losgelassen). Beispielsweise kann `input.cross.pressed` eine
Interaktion einmal auslösen. Die weiteren Spielaktionen sind noch nicht angebunden.
Das Steuerkreuz wird separat bereitgestellt und bewegt die Blöcke nicht.
Das bisherige Kompatibilitätsfeld `action` wurde entfernt; Kreuz wird über `cross` gelesen.

Beide Sticks werden auf den Bereich -1 bis 1 begrenzt, mit einer pro Achse
angewandten Deadzone von 12 % und linearer Neuskalierung außerhalb davon;
positive Y-Werte zeigen nach unten. Die Deadzone ist ein vorläufiger Parameter.
Die Wertebereiche und Tasten-Zuordnungen wurden im lokalen Bluepad32-DS4-Parser
geprüft: Sticks `(raw - 127) * 4` (-508 bis 512), Trigger `raw * 4` (0 bis 1020).
Die negative Stick-Endlage liegt durch die symmetrische Division durch 512 leicht über -1.
Eine Kalibrierung am echten Controller steht noch aus.

Bei Verbindungsverlust werden Achsen und gehaltene Tasten neutral; zuvor gehaltene
Tasten liefern einmal `released`. Beim Wechsel zum anderen Gamepad beginnt dessen
Tastenzustand neu. Der Touchpad-Klick wird vom installierten DS4-Parser nicht als
Gamepad-Taste ausgegeben; Touch-Koordinaten gehören zum hier deaktivierten virtuellen
Mausgerät. PS kann zusätzlich vom Bluetooth-Backend behandelt werden.

Die Erweiterung verwendet feste Strukturen ohne neue Heap-Allokationen oder
Grafikspeicher. Hardwaretests für alle Tasten, Stick-Deadzone, Trigger-Endlagen sowie
Disconnect/Reconnect stehen noch aus. Eingaben werden einmal pro Frame abgefragt;
sehr kurze Tastendrücke zwischen zwei Abfragen können weiterhin verpasst werden.

Prüfstand dieser Erweiterung: Ein temporärer Host-Test mit Bluepad32-Testdouble
prüfte Tastenmapping, Flanken, Snapshot, Wertebegrenzung und Controllerwechsel
erfolgreich. `Controller.cpp` ließ sich mit der ESP32-Toolchain und den tatsächlichen
Projekt-Headern kompilieren. Der vollständige `pio run -e esp32dev` scheiterte im
Build-Werkzeug an `No module named 'SCons.Tool.FortranCommon'`. Der Benutzer
bestätigt inzwischen einen funktionierenden VSCode-Build; die vollständigen
Controller-Hardwaretests stehen weiterhin aus.

## Geplante nächste Schritte

- Tatsächliche Framerate und Eingabereaktion der Delta-Time-Schleife auf der Hardware messen
- Spielfigur und Sprite-Darstellung
- Tilemap, Kollisionen und grundlegende 2D-Spielmechaniken
- Weitere Controller-Aktionen

## Hinweise

Dieses Repository ist ein Lern- und Entwicklungsprojekt, kein fertiges Spiel. Es nutzt Drittanbieter-Komponenten mit eigenen Lizenzen; Details stehen in den jeweiligen Projekten. Für dieses Projekt gilt die Lizenz in [`LICENSE`](LICENSE).
