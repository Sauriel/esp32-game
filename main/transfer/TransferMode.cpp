#include "TransferMode.h"
#include "FileServer.h"
#include "TransferView.h"
#include "storage/SdStorage.h"
#include "network/WifiConnection.h"
#include "app/BootMode.h"
#include "app/ModeSwitch.h"
#include <Arduino.h>
namespace TransferMode {
namespace { bool serving = false, attempted = false; uint32_t lastDraw = 0; }
void begin() {
    SdStorage::begin();
    app_log_heap("Transfer SD");
    WifiConnection::begin();
    app_log_heap("Transfer WLAN");
    TransferView::render();
}
void update() {
    FileServer::logUploadStatus();
    if (!ModeSwitch::pending()) {
        WifiConnection::update();
        if (WifiConnection::connected() && !attempted) {
            attempted = true; serving = FileServer::begin();
            app_log_heap("Transfer HTTP");
        }
        if (!WifiConnection::connected() && serving && !busy()) {
            if (FileServer::stop()) { serving = false; attempted = false; }
        }
    }
    if (millis() - lastDraw >= 100) { lastDraw = millis(); TransferView::render(); }
}
bool stop() {
    // Aufrufer ist Arduino-Task, kein HTTP-Handler. stop wartet auf Serverende.
    if (!FileServer::stop()) return false;
    SdStorage::end();
    WifiConnection::stop();
    return true;
}
bool busy() { return FileServer::busy(); }
}
