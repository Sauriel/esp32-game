#pragma once
#include "PlayerSprite.h"

namespace TitleBackground {
constexpr unsigned sourceWidth = 480, sourceHeight = 272;
constexpr unsigned width = sourceWidth / 2, height = sourceHeight / 2;
constexpr uint32_t address = PlayerSprite::bytes;
constexpr uint32_t bytes = width * height * 2;
bool load();
// Aufrufer haelt SdStorage::DisplayGuard.
void draw();
const char* error();
}
