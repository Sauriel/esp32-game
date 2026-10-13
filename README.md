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

Der SD-CS wird beim Start auf **HIGH** gesetzt, damit die SD-Karte den SPI-Bus nicht stört. Im Transfer-Modus wird die Karte jetzt über Arduino-SD/FAT auf demselben SPI-Bus eingebunden; der Hardwaretest steht aus.

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

## Dateiübertragung über WLAN (neu, Hardwareprüfung ausstehend)

Die Firmware besitzt zwei getrennte Startpfade. Im normalen **Game-Modus** bleibt
WLAN aus und die Controller-Demo läuft wie bisher. **Transfer** startet Arduino direkt
über den vorhandenen `arduino_bootstrap()`, ohne `btstack_init()`, `uni_init()` oder
GameInput/Game aufzurufen. Die Bootentscheidung fällt bereits in `main.c`.
`CONFIG_AUTOSTART_ARDUINO` muss weiterhin deaktiviert bleiben.

### WLAN konfigurieren und bedienen

1. `main/config/WifiCredentials.example.h` nach `main/config/WifiCredentials.h`
   kopieren und dort `ssid` und `password` für ein vorhandenes 2,4-GHz-WLAN eintragen.
   Die lokale Datei ist git-ignoriert; Passwörter werden nicht geloggt.
2. Die Firmware selbst in VSCode/PlatformIO bauen und hochladen. Ohne lokale
   Konfiguration bleibt sie baubar; Transfer zeigt „WLAN nicht konfiguriert“.
3. Im Spiel **KEY (GPIO0) drücken und loslassen**. Erst nach entprelltem Loslassen
   wird Transfer angefordert und ein kontrollierter Neustart ausgeführt.
4. Die auf dem Display/seriellen Monitor angegebene `http://<IP>/`-Adresse im Browser
   eines Geräts im selben WLAN öffnen. Ordner sind navigierbar; Dateien lassen sich
   hochladen und herunterladen. `Ordner erstellen` legt einen Unterordner an;
   die Einträge besitzen Buttons zum Umbenennen, Verschieben und Löschen.
5. Mit KEY oder **Spiel starten** im Browser zur Demo zurückkehren.

KEY niemals beim Reset gedrückt halten: GPIO0 bestimmt den Bootmodus des ESP32.
Ein bereits beim Programmstart gedrückter KEY muss erst losgelassen werden; dieses
Loslassen wechselt den Modus nicht. Entprellung: 30 ms, ohne Interrupt/Warteschleife.
Vor einem angeforderten Neustart wird KEY erneut auf stabiles HIGH geprüft.

`BootMode` speichert ein versioniertes einmaliges Ticket im NVS-Namespace
`boot_mode`. Beim Boot wird es gelesen und vor Verwendung gelöscht/committet.
Nur ein gültiges Ticket nach einem Software-Neustart aktiviert den Zielmodus;
Power-on, Reset-Taste, Watchdog oder ein späterer Reset ohne Ticket starten Game.
Fehler beim Speichern/Löschen werden geloggt und angezeigt; ohne erfolgreiches
Speichern gibt es keinen automatischen Neustart. NVS wird nicht pro Frame geschrieben.
Bei fehlerhafter **NVS-Initialisierung** stoppt der Start mit serieller Fehlermeldung,
bevor Arduino/Bluepad32 ihre vorhandene automatische NVS-Formatierung erreichen.
Dann starten auch Display/Demo nicht; die Partition bleibt für Diagnose erhalten.
Es wird keine zusätzliche Firmware-/App-Partition angelegt.

### IRAM beim gemeinsamen Game-/Transfer-Image

Beim ersten Benutzer-Build mit WLAN meldete der Linker einen Überlauf von
`.iram0.text` um **11.448 Byte**. Beide Modi sind in derselben Firmware gebunden;
die Startverzweigung und spätere Bluetooth-Heapfreigabe verkleinern die beim Linken
benötigte IRAM-Belegung nicht.

`CONFIG_ESP_WIFI_IRAM_OPT` und `CONFIG_ESP_WIFI_RX_IRAM_OPT` sind deshalb in
`sdkconfig.defaults` und `sdkconfig.esp32dev` deaktiviert. Die lokalen ESP-IDF-5.4.2-
Linkerregeln verschieben die damit optional in IRAM abgelegten WLAN-Funktionen
in den Flash. Das kann den WLAN-Durchsatz reduzieren; Upload/Download und
Displayreaktion sind erneut auf Hardware zu messen. Die Änderung lässt die
Bluetooth-Konfiguration und Compileroptimierung unverändert.

