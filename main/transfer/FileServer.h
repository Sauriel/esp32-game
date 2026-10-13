#pragma once
#include <stdint.h>
namespace FileServer {
struct Progress { bool active; uint32_t completed; uint32_t total; const char* status; };
bool begin();
bool stop();
bool busy();
Progress progress();
const char* error();
// Arduino-Task: sparsame Diagnose auch bei blockiertem Datei-Worker.
void logUploadStatus();
}
