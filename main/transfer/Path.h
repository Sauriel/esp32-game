#pragma once
#include <stddef.h>
namespace TransferPath {
// Query-Komponente dekodieren, dann FAT-taugliche Pfadsegmente validieren.
bool decode(const char* encoded, char* output, size_t capacity);
bool temporary(const char* name);
}