Der Agent hat dafür keinen Build gestartet. Nach erneutem Build durch den Benutzer
die tatsächliche IRAM-Reserve, Flashgröße und App-Partitionsgrenze prüfen.
Falls PlatformIO die bisher generierte Konfiguration weiterverwendet, im Benutzer-
Build zunächst Clean ausführen und anschließend neu bauen. Generierte Dateien unter
`.pio/` werden nicht von Hand geändert.

### App-Partition und Bootschleife nach Upload

Ein Benutzer-Upload scheiterte vor dem Anwendungsstart: Das Firmware-Image war
1.436.608 Byte groß, die tatsächlich geflashte `factory`-Partition nur 1.048.576 Byte.
Der Bootloader meldete `Image length ... doesn't fit in partition length ...`
und startete wiederholt neu. Das ist ein Partitionsfehler, kein KEY-/NVS-Moduswechsel.

Die SDK-Konfiguration wählt bereits `partitions_singleapp_large_coredump.csv`.
Der PlatformIO-Builder verwendet für seine erzeugte und hochgeladene Tabelle
jedoch unabhängig davon standardmäßig `partitions_singleapp.csv`. Deshalb steht
in `platformio.ini` jetzt ausdrücklich:

```ini
board_build.partitions = partitions_singleapp_large_coredump.csv
```

Damit verwenden beide Wege die vorhandene ESP-IDF-Tabelle: NVS bei `0x9000`
mit `0x6000` Byte, PHY bei `0xf000` mit `0x1000` Byte, eine einzige Factory-App
bei `0x10000` mit **1500 KiB (1.536.000 Byte)** und anschließend 64 KiB Core-Dump.
Das gemeldete Image lässt darin rechnerisch 99.392 Byte Reserve; die Größe jedes
neuen Builds muss weiterhin geprüft werden. Die Tabelle bleibt innerhalb der
vorhandenen 4 MB Flash. Es gibt keine OTA- oder zweite App-Partition.

Zur Übernahme selbst **Clean, Build und den vollständigen normalen Upload** ausführen,
damit neben der Firmware auch `partitions.bin` bei `0x8000` neu geflasht wird:

```bash
pio run -e esp32dev -t clean
pio run -e esp32dev
pio run -e esp32dev -t upload
```

Ein vollständiges Flash-Erase ist hierfür nicht erforderlich. Danach muss der
Bootlog für `factory` die Länge `00177000` statt `00100000` zeigen.
Der Agent hat die vorhandene CSV und die PlatformIO-Auswahl geprüft, aber keinen
Build oder Upload ausgeführt; der erneute Hardwarestart ist noch zu bestätigen.

### Aufbau und Ressourcen

- `config/`: gemeinsame Pinbelegung und lokale WLAN-Konfiguration.
- `input/ModeButton`: GPIO-Polling und separat prüfbare Entprellung.
- `app/BootMode`, `app/ModeSwitch`: Bootentscheidung, NVS und Wechselkoordination.
- `storage/SdStorage`: Arduino-SD-Mount `/sd`, POSIX-Dateioperationen und SPI-Sperre.
- `network/WifiConnection`: Station-Modus, IP und maximal drei Verbindungsversuche
  pro Verbindungsphase (je 10 s, dazwischen 5 s Backoff; nach Verbindungsverlust erneut).
- `transfer/`: HTTP, ein Datei-Worker, Statusdarstellung und eingebettete Browserseite.

