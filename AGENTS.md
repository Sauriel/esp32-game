# AGENTS.md — ESP32 / Gameduino 2 Dungeon Game

> Verbindliche Projektanweisungen für KI-Coding-Agenten. Diese Datei beschreibt die **tatsächliche Hardware**, den **bereits getesteten Projektstand** und die **technischen Grenzen** eines eigenständigen Zelda-artigen 2D-Dungeonspiels. **Vor jeder Änderung `README.md`, `platformio.ini`, `sdkconfig.defaults` sowie den relevanten Quellcode lesen.** Diese Datei ist keine Behauptung, dass geplante Features bereits funktionieren.

## 1. Ziel und Nichtziele

- Entwickle ein eigenständiges, lokal auf der Hardware laufendes **2D-Top-down-Dungeon-Action-Adventure im Stil klassischer Zelda-Spiele**: Erkundung, Spielfigur, Kollisionen, Gegner, Kämpfe, Interaktionen, Türen, Schlüssel, Rätsel, Inventar und später ggf. Speichern/Laden.
- **Kein RPG Maker als Laufzeitumgebung**: Nur die **Layout- und Maßkonventionen von RPG Maker MZ** sind Inspiration/Importformat für eigens angefertigte, entsprechend lizenzierte Grafiken. Das Spiel braucht weder RPG Maker MZ noch dessen Runtime oder offizielle Assets.
- Zielhardware bleibt der **vorhandene klassische ESP32**, nicht ein ESP32-S3/C3/C6 oder ein PC. Nicht stillschweigend Hardwarefeatures wie PSRAM, GPU-Shader, große Framebuffer oder mehr Flash annehmen.
- Native Firmware in **C++ mit ESP-IDF und Arduino als ESP-IDF-Komponente**, gebaut über **PlatformIO**. Bestehende, funktionierende Architektur behalten; keine Umstellung auf Arduino IDE, andere Grafikengine, Emulator, Web-Frontend oder andere Bluetooth-Bibliothek ohne konkrete Begründung und Rücksprache.
- **Lernprojekt**: Bevorzuge nachvollziehbare, kleine, schrittweise Änderungen, verständlichen Code und realistische Tests auf der echten Hardware.

## 2. Verifizierte Hardware und Grenzwerte

| Komponente | Festlegung / Grenze | Konsequenz |
| --- | --- | --- |
| MCU | HIMALAYA ESP32-ST / klassischer ESP32-D0WDQ6, ESP32 Dev Module | Xtensa LX6, 2 Kerne, max. 240 MHz; **kein ESP32-S3** |
| Interner RAM | **520 KiB SRAM insgesamt (Chip-Nennwert)** | **Nicht** komplett als Heap nutzbar: Firmware, IDF, Arduino, Bluetooth, Stacks und statische Daten brauchen Speicher. Tatsächlichen freien Heap und größten freien Block messen. |
| Interner ROM | 448 KiB | Nicht als frei beschreibbarer Laufzeit-RAM behandeln. |
| Firmware-Flash | **4 MB** (im bisherigen Build erkannt) | Programm/Partitionen/NVS teilen sich diesen Speicher; 4 MB sind **nicht** das maximale Firmware-Image. Partitionstabelle und `pio run` prüfen. |
| PSRAM | **Nicht nachgewiesen, standardmäßig als nicht vorhanden behandeln** | Niemals große Puffer in vermeintlichen externen RAM planen; erst Hardware und `heap_caps_get_total_size(MALLOC_CAP_SPIRAM)` prüfen. |
| Gameduino 2 | FT800 (Embedded Video Engine) | Eigener Grafikprozessor; Render-Kommandos über SPI statt PC-artigem Framebuffer. |
| Bildschirm | **480 × 272 Pixel** | Fixe Zielauflösung und UI-/Kamera-Basis. |
| FT800 Grafik-RAM | **256 KiB RAM_G** | Separater Speicher auf dem FT800 für Bitmap-/Grafikdaten und andere Ressourcen; **kein** ESP32-Heap. |
| FT800 Display-List-RAM | **8 KiB RAM_DL** | Begrenzte Zahl/Größe an Display-List-Instruktionen pro Frame. |
| FT800 Command-FIFO | **4 KiB RAM_CMD** | Co-Prozessor-Kommandos müssen korrekt gepuffert/abgearbeitet werden. |
| FT800 Palette | 1 KiB RAM_PAL | Nur bei passenden Palettenformaten relevant; nicht zu RAM_G addieren. |
| Externer Massenspeicher | **16-GB-MicroSD** am Gameduino 2 | Für Spielgrafiken, Maps und später ggf. Saves geeignet, **kein zusätzliches RAM**. Zugriff aktuell **noch nicht getestet/implementiert**. |
| Eingabegerät | **Sony DualShock 4 (PS4) über Bluetooth** | Vorhandene Bluepad32-Integration benutzen, Bluetooth-Verbindung und Reconnect robust behandeln. |
| Sonstiges | Wi-Fi am ESP32 vorhanden | Für den Offline-Spielbetrieb nicht erforderlich, nicht ohne Bedarf aktivieren. |

