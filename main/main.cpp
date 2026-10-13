#include "display/Display.h"
#include "game/Game.h"
#include "input/Controller.h"
#include "timing/FrameTiming.h"

void setup() {
    Display::begin();
    GameInput::begin();
    Game::begin();
}

void loop() {
    const float deltaTime = FrameTiming::beginFrame();

    GameInput::update();
    Game::update(GameInput::getInput(), deltaTime);
    Game::render(FrameTiming::fps());

    FrameTiming::endFrame();
}