GD2 besitzt einen eigenen einfachen SD-Leser (`GD.SD`). `GD.begin(0, …)` aktiviert
`GD_STORAGE` nicht. Dieser Leser wird nicht zum FAT-Schreiben verwendet.
Arduino-SD 3.2.1 nutzt die bereits initialisierte globale `SPI`-Instanz und eigene
SPI-Transaktionen; `SPI.begin()` kehrt bei vorhandenem Bus unverändert zurück.
Eine gemeinsame Mutex schützt jede SD-Operation sowie die **gesamte** Displayliste
inklusive `GD.swap()`/`GD.finish()`. Vor SD-Mount und nach jedem Transfer-Bild wird
GD2 mit `GD.__end()` deselektiert; das nächste Bild öffnet den Stream mit `GD.resume()`
innerhalb einer SPI-Transaktion (1 MHz). SD verwendet 4 MHz, ohne neuen SPI-Treiber.
Während Dateiübertragungen wird die Bus-Sperre nach jedem Stück freigegeben;
Displayupdates erfolgen höchstens alle 100 ms und überspringen bei belegtem Bus ein Bild.
Die Game-Darstellung verwendet weiterhin ihren bisherigen Treiberpfad.
Die Command-Stream-Aufrufe liegen hinter der schlanken Display-Schnittstelle:
`SdStorage.cpp` bindet `GD2.h` nicht ein, weil dessen eigene `struct dirent` mit
dem POSIX-Verzeichnisheader kollidiert. Die SPI-Sperre bleibt beim Storage-Modul.

HTTP läuft mit zwei Client-Sockets, einem Server-Task und genau einem Datei-Worker.
Der Worker ermöglicht die Browser-Wechselanforderung während einer laufenden Datei;
zusätzliche Dateioperationen erhalten HTTP 503. Beide Tasks haben je 8192 Byte Stack,
der einzige statische Streaming-Puffer ist 4096 Byte. Hinzu kommen begrenzte Pfad-/JSON-
Stackpuffer, FAT-LFN-Arbeitsbereich, WLAN/TCP-/HTTP-Speicher und Treiberstrukturen.
Der 4-KiB-Puffer ist auch im Game-Build statisch gebunden. Es gibt keine dateigroßen
RAM-Puffer und keine neuen Bitmap-Assets: **zusätzliche RAM_G-Belegung 0 Byte**.
Die kleine Transfer-Displayliste nutzt FT800-ROM-Schriften; ihre tatsächliche Größe
und Framezeiten sind auf Hardware noch zu messen. Es wird kein PSRAM vorausgesetzt.

