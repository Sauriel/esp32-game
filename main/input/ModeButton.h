#pragma once
#include <stdint.h>
namespace ModeButton {
// Reine Zustandsmaschine, auch ohne GPIO auf dem Host pruefbar.
class Debouncer {
public:
    void begin(bool pressed, uint32_t now);
    bool update(bool pressed, uint32_t now);
    bool releasedStable(uint32_t now) const;
private:
    bool raw = false, stable = false, armed = false, completedPress = false;
    uint32_t changedAt = 0;
};
void begin();
bool update();
bool releasedStable();
}
