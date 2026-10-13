#include "SdStorage.h"
#include "config/BoardConfig.h"
#include "display/Display.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <esp_log.h>
namespace SdStorage {
namespace {
StaticSemaphore_t mutexBuffer;
SemaphoreHandle_t mutex = nullptr;
bool mounted = false;
const char* failure = nullptr;
DIR* directory = nullptr;
struct StorageGuard {
    explicit StorageGuard(const char* operation = "SD operation") {
        const int savedErrno = errno;
        unsigned waitedSeconds = 0;
        while (xSemaphoreTake(mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
            ESP_LOGW("SdStorage", "Waiting for SPI lock: operation=%s waited=%u s",
                operation, ++waitedSeconds);
        }
        if (waitedSeconds) ESP_LOGI("SdStorage", "SPI lock acquired: operation=%s", operation);
        errno = savedErrno;
    }
    ~StorageGuard() {
        const int savedErrno = errno;
        digitalWrite(BoardConfig::sdCs, HIGH); xSemaphoreGive(mutex);
        errno = savedErrno;
    }
};
bool fullPath(const char* path, char* output) {
    const int length = snprintf(output, pathCapacity + 4, "/sd%s", path);
    return length >= 0 && length < int(pathCapacity + 4);
}
}
DisplayGuard::DisplayGuard() : locked(false) {
    if (mutex && xSemaphoreTake(mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
        locked = true;
        digitalWrite(BoardConfig::sdCs, HIGH);
        SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
        Display::resumeCommandStream();
    }
}
DisplayGuard::~DisplayGuard() {
    if (locked) {
        Display::endCommandStream(); // Auch nach finish() bleibt GD2 sonst im Command-Stream.
        SPI.endTransaction();
        xSemaphoreGive(mutex);
    }
}
bool begin() {
    mutex = xSemaphoreCreateMutexStatic(&mutexBuffer);
    Display::endCommandStream(); // Noch kein Server/konkurrierender Task vorhanden.
    StorageGuard guard;
    // SPI.begin() in SD.begin() ist beim vorhandenen SPIClass idempotent.
    // GD_STORAGE bleibt deaktiviert; ausschliesslich dieser Besitzer mountet.
    mounted = SD.begin(BoardConfig::sdCs, SPI, 4000000, "/sd", 2, false);
    if (!mounted) {
        failure = "SD-Karte nicht lesbar (FAT/Verkabelung pruefen)";
        Serial.println(failure);
    }
    return mounted;
}
void end() { StorageGuard guard; if (mounted) SD.end(); mounted = false; }
bool ready() { return mounted; }
const char* error() { return failure; }
bool statPath(const char* path, struct stat& info) {
    StorageGuard guard; char full[pathCapacity + 4];
    return fullPath(path, full) && ::stat(full, &info) == 0;
}
int openRead(const char* path) {
    StorageGuard guard; char full[pathCapacity + 4];
    return fullPath(path, full) ? ::open(full, O_RDONLY) : -1;
}
int create(const char* path) {
    StorageGuard guard; char full[pathCapacity + 4];
    return fullPath(path, full) ? ::open(full, O_WRONLY | O_CREAT | O_EXCL, 0666) : -1;
}
int read(int fd, void* data, size_t count) { StorageGuard guard; return ::read(fd, data, count); }
int write(int fd, const void* data, size_t count) { StorageGuard guard("write"); return ::write(fd, data, count); }
bool close(int fd, bool sync) {
    StorageGuard guard("sync-close");
    int syncResult = 0, syncError = 0;
    if (sync) {
        ESP_LOGI("SdStorage", "fsync begin: fd=%d", fd);
        syncResult = ::fsync(fd);
        syncError = syncResult == 0 ? 0 : errno;
        ESP_LOGI("SdStorage", "fsync complete: result=%d errno=%d", syncResult, syncError);
    }
    ESP_LOGI("SdStorage", "close begin: fd=%d", fd);
    const int closeResult = ::close(fd);
    const int closeError = closeResult == 0 ? 0 : errno;
    ESP_LOGI("SdStorage", "close complete: result=%d errno=%d", closeResult, closeError);
    // Logging darf die vom Dateiserver ausgewertete Fehlernummer nicht ersetzen.
    errno = syncResult != 0 ? syncError : closeError;
    return syncResult == 0 && closeResult == 0;
}
bool remove(const char* path) {
    StorageGuard guard; char full[pathCapacity + 4];
    return fullPath(path, full) && ::unlink(full) == 0;
}
bool makeDirectory(const char* path) {
    StorageGuard guard("mkdir"); char full[pathCapacity + 4];
    return fullPath(path, full) && ::mkdir(full, 0777) == 0;
}
bool removeDirectory(const char* path) {
    StorageGuard guard("rmdir"); char full[pathCapacity + 4];
    return fullPath(path, full) && ::rmdir(full) == 0;
}
bool rename(const char* from, const char* to) {
    StorageGuard guard; char source[pathCapacity + 4], target[pathCapacity + 4];
    struct stat info;
    if (!fullPath(from, source) || !fullPath(to, target)) return false;
    if (::stat(target, &info) == 0) { errno = EEXIST; return false; }
    if (errno != ENOENT) return false;
    return ::rename(source, target) == 0;
}
bool listBegin(const char* path) {
    StorageGuard guard; char full[pathCapacity + 4];
    if (directory || !fullPath(path, full)) return false;
    directory = ::opendir(full); return directory != nullptr;
}
int listNext(char* name, size_t capacity) {
    StorageGuard guard;
    errno = 0;
    const auto entry = ::readdir(directory);
    if (!entry) return errno ? -1 : 0;
    if (strlen(entry->d_name) >= capacity) return -1;
    strcpy(name, entry->d_name); return 1;
}
bool listEnd() {
    StorageGuard guard;
    const bool ok = directory && ::closedir(directory) == 0;
    directory = nullptr; return ok;
}
}
