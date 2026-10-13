#pragma once
#include <stddef.h>
#include <stdint.h>

namespace PlayerPng {
// Begrenzter Streaming-Decoder fuer das 144x192-Sheet. Keine Hardwareabhaengigkeit.
using Read = int (*)(void*, uint8_t*, size_t);
using WriteRow = bool (*)(void*, unsigned, uint8_t*, size_t);
bool decode(void* context, Read read, WriteRow write, const char*& error);
size_t workspaceBytes();
}
