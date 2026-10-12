#include "Game.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>

namespace Game {

    namespace {
        constexpr int blockHalfSize = 10;
        constexpr int statusHeight = 32;
        constexpr float movementPerUpdate = 8.0f;
        constexpr uint32_t buttonMessageDurationMs = 1000;

        struct Block {
            float x;
            float y;
        };

        Block leftBlock = {160.0f, 136.0f};
        Block rightBlock = {320.0f, 136.0f};
        bool controllerConnected = false;
        const char* buttonMessage = nullptr;
        uint32_t buttonPressedAt = 0;

        struct ButtonLabel {
            GameInput::ButtonState GameInput::InputState::* button;
            const char* message;
        };

        constexpr ButtonLabel buttonLabels[] = {
            {&GameInput::InputState::cross, "X (Cross) Pressed"},
            {&GameInput::InputState::circle, "Circle Pressed"},
            {&GameInput::InputState::square, "Square Pressed"},
            {&GameInput::InputState::triangle, "Triangle Pressed"},
            {&GameInput::InputState::dpadUp, "D-Pad Up Pressed"},
            {&GameInput::InputState::dpadDown, "D-Pad Down Pressed"},
            {&GameInput::InputState::dpadLeft, "D-Pad Left Pressed"},
            {&GameInput::InputState::dpadRight, "D-Pad Right Pressed"},
            {&GameInput::InputState::l1, "L1 Pressed"},
            {&GameInput::InputState::r1, "R1 Pressed"},
            {&GameInput::InputState::l2, "L2 Pressed"},
            {&GameInput::InputState::r2, "R2 Pressed"},
            {&GameInput::InputState::l3, "L3 Pressed"},
            {&GameInput::InputState::r3, "R3 Pressed"},
            {&GameInput::InputState::share, "Share Pressed"},
            {&GameInput::InputState::options, "Options Pressed"},
            {&GameInput::InputState::ps, "PS Pressed"},
        };

        void moveBlock(Block& block, float x, float y) {
            block.x = constrain(block.x + x * movementPerUpdate,
                                float(blockHalfSize), float(480 - blockHalfSize));
            block.y = constrain(block.y + y * movementPerUpdate,
                                float(statusHeight + blockHalfSize), float(272 - blockHalfSize));
        }

        void drawBlock(const Block& block) {
            const int x = static_cast<int>(block.x);
            const int y = static_cast<int>(block.y);
            GD.Vertex2ii(x - blockHalfSize, y - blockHalfSize);
            GD.Vertex2ii(x + blockHalfSize, y + blockHalfSize);
        }
    }

    void begin() {
        leftBlock = {160.0f, 136.0f};
        rightBlock = {320.0f, 136.0f};
        controllerConnected = false;
        buttonMessage = nullptr;
        buttonPressedAt = 0;
    }

    void update(const GameInput::InputState& input) {
        controllerConnected = input.connected;
        if (!controllerConnected) {
            buttonMessage = nullptr;
            return;
        }

        moveBlock(leftBlock, input.moveX, input.moveY);
        moveBlock(rightBlock, input.lookX, input.lookY);

        const uint32_t now = millis();
        if (buttonMessage && now - buttonPressedAt >= buttonMessageDurationMs) {
            buttonMessage = nullptr;
        }
        for (const auto& label : buttonLabels) {
            if ((input.*label.button).pressed) {
                buttonMessage = label.message;
                buttonPressedAt = now;
                break; // Bei gleichzeitigen Flanken gilt die Tabellenreihenfolge.
            }
        }
    }

    void render() {
        GD.ClearColorRGB(15, 25, 65);
        GD.Clear();

        GD.Begin(RECTS);
        GD.ColorRGB(255, 180, 50);
        drawBlock(leftBlock);
        GD.ColorRGB(70, 190, 255);
        drawBlock(rightBlock);
        GD.End();

        GD.ColorRGB(255, 255, 255);
        const char* status = controllerConnected
            ? (buttonMessage ? buttonMessage : "Press a button")
            : "Controller disconnected";
        GD.cmd_text(240, statusHeight / 2, 26, OPT_CENTER, status);

        GD.swap();
    }

}
