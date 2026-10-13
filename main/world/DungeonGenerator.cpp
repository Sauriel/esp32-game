#include "DungeonGenerator.h"
#include <algorithm>

namespace DungeonGenerator {
static_assert(Room::maxColumns == 64 && Room::maxRows == 48,
              "Generator sectors must be adapted when map dimensions change");
namespace {
struct Random {
    uint32_t state;
    unsigned next(unsigned limit) {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return state % limit;
    }
};
struct Area { int x, y, width, height;
    int centerX() const { return x + width / 2; }
    int centerY() const { return y + height / 2; }
};
void floor(Room::Map& map, int x, int y) {
    map.tiles[y * map.columns + x] = Room::Tile::Floor;
}
void horizontal(Room::Map& map, int x1, int x2, int y) {
    for (int x = std::min(x1, x2) - 1; x <= std::max(x1, x2) + 1; ++x)
        for (int dy = -1; dy <= 1; ++dy) floor(map, x, y + dy);
}
void vertical(Room::Map& map, int x, int y1, int y2) {
    for (int y = std::min(y1, y2) - 1; y <= std::max(y1, y2) + 1; ++y)
        for (int dx = -1; dx <= 1; ++dx) floor(map, x + dx, y);
}
void connect(Room::Map& map, const Area& from, const Area& to, Random& random) {
    if (random.next(2)) {
        horizontal(map, from.centerX(), to.centerX(), from.centerY());
        vertical(map, to.centerX(), from.centerY(), to.centerY());
    } else {
        vertical(map, from.centerX(), from.centerY(), to.centerY());
        horizontal(map, from.centerX(), to.centerX(), to.centerY());
    }
}
}
void generate(Room::Map& map, uint32_t seed) {
    map.columns = Room::maxColumns; map.rows = Room::maxRows;
    map.seed = seed ? seed : 0x6d2b79f5U;
    Random random{map.seed};
    for (auto& tile : map.tiles) tile = Room::Tile::Ceiling;
    // Neun begrenzte Sektoren: Platz fuer Decke + zwei Wandreihen im Norden.
    Area areas[9];
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            auto& area = areas[row * 3 + column];
            area = {4 + column * 18 + int(random.next(3)),
                    4 + row * 13 + int(random.next(2)),
                    8 + int(random.next(5)), 5 + int(random.next(4))};
            for (int y = area.y; y < area.y + area.height; ++y)
                for (int x = area.x; x < area.x + area.width; ++x) floor(map, x, y);
        }
    }
    // Durchgehende Schlangenfolge garantiert einen zusammenhaengenden Boden.
    constexpr unsigned order[] = {0, 1, 2, 5, 4, 3, 6, 7, 8};
    for (unsigned i = 1; i < 9; ++i) connect(map, areas[order[i - 1]], areas[order[i]], random);
    // Zusaetzliche lokale Verbindungen erlauben Schleifen statt nur einer Kette.
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const int i = row * 3 + column;
            if (column < 2 && random.next(3) == 0) connect(map, areas[i], areas[i + 1], random);
            if (row < 2 && random.next(3) == 0) connect(map, areas[i], areas[i + 3], random);
        }
    }
    // Luecken unter drei Tiles mit Boden verbinden: zwei Wandreihen plus
    // mindestens eine Deckenreihe muessen zwischen Bodenstreifen Platz haben.
    for (int x = 1; x < map.columns - 1; ++x) {
        int previousFloor = -1;
        for (int y = 1; y < map.rows - 1; ++y) {
            if (map.tiles[y * map.columns + x] != Room::Tile::Floor) continue;
            if (previousFloor >= 0 && y - previousFloor <= 3)
                for (int fill = previousFloor + 1; fill < y; ++fill) floor(map, x, fill);
            previousFloor = y;
        }
    }
    // Nur solide Zellen umfaerben: Verbindungen bleiben unveraendert begehbar.
    for (int y = 1; y < map.rows - 1; ++y) {
        for (int x = 1; x < map.columns - 1; ++x) {
            auto& tile = map.tiles[y * map.columns + x];
            if (tile == Room::Tile::Ceiling &&
                (map.tiles[(y + 1) * map.columns + x] == Room::Tile::Floor ||
                 (y + 2 < map.rows && map.tiles[(y + 2) * map.columns + x] == Room::Tile::Floor)))
                tile = Room::Tile::Wall;
        }
    }
    map.spawnColumn = areas[0].centerX(); map.spawnRow = areas[0].centerY();
}
}
