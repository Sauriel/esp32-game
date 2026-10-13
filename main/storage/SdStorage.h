#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
namespace SdStorage {
constexpr size_t pathCapacity = 240;
constexpr size_t nameCapacity = 128;
// Display Guard nimmt dieselbe Sperre wie alle SD-Operationen. Kein nesting.
class DisplayGuard {
public:
    DisplayGuard();
    ~DisplayGuard();
    bool acquired() const { return locked; }
private: bool locked;
};
bool begin();
void end();
bool ready();
const char* error();
// Alle Pfade sind bereits validierte SD-relative Pfade mit fuehrendem '/'.
bool statPath(const char* path, struct stat& info);
int openRead(const char* path);
int create(const char* path);
int read(int fd, void* data, size_t count);
int write(int fd, const void* data, size_t count);
bool close(int fd, bool sync);
bool remove(const char* path);
bool makeDirectory(const char* path);
bool removeDirectory(const char* path);
bool rename(const char* from, const char* to);
bool listBegin(const char* path);
// 1=Eintrag, 0=Ende, -1=Fehler. Ein einziger Iterator im Server-Task.
int listNext(char* name, size_t capacity);
bool listEnd();
}
