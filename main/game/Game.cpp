#include "Game.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <cstdio>
#include <cmath>
#include "assets/PlayerSprite.h"
#include "assets/RoomTiles.h"
#include "world/Room.h"
#include "storage/SdStorage.h"
#include "storage/DungeonFile.h"
#include "app/BootMode.h"
#include <esp_random.h>
#include <esp_log.h>

namespace Game {

    namespace {
        // Pixel pro Sekunde: zuvor 8 Pixel pro Update bei etwa 10 Updates/s.
        constexpr float moveSpeedPixelsPerSecond = 80.0f;

        constexpr int playerHalfSize = PlayerSprite::frameSize / 2;
        // 16x12px-Fussbox, unabhaengig vom 48x48px-Renderrechteck.
        constexpr float bodyWidth = 16.0f, bodyHeight = 12.0f;
        constexpr float bodyOffsetY = 12.0f;
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
            Room::Box body{player.x - bodyWidth / 2, player.y + bodyOffsetY,
                           bodyWidth, bodyHeight};
            Room::move(body, deltaX, deltaY);
            player.x = body.x + bodyWidth / 2;
            player.y = body.y - bodyOffsetY;
        }

        void startScene() {
            const auto& map = Room::map();
            player = {(map.spawnColumn + 0.5f) * Room::tileSize,
                      (map.spawnRow + 0.5f) * Room::tileSize - bodyOffsetY - bodyHeight / 2};
            direction = 0;
            walkStep = 0;
            walkElapsed = 0;
            walking = false;
            controllerConnected = false;
            buttonMessage = nullptr;
            buttonPressedAt = 0;
            if (!PlayerSprite::load()) Serial.println(PlayerSprite::error());
            if (!RoomTiles::load()) Serial.println(RoomTiles::error());
            app_log_heap("Dungeon bereit");
        }
    }

    void begin() { begin(esp_random()); }

    void begin(uint32_t seed) {
        app_log_heap("Vor Dungeon-Erzeugung");
        Room::generate(seed);
        ESP_LOGI("Game", "Dungeon v1 seed=%lu tiles=%dx%d world=%dx%d map=%u bytes two_buffers=%u",
            (unsigned long)Room::map().seed, Room::columns(), Room::rows(),
            Room::width(), Room::height(), unsigned(sizeof(Room::Map)), unsigned(2 * sizeof(Room::Map)));
        startScene();
    }

    bool beginFromLevel(const char* path) {
        if (!DungeonFile::load(path)) {
            Serial.println(DungeonFile::error());
            return false;
        }
        startScene();
        return true;
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
        const auto camera = Camera::follow(player.x, player.y);
        RoomTiles::drawGround(camera);

        PlayerSprite::draw(Camera::centerX - playerHalfSize,
                           Camera::centerY - playerHalfSize,
                           direction, walking ? walkColumns[walkStep] : 1);
        RoomTiles::drawCeiling(camera);
        if (!PlayerSprite::ready()) {
            GD.ColorRGB(255, 180, 50);
            GD.cmd_text(240, 184, 26, OPT_CENTER, PlayerSprite::error());
        }
        if (RoomTiles::error()) {
            GD.ColorRGB(255, 180, 50);
            GD.cmd_text(240, 242, 26, OPT_CENTER, "Tile fehlt/ungueltig:");
            GD.cmd_text(240, 260, 20, OPT_CENTER, RoomTiles::error());
        }

        GD.ColorRGB(0, 0, 0);
        GD.ColorA(180);
        GD.Begin(RECTS);
        GD.Vertex2ii(0, 0); GD.Vertex2ii(Camera::width, statusHeight);
        GD.End();
        GD.ColorA(255);
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