**Wichtig:** Die 520 KiB MCU-RAM, 256 KiB FT800-RAM und 16 GB SD sind **drei verschiedene Speicherbereiche**. Keine GPU-Datenstruktur größer als RAM_G vorsehen, selbst wenn genug Platz auf der SD ist. Auf der MCU zusätzlich Heap-Fragmentierung und zusammenhängende freie Blöcke berücksichtigen. Die verfügbare RAM-/Flash-Menge ist zur Laufzeit bzw. im Build zu ermitteln, **keine erfundenen „freien X KiB“ als harte Zahl ausgeben**.

### Praktische Speicherfolgen

- Ein einzelnes **48 × 48 px**-Bild benötigt unkomprimiert z. B. **4.608 Byte** bei RGB565 bzw. ARGB4 (16 Bit/Pixel), **9.216 Byte** bei 32 Bit/Pixel. Format, Transparenz und FT800-Kompatibilität berücksichtigen.
- **256 KiB RAM_G** würden theoretisch nur etwa **56 einzelne 48×48-RGB565-Bilder** aufnehmen, wenn sonst *nichts* dort läge. Das ist **kein nutzbares Assetbudget**: Alignment, andere Ressourcen, Command-/Asset-Arbeitsabläufe und Reserven senken es. Außerdem können animierte Figuren viele Frames benötigen.
- Eine volle **480 × 272**-Oberfläche belegt schon **261.120 Byte bei 16 Bit/Pixel** – nahezu ganz RAM_G. **Keinen Fullscreen-Framebuffer** in RAM_G oder MCU-SRAM vorsehen.
- Niemals ganze MZ-Tilesheets oder PNG-Dateien ungeprüft in RAM laden. Werkzeuge am Entwicklungs-PC zur Konvertierung, Zuschneidung, Palettierung, Atlas-Erstellung und Komprimierung sind ausdrücklich erwünscht.
- Mapdaten als kompakte Tile-IDs, Metadaten und Kollisionen speichern, nicht als Bitmap der gesamten Karte. Ressourcen pro Raum/Zone gezielt laden, freigeben oder ersetzen.
- Rendering über **FT800-Display-Listen, Bitmap-Handles und ggf. passende Hardwarefunktionen**; nicht pixelweise mit dem ESP32 malen.

## 3. Hardware-Verkabelung (bestehende, getestete Grundverbindung)

| Gameduino 2 | ESP32-Pin | Funktion / Status |
| --- | --- | --- |
| +5V | 5V | Stromversorgung Gameduino |
| GND | GND | Gemeinsame Masse |
| D13 / SCK | GPIO18 | Gemeinsamer SPI-Takt |
| D12 / MISO | GPIO19 | SPI MISO |
| D11 / MOSI | GPIO23 | SPI MOSI |
| D8 / GPU sel | GPIO5 | FT800 Chip Select; Display funktioniert |
| D9 / SD sel | GPIO27 | SD-Karten Chip Select; SD-Zugriff **nicht** verifiziert |
| D2 / INT | GPIO26 | Optionaler FT800-Interrupt aus älterer Setup-Referenz; aktuelle Verwendung/Verdrahtung im Projekt prüfen |
| A0 / Accl X | GPIO32 | Optionaler Beschleunigungssensor; Funktion nicht verifiziert |
| A1 / Accl Y | GPIO33 | Optionaler Beschleunigungssensor; Funktion nicht verifiziert |
| A2 / Accl Z | GPIO34 | Optional, **nur Eingang**; Funktion nicht verifiziert |
| VIN | — | **Nicht anschließen** |

