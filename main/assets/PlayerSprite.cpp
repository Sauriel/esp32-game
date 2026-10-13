#include "PlayerSprite.h"
#include "PlayerPng.h"
#include "storage/SdStorage.h"
#include "app/BootMode.h"
#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <esp_log.h>

namespace PlayerSprite {
namespace {
bool loaded = false;
const char* failure = nullptr;
struct LoadContext { int fd; bool heapLogged = false; };
int read(void* context, uint8_t* data, size_t n) {
    return SdStorage::read(static_cast<LoadContext*>(context)->fd, data, n);
}
bool writeRow(void* context, unsigned y, uint8_t* data, size_t) {
    auto& load = *static_cast<LoadContext*>(context);
    if (!load.heapLogged) { app_log_heap("PNG-Workspace aktiv"); load.heapLogged = true; }
    SdStorage::DisplayGuard guard;
    if (!guard.acquired()) return false;
    // Sheetzeile in drei Framezeilen umpacken; Frames danach ueber Cell auswaehlbar.
    for (unsigned column = 0; column < 3; ++column) {
        const unsigned cell = (y / frameSize) * 3 + column;
        GD.wr_n(cell * frameBytes + (y % frameSize) * frameSize * 2,
            data + column * frameSize * 2, frameSize * 2);
    }
    return true;
}
}
bool load() {
    loaded = false; failure = nullptr;
    const uint32_t start = millis();
    if (!SdStorage::begin()) { failure = "SD nicht lesbar"; return false; }
    struct stat info{};
    if (!SdStorage::statPath("/player.png", info) || !S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size > 256 * 1024) {
        failure = "player.png fehlt/Dateigroesse ungueltig"; return false;
    }
    int fd = SdStorage::openRead("/player.png");
    if (fd < 0) { failure = "player.png nicht lesbar"; return false; }
    app_log_heap("Vor PNG-Laden");
    ESP_LOGI("PlayerSprite", "PNG workspace=%u bytes", unsigned(PlayerPng::workspaceBytes()));
    LoadContext context{fd};
    loaded = PlayerPng::decode(&context, read, writeRow, failure);
    if (!SdStorage::close(fd, false)) { loaded = false; failure = "player.png Schliessen fehlgeschlagen"; }
    app_log_heap("Nach PNG-Laden");
    ESP_LOGI("PlayerSprite", "load=%s file=%ld bytes time=%lu ms RAM_G reserved=%lu/262144 rest=%lu",
        loaded ? "OK" : failure, long(info.st_size), (unsigned long)(millis() - start),
        (unsigned long)bytes, (unsigned long)(262144 - bytes));
    return loaded;
}
bool ready() { return loaded; }
const char* error() { return failure; }
void draw(int x, int y, uint8_t direction, uint8_t column) {
    if (!loaded || direction >= 4 || column >= 3) return;
    GD.ColorRGB(255, 255, 255);
    GD.BitmapHandle(0);
    GD.BitmapSource(0);
    GD.BitmapLayout(ARGB4, frameSize * 2, frameSize);
    GD.BitmapSize(NEAREST, BORDER, BORDER, frameSize, frameSize);
    GD.Begin(BITMAPS);
    GD.Vertex2ii(x, y, 0, direction * 3 + column);
    GD.End();
}
}
