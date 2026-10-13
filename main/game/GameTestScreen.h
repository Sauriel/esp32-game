#pragma once
#include "input/Controller.h"

namespace GameTestScreen {
void begin();
void update(const GameInput::InputState& input, float deltaTime);
void render(float fps, const char* statusOverride = nullptr);
}
