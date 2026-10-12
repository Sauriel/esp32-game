#pragma once

namespace GameInput {
    struct InputState {
        float moveX = 0.0f;
        float moveY = 0.0f;
        bool action = false;
    };

    void begin();
    void update();

    InputState getInput();
}