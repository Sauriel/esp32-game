#include "ModeButton.h"
#ifndef HOST_TEST
#include <Arduino.h>
#include "config/BoardConfig.h"
#endif
namespace ModeButton {
void Debouncer::begin(bool pressed, uint32_t now) {
    raw = stable = pressed;
    armed = completedPress = false;
    changedAt = now;
}
bool Debouncer::update(bool pressed, uint32_t now) {
    if (pressed != raw) { raw = pressed; changedAt = now; }
    if (now - changedAt < 30) return false;
    if (!raw && !stable) armed = true;
    if (stable == raw) return false;
    stable = raw;
    if (stable) { completedPress = armed; return false; }
    const bool event = completedPress;
    armed = true;
    completedPress = false;
    return event;
}
bool Debouncer::releasedStable(uint32_t now) const {
    return !raw && !stable && now - changedAt >= 30;
}
#ifndef HOST_TEST
namespace { Debouncer button; }
void begin() {
    pinMode(BoardConfig::key, INPUT_PULLUP);
    button.begin(digitalRead(BoardConfig::key) == LOW, millis());
}
bool update() { return button.update(digitalRead(BoardConfig::key) == LOW, millis()); }
bool releasedStable() {
    // Auch zwischen Polling und Restart den realen Boot-Strapping-Pin pruefen.
    return digitalRead(BoardConfig::key) == HIGH && button.releasedStable(millis());
}
#endif
}