- ESP32-GPIOs sind **nicht 5-V-tolerant**. Neue Verbindungen/Signale vor Benutzung auf Pegel prüfen. Gameduino über **5V** versorgen und GND gemeinsam verbinden.
- FT800 und SD teilen **denselben SPI-Bus**, verwenden aber **verschiedene CS-Leitungen** (GPIO5 und GPIO27). **Nie beide Geräte gleichzeitig selektieren**. SPI-Transaktionen/Frequenzen sauber verwalten.
- Im laufenden Projekt wird SD-CS beim Start **HIGH** gesetzt, damit die SD-Karte den Displaybetrieb nicht stört. Das ist **kein Nachweis für funktionierende SD-Lesezugriffe**.
- **GPIO6–GPIO11**, insbesondere **GPIO9 und GPIO10**, nicht als freie GPIOs konfigurieren: Sie werden beim klassischen ESP32 für SPI-Flash benutzt. Achtung auf Bootstrap-/Sonderfunktionen anderer Pins; bestehende Belegung nicht beiläufig ändern.
- Bei der älteren Arduino/Gameduino2-Bibliothek musste in `transports/wiring.h` das `pinMode(9/10, OUTPUT)` unter `#if !defined(ESP32)` gestellt werden, um Watchdog-Resets zu vermeiden. Die aktuelle projektlokale Bibliothek auf diesen Schutz prüfen und ihn nicht versehentlich rückgängig machen.
- Zusätzlich war für neueren Compiler in `GD2.cpp` ein Typ-Fix nötig: `min<uint32_t>(512U, r.size - r.offset)`. Nicht ungeprüft durch alte Bibliotheksversion ersetzen.
- Bewährte ältere Minimalinitialisierung war `SPI.begin(18, 19, 23, 5)` und `GD.begin(0, 5, 27)` **ohne SD-Initialisierung**. Aktuelle Projekt-Initialisierung ist maßgeblich, solange sie funktioniert.

## 4. Toolchain und vorhandenes Projekt

**Laut bestehender `README.md` getestete Kombination (keine stillschweigenden Upgrades):**

| Teil | Version |
| --- | --- |
| ESP-IDF | 5.4.2 |
| Arduino-ESP32 (ESP-IDF-Komponente) | 3.2.1 |
| Bluepad32 | 4.2.0 |
| BTstack | 1.6.2 |
| PlatformIO pioarduino | 54.03.21 |

- `components/gameduino2/` und `components/bluepad32_arduino/` enthalten **angepasste** Bibliotheken. Änderungen **gezielt, minimal und dokumentiert** halten; Bibliotheksupdates können die funktionierenden ESP32-Fixes entfernen.
- Externe Komponenten `arduino`, `bluepad32`, `btstack`, `cmd_nvs` und `cmd_system` sind laut README nicht versioniert und werden mit `setup-deps.sh` installiert. **Nie unbesehen externe Source-Trees überschreiben**.
- `main/main.c`: ESP-IDF-/Bluepad32-Einstieg; `main/main.cpp`: Arduino `setup()`/`loop()`. Zuständigkeit beider Eingänge respektieren.
- Die Struktur unter `main/display/`, `main/game/`, `main/input/` **weiterverwenden**; neue Systeme sinnvoll ergänzen (z. B. `assets/`, `world/`, `combat/`), statt alles in eine große Datei zu schreiben.
- **Keine projektfremde API erfinden**: Header, Signaturen, tatsächlich eingesetzte Gameduino-Version und Controller-Datenstrukturen im Repository prüfen.

### Standardbefehle

```sh
bash setup-deps.sh         # nur falls Abhängigkeiten fehlen; nicht blind überschreiben
pio run -e esp32dev       # Firmware bauen
pio run -e esp32dev -t upload
pio device monitor -e esp32dev
```

Der Upload-Port in `platformio.ini` ist derzeit gerätespezifisch (`/dev/serial/by-id/...`); auf anderen Hosts ggf. `/dev/ttyUSB0`. Monitor üblicherweise **115200 Baud**, sofern Konfiguration nichts anderes angibt. Bei älteren Logs gab es mehrere installierte PlatformIO-Core-Versionen; Toolchain-Probleme von echten Compilerfehlern unterscheiden.

**Bisher wirklich funktionsfähig:** FT800-Display über SPI, DualShock-4-Verbindung über Bluepad32, Bewegung eines orangefarbenen Rechtecks mit dem linken Analogstick, Bildschirmbegrenzung und grundlegende Modultrennung.

