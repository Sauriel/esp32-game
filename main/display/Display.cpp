#include "Display.h"
#include "config/BoardConfig.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>

namespace Display {

    void begin() {
        pinMode(BoardConfig::displayCs, OUTPUT);
        digitalWrite(BoardConfig::displayCs, HIGH);

        pinMode(BoardConfig::sdCs, OUTPUT);
        digitalWrite(BoardConfig::sdCs, HIGH);

        delay(100);

        SPI.begin(BoardConfig::spiSck, BoardConfig::spiMiso, BoardConfig::spiMosi, BoardConfig::displayCs);

        GD.begin(0, BoardConfig::displayCs, BoardConfig::sdCs);
    }

    void resumeCommandStream() {
        GD.resume();
    }

    void endCommandStream() {
        GD.__end();
    }

}
