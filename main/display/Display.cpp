#include "Display.h"

#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>

namespace Display {

    namespace {
        constexpr int PIN_SCK   = 18;
        constexpr int PIN_MISO  = 19;
        constexpr int PIN_MOSI  = 23;
        constexpr int PIN_CS    = 5;
        constexpr int PIN_SD_CS = 27;
    }

    void begin() {
        pinMode(PIN_CS, OUTPUT);
        digitalWrite(PIN_CS, HIGH);

        pinMode(PIN_SD_CS, OUTPUT);
        digitalWrite(PIN_SD_CS, HIGH);

        delay(100);

        SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);

        GD.begin(0, PIN_CS, PIN_SD_CS);
    }

}