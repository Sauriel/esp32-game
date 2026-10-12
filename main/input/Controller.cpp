#include "Controller.h"

#include <Arduino.h>
#include <Bluepad32.h>

namespace GameInput {

    namespace {
        ControllerPtr controllers[BP32_MAX_GAMEPADS] = {};
        ControllerPtr activeController = nullptr;
        InputState currentInput;

        // DS4-Parser: (Rohwert - 127) * 4, also -508..512.
        // 512 entspricht zugleich der Bluepad32-Stick-Normalisierung.
        constexpr float stickRange = 512.0f;
        constexpr float stickDeadzone = 0.12f;
        // DS4-Parser: 8-Bit-Trigger * 4, also 0..1020.
        constexpr float ds4TriggerRange = 1020.0f;

        float normalizeAxis(int32_t raw) {
            const float value = constrain(raw / stickRange, -1.0f, 1.0f);
            if (value > stickDeadzone) {
                return (value - stickDeadzone) / (1.0f - stickDeadzone);
            }
            if (value < -stickDeadzone) {
                return (value + stickDeadzone) / (1.0f - stickDeadzone);
            }
            return 0.0f;
        }

        ButtonState readButton(bool held, const ButtonState& previous) {
            return {held, held && !previous.held, !held && previous.held};
        }

        void onConnected(ControllerPtr ctl) {
            for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
                if (controllers[i] == nullptr) {
                    controllers[i] = ctl;

                    Console.printf(
                        "Controller verbunden: %s (Slot %d)\n",
                        ctl->getModelName(),
                        i
                    );

                    return;
                }
            }

            Console.println("Kein freier Controller-Slot");
        }

        void onDisconnected(ControllerPtr ctl) {
            for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
                if (controllers[i] == ctl) {
                    if (activeController == ctl) {
                        activeController = nullptr;
                    }
                    controllers[i] = nullptr;
                    Console.printf("Controller getrennt: Slot %d\n", i);
                    return;
                }
            }
        }
    }

    void begin() {
        currentInput = {};
        activeController = nullptr;
        Console.printf("Bluepad32: %s\n", BP32.firmwareVersion());

        BP32.setup(&onConnected, &onDisconnected, true);

        BP32.enableVirtualDevice(false);
        BP32.enableBLEService(false);
    }

    void update() {
        BP32.update();

        ControllerPtr selected = nullptr;
        for (auto ctl : controllers) {
            if (!ctl || !ctl->isConnected() || !ctl->isGamepad()) {
                continue;
            }

            selected = ctl;
            break;
        }

        // Ein Controllerwechsel darf keine Flanken zwischen zwei Spielern ableiten.
        // Beim Disconnect werden gehaltene Tasten einmal als released gemeldet.
        const InputState previous = selected && selected != activeController
            ? InputState{} : currentInput;
        InputState next;
        next.connected = selected != nullptr;
        if (selected) {
            next.moveX = normalizeAxis(selected->axisX());
            next.moveY = normalizeAxis(selected->axisY());
            next.lookX = normalizeAxis(selected->axisRX());
            next.lookY = normalizeAxis(selected->axisRY());
            const float triggerRange = selected->getModel() == Controller::CONTROLLER_TYPE_PS4Controller
                ? ds4TriggerRange : 1023.0f; // Bluepad32-Standard fuer andere Gamepads.
            next.leftTrigger = constrain(selected->brake() / triggerRange, 0.0f, 1.0f);
            next.rightTrigger = constrain(selected->throttle() / triggerRange, 0.0f, 1.0f);
        }

        const uint8_t dpad = selected ? selected->dpad() : 0;
        next.cross = readButton(selected && selected->a(), previous.cross);
        next.circle = readButton(selected && selected->b(), previous.circle);
        next.square = readButton(selected && selected->x(), previous.square);
        next.triangle = readButton(selected && selected->y(), previous.triangle);
        next.dpadUp = readButton(dpad & DPAD_UP, previous.dpadUp);
        next.dpadDown = readButton(dpad & DPAD_DOWN, previous.dpadDown);
        next.dpadLeft = readButton(dpad & DPAD_LEFT, previous.dpadLeft);
        next.dpadRight = readButton(dpad & DPAD_RIGHT, previous.dpadRight);
        next.l1 = readButton(selected && selected->l1(), previous.l1);
        next.r1 = readButton(selected && selected->r1(), previous.r1);
        next.l2 = readButton(selected && selected->l2(), previous.l2);
        next.r2 = readButton(selected && selected->r2(), previous.r2);
        next.l3 = readButton(selected && selected->thumbL(), previous.l3);
        next.r3 = readButton(selected && selected->thumbR(), previous.r3);
        next.share = readButton(selected && selected->miscSelect(), previous.share);
        next.options = readButton(selected && selected->miscStart(), previous.options);
        next.ps = readButton(selected && selected->miscSystem(), previous.ps);

        activeController = selected;
        currentInput = next;
    }

    InputState getInput() {
        return currentInput;
    }

}
