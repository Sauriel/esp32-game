#pragma once

namespace TitleBackground {
bool load();
// Aufrufer haelt SdStorage::DisplayGuard.
void draw();
const char* error();
}
