#include "RoomTiles.h"
#include "PlayerPng.h"
#include "TitleBackground.h"
#include "storage/SdStorage.h"
#include "app/BootMode.h"
#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <esp_log.h>

namespace RoomTiles {
namespace {
constexpr unsigned quarterSize = Room::tileSize / 2;
constexpr unsigned sheetWidth = 96;
constexpr uint32_t quarterBytes = quarterSize * quarterSize * 2;
constexpr uint32_t floorBytes = sheetWidth * 144 * 2;
constexpr uint32_t wallBytes = sheetWidth * 96 * 2;
constexpr uint32_t ceilingBytes = floorBytes;
constexpr uint32_t address = TitleBackground::address + TitleBackground::bytes;
constexpr uint32_t endAddress = address + floorBytes + wallBytes + ceilingBytes;
static_assert(address % 4 == 0 && quarterBytes % 4 == 0 && endAddress <= 256 * 1024,
              "Room tiles exceed RAM_G budget");
static_assert(Room::columns * Room::rows * 4 * 8 + 512 < 4096,
              "Room bitmap commands exceed reserved display-list budget");
struct Sheet {
    const char* path;
    unsigned height;
    uint32_t address;
    uint8_t handle;
    Room::Tile material;
    bool loaded = false;
};
Sheet sheets[] = {
    {"/tiles/floor.png", 144, address, 2, Room::Tile::Floor},
    {"/tiles/wall.png", 96, address + floorBytes, 3, Room::Tile::Wall},
    {"/tiles/ceiling.png", 144, address + floorBytes + wallBytes, 4, Room::Tile::Ceiling},
};
const char* failure = nullptr;
struct LoadContext {
    int fd;
    Sheet* sheet;
    bool heapLogged = false;
};
int read(void* context, uint8_t* data, size_t n) {
    return SdStorage::read(static_cast<LoadContext*>(context)->fd, data, n);
}
bool writeRow(void* context, unsigned y, uint8_t* data, size_t n) {
    auto& load = *static_cast<LoadContext*>(context);
    if (n != sheetWidth * 2) return false;
    if (!load.heapLogged) { app_log_heap("Tile-PNG-Workspace aktiv"); load.heapLogged = true; }
    SdStorage::DisplayGuard guard;
    if (!guard.acquired()) return false;
    // Quarter-major ARGB4: vier 24px-Spalten, danach die naechste Quarterzeile.
    for (unsigned column = 0; column < 4; ++column) {
        const unsigned cell = (y / quarterSize) * 4 + column;
        const uint32_t destination = load.sheet->address + cell * quarterBytes +
                                     (y % quarterSize) * quarterSize * 2;
        GD.wr_n(destination, data + column * quarterSize * 2, quarterSize * 2);
    }
    return true;
}
bool loadSheet(Sheet& sheet, const char*& reason) {
    struct stat info{};
    if (!SdStorage::statPath(sheet.path, info) || !S_ISREG(info.st_mode) ||
        info.st_size <= 0 || info.st_size > 256 * 1024) {
        reason = "Tile-PNG fehlt/Dateigroesse ungueltig";
        return false;
    }
    const int fd = SdStorage::openRead(sheet.path);
    if (fd < 0) { reason = "Tile-PNG nicht lesbar"; return false; }
    const uint32_t start = millis();
    LoadContext context{fd, &sheet};
    bool ok = PlayerPng::decode(&context, read, writeRow, reason, sheetWidth, sheet.height);
    if (!SdStorage::close(fd, false)) { ok = false; reason = "Tile-PNG Schliessen fehlgeschlagen"; }
    ESP_LOGI("RoomTiles", "%s load=%s file=%ld bytes time=%lu ms", sheet.path,
        ok ? "OK" : reason, long(info.st_size), (unsigned long)(millis() - start));
    return ok;
}

// MZ-Quarterkoordinaten: 96x144 Boden/Decke mit Vorschau in den oberen 48px,
// 96x96 Wand ohne Vorschau. Kein Counter-, Wasser- oder Animations-Autotile.
unsigned quarterCell(Room::Tile material, int column, int row, unsigned quadrant) {
    const bool right = quadrant & 1, bottom = quadrant & 2;
    const int dx = right ? 1 : -1, dy = bottom ? 1 : -1;
    const bool horizontal = Room::sameTile(column + dx, row, material);
    const bool vertical = Room::sameTile(column, row + dy, material);
    unsigned x = right ? 1 : 2;
    unsigned y = material == Room::Tile::Wall ? (bottom ? 1 : 2) : (bottom ? 3 : 4);
    if (!horizontal) x = right ? 3 : 0;
    if (!vertical) y = material == Room::Tile::Wall ? (bottom ? 3 : 0) : (bottom ? 5 : 2);
    if (material != Room::Tile::Wall && horizontal && vertical &&
        !Room::sameTile(column + dx, row + dy, material)) {
        // Innenecken im rechten oberen 48x48-Block des MZ-Sheets.
        x = right ? 3 : 2;
        y = bottom ? 1 : 0;
    }
    return y * 4 + x;
}
void drawSheet(const Sheet& sheet) {
    GD.SaveContext();
    GD.ColorRGB(255, 255, 255);
    if (sheet.loaded) {
        GD.BitmapHandle(sheet.handle);
        GD.BitmapLayout(ARGB4, quarterSize * 2, quarterSize);
        GD.Begin(BITMAPS);
    } else {
        GD.ColorRGB(sheet.material == Room::Tile::Floor ? 0x63705c :
                    sheet.material == Room::Tile::Wall ? 0x454c50 : 0x242930);
        GD.LineWidth(16);
        GD.Begin(RECTS);
    }
    for (int row = 0; row < Room::rows; ++row) {
        const unsigned halfHeight = Room::rowHeight(row) / 2;
        if (sheet.loaded) GD.BitmapSize(NEAREST, BORDER, BORDER, quarterSize, halfHeight);
        for (int column = 0; column < Room::columns; ++column) {
            if (Room::tile(column, row) != sheet.material) continue;
            const int x = column * Room::tileSize, y = Room::rowY(row);
            if (!sheet.loaded) {
                GD.Vertex2ii(x, y);
                GD.Vertex2ii(x + Room::tileSize, y + Room::rowHeight(row));
                continue;
            }
            for (unsigned quadrant = 0; quadrant < 4; ++quadrant) {
                const bool bottom = quadrant & 2;
                // Der vorletzte Raumstreifen ist 32px hoch: innen kuerzen,
                // aeussere obere/untere Autotile-Raender bleiben erhalten.
                const unsigned crop = bottom ? quarterSize - halfHeight : 0;
                GD.BitmapSource(sheet.address + quarterCell(sheet.material, column, row, quadrant) *
                                quarterBytes + crop * quarterSize * 2);
                GD.Vertex2ii(x + (quadrant & 1) * quarterSize,
                             y + (bottom ? halfHeight : 0), sheet.handle, 0);
            }
        }
    }
    GD.End();
    GD.RestoreContext();
}
}
bool load() {
    failure = nullptr;
    for (auto& sheet : sheets) sheet.loaded = false;
    if (!SdStorage::begin()) { failure = "Tiles: SD nicht lesbar"; return false; }
    app_log_heap("Vor Raum-Tiles Laden");
    for (auto& sheet : sheets) {
        const char* reason = nullptr;
        sheet.loaded = loadSheet(sheet, reason);
        if (!sheet.loaded) {
            ESP_LOGE("RoomTiles", "%s: %s", sheet.path, reason);
            if (!failure) failure = sheet.path; // Konkrete erste fehlerhafte Datei im Screen.
        }
    }
    app_log_heap("Nach Raum-Tiles Laden");
    ESP_LOGI("RoomTiles", "ARGB4 tiles=%lu RAM_G reserved=%lu/262144 rest=%lu workspace=%u",
        (unsigned long)(floorBytes + wallBytes + ceilingBytes), (unsigned long)endAddress,
        (unsigned long)(262144 - endAddress), unsigned(PlayerPng::workspaceBytes()));
    return failure == nullptr;
}
const char* error() { return failure; }
void drawGround() { drawSheet(sheets[0]); drawSheet(sheets[1]); }
void drawCeiling() { drawSheet(sheets[2]); }
}
