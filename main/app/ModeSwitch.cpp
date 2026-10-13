#include "ModeSwitch.h"
#include "BootMode.h"
#include "input/ModeButton.h"
#include "transfer/TransferMode.h"
#include <Arduino.h>
#include <atomic>
#include <esp_system.h>
namespace ModeSwitch {
namespace {
std::atomic<bool> requested{false};
std::atomic<bool> abortTransfer{false};
std::atomic<bool> failed{false};
bool timing = false, saved = false, stopped = false;
uint32_t requestedAt = 0;
const char* failure = nullptr;
}
void request() { if (!failed.load()) requested.store(true); }
bool pending() { return requested.load(); }
bool shouldAbortTransfer() { return abortTransfer.load(); }
const char* error() { return failure ? failure : boot_mode_error(); }
const char* status() {
    if (error()) return error();
    if (!pending()) return nullptr;
    return boot_mode_get() == BOOT_MODE_GAME ? "Wechsel zur Dateiuebertragung ..." : "Wechsel zum Spiel ...";
}
void update() {
    if (!pending() || failure) return;
    const uint32_t now = millis();
    if (!timing) { requestedAt = now; timing = true; }
    if (now - requestedAt >= 15000) abortTransfer.store(true);
    const bool transfer = boot_mode_get() == BOOT_MODE_TRANSFER;
    if (transfer && TransferMode::busy()) return;
    // Antwort darf zuerst enden; erneut stabil losgelassenen KEY abwarten.
    if (now - requestedAt < 300 || !ModeButton::releasedStable()) return;
    if (!saved) {
        saved = boot_mode_save(transfer ? BOOT_MODE_GAME : BOOT_MODE_TRANSFER);
        if (!saved) {
            failure = boot_mode_error(); failed.store(true);
            requested.store(false); abortTransfer.store(false);
            return;
        }
    }
    if (transfer && !stopped) {
        stopped = TransferMode::stop();
        if (!stopped) return;
    }
    if (ModeButton::releasedStable()) esp_restart();
}
}
