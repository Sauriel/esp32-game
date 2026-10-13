#include "Room.h"
#include "DungeonGenerator.h"
#include <algorithm>
#include <cmath>

namespace Room {
namespace {
Map current;
bool overlaps(float start, float size, int minimum, int maximum) {
    return start < maximum && start + size > minimum;
}
int cell(float coordinate) { return int(floorf(coordinate / tileSize)); }
}
void generate(uint32_t seed) { DungeonGenerator::generate(current, seed); }
const Map& map() { return current; }
int columns() { return current.columns; }
int rows() { return current.rows; }
int width() { return columns() * tileSize; }
int height() { return rows() * tileSize; }
Tile tile(int column, int row) {
    if (column < 0 || column >= columns() || row < 0 || row >= rows()) return Tile::Ceiling;
    return current.tiles[row * columns() + column];
}
bool sameTile(int column, int row, Tile material) { return tile(column, row) == material; }
bool validate(const Map& candidate, const char*& error) {
    error = "Level: ungueltige Kartengroesse";
    if (candidate.columns < 3 || candidate.columns > maxColumns ||
        candidate.rows < 3 || candidate.rows > maxRows) return false;
    error = "Level: Startposition nicht auf Boden";
    if (candidate.spawnColumn >= candidate.columns || candidate.spawnRow >= candidate.rows ||
        candidate.tiles[candidate.spawnRow * candidate.columns + candidate.spawnColumn] != Tile::Floor) return false;
    for (int row = 0; row < candidate.rows; ++row) {
        for (int column = 0; column < candidate.columns; ++column) {
            const Tile material = candidate.tiles[row * candidate.columns + column];
            error = "Level: unbekanntes Material";
            if (material != Tile::Floor && material != Tile::Wall && material != Tile::Ceiling) return false;
            error = "Level: Kartenrand muss Decke sein";
            if ((row == 0 || column == 0 || row == candidate.rows - 1 ||
                 column == candidate.columns - 1) && material != Tile::Ceiling) return false;
        }
    }
    error = nullptr;
    return true;
}
bool use(const Map& candidate, const char*& error) {
    if (!validate(candidate, error)) return false;
    current = candidate;
    return true;
}

void move(Box& body, float deltaX, float deltaY) {
    float nextX = std::max(0.0f, std::min(body.x + deltaX, float(width()) - body.width));
    const int firstColumn = std::max(0, cell(std::min(body.x, nextX)));
    const int lastColumn = std::min(columns() - 1, cell(std::max(body.x, nextX) + body.width));
    const int firstRow = std::max(0, cell(body.y));
    const int lastRow = std::min(rows() - 1, cell(body.y + body.height));
    for (int row = firstRow; row <= lastRow; ++row) {
        const int top = row * tileSize, bottom = top + tileSize;
        if (!overlaps(body.y, body.height, top, bottom)) continue;
        for (int column = firstColumn; column <= lastColumn; ++column) {
            if (tile(column, row) == Tile::Floor) continue;
            const int left = column * tileSize, right = left + tileSize;
            if (deltaX > 0 && body.x + body.width <= left && nextX + body.width > left)
                nextX = left - body.width;
            if (deltaX < 0 && body.x >= right && nextX < right) nextX = right;
        }
    }
    body.x = nextX;
    float nextY = std::max(0.0f, std::min(body.y + deltaY, float(height()) - body.height));
    const int xFirst = std::max(0, cell(body.x));
    const int xLast = std::min(columns() - 1, cell(body.x + body.width));
    const int yFirst = std::max(0, cell(std::min(body.y, nextY)));
    const int yLast = std::min(rows() - 1, cell(std::max(body.y, nextY) + body.height));
    for (int row = yFirst; row <= yLast; ++row) {
        const int top = row * tileSize, bottom = top + tileSize;
        for (int column = xFirst; column <= xLast; ++column) {
            if (tile(column, row) == Tile::Floor ||
                !overlaps(body.x, body.width, column * tileSize, (column + 1) * tileSize)) continue;
            if (deltaY > 0 && body.y + body.height <= top && nextY + body.height > top)
                nextY = top - body.height;
            if (deltaY < 0 && body.y >= bottom && nextY < bottom) nextY = bottom;
        }
    }
    body.y = nextY;
}
}
