#include "Controller.h"

#include <Arduino.h>
#include <Bluepad32.h>

namespace GameInput {

    namespace {
        ControllerPtr controllers[BP32_MAX_GAMEPADS] = {};

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
                    controllers[i] = nullptr;
                    Console.printf("Controller getrennt: Slot %d\n", i);
                    return;
                }
            }
        }
    }

    void begin() {
        Console.printf("Bluepad32: %s\n", BP32.firmwareVersion());

        BP32.setup(&onConnected, &onDisconnected, true);

        BP32.enableVirtualDevice(false);
        BP32.enableBLEService(false);
    }

    void update() {
        BP32.update();
    }

    InputState getInput() {
        InputState input;

        for (auto ctl : controllers) {
            if (!ctl || !ctl->isConnected() || !ctl->isGamepad()) {
                continue;
            }

            input.moveX = ctl->axisX() / 512.0f;
            input.moveY = ctl->axisY() / 512.0f;
            input.action = ctl->a();

            break;
        }

        return input;
    }

}