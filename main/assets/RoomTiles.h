#pragma once
#include "world/Room.h"

namespace RoomTiles {
bool load();
const char* error();
// Aufrufer haelt SdStorage::DisplayGuard. Decke separat als Vordergrund zeichnen.
void drawGround();
void drawCeiling();
}