**Noch offen/nicht getestet:** SD-Zugriff, Sprite- und Tileset-Loading, Tilemaps, Audio, Kampf-/Inventarsystem, Speichern/Laden, Delta-Time-basierte Hauptschleife und vollständig reproduzierbarer Clean-Clone-Build. Der derzeitige Prototyp benutzt ein festes `delay(100)`; dies **nicht** als gewünschtes Game-Loop-Design übernehmen.

## 5. Grafikformat: RPG Maker MZ **als Layoutkonvention**, nicht als Engine

Alle geplanten Grafiken sind **eigenständig erstellt** und **für die Nutzung in anderen Engines freigegeben**. Es wird **nicht** vorausgesetzt, dass originale RPG-Maker-MZ-Grafiken oder deren Lizenz verwendet werden.

- **Tilegröße: 48 × 48 px** als **Quellformat**. Ein statisches Tilesheet B/C/D/E hat üblicherweise **768 × 768 px (16 × 16 Tiles)**; A5: **384 × 768 px (8 × 16 Tiles)**. A1–A4 haben spezielle Autotile-Layouts.
- **Charaktersheets:** einzelne Figur üblicherweise **3 Spalten × 4 Blickrichtungen** (Down, Left, Right, Up), bei 48×48-Pixel-Frames **144 × 192 px**. Standbild gewöhnlich mittlere Spalte; Gehfolge typischerweise **Mitte → Links → Mitte → Rechts**. Eine Standard-Sammeldatei für 8 Charaktere besteht aus **4 Spalten × 2 Reihen dieser Figurenblöcke** (bei 48px-Frames **576 × 384 px**).
- **Autotiles sind nicht einfach fertige 48px-Tiles**: A1–A4 können Teil-Tiles, Nachbarschaftsregeln und Animation erfordern. **Zunächst einfache statische Tiles (A5/B–E) unterstützen.** Autotiles später entweder in einem Offline-Konverter zu fertigen Varianten vorverarbeiten oder mit einer explizit dokumentierten Logik umsetzen.
- Die Quellgrafiken dürfen dem MZ-Layout entsprechen, **müssen aber nicht unverändert im FT800-Grafikspeicher landen**. Vorverarbeitung auf dem PC ist bevorzugt. Das Laufzeitformat richtet sich nach den **tatsächlichen FT800-Bitmapformaten** (z. B. RGB565, ARGB4, L8/Palette nach Eignung), Alpha-Bedarf und Speicherverbrauch; **PNG-Dekodierung auf dem FT800 nicht einfach voraussetzen**.
- **Nicht automatisch auf 48×48 Darstellung festlegen:** Auf **480×272** passen nur 10 volle 48px-Tiles horizontal und 5 volle vertikal. Bei einem kleineren *Darstellungsmaßstab* oder gekacheltem Scroll-Viewport ist die Spielwelt besser überschaubar. Die Festlegung des Rendermaßstabs ist eine **offene Designentscheidung** und muss von den Quellformaten getrennt bleiben. Bei Pixel-Art nach Möglichkeit ganzzahlige Skalierungsfaktoren oder bewusst gewählte Filterung berücksichtigen.
- Figurgröße, Kollisionsbox, Renderrechteck und Tilegröße sind **unabhängige Konzepte**: Kollisionen nicht anhand transparenter Bitmap-Ränder berechnen.

## 6. Rendering- und Asset-Regeln

