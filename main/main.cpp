#include <Arduino.h>
#include <Bluepad32.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "input/Controller.h"
#include "display/Display.h"
#include "game/Game.h"

namespace {
    constexpr uint32_t FRAME_TIME_US = (1000000 + 59) / 60;
    constexpr float MAX_DELTA_TIME = 0.05f;
}

void setup() {
    Display::begin();
    GameInput::begin();
    Game::begin();
}

void loop() {
    static uint32_t lastFrameTime = micros();

    const uint32_t frameStart = micros();
    const uint32_t frameInterval = frameStart - lastFrameTime;

    // Vergangene Zeit seit dem letzten Frame in Sekunden
    float deltaTime = frameInterval / 1000000.0f;
    lastFrameTime = frameStart;

    // Große Zeitsprünge begrenzen, z. B. nach einer Verzögerung
    deltaTime = min(deltaTime, MAX_DELTA_TIME);

    GameInput::update();

    Game::update(GameInput::getInput(), deltaTime);
    Game::render();

#ifndef NDEBUG
    // Schleifenrate messen; GD.swap() garantiert keine sofortige Bildausgabe.
    static uint32_t reportStart = frameStart;
    static uint32_t frameCount = 0;
    static uint32_t worstFrameUs = 0;
    static uint32_t clampedFrames = 0;
    ++frameCount;
    worstFrameUs = max(worstFrameUs, frameInterval);
    if (frameInterval > 50000) {
        ++clampedFrames;
    }
    const uint32_t reportElapsed = frameStart - reportStart;
    if (reportElapsed >= 5000000) {
        Console.printf("Loop: %.1f FPS, worst %.1f ms, dt limited: %lu; "
                       "heap free/min/largest: %lu/%lu/%lu bytes\n",
                       (frameCount - 1) * 1000000.0f / reportElapsed,
                       worstFrameUs / 1000.0f,
                       static_cast<unsigned long>(clampedFrames),
                       static_cast<unsigned long>(esp_get_free_heap_size()),
                       static_cast<unsigned long>(esp_get_minimum_free_heap_size()),
                       static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
        reportStart = frameStart;
        frameCount = 1;
        worstFrameUs = 0;
        clampedFrames = 0;
    }
#endif

    // Auf maximal 60 FPS begrenzen
    const uint32_t elapsed = micros() - frameStart;

    if (elapsed < FRAME_TIME_US) {
        // Aufrunden: Abrunden kann die Zielrate ueberschreiten oder delay(0) ergeben.
        delay((FRAME_TIME_US - elapsed + 999) / 1000);
    } else {
        // Auch bei langsamen Frames andere Tasks und den Idle-Task laufen lassen.
        delay(1);
    }
}
