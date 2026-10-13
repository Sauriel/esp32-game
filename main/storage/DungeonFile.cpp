#include "DungeonFile.h"
#include "SdStorage.h"
#include "world/LevelFormat.h"
#include <cstring>
#include <esp_log.h>

namespace DungeonFile {
namespace {
Room::Map candidate; // Zweiter fester Kartenpuffer statt grossem Task-Stack.
const char* failure = nullptr;
bool validPath(const char* path) {
    if (!path || strncmp(path, "/levels/", 8)) return false;
    const size_t length = strlen(path);
    if (length <= 12 || length >= SdStorage::pathCapacity || strcmp(path + length - 4, ".dng")) return false;
    for (size_t i = 8; i < length - 4; ++i) {
        const char c = path[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') &&
            !(c >= '0' && c <= '9') && c != '_' && c != '-') return false;
    }
    return true;
}
bool prepare(const char* path) {
    failure = nullptr;
    if (!validPath(path)) { failure = "Level: Pfad /levels/<Name>.dng erwartet"; return false; }
    if (!SdStorage::begin()) { failure = "Level: SD nicht lesbar"; return false; }
    return true;
}
int read(void* context, uint8_t* data, size_t n) {
    return SdStorage::read(*static_cast<int*>(context), data, n);
}
int write(void* context, const uint8_t* data, size_t n) {
    return SdStorage::write(*static_cast<int*>(context), data, n);
}
}
bool load(const char* path) {
    if (!prepare(path)) return false;
    struct stat info{};
    if (!SdStorage::statPath(path, info) || !S_ISREG(info.st_mode) || info.st_size <= 0 ||
        info.st_size > static_cast<off_t>(LevelFormat::maxFileBytes)) {
        failure = "Level: Datei fehlt/Groesse ungueltig"; return false;
    }
    int fd = SdStorage::openRead(path);
    if (fd < 0) { failure = "Level: Datei nicht lesbar"; return false; }
    bool ok = LevelFormat::decode(candidate, &fd, read, failure);
    if (!SdStorage::close(fd, false)) { ok = false; failure = "Level: Schliessen fehlgeschlagen"; }
    if (ok) ok = Room::use(candidate, failure);
    ESP_LOGI("DungeonFile", "load %s: %s", path, ok ? "OK" : failure);
    return ok;
}
bool save(const char* path) {
    if (!prepare(path) || !Room::validate(Room::map(), failure)) return false;
    int fd = SdStorage::create(path); // O_EXCL: vorhandene Level bleiben erhalten.
    if (fd < 0) { failure = "Level: Datei vorhanden/Anlegen fehlgeschlagen"; return false; }
    bool ok = LevelFormat::encode(Room::map(), &fd, write, failure);
    if (!SdStorage::close(fd, true)) { ok = false; failure = "Level: Sync/Schliessen fehlgeschlagen"; }
    if (!ok && !SdStorage::remove(path)) ESP_LOGE("DungeonFile", "Unvollstaendige Datei nicht entfernt: %s", path);
    ESP_LOGI("DungeonFile", "save %s: %s", path, ok ? "OK" : failure);
    return ok;
}
const char* error() { return failure; }
}
