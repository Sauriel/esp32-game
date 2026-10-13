#pragma once
#include "Room.h"
#include <cmath>

namespace Camera {
constexpr int width = 480, height = 272;
constexpr int centerX = width / 2, centerY = height / 2;
constexpr int maxVisibleColumns = (width + Room::tileSize - 1) / Room::tileSize + 1;
constexpr int maxVisibleRows = (height + Room::tileSize - 1) / Room::tileSize + 1;
struct View {
    int x, y; // Weltpixel der linken oberen Ecke; auch negativ erlaubt.
    int firstColumn() const { return int(floorf(float(x) / Room::tileSize)); }
    int firstRow() const { return int(floorf(float(y) / Room::tileSize)); }
    int lastColumn() const { return int(floorf(float(x + width - 1) / Room::tileSize)); }
    int lastRow() const { return int(floorf(float(y + height - 1) / Room::tileSize)); }
};
inline View follow(float playerX, float playerY) {
    return {int(floorf(playerX)) - centerX, int(floorf(playerY)) - centerY};
}
}
