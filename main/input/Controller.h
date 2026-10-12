#pragma once

namespace GameInput {
    struct ButtonState {
        bool held = false;
        bool pressed = false;   // Nur im Update beim Druecken gesetzt.
        bool released = false;  // Nur im Update beim Loslassen gesetzt.
    };

    struct InputState {
        bool connected = false;

        // Linker/rechter Stick: [-1, 1], mit Deadzone; +Y zeigt nach unten.
        float moveX = 0.0f;
        float moveY = 0.0f;
        float lookX = 0.0f;
        float lookY = 0.0f;
        // Analoge Trigger: [0, 1]. Digitale Tasten separat als l2/r2.
        float leftTrigger = 0.0f;
        float rightTrigger = 0.0f;

        ButtonState cross;
        ButtonState circle;
        ButtonState square;
        ButtonState triangle;
        ButtonState dpadUp;
        ButtonState dpadDown;
        ButtonState dpadLeft;
        ButtonState dpadRight;
        ButtonState l1;
        ButtonState r1;
        ButtonState l2;
        ButtonState r2;
        ButtonState l3;
        ButtonState r3;
        ButtonState share;
        ButtonState options;
        ButtonState ps;

    };

    void begin();
    void update();

    // Wiederholtes Lesen veraendert den Snapshot und seine Flanken nicht.
    InputState getInput();
}