1. **FT800 zuerst:** Nutze vorhandene Gameduino-/FT800-APIs, Hardware-Bitmaps und Display-Listen; keine Software-Framebuffer-Architektur.
2. **Stets VRAM-Budget:** Für jede neue Assetgruppe berechnen/dokumentieren: Pixelformat, Breite, Höhe, Bytes, Alignment, Sprite-Frames, Residency und Reserven. Speicher für Sprites, Tiles, UI und dynamisch geladene Assets nicht doppelt verplanen.
3. **Asset-Streaming ist kein Freifahrtschein:** Daten auf der SD brauchen Lesezeit, SPI-Bandbreite und ggf. temporären MCU-/FT800-Speicher. Lade bevorzugt **bei Raumwechsel** oder in wohldefinierten Ladephasen; vermeide große synchrone SD-Transfers mitten in jedem Frame.
4. **Texturgröße und FT800-Limits beachten:** Bildformate, Strides, Filterung, Tile-/Wrap-Modi, maximale zulässige Abmessungen und Bitmap-Handles **gegen die genutzte FT800-API/Dokumentation prüfen**; keine GPU-Möglichkeiten neuerer EVE-Generationen voraussetzen.
5. **Display-Listen begrenzen:** Display-Listen sind endlich (**8 KiB**). Sichtbare Tiles/Objekte effizient cullen, draw calls gruppieren und keine unbeschränkte Zahl von Primitiven erzeugen. Co-Prozessor-FIFO korrekt synchronisieren.
6. **Nur sichtbare Elemente zeichnen:** Kamera-/Viewport-Culling mit Rand für teilweise sichtbare Tiles; Weltkoordinaten nicht mit Bildschirmkoordinaten vermischen.
7. **Keine großen Byte-Arrays in Firmware einbetten**, wenn sie als Assets auf SD liegen können. Kleine Debug-/Fallback-Assets im Flash sind okay, sofern Flashbudget passt.
8. **Formatkonverter reproduzierbar halten:** Quell-PNGs und ein Skript/Build-Schritt können im Repository liegen; generierte Assets mit dokumentiertem Format, Palette, Endianness, Bytegröße, Atlas-Koordinaten und Versionierung ausgeben.
9. **Die SD ist noch nicht bewiesen:** Vor umfangreicher Asset-Pipeline zuerst SPI-SD-Initialisierung, Datei lesen, Dateigröße prüfen und Konfliktfreiheit mit parallelem FT800-Betrieb testen.

## 7. Spielarchitektur und Performance

- **Trenne Eingabe, Spiellogik, Weltzustand, Physik/Kollision, Rendering, Audio (später) und Asset-Management.** Ein Subsystem soll keine impliziten Abhängigkeiten auf den Zustand eines anderen haben.
- Eine **zeitbasierte Game-Loop** einführen: Zeit mit monotoner Uhr bestimmen, Bewegung und Animation unabhängig vom `delay(100)` des Prototyps. Für reproduzierbare Physik ist ein **Fixed-Timestep-Update** (z. B. zunächst 30 oder 60 Updates/s) mit renderseitiger Entkopplung sinnvoll; tatsächlich erreichbare Framerate **messen statt versprechen**.
- Vorläufige Entwicklungsziele: **30 FPS stabil** bevorzugen; **60 FPS** nur wenn real messbar ohne unerwünschte Einbrüche. Zahlen sind Zielwerte, **keine bestätigte FT800-Leistungsgarantie**.
- Bluetooth-Events und Controllerzustand regelmäßig abfragen, ohne die Renderloop durch blockierende Operationen auszubremsen. Keine `delay()`-Warteschleifen, die Eingaben oder Watchdog verhungern lassen.
- Ein kompakter **Raum-/Dungeon-Datenaufbau** eignet sich besser als eine vollständig geladene Welt. Trenne statische Tilemaps, dynamische Entities, Kollisionsflags, Trigger und persistente Zustände.
- **Feste maximale Kapazitäten** für gleichzeitige Entities/Projektile/Partikel sind auf Mikrocontrollern oft besser als unbeschränkte Container. Größen nicht frei erfinden; anhand RAM-Messungen bestimmen. Speicherallokationen während des Frame-Loops möglichst vermeiden.
- Kollisionen auf dedizierten Collision-Layern/Hitzonen, nicht auf Bildpixeln. Animationen und Gameplay-Logik bleiben unabhängig vom Assetlayout.
- Erst **kleine, funktionierende vertikale Scheiben**: ein Raum → Kollision → Spielfigur-Sprite → Kamera/Tilemap → Türen → Gegner → Kampf → Übergänge → Save-System.
- **Audio:** vorhandene Gameduino-/FT800-Möglichkeiten prüfen; Sound-Hardware/Pins/Qualität und Streaming-Limits sind noch nicht verifiziert. Kein mehrkanaliges Audio oder Musikstreaming als vorhandene Funktion deklarieren.

## 8. Controller: DualShock 4 / Bluepad32

**Bereits getestet:** Bluetooth-Verbindung und Bewegung mit dem **linken Analogstick**. Alle übrigen Zuordnungen sind ein **vorgeschlagenes Spiel-Layout**, nicht bereits implementiert.

