#pragma once
#include <stddef.h>
#include <stdint.h>

namespace PlayerPng {
constexpr size_t maxFileBytes = 1024 * 1024;
// Begrenzter Streaming-Decoder; liefert ARGB4-Zeilen (little-endian).
using Read = int (*)(void*, uint8_t*, size_t);
using WriteRow = bool (*)(void*, unsigned, uint8_t*, size_t);
bool decode(void* context, Read read, WriteRow write, const char*& error,
            unsigned width = 144, unsigned height = 192,
            size_t fileLimit = 256 * 1024);
size_t workspaceBytes();
}