Im Transfer-Start wird `esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)` nur im
Controllerstatus `IDLE` aufgerufen und der Fehlercode geprüft. Die Freigabe gilt bis
zum Neustart und gibt keinen statisch gebundenen BTstack-Speicher frei. Heapdiagnosen
vor/nach relevanten Phasen melden freien internen Heap, größten Block, interne
Low-Watermark sowie freien Gesamt-Heap und dessen Minimum. Tatsächliche Werte,
Firmwaregröße, statischer RAM und die Grenze der App-Partition sind noch zu messen.
Die APIs wurden gegen die lokale Integration und die
[ESP-IDF-5.4.2-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32/api-reference/bluetooth/controller_vhci.html)
sowie den [Arduino-SD-3.2.1-Quellcode](https://github.com/espressif/arduino-esp32/blob/3.2.1/libraries/SD/src/SD.cpp)
geprüft; die Toolchain und Fremdkomponenten bleiben unverändert.

### Ordner und Dateien verwalten

Die einfache Oberfläche verwendet Eingabedialoge und aktualisiert nach jeder
erfolgreichen Aktion die Dateiliste. `Ordner erstellen` fragt einen einzelnen Namen
im aktuellen Ordner ab. `Umbenennen` behält den übergeordneten Ordner bei;
`Verschieben` fragt einen vorhandenen absoluten SD-Zielordner (z. B. `/assets`) ab
und behält den Namen bei. Auch gefüllte Ordner können verschoben/umbenannt werden:
FAT verschiebt ihren Verzeichniseintrag, ohne die enthaltenen Dateien zu kopieren.
Zielnamen werden niemals überschrieben; auch reine Änderungen der Großschreibung
werden zunächst als Namenskonflikt abgelehnt.

`Löschen` benötigt eine Browserbestätigung und entfernt eine Datei oder einen
**leeren Ordner**. Gefüllte Ordner erhalten einen klaren Konfliktfehler; deren Inhalt
muss zuerst gelöscht werden. Es gibt keinen Papierkorb oder rekursives Löschen.
Die SD-Wurzel und reservierte Upload-Temp-Pfade können nicht geändert werden.
Ein Ordner darf nicht in sich selbst oder einen eigenen Unterordner verschoben werden.
Bestehende Pfade werden anhand der tatsächlichen langen FAT-Namen mit FAT-Unicode-
Großschreibvergleich aufgelöst; DOS-Kurznamensaliases werden für diese Aktionen
nicht akzeptiert. Damit umgehen alternative Schreibweisen den Unterordnerschutz nicht.

Die Aktionen werden als kleines URL-kodiertes Formular an `POST /api/action`
gesendet (maximal 1535 Byte Body; Quelle/Ziel je maximal 239 UTF-8-Bytes).
Alle laufen im vorhandenen Datei-Worker, mit denselben SPI-Sperren und der
Server-Benachrichtigung nach Async-Abschluss. Während Upload oder einer anderen
Dateiaktion erhalten weitere Operationen HTTP 503; ein angeforderter Moduswechsel
stoppt die Annahme neuer Aktionen. `FileActions` enthält die Regeln,
`SdStorage` ergänzt nur `mkdir`/`rmdir` und das bestehende Rename.
Es gibt keinen zusätzlichen Datei-/Listenpuffer oder zusätzlichen Task.

Der Logtag `FileAction` meldet Aktion, Quelle/Ziel, Ergebnis und bei Datei-I/O-
Fehlern `errno`. Unzulässige Requests und HTTP-Konflikte werden ebenfalls geloggt;
es gibt keine Logzeile je Verzeichniseintrag. Der Server meldet jetzt
`diagnostics=file-manager-1`. Nach Benutzer-Build/Upload die Seite neu laden.
Die isolierten Tests `python3 tests/run_file_action_tests.py` prüfen den echten
Aktionscode auf einem temporären Host-Dateisystem mit Storage-/FAT-Testdoubles:
Erstellen, Löschen, Umbenennen, gefüllte Ordner verschieben, Zielkonflikte,
Wurzelschutz, eigene Unterordner (auch andere Schreibweise/Unicode), Kurznamens-
aliases sowie Rename-Fehler mit Erhalt der Quelldatei. Die Tests und JavaScript-
Syntaxprüfung bestanden; ein Firmware-Build wurde nicht ausgeführt.
Die neuen Funktionen auf SD/FAT, Unicode-Namen, Wiederholung auf derselben
Browserverbindung und KEY/Browser-Moduswechsel sind noch auf Hardware zu prüfen.

### Upload und Download

Der Server verwendet Binär-POST, keine Multipart-Puffer. Downloads und JSON-Listen
werden in begrenzten Stücken gesendet. Uploads überschreiben keine Dateien (HTTP 409).
Eine reservierte `.transfer-XXXXXXXX.part` im Zielordner wird exklusiv angelegt,
vollständig beschrieben, mit `fsync()`/`close()` geprüft und erst dann umbenannt.
Bei Abbruch wird sie entfernt; scheitert das, erscheint ein serieller Fehler.
Stromausfallreste bleiben reserviert, sind weder sichtbar noch per HTTP zugänglich
und können bei Bedarf am PC entfernt werden. Es wird keine Stromausfallsicherheit
von FAT zugesichert. Die Karte wird niemals automatisch formatiert.

Pfade werden einmal URL-dekodiert und auf gültiges UTF-8, Traversal, NUL/Steuerzeichen,
Backslash, FAT-Sonderzeichen, abschließende Punkte/Leerzeichen und reservierte
Temp-Namen geprüft. Maximal 239 UTF-8-Bytes je Pfad und 127 je Segment; für temporäre
Namen muss im Zielordner zusätzlich Platz bleiben. JSON escapt Namen, die Browserseite
verwendet `textContent`. Lange FAT-Namen (127 Zeichen) mit UTF-8 werden in
`sdkconfig.defaults` **und** der bestehenden `sdkconfig.esp32dev` aktiviert;
das HTTP-URI-Limit steigt für URL-kodierte Pfade auf 768 Byte.

Socket-Sende-/Empfangswartezeit ist 2 s; eine Dateioperation hat ein Gesamtlimit von
10 Minuten. Bei Wechsel werden neue Dateioperationen abgelehnt. Eine laufende
Operation darf bis zu 15 s weiterlaufen und wird danach zwischen den Datenstücken
abgebrochen (zuzüglich einer laufenden Socket-/SD-Operation). Der Browser erhält
zunächst seine Antwort. Der Arduino-Task stoppt anschließend HTTP/Worker, unmountet
SD, schaltet WLAN ab und startet erst nach gespeichertem Ticket und erneut stabil
losgelassenem KEY neu. Der HTTP-Handler ruft weder Restart noch `httpd_stop()` auf.
SD-Treiberzugriffe und `GD.finish()` besitzen eigene Wartezeiten; ein defekter Bus
muss deshalb ausdrücklich auf Hardware getestet werden.
Die lokale HTTP-Oberfläche hat keine Anmeldung und keinen AP-/Portal-Fallback.

### Upload-Diagnose im seriellen Monitor

Beim Serverstart muss `HTTP ready: diagnostics=file-manager-1` mit Compilerzeit und
Seitengröße erscheinen. Ein Neuladen der Seite erzeugt `GET /` mit derselben Version;
der HTTP-Header `X-Transfer-Diagnostics` zeigt sie ebenfalls. Datei-Requests werden
schon beim Eintritt in den HTTP-Handler, vor/nach der Async-Übergabe und am Start/Ende
des Workers geloggt. So lässt sich ein fehlendes `Upload: Start` vor dem SD-Zugriff
eingrenzen.

Ein Benutzerlog zeigte eine erfolgreich abgeschlossene Verzeichnisliste, aber
keinen folgenden Upload-Request am HTTP-Handler. Im lokalen ESP-IDF-5.4.2-Code
nimmt der Server Async-Verbindungen aus der `select()`-Überwachung; deren
`httpd_req_async_handler_complete()` setzt nur den Async-Status zurück und weckt
den Server nicht auf. Bei Wiederverwendung derselben Verbindung konnte der nächste
Request deshalb ungelesen bleiben. Nach jedem Worker-Abschluss wird jetzt mit
`httpd_queue_work()` der HTTP-Task geweckt, damit er seine Deskriptoren erneuert.
Der Callback gibt zugleich die Dateioperation für den nächsten Request frei.
Scheitert das Einreihen, bleiben neue Dateioperationen gesperrt; der Arduino-Task
versucht die Benachrichtigung maximal einmal pro Sekunde erneut und loggt den Fehler.
Die Fremdkomponente bleibt unverändert. Im Log muss nach `Worker complete` nun
`Async completion processed on server; connection ready` erscheinen. Erneut
Auflisten und anschließend Upload auf derselben Browserverbindung testen; die
Der Benutzer hat den funktionierenden Upload nach dieser Korrektur bestätigt.
Kein Build durch den Agenten.

In einem vorhandenen Firmware-Artefakt standen bereits neue Upload-Logs neben altem
HTML (`Upload: 47 %`): PlatformIO hatte beim inkrementellen Build die eingebettete
Seite nicht erneuert. Der Pre-Build-Hook `scripts/embed_web.py` aktualisiert deshalb
vor dem Builder eine vorhandene `index.html.S` mit dem ESP-IDF-Konverter, wenn deren
Inhalt abweicht. Beim Clean-Build übernimmt weiterhin der CMake-Hook die Erzeugung.
Der Agent hat keinen Build gestartet; nach Benutzer-Build und Upload die Seite neu
laden (ggf. Strg+Shift+R). Der Fortschrittstext muss `Vom Browser gesendet` lauten.

Upload-Logs verwenden die Tags `Upload` und `SdStorage` beim bestehenden INFO-
Loglevel. Sie melden Start und erwartete Länge, validierten Zielpfad, Temp-Datei,
den ersten Empfang/Schreibvorgang, Abschluss mit `fsync`/`close`, Umbenennung,
HTTP-Antwort und Fehler mit Rückgabewert/`errno`. Dateiinhalte und WLAN-Zugangsdaten
werden nicht ausgegeben. Empfangene und erfolgreich geschriebene Bytes sind getrennt.

Der Arduino-Task meldet ungefähr einmal pro Sekunde `Progress` mit den Zählern,
der laufenden Phase und deren bisheriger Dauer (`phase_ms`). Dafür werden wenige
atomare Statusfelder verwendet, keine zusätzlichen Dateipuffer. Die Phase kann
`http-recv`, `sd-write`, `sync-close`, `rename` oder eine Vorbereitung/Cleanup sein.
Wartet eine SD-Operation auf die gemeinsame SPI-Sperre, protokolliert der Worker
zusätzlich einmal pro Sekunde `Waiting for SPI lock`. Die Sperre bleibt erforderlich;
die Diagnose ändert keine Upload-/Socket-Zeitlimits. Ein bereits blockierter
Arduino-Task kann selbst keine Fortschrittsmeldung ausgeben.

Die Browser-Prozentanzeige misst nur dessen gesendete Daten. Auch 100 % sind noch
keine bestätigte Speicherung; erst die erfolgreiche Serverantwort bestätigt diese.
Bei einem Hänger den seriellen Abschnitt von `Upload: Start` bis zur Fehlermeldung
bzw. mehrere Sekunden Stillstand erfassen. `sd-write` plus SPI-Wartewarnungen deutet
auf eine belegte Bus-Sperre, `sd-write` ohne diese Warnungen auf den laufenden
Schreib-/Treiberaufruf; `http-recv` zeigt einen laufenden Netzwerkempfang.
Die Logs dienen zur Eingrenzung; die oben beschriebene Korrektur der Async-
Benachrichtigung muss noch auf Hardware bestätigt werden. Kein Firmware-Build
durch den Agenten.

### Prüfstand und manuelle Abnahme

Auf Benutzerwunsch wurde **kein Firmware-Build, Upload oder Hardwaretest** ausgeführt.
Die isolierten Hosttests mit NVS-Testdouble prüfen den tatsächlichen Entprellungs-,
Pfad- und BootMode-Code: Prellen/Halten, Start mit gedrückter Taste, Uhrüberlauf,
Pfadgrenzen/Traversal/UTF-8, einmaliges Lesen/Entfernen, ungültige Tickets,
Power-on sowie Speicher-/Commitfehler. Ausführen mit `python3 tests/run_host_tests.py`
(erfordert g++; baut nur das Testprogramm in einem temporären Verzeichnis).
Alle elf Hosttests bestanden; außerdem bestanden die JavaScript-Syntaxprüfung mit
`node --check` und `git diff --check`. Die Tests ersetzen keine Prüfung von
NVS/SD/WLAN/HTTP auf dem ESP32.

Noch durchzuführen:

1. Firmware selbst bauen; Flash-/RAM-Bericht und tatsächliche App-Partition prüfen.
2. Normalstart: beide Sticks, Tastenanzeige, FPS und Reconnect; WLAN bleibt aus.
3. KEY kurz/lang/prellend betätigen; Start mit gehaltenem KEY und Reset ohne Ticket;
   wiederholte Wechsel dürfen keine Neustartschleife erzeugen.
4. Transfer: keine Bluetooth-Initialisierung, Heaplogs, WLAN-Verbindung und URL prüfen.
5. Datei deutlich größer als freier Heap hochladen/herunterladen und SHA-256 vergleichen;
   Unicode-Namen, Unterordner und leere Dateien testen.
6. Konfliktnamen und Traversal (z. B. `/../x`, `/%2e%2e/x`, `/%00x`) ablehnen;
   temporäre Dateien dürfen nicht als Assets erscheinen.
7. Fehlende/volle/unlesbare SD, falsche/fehlende WLAN-Konfiguration, Netzwerkverlust
   und Uploadabbruch: Fehlermeldungen, keine falsche Erfolgsmeldung, KEY bleibt bedienbar.
8. KEY **und Browser** während Upload/Download nutzen: Abschluss oder zeitlich begrenzter
   Abbruch, saubere Dateihandles und anschließend funktionierende Controller-Demo.
9. Display während SD-Übertragung auf SPI-Konflikte prüfen; ggf. FT800 REG_ID `0x7C`,
   tatsächliche Ladelaufzeiten und Heap-Minima dokumentieren.

## Geplante nächste Schritte

- Tatsächliche Framerate und Eingabereaktion der Delta-Time-Schleife auf der Hardware messen
- Spielfigur und Sprite-Darstellung
- Tilemap, Kollisionen und grundlegende 2D-Spielmechaniken
- Weitere Controller-Aktionen

## Hinweise

Dieses Repository ist ein Lern- und Entwicklungsprojekt, kein fertiges Spiel. Es nutzt Drittanbieter-Komponenten mit eigenen Lizenzen; Details stehen in den jeweiligen Projekten. Für dieses Projekt gilt die Lizenz in [`LICENSE`](LICENSE).
