#include "Game.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>

namespace Game {

    namespace {
        int playerX = 240;
        int playerY = 136;
    }

    void begin() {
        playerX = 240;
        playerY = 136;
    }

    void update(const GameInput::InputState& input) {
        playerX += static_cast<int>(input.moveX * 8.0f);
        playerY += static_cast<int>(input.moveY * 8.0f);

        playerX = constrain(playerX, 10, 470);
        playerY = constrain(playerY, 10, 262);
    }

    void render() {
        GD.ClearColorRGB(15, 25, 65);
        GD.Clear();

        GD.Begin(RECTS);
        GD.ColorRGB(255, 180, 50);
        GD.Vertex2ii(playerX - 10, playerY - 10);
        GD.Vertex2ii(playerX + 10, playerY + 10);

        GD.swap();
    }

}