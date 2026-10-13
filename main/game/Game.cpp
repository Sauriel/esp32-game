#include "Game.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <cstdio>
#include <cmath>
#include "assets/PlayerSprite.h"
#include "storage/SdStorage.h"

namespace Game {

    namespace {
        // Pixel pro Sekunde: zuvor 8 Pixel pro Update bei etwa 10 Updates/s.
        constexpr float moveSpeedPixelsPerSecond = 80.0f;

        constexpr int playerHalfSize = PlayerSprite::frameSize / 2;
        constexpr float walkFrameSeconds = 0.14f;
        constexpr uint8_t walkColumns[] = {1, 0, 1, 2};
        constexpr int statusHeight = 32;
        constexpr uint32_t buttonMessageDurationMs = 1000;

        struct Player {
            float x;
            float y;
        };

        Player player = {240.0f, 136.0f};
        uint8_t direction = 0; // Down, Left, Right, Up = Sheetreihen.
        unsigned walkStep = 0;
        float walkElapsed = 0;
        bool walking = false;
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

        void movePlayer(float deltaX, float deltaY) {
            player.x = constrain(player.x + deltaX,
                                 float(playerHalfSize), float(480 - playerHalfSize));
            player.y = constrain(player.y + deltaY,
                                 float(statusHeight + playerHalfSize), float(272 - playerHalfSize));
        }

    }

    void begin() {
        player = {240.0f, 136.0f};
        direction = 0;
        walkStep = 0;
        walkElapsed = 0;
        walking = false;
        controllerConnected = false;
        buttonMessage = nullptr;
        buttonPressedAt = 0;
        if (!PlayerSprite::load()) Serial.println(PlayerSprite::error());
    }

    void update(const GameInput::InputState& input, float deltaTime) {
        controllerConnected = input.connected;
        if (!controllerConnected) {
            buttonMessage = nullptr;
            walking = false; walkStep = 0; walkElapsed = 0;
            return;
        }

        const float movement = moveSpeedPixelsPerSecond * deltaTime;
        if (input.moveX != 0 || input.moveY != 0) {
            if (fabsf(input.moveX) > fabsf(input.moveY)) direction = input.moveX < 0 ? 1 : 2;
            else direction = input.moveY < 0 ? 3 : 0;
        }
        const Player before = player;
        movePlayer(input.moveX * movement, input.moveY * movement);
        walking = player.x != before.x || player.y != before.y;
        if (walking) {
            walkElapsed += deltaTime;
            while (walkElapsed >= walkFrameSeconds) {
                walkElapsed -= walkFrameSeconds;
                walkStep = (walkStep + 1) % 4;
            }
        } else {
            walkElapsed = 0; walkStep = 0;
        }

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

    void render(float fps, const char* statusOverride) {
        SdStorage::DisplayGuard guard;
        if (!guard.acquired()) return;
        GD.ClearColorRGB(15, 25, 65);
        GD.Clear();

        PlayerSprite::draw(static_cast<int>(player.x) - playerHalfSize,
                           static_cast<int>(player.y) - playerHalfSize,
                           direction, walking ? walkColumns[walkStep] : 1);
        if (!PlayerSprite::ready()) {
            GD.ColorRGB(255, 180, 50);
            GD.cmd_text(240, 136, 26, OPT_CENTER, PlayerSprite::error());
        }

        GD.ColorRGB(255, 255, 255);
        const char* status = controllerConnected
            ? (buttonMessage ? buttonMessage : "Press a button")
            : "Controller disconnected";
        if (statusOverride) status = statusOverride;
        GD.cmd_text(240, statusHeight / 2, 26, OPT_CENTER, status);

        char fpsText[20] = "FPS: --";
        if (fps >= 0.0f) {
            snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
        }
        GD.cmd_text(472, statusHeight / 2, 26, OPT_RIGHTX | OPT_CENTERY, fpsText);

        GD.swap();
        // swap() reiht nur Kommandos ein. Vor dem naechsten Frame abarbeiten,
        // damit alte Bilder keine sichtbare Eingabeverzoegerung aufbauen.
        GD.finish();
    }

}
