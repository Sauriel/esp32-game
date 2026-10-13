#include "FrameTiming.h"

#include <Arduino.h>

namespace FrameTiming {
    namespace {
        constexpr uint32_t targetFrameTimeUs = (1000000 + 59) / 60;
        constexpr uint32_t maxDeltaTimeUs = 50000;
        constexpr uint32_t fpsWindowUs = 1000000;

        bool started = false;
        uint32_t frameStartUs = 0;
        uint32_t sampleElapsedUs = 0;
        uint32_t sampleFrames = 0;
        float measuredFps = -1.0f;
    }

    float beginFrame() {
        const uint32_t now = micros();
        if (!started) {
            started = true;
            frameStartUs = now;
            return 0.0f;
        }

        // Unsigned-Differenz bleibt auch beim micros()-Ueberlauf korrekt.
        const uint32_t intervalUs = now - frameStartUs;
        frameStartUs = now;

        // Vollstaendige Intervalle zaehlen, unabhaengig vom Delta-Time-Limit.
        sampleElapsedUs += intervalUs;
        ++sampleFrames;
        if (sampleElapsedUs >= fpsWindowUs) {
            measuredFps = sampleFrames * 1000000.0f / sampleElapsedUs;
            sampleElapsedUs = 0;
            sampleFrames = 0;
        }

        return min(intervalUs, maxDeltaTimeUs) / 1000000.0f;
    }

    float fps() {
        return measuredFps;
    }

    void endFrame() {
        const uint32_t elapsedUs = micros() - frameStartUs;
        if (elapsedUs < targetFrameTimeUs) {
            // Auf ganze Millisekunden aufrunden, damit die Zielrate nicht ueberschritten wird.
            delay((targetFrameTimeUs - elapsedUs + 999) / 1000);
        } else {
            // Auch bei langsamen Frames anderen Tasks Rechenzeit geben.
            delay(1);
        }
    }
}
