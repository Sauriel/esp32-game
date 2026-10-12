#pragma once

#include "input/Controller.h"

namespace Game {
    void begin();
    void update(const GameInput::InputState& input);
    void render();
}