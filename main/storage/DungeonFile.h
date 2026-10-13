#pragma once
namespace DungeonFile {
// Nur in einer Ladephase im Game-Task aufrufen; nicht unter DisplayGuard.
// Pfade: /levels/<ASCII-Name>.dng, kein Unterordner. Aktuelle Karte bei Fehler erhalten.
bool load(const char* path);
// Export der aktuellen Karte; erstellt exklusiv, ueberschreibt keine Dateien.
bool save(const char* path);
const char* error();
}
