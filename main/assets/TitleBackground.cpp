#include "TitleBackground.h"
#include "PlayerPng.h"
#include "PlayerSprite.h"
#include "storage/SdStorage.h"
#include "app/BootMode.h"
#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <esp_log.h>
#include <cerrno>

namespace TitleBackground {
namespace {
constexpr size_t fileLimit = PlayerPng::maxFileBytes;
static_assert(address % 4 == 0 && address + bytes <= 256 * 1024,
              "Title and player exceed RAM_G budget");
bool loaded = false;
const char* failure = nullptr;
struct LoadContext { int fd; bool heapLogged = false; };
int read(void* context, uint8_t* data, size_t n) {
    return SdStorage::read(static_cast<LoadContext*>(context)->fd, data, n);
}
bool writeRow(void* context, unsigned y, uint8_t* data, size_t n) {
    if (n != sourceWidth * 2) return false;
    auto& load = *static_cast<LoadContext*>(context);
    if (!load.heapLogged) { app_log_heap("Titel-PNG-Workspace aktiv"); load.heapLogged = true; }
    if (y % 2) return true;
    // In-place auf halbe Aufloesung reduzieren, kein Bild-/Zeilen-Zusatzpuffer.
    for (unsigned x = 0; x < width; ++x) {
        data[x * 2] = data[x * 4];
        data[x * 2 + 1] = data[x * 4 + 1];
    }
    SdStorage::DisplayGuard guard;
    if (!guard.acquired()) return false;
    GD.wr_n(address + (y / 2) * width * 2, data, width * 2);
    return true;
}
}
bool load() {
    loaded = false; failure = nullptr;
    const uint32_t start = millis();
    if (!SdStorage::begin()) { failure = "Titelbild: SD nicht lesbar"; return false; }
    struct stat info{};
    if (!SdStorage::statPath("/system/title.png", info)) {
        ESP_LOGE("TitleBackground", "stat /system/title.png failed: errno=%d", errno);
        failure = "system/title.png fehlt/nicht lesbar"; return false;
    }
    if (!S_ISREG(info.st_mode)) {
        failure = "system/title.png ist keine Datei"; return false;
    }
    if (info.st_size <= 0) {
        failure = "system/title.png ist leer"; return false;
    }
    if (info.st_size > static_cast<off_t>(fileLimit)) {
        ESP_LOGE("TitleBackground", "PNG file=%ld bytes exceeds limit=%u bytes",
            long(info.st_size), unsigned(fileLimit));
        failure = "system/title.png groesser als 1 MiB"; return false;
    }
    const int fd = SdStorage::openRead("/system/title.png");
    if (fd < 0) { failure = "system/title.png nicht lesbar"; return false; }
    app_log_heap("Vor Titel-PNG-Laden");
    ESP_LOGI("TitleBackground", "Expected PNG=480x272 workspace=%u bytes", unsigned(PlayerPng::workspaceBytes()));
    LoadContext context{fd};
    loaded = PlayerPng::decode(&context, read, writeRow, failure, sourceWidth, sourceHeight, fileLimit);
    if (!SdStorage::close(fd, false)) { loaded = false; failure = "Titel-PNG Schliessen fehlgeschlagen"; }
    app_log_heap("Nach Titel-PNG-Laden");
    ESP_LOGI("TitleBackground", "load=%s file=%ld bytes time=%lu ms title=%lu player=%lu RAM_G rest=%lu",
        loaded ? "OK" : failure, long(info.st_size), (unsigned long)(millis() - start),
        (unsigned long)bytes, (unsigned long)address, (unsigned long)(262144 - address - bytes));
    return loaded;
}
const char* error() { return failure; }
void draw() {
    if (!loaded) return;
    GD.SaveContext();
    GD.ColorRGB(255, 255, 255);
    GD.BitmapHandle(1);
    GD.BitmapSource(address);
    GD.BitmapLayout(ARGB4, width * 2, height);
    GD.BitmapSize(NEAREST, BORDER, BORDER, sourceWidth, sourceHeight);
    // FT800 inverse Bitmaptransformation in 8.8: 0.5 = 128.
    GD.BitmapTransformA(128); GD.BitmapTransformB(0); GD.BitmapTransformC(0);
    GD.BitmapTransformD(0); GD.BitmapTransformE(128); GD.BitmapTransformF(0);
    GD.Begin(BITMAPS);
    GD.Vertex2ii(0, 0, 1, 0);
    GD.End();
    GD.RestoreContext();
}
}
