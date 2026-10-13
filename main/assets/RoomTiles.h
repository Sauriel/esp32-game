#pragma once
#include "world/Room.h"
#include "world/Camera.h"

namespace RoomTiles {
bool load();
const char* error();
// Aufrufer haelt SdStorage::DisplayGuard. Decke separat als Vordergrund zeichnen.
void drawGround(const Camera::View& camera);
void drawCeiling(const Camera::View& camera);
}