| Eingabe | Vorgeschlagene Aktion |
| --- | --- |
| Linker Stick | Spielfigur bewegen (analog, Deadzone erforderlich) |
| D-Pad | Alternative 4-Wege-Bewegung / Menünavigation |
| Kreuz (×) | Interagieren / Bestätigen |
| Quadrat (□) | Primärer Angriff |
| Kreis (○) | Ausweichen / Abbrechen |
| Dreieck (△) | Ausgerüsteten Gegenstand benutzen |
| L1 / R1 | Schnellwechseln zwischen Items/Waffen |
| L2 | Alternative Aktion (später definieren) |
| R2 | Alternative Aktion (später definieren) |
| Options | Pause / Menü |
| Share | Karte (später) |
| Rechter Stick | Optional z. B. Blickrichtung/Zielen, **nicht** für Kernmechaniken erforderlich |
| L3 / R3 / Touchpad / PS | Zunächst **nicht verpflichtend**; PS-Taste kann systemseitig behandelt werden |

- **Tatsächliche Bluepad32-Button-Bits und Achsen im installierten Quellcode prüfen**: `buttonA/B/X/Y`-Namen oder Bitmasken entsprechen nicht automatisch Playstation-Symbolen. Ein **einheitliches internes Action-Mapping** verwenden, damit Hardware/Backend und Game-Logik entkoppelt sind.
- Stick-Achsen nach gemessener Bluepad32-Range normalisieren, Deadzone anwenden, optional diagonale Geschwindigkeit normalisieren. **Keine festen Rohwertbereiche raten.**
- **Pressed / Held / Released** getrennt auswerten; Menüs und Interaktionen sollen nicht bei jedem Frame wiederholt auslösen. Controllerstatus pro Update als konsistenten Snapshot bereitstellen.
- Ohne Verbindung: stabile Anzeige/Fehlermeldung und Verbindungswiederaufnahme, **keine** Nullpointer-Zugriffe oder eingefrorene Firmware.
- Bluetooth-Stack und Speicherbedarf berücksichtigen. Wi-Fi nicht zusätzlich starten, solange es nicht gebraucht wird.

## 9. Messung, Fehlersuche und Akzeptanzkriterien

Bei RAM-/Performance-relevanten Änderungen im Debug-Build soweit möglich protokollieren:

- ESP32: `esp_get_free_heap_size()`, `esp_get_minimum_free_heap_size()` sowie **größten zusammenhängenden Block** mit `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)`; ggf. getrennt internes DMA-fähiges RAM prüfen.
- Build: Flash-/RAM-Verbrauch und Partitionierungsgrenzen über PlatformIO-/IDF-Ausgaben; keinesfalls nur Rohgröße „4 MB“ annehmen.
- Grafik: aufsummierte tatsächliche **RAM_G-Belegung**, Restplatz, Zahl sichtbarer Tiles/Sprites, Display-List-/Command-Budget soweit messbar.
- Spiel: durchschnittliche und Worst-Case-Framezeit, SD-Ladedauer, Bluetooth-Eingabelatenz sowie Verhalten beim Controller-Disconnect.
- Nach Änderung der Display-/SPI-Initialisierung mit bekanntem Minimalbild testen. In der alten Referenz lieferte **FT800 `REG_ID` (Adresse `0x102400`) den Wert `0x7C`** als SPI-Sanity-Check.
- Serielles Logging sparsam im Frame-Loop; aussagekräftige Fehlermeldungen für fehlende Assets, SD-Mount-Fehler, Heap-/VRAM-Erschöpfung und Controllerverlust.

**Definition of Done für eine Codeänderung:**

1. Bestehende Verdrahtung und die funktionierende Bluetooth-/Displayintegration bleiben erhalten.
2. Kompiliert soweit lokal möglich mit `pio run -e esp32dev`; **Build-Erfolg nicht behaupten, wenn nicht tatsächlich ausgeführt**.
3. Ressourcenlimits nachweislich eingehalten oder die noch nötige Messung deutlich benannt.
4. Keine zusätzlichen Hardcoded-Pins, ungeprüften API-Behauptungen oder unkontrollierten großen Heap-/VRAM-Allokationen.
5. Neu eingeführte Eingaben und Spielmechaniken sind von Hardwaredetails sinnvoll entkoppelt.
6. Geänderter Funktionsumfang, neue Assets/Tools, bekannte Grenzen und nötige Hardwaretests in README oder passender Dokumentation beschrieben.

## 10. Arbeitsweise für Coding-Agenten

