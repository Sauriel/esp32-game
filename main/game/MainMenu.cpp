#include "MainMenu.h"
#include "assets/TitleBackground.h"
#include "storage/SdStorage.h"
#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <cstdio>

namespace MainMenu {
namespace {
unsigned selected = 0;
int previousDirection = 0;
bool controllerConnected = false;
bool awaitingNeutral = true;
}
void begin() {
    selected = 0;
    previousDirection = 0;
    controllerConnected = false;
    awaitingNeutral = true;
}
Choice update(const GameInput::InputState& input) {
    controllerConnected = input.connected;
    if (!input.connected) {
        previousDirection = 0; awaitingNeutral = true;
        return Choice::None;
    }
    const bool up = input.dpadUp.held || input.moveY < -0.5f;
    const bool down = input.dpadDown.held || input.moveY > 0.5f;
    const int direction = up == down ? 0 : (up ? -1 : 1);
    if (awaitingNeutral) {
        if (!up && !down && !input.cross.held && !input.ps.held) awaitingNeutral = false;
        return Choice::None;
    }
    if (direction && direction != previousDirection) selected = 1 - selected;
    previousDirection = direction;
    if (input.cross.pressed) return selected == 0 ? Choice::Play : Choice::Demo;
    return Choice::None;
}
void render(float fps, const char* statusOverride) {
    SdStorage::DisplayGuard guard;
    if (!guard.acquired()) return;
    GD.ClearColorRGB(15, 25, 65);
    GD.Clear();
    TitleBackground::draw();

    GD.ColorRGB(0, 0, 0);
    GD.ColorA(180);
    GD.Begin(RECTS);
    GD.Vertex2ii(130, 84); GD.Vertex2ii(350, 210);
    GD.End();
    GD.ColorA(255);
    GD.ColorRGB(255, 255, 255);
    GD.cmd_text(240, 102, 27, OPT_CENTER, "Hauptmenue");
    const char* labels[] = {"Spielen", "Demo"};
    for (unsigned i = 0; i < 2; ++i) {
        const int y = 144 + i * 42;
        if (i == selected) {
            GD.ColorRGB(255, 180, 50);
            GD.cmd_text(160, y, 28, OPT_CENTER, ">");
        } else GD.ColorRGB(255, 255, 255);
        GD.cmd_text(240, y, 28, OPT_CENTER, labels[i]);
    }
    GD.ColorRGB(255, 255, 255);
    GD.cmd_text(240, 252, 26, OPT_CENTER, controllerConnected
        ? "Stick / D-Pad: Auswahl   X: Start" : "Controller verbinden");
    if (TitleBackground::error()) {
        GD.ColorRGB(255, 180, 50);
        GD.cmd_text(240, 226, 20, OPT_CENTER, TitleBackground::error());
    }
    GD.ColorRGB(255, 255, 255);
    if (statusOverride) GD.cmd_text(240, 16, 26, OPT_CENTER, statusOverride);
    char fpsText[20] = "FPS: --";
    if (fps >= 0.0f) snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);
    GD.cmd_text(472, 16, 26, OPT_RIGHTX | OPT_CENTERY, fpsText);
    GD.swap();
    GD.finish();
}
}
