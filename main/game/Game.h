#pragma once

#include "input/Controller.h"

namespace Game {
    void begin();
    // deltaTime ist die vergangene Zeit in Sekunden.
    void update(const GameInput::InputState& input, float deltaTime);
    void render(float fps);
}
