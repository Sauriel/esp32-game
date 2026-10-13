#include <Arduino.h>
#include "app/BootMode.h"
#include "app/ModeSwitch.h"
#include "display/Display.h"
#include "app/GameMode.h"
#include "input/Controller.h"
#include "input/ModeButton.h"
#include "timing/FrameTiming.h"
#include "transfer/TransferMode.h"
void setup() {
    Serial.begin(115200);
    ModeButton::begin();
    Display::begin();
    app_log_heap("Display bereit");
    if (boot_mode_get() == BOOT_MODE_TRANSFER) {
        TransferMode::begin();
    } else {
        GameInput::begin();
        GameMode::begin();
        app_log_heap("Game bereit");
    }
}
void loop() {
    const float deltaTime = FrameTiming::beginFrame();
    if (ModeButton::update()) ModeSwitch::request();
    if (boot_mode_get() == BOOT_MODE_TRANSFER) {
        TransferMode::update();
    } else {
        GameInput::update();
        GameMode::update(GameInput::getInput(), deltaTime);
        GameMode::render(FrameTiming::fps(), ModeSwitch::status());
    }
    ModeSwitch::update();
    FrameTiming::endFrame();
}
