#include "Room.h"
#include <algorithm>

namespace Room {
namespace {
Tile tiles[rows][columns]{};
bool overlaps(float start, float size, int minimum, int maximum) {
    return start < maximum && start + size > minimum;
}
}
void begin() {
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            tiles[row][column] = row == 0 || row == rows - 1 ||
                column == 0 || column == columns - 1 ? Tile::Ceiling :
                row <= 2 ? Tile::Wall : Tile::Floor;
        }
    }
}
Tile tile(int column, int row) { return tiles[row][column]; }
bool sameTile(int column, int row, Tile material) {
    return column >= 0 && column < columns && row >= 0 && row < rows &&
        tile(column, row) == material;
}
int rowY(int row) { return row == rows - 1 ? floorBottom : row * tileSize; }
int rowHeight(int row) { return row == rows - 2 ? floorBottom - rowY(row) : tileSize; }

void move(Box& body, float deltaX, float deltaY) {
    float nextX = std::max(0.0f, std::min(body.x + deltaX, float(width) - body.width));
    for (int row = 0; row < rows; ++row) {
        const int top = rowY(row), bottom = top + rowHeight(row);
        if (!overlaps(body.y, body.height, top, bottom)) continue;
        for (int column = 0; column < columns; ++column) {
            if (tile(column, row) == Tile::Floor) continue;
            const int left = column * tileSize, right = left + tileSize;
            if (deltaX > 0 && body.x + body.width <= left && nextX + body.width > left)
                nextX = left - body.width;
            if (deltaX < 0 && body.x >= right && nextX < right) nextX = right;
        }
    }
    body.x = nextX;
    float nextY = std::max(0.0f, std::min(body.y + deltaY, float(height) - body.height));
    for (int row = 0; row < rows; ++row) {
        const int top = rowY(row), bottom = top + rowHeight(row);
        for (int column = 0; column < columns; ++column) {
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
