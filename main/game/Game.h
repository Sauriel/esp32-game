#pragma once

#include <stdint.h>
#include "input/Controller.h"

namespace Game {
    void begin();
    // Reproduzierbarer Random-Dungeon fuer einen konkreten Seed.
    void begin(uint32_t seed);
    // Vorbereitung fuer Story: bei Fehler bleibt die bisherige Szene erhalten.
    bool beginFromLevel(const char* path);
    // deltaTime ist die vergangene Zeit in Sekunden.
    void update(const GameInput::InputState& input, float deltaTime);
    void render(float fps, const char* statusOverride = nullptr);
}
