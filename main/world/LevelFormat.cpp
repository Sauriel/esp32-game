#include "LevelFormat.h"
#include <cstdio>
#include <cstring>

namespace LevelFormat {
namespace {
struct Reader {
    void* context;
    Read read;
    uint8_t buffer[128]{};
    size_t offset = 0, count = 0, total = 0;
    int byte() {
        if (offset == count) {
            const int n = read(context, buffer, sizeof(buffer));
            if (n < 0 || n > int(sizeof(buffer))) return -2;
            if (!n) return -1;
            total += size_t(n);
            if (total > maxFileBytes) return -2;
            offset = 0; count = size_t(n);
        }
        return buffer[offset++];
    }
    bool line(char* output, size_t capacity) {
        size_t length = 0;
        for (;;) {
            const int c = byte();
            if (c < 0 || c == 0) return false;
            if (c == '\n') {
                if (length && output[length - 1] == '\r') --length;
                output[length] = 0;
                return true;
            }
            if (length + 1 >= capacity) return false;
            output[length++] = char(c);
        }
    }
};
bool number(const char*& p, uint32_t& result) {
    while (*p == ' ') ++p;
    if (*p < '0' || *p > '9') return false;
    result = 0;
    while (*p >= '0' && *p <= '9') {
        const unsigned digit = unsigned(*p++ - '0');
        if (result > (UINT32_MAX - digit) / 10) return false;
        result = result * 10 + digit;
    }
    return *p == ' ' || *p == 0;
}
bool writeAll(void* context, Write write, const char* text, size_t size) {
    const auto* data = reinterpret_cast<const uint8_t*>(text);
    while (size) {
        const int n = write(context, data, size);
        if (n <= 0 || size_t(n) > size) return false;
        data += n; size -= size_t(n);
    }
    return true;
}
}
bool decode(Room::Map& result, void* context, Read read, const char*& error) {
    Reader reader{context, read};
    char line[96];
    error = "Level: ungueltiger DNG1-Header";
    if (!reader.line(line, sizeof(line)) || strncmp(line, "DNG1 ", 5)) return false;
    const char* p = line + 5;
    uint32_t fields[5];
    for (auto& field : fields) if (!number(p, field)) return false;
    while (*p == ' ') ++p;
    if (*p) return false;
    error = "Level: ungueltige Kartengroesse/Startposition";
    if (fields[0] < 3 || fields[0] > Room::maxColumns || fields[1] < 3 ||
        fields[1] > Room::maxRows || fields[2] >= fields[0] || fields[3] >= fields[1]) return false;
    result.columns = uint16_t(fields[0]); result.rows = uint16_t(fields[1]);
    result.spawnColumn = uint16_t(fields[2]); result.spawnRow = uint16_t(fields[3]);
    result.seed = fields[4];
    for (int row = 0; row < result.rows; ++row) {
        error = "Level: Zeile fehlt/Laenge falsch/Lesefehler";
        if (!reader.line(line, sizeof(line)) || strlen(line) != result.columns) return false;
        for (int column = 0; column < result.columns; ++column) {
            Room::Tile material;
            switch (line[column]) {
            case '.': material = Room::Tile::Floor; break;
            case 'W': material = Room::Tile::Wall; break;
            case '#': material = Room::Tile::Ceiling; break;
            default: error = "Level: unbekanntes Tilezeichen"; return false;
            }
            result.tiles[row * result.columns + column] = material;
        }
    }
    error = "Level: Daten nach Raster/Dateilimit/Lesefehler";
    if (reader.byte() != -1) return false;
    return Room::validate(result, error);
}
bool encode(const Room::Map& map, void* context, Write write, const char*& error) {
    if (!Room::validate(map, error)) return false;
    char line[96];
    const int n = snprintf(line, sizeof(line), "DNG1 %u %u %u %u %lu\n",
        unsigned(map.columns), unsigned(map.rows), unsigned(map.spawnColumn),
        unsigned(map.spawnRow), (unsigned long)map.seed);
    error = "Level: Schreiben fehlgeschlagen";
    if (n < 0 || size_t(n) >= sizeof(line) || !writeAll(context, write, line, size_t(n))) return false;
    for (int row = 0; row < map.rows; ++row) {
        for (int column = 0; column < map.columns; ++column) {
            const auto tile = map.tiles[row * map.columns + column];
            line[column] = tile == Room::Tile::Floor ? '.' : tile == Room::Tile::Wall ? 'W' : '#';
        }
        line[map.columns] = '\n';
        if (!writeAll(context, write, line, map.columns + 1)) return false;
    }
    error = nullptr;
    return true;
}
}
