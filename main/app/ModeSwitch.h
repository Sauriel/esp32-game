#pragma once
namespace ModeSwitch {
void request(); // Thread-safe; KEY und HTTP nutzen denselben Wunsch.
bool pending();
bool shouldAbortTransfer();
void update(); // Nur Arduino-Task; Shutdown/Restart niemals im HTTP-Handler.
const char* error();
const char* status();
}
