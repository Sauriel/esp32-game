#include "FileActions.h"
#include "storage/SdStorage.h"
#include <esp_log.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
extern "C" {
#include <ff.h>
}
namespace FileActions {
namespace {
constexpr size_t capacity = SdStorage::pathCapacity;
const char* actionName(Action action) {
    switch (action) {
        case Action::CreateDirectory: return "mkdir";
        case Action::Delete: return "delete";
        case Action::Move: return "move";
        case Action::Rename: return "rename";
    }
    return "invalid";
}
uint32_t codepoint(const unsigned char*& p) {
    const unsigned char c = *p++;
    if (c < 128) return c;
    unsigned count = c < 0xe0 ? 1 : (c < 0xf0 ? 2 : 3);
    uint32_t result = c & (count == 1 ? 31 : (count == 2 ? 15 : 7));
    while (count--) result = (result << 6) | (*p++ & 63);
    return result;
}
bool sameName(const char* a, const char* b) {
    auto left = reinterpret_cast<const unsigned char*>(a);
    auto right = reinterpret_cast<const unsigned char*>(b);
    while (*left && *right) {
        if (ff_wtoupper(codepoint(left)) != ff_wtoupper(codepoint(right))) return false;
    }
    return !*left && !*right;
}
bool parentOf(const char* path, char* parent) {
    if (!path || path[0] != '/' || strlen(path) >= capacity || !strcmp(path, "/")) return false;
    strcpy(parent, path);
    char* slash = strrchr(parent, '/');
    if (slash == parent) parent[1] = 0; else *slash = 0;
    return true;
}
// Auf tatsaechliche lange FAT-Namen aufloesen. Damit koennen weder andere
// Grossschreibung noch DOS-Kurznamensaliases den Unterordner-Schutz umgehen.
// Ein Iterator, keine unbeschraenkte Liste oder zusaetzlichen Dateihandles.
bool canonical(const char* input, char* output) {
    strcpy(output, "/");
    if (!strcmp(input, "/")) return true;
    const char* segment = input + 1;
    while (*segment) {
        const char* slash = strchr(segment, '/');
        const size_t length = slash ? size_t(slash - segment) : strlen(segment);
        if (!length || length >= SdStorage::nameCapacity) { errno = EINVAL; return false; }
        char wanted[SdStorage::nameCapacity], name[SdStorage::nameCapacity];
        memcpy(wanted, segment, length); wanted[length] = 0;
        if (!SdStorage::listBegin(output)) return false;
        bool found = false;
        int next;
        while ((next = SdStorage::listNext(name, sizeof(name))) > 0) {
            if (sameName(wanted, name)) { found = true; break; }
        }
        const int readError = errno;
        if (!SdStorage::listEnd()) return false;
        if (!found) { errno = next < 0 ? readError : ENOENT; return false; }
        const size_t used = strlen(output);
        const size_t separator = used > 1 ? 1 : 0;
        if (used + separator + strlen(name) >= capacity) { errno = ENAMETOOLONG; return false; }
        if (separator) strcat(output, "/");
        strcat(output, name);
        if (!slash) return true;
        segment = slash + 1;
    }
    errno = EINVAL; return false;
}
bool atOrBelow(const char* source, const char* target) {
    const size_t length = strlen(source);
    return !strncmp(source, target, length) && (target[length] == 0 || target[length] == '/');
}
Result failure(Error error, const char* message) { return {error, message}; }
Result perform(Action action, const char* source, const char* target) {
    if (!source || source[0] != '/' || !strcmp(source, "/"))
        return failure(Error::Invalid, "SD-Wurzel kann nicht geaendert werden");
    if (!SdStorage::ready()) return failure(Error::Storage, "SD nicht verfuegbar");
    char resolved[capacity], parent[capacity], destination[capacity];
    struct stat info;
    if (action == Action::CreateDirectory) {
        if (!parentOf(source, parent) || !canonical(parent, resolved))
            return failure(Error::NotFound, "Zielordner nicht gefunden");
        const int length = snprintf(destination, sizeof(destination), "%s%s%s", resolved,
            strcmp(resolved, "/") ? "/" : "", strrchr(source, '/') + 1);
        if (length < 0 || length >= int(sizeof(destination))) return failure(Error::Invalid, "Pfad zu lang");
        if (SdStorage::statPath(destination, info)) return failure(Error::Conflict, "Zielname existiert bereits");
        if (errno != ENOENT) return failure(Error::Storage, "Ziel nicht pruefbar");
        return SdStorage::makeDirectory(destination) ? Result{Error::None, "Ordner erstellt"}
            : failure(Error::Storage, "Ordner konnte nicht erstellt werden");
    }
    if (!canonical(source, resolved) || !SdStorage::statPath(resolved, info))
        return failure(Error::NotFound, "Quelle nicht gefunden oder nicht lesbar");
    if (action == Action::Delete) {
        if (S_ISDIR(info.st_mode)) {
            if (!SdStorage::listBegin(resolved)) return failure(Error::Storage, "Ordner nicht lesbar");
            char name[SdStorage::nameCapacity];
            bool empty = true;
            int next = 0;
            while ((next = SdStorage::listNext(name, sizeof(name))) > 0) {
                if (strcmp(name, ".") && strcmp(name, "..")) { empty = false; break; }
            }
            const bool closed = SdStorage::listEnd();
            if (next < 0 || !closed) return failure(Error::Storage, "Ordner nicht pruefbar");
            if (!empty) return failure(Error::Conflict, "Ordner ist nicht leer; Inhalt zuerst loeschen");
            return SdStorage::removeDirectory(resolved) ? Result{Error::None, "Ordner geloescht"}
                : failure(Error::Storage, "Ordner konnte nicht geloescht werden");
        }
        return SdStorage::remove(resolved) ? Result{Error::None, "Datei geloescht"}
            : failure(Error::Storage, "Datei konnte nicht geloescht werden");
    }
    if (!target || !parentOf(target, parent) || !canonical(parent, destination))
        return failure(Error::NotFound, "Zielordner nicht gefunden");
    if (action == Action::Rename) {
        char sourceParent[capacity];
        parentOf(resolved, sourceParent);
        if (strcmp(sourceParent, destination)) return failure(Error::Invalid, "Umbenennen muss im selben Ordner erfolgen");
    }
    if (S_ISDIR(info.st_mode) && atOrBelow(resolved, destination))
        return failure(Error::Invalid, "Ordner kann nicht in sich selbst verschoben werden");
    const size_t used = strlen(destination), separator = used > 1 ? 1 : 0;
    const char* name = strrchr(target, '/') + 1;
    if (used + separator + strlen(name) >= capacity) return failure(Error::Invalid, "Zielpfad zu lang");
    if (separator) strcat(destination, "/");
    strcat(destination, name);
    if (SdStorage::statPath(destination, info)) return failure(Error::Conflict, "Zielname existiert bereits");
    if (errno != ENOENT) return failure(Error::Storage, "Ziel nicht pruefbar");
    return SdStorage::rename(resolved, destination) ? Result{Error::None, "Eintrag verschoben/umbenannt"}
        : failure(Error::Storage, "Verschieben/Umbenennen fehlgeschlagen");
}
}
Result execute(Action action, const char* source, const char* target) {
    ESP_LOGI("FileAction", "Start: action=%s source=%s target=%s", actionName(action), source ? source : "", target ? target : "");
    const auto result = perform(action, source, target);
    if (result.error == Error::None) ESP_LOGI("FileAction", "Complete: action=%s result=%s", actionName(action), result.message);
    else ESP_LOGW("FileAction", "Failed: action=%s result=%s errno=%d", actionName(action), result.message,
        result.error == Error::Storage || result.error == Error::NotFound ? errno : 0);
    return result;
}
}
