#include <Arduino.h>

#include "input/Controller.h"
#include "display/Display.h"
#include "game/Game.h"

void setup() {
    Display::begin();
    GameInput::begin();
    Game::begin();
}

void loop() {
    GameInput::update();

    Game::update(GameInput::getInput());
    Game::render();

    delay(100);
}