#pragma once
#include <stdint.h>

namespace Room {
constexpr int tileSize = 48;
constexpr int columns = 10, rows = 6;
constexpr int width = columns * tileSize, height = 272;
constexpr int floorLeft = tileSize, floorTop = 3 * tileSize;
constexpr int floorRight = width - tileSize, floorBottom = height - tileSize;
enum class Tile : uint8_t { Floor, Wall, Ceiling };
struct Box { float x, y, width, height; };

// Ein fester Raum. Tilematerial und Kollision sind unabhaengig von den Bitmaps.
void begin();
Tile tile(int column, int row);
bool sameTile(int column, int row, Tile material);
int rowY(int row);
int rowHeight(int row);
// Achsenweise Sweep-Kollision, erlaubt Gleiten an Waenden ohne Tunneln.
void move(Box& body, float deltaX, float deltaY);
}
