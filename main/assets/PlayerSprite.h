#pragma once
#include <stdint.h>

namespace PlayerSprite {
constexpr int frameSize = 48;
constexpr uint32_t frameBytes = frameSize * frameSize * 2;
constexpr uint32_t bytes = frameBytes * 12;
// Frame-major ARGB4 in RAM_G ab Adresse 0, Handle 0.
bool load();
bool ready();
const char* error();
void draw(int x, int y, uint8_t direction, uint8_t column);
}
