#pragma once
#include "Room.h"
#include <stddef.h>

namespace LevelFormat {
constexpr size_t maxFileBytes = 4096;
using Read = int (*)(void*, uint8_t*, size_t);
using Write = int (*)(void*, const uint8_t*, size_t);
// DNG1-Textformat; kein SD-/Hardwarewissen. Ausgabe erst nach Erfolg uebernehmen.
bool decode(Room::Map& result, void* context, Read read, const char*& error);
bool encode(const Room::Map& map, void* context, Write write, const char*& error);
}
