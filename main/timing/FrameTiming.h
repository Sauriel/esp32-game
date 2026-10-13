#pragma once

namespace FrameTiming {
    // Startet den Frame und liefert Delta-Time in Sekunden (maximal 50 ms).
    float beginFrame();
    // Gemessene Frames/s; vor dem ersten Messfenster negativ.
    float fps();
    // Wartet die verbleibende Zeit bis zur Zielrate von maximal 60 Frames/s.
    void endFrame();
}
