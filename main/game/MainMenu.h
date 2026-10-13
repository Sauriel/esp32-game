#pragma once
#include "input/Controller.h"

namespace MainMenu {
enum class Choice { None, Play, Demo };
void begin();
Choice update(const GameInput::InputState& input);
void render(float fps, const char* statusOverride = nullptr);
}
