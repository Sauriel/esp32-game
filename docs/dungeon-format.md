# Dungeon-Leveldateien: DNG1

Random und spätere Story-Level verwenden dieselbe `Room::Map`. Version 1 speichert
das vollständige statische Layout, keine Bitmapdaten. Es ist kein Spielstand:
aktuelle Spielerposition, Inventar, Gegner, Türen und Fortschritt sind nicht enthalten.

## Format

ASCII-Text ohne BOM. Erste Zeile:

```text
DNG1 <Spalten> <Zeilen> <Startspalte> <Startzeile> <Seed>
```

Danach exakt `Zeilen` Rasterzeilen mit jeweils exakt `Spalten` Zeichen:

| Zeichen | Material | Kollision |
| --- | --- | --- |
| `.` | Boden (`floor.png`) | begehbar |
| `W` | Wand (`wall.png`) | solide |
| `#` | Decke (`ceiling.png`) | solide |

Koordinaten beginnen links oben bei `(0, 0)`, X nach rechts, Y nach unten.
Der Start bezeichnet das Tile, in dessen Mitte die Fussbox beginnt. Die Kamera
zentriert anschliessend den Sprite-Mittelpunkt auf `(240, 136)` des Displays.
Jede Rasterzelle ist 48 × 48 Weltpixel gross. Der Seed ist Metadatum, beim Laden
wird **das gespeicherte Raster** übernommen, nicht erneut generiert.

Es gelten 3…64 Spalten, 3…48 Zeilen und maximal 4096 Dateibytes. Ganzzahlen sind
vorzeichenlos und dezimal; Seed 0…4294967295. Der gesamte äussere Rasterrand muss
`#` sein, das Starttile muss `.` sein. Unbekannte Versionen/Zeichen, falsche Zeilen-
längen, Zahlenüberläufe, fehlende Zeilen, NUL und Daten hinter dem Raster werden
abgewiesen. Kommentare, Leerzeilen und Tabs sind nicht vorgesehen. Zeilenenden
dürfen LF oder CRLF sein; auch die letzte Rasterzeile braucht einen Zeilenumbruch.
Der Encoder schreibt LF. Der Header bleibt unter 96 Bytes.

Bei festen Leveln werden die Wand-/Deckenzellen ausdrücklich eingezeichnet;
der Lader korrigiert oder generiert das Layout nicht. Für die übliche Optik stehen
oberhalb eines Bodenbereichs zwei Reihen `W` (96 Pixel Wand) und darüber mindestens
eine Reihe `#`. Der Lader prüft Material, Rand und Start, aber nicht ästhetische
Wandhöhe oder Erreichbarkeit aller Bodenbereiche. Abgetrennte Bereiche können für
spätere Türen sinnvoll sein. Die Autotile-Teilbilder werden zur Laufzeit aus dem
Material und seinen Nachbarn bestimmt und sind nicht in der Datei gespeichert.

Eine vollständige, von Hand erstellte Beispielkarte liegt in
[assets/levels/example.dng](../assets/levels/example.dng). Auf SD nach
`/levels/example.dng` kopieren. Sie hat 16 × 12 Tiles und Starttile `(7, 6)`.
Die drei Textursheets bleiben unter `/tiles`, das Spieler-Sheet unter `/player.png`.

## Firmware-APIs

- `Game::begin()` erzeugt eine neue Zufallskarte; derzeit vom Menü „Spielen“ aufgerufen.
- `Game::begin(seed)` erzeugt die gleiche Karte für den gleichen Seed in Generatorversion 1.
- `Game::beginFromLevel("/levels/example.dng")` lädt eine feste Karte und setzt Spieler/Animation zurück.
  Bei Ladefehler bleibt die bisherige Karte und Szene erhalten; Fehler wird seriell ausgegeben.
- `DungeonFile::save("/levels/export.dng")` exportiert das aktuelle Layout.
  Das Verzeichnis `/levels` muss vorhanden sein. Bestehende Dateien werden nicht überschrieben.
- `DungeonFile::error()` liefert den letzten Datei-/Formatfehler.

Diese Aufrufe gehören in Ladephasen im Game-Task, ausserhalb von `DisplayGuard`,
und dürfen nicht gleichzeitig aus mehreren Tasks erfolgen. Der aktuelle Menüstand
bietet noch keine Story-Auswahl und keinen Exportknopf; keine automatische Speicherung.
Zulässige SD-Pfade sind `/levels/<Name>.dng`; Name aus ASCII-Buchstaben, Ziffern,
`_` und `-`, ohne Unterordner. Pfadlänge maximal 239 Bytes.

`world/LevelFormat` übernimmt Hardware-unabhängiges Lesen/Schreiben über Callbacks.
`storage/DungeonFile` verbindet es mit den vorhandenen gesperrten SD-Operationen.
Das Laden erfolgt zuerst in einen zweiten statischen Kartenpuffer. Erst nach
vollständiger Formatprüfung **und** erfolgreichem Schliessen wird die aktive Karte
ersetzt. Export schreibt eine exklusiv neu angelegte Datei und prüft `fsync`/`close`.
Bei Fehler wird die unvollständige neue Datei entfernt; scheitert auch das, wird
der Pfad seriell protokolliert. Bei Stromausfall kann eine Teildatei zurückbleiben;
FAT-Export ist nicht stromausfallsicher. Es gibt keine dateigrossen Heap-Puffer.

## Manuelle Abnahme durch den Benutzer

Es wurden keine Builds oder Tests durch den Agenten ausgeführt. Firmware selbst
bauen und hochladen; Flash/App-Partitionsreserve, statischen RAM und Heap prüfen.
Random mehrfach starten, Seeds aus dem Log erfassen, Räume/Gänge erkunden,
Kamera bei allen Richtungen und an Kartengrenzen sowie Kollisionen prüfen.
Für eine reproduzierbare Sichtprüfung kann der bestehende Play-Startaufruf vorübergehend
`Game::begin(12345)` verwenden; Änderungen am Generator können die Karte künftig ändern.

Die Datei-APIs sind noch nicht im Menü verdrahtet. Für die manuelle Levelabnahme
kann der Play-Startaufruf vorübergehend `Game::beginFromLevel("/levels/example.dng")`
verwenden (Rückgabewert prüfen; bei Fehler im Menü bleiben). Startposition, Kamera,
Wände und Autotile-Ecken auf dieser festen Karte prüfen. Falsche Version, unbekannte
Materialien, offene Randzelle, Start auf Wand, zu lange/fehlende Zeile, Zahlenüberlauf
und übergrosses File müssen abgelehnt werden und die vorherige Karte erhalten.
Einen expliziten Export in einer Ladephase auslösen, Datei am PC öffnen und erneut
laden; Layout/Starttile müssen übereinstimmen. Ein zweiter Export auf denselben Pfad
muss scheitern und die vorhandene Datei erhalten. SD-Fehler und Moduswechsel prüfen.
Lade-/Export-APIs, Renderer und Generator sind bislang nur statisch durchgesehen.
