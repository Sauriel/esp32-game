#include "GameMode.h"
#include "game/MainMenu.h"
#include "game/Game.h"
#include "game/GameTestScreen.h"
#include "assets/TitleBackground.h"
#include <Arduino.h>

namespace GameMode {
namespace {
enum class Screen { Menu, Play, Demo };
Screen screen = Screen::Menu;
}
void begin() {
    screen = Screen::Menu;
    MainMenu::begin();
    if (!TitleBackground::load()) Serial.println(TitleBackground::error());
}
void update(const GameInput::InputState& input, float deltaTime) {
    // Globale Rueckkehr hat Vorrang vor Menuebestaetigung und Szenenaktionen.
    if (input.connected && input.ps.pressed) {
        screen = Screen::Menu;
        MainMenu::begin();
        return;
    }
    if (screen == Screen::Menu) {
        const auto choice = MainMenu::update(input);
        if (choice == MainMenu::Choice::Play) {
            Game::begin(); screen = Screen::Play;
        } else if (choice == MainMenu::Choice::Demo) {
            GameTestScreen::begin(); screen = Screen::Demo;
        }
        return; // Bestaetigungsflanke nicht an die gestartete Ansicht weiterreichen.
    }
    if (input.connected && input.options.pressed) {
        screen = Screen::Menu;
        MainMenu::begin();
        return;
    }
    if (screen == Screen::Play) Game::update(input, deltaTime);
    else GameTestScreen::update(input, deltaTime);
}
void render(float fps, const char* statusOverride) {
    switch (screen) {
    case Screen::Menu: MainMenu::render(fps, statusOverride); break;
    case Screen::Play: Game::render(fps, statusOverride); break;
    case Screen::Demo: GameTestScreen::render(fps, statusOverride); break;
    }
}
}
