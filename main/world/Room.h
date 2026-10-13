#pragma once
#include <stdint.h>

namespace Room {
constexpr int tileSize = 48;
constexpr int maxColumns = 64, maxRows = 48;
enum class Tile : uint8_t { Floor, Wall, Ceiling };
struct Map {
    uint16_t columns = 0, rows = 0;
    uint16_t spawnColumn = 0, spawnRow = 0;
    uint32_t seed = 0;
    Tile tiles[maxColumns * maxRows]{}; // Aktive Zeilen haben Stride columns.
};
static_assert(sizeof(Map) <= 3100, "Dungeon map exceeds RAM budget");
struct Box { float x, y, width, height; };

// Generator und Leveldateien teilen dasselbe Raster; keine Bitmap-/SD-Abhaengigkeit.
void generate(uint32_t seed);
const Map& map();
bool validate(const Map& candidate, const char*& error);
bool use(const Map& candidate, const char*& error);
int columns();
int rows();
int width();
int height();
// Ausserhalb der Karte liegt solide Decke.
Tile tile(int column, int row);
bool sameTile(int column, int row, Tile material);
// Achsenweise Sweep-Kollision, erlaubt Gleiten an Waenden ohne Tunneln.
void move(Box& body, float deltaX, float deltaY);
}