- **Zuerst Bestand prüfen, dann patchen.** Relevante Dateien, Compilerflags, PIO-Konfiguration, Komponenten und vorhandene Schnittstellen lesen. Bestehende funktionierende Integrationen nicht „aufräumen“, bis ihre Rolle verstanden ist.
- **Kleine und prüfbare Änderungen**, vorzugsweise pro Schritt ein Ziel. Bei großen Features vorher kurz Architektur, benötigten RAM/VRAM und Testplan nennen.
- **Verifiziert vs. angenommen vs. geplant** klar kennzeichnen. SD-Zugriff, Sound, MZ-Asset-Importer oder bestimmte FT800-Kommandos niemals als implementiert ausgeben, wenn sie nicht im Code stehen und geprüft sind.
- Keine beliebigen Libraries, inkompatiblen Frameworks oder ESP32-Varianten hinzufügen; Auswirkungen auf Flash, Heap, Bluetooth und SPI erklären.
- Keine Änderungen an Boardpinning, Partitionstabelle, Bluepad32/BTstack oder fremden Gameduino-Treibercode ohne nachvollziehbaren Grund und Hinweis auf Regressionstests.
- Bevorzugt **C++** für Firmware. **Python/Node.js/TypeScript** sind optional für Offline-Asset-Tools auf dem Entwicklungs-PC, **nicht** als Laufzeit des ESP32-Spiels.
- Spiellogik PC-seitig soweit möglich isoliert testbar halten, ohne vorzugeben, dass ein Desktop-Test die echte Hardware vollständig ersetzt.
- Den Benutzer über Entscheidungen mit großen Auswirkungen (Rendermaßstab, SD-Dateiformat, Speicheraufteilung, Kampfsteuerung, Austausch von Bibliotheken) informieren; keine stillschweigenden Designfestlegungen als Hardwarezwang darstellen.

## 11. Offene Entscheidungen / zuerst zu verifizieren

1. **SD-Karte:** zuverlässiger SPI-Zugriff auf die 16-GB-Karte bei gemeinsamem Bus und funktionierendem Display; Dateisystem/FAT-Konfiguration testen.
2. **Freier RAM im realen Build:** Messung mit aktivem Bluetooth + FT800, einschließlich größtem freien Block und Low-Watermark.
3. **Assetformat / FT800-Upload:** erster eigener 48×48-Sprite mit Transparenz, korrekte Darstellung und tatsächlich benötigte Bytes messen.
4. **48px-Grafik vs. Displaygröße:** Darstellungsmaßstab, Kamera und nutzbarer Spiel-Viewport definieren.
5. **Controller-Mapping:** alle DS4-Tasten/Achsen mit der konkreten Bluepad32-Version auslesen und dann das gewünschte Layout bestätigen.
6. **Spieltempo:** Fixed-Timestep implementieren, tatsächliche Bildrate messen.
7. **Audio und Saves:** Hardware-API/Dateisystem/Schreibstrategie erst prüfen, bevor sie als Feature eingeplant werden.
8. **Reproduzierbarkeit:** `setup-deps.sh` plus frischer Checkout noch vollständig testen.

## 12. Referenzen

**Projektintern:** `README.md` (aktueller Prototyp, Versionen, Struktur und Build), `ESP32_Gameduino2_Setup.md` (ältere Verkabelungs-/Treiber-Referenz; möglicherweise nicht im Repository), tatsächliche Projektdateien unter `main/` und `components/`.

**Technische Hersteller-/Formatreferenzen:**

- ESP32 Datenblatt: https://documentation.espressif.com/esp32_datasheet_en.html
- FT800 Datenblatt mit Memory Map: https://riverdi.com/wp-content/uploads/2024/05/DS_FT800_Embedded_Video_Engine.pdf
- RPG Maker MZ Tileset-Formate: https://rpgmakerofficial.com/product/MZ_help-en/01_11_01.html
- RPG-Maker-Charaktersheet-Konventionen: https://www.rpgmakerweb.com/blog/charsets-structure-and-avoiding-traps

**Priorität bei Widersprüchen:** Tatsächlich gemessene Hardware und im aktuellen Repo erfolgreich getesteter Code > aktuelle projektinterne `README.md`/Konfiguration > ältere Setup-Referenz > allgemeine Datenblätter/Beispiele. Hardware-Hardlimits gelten unabhängig davon.
