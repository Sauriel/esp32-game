#include "TransferView.h"
#include "FileServer.h"
#include "storage/SdStorage.h"
#include "network/WifiConnection.h"
#include "app/ModeSwitch.h"
#include <Arduino.h>
#include <SPI.h>
#include <GD2.h>
#include <stdio.h>
namespace TransferView {
void render() {
    SdStorage::DisplayGuard guard;
    if (!guard.acquired()) return;
    GD.ClearColorRGB(15, 25, 65); GD.Clear(); GD.ColorRGB(255, 255, 255);
    GD.cmd_text(240, 25, 28, OPT_CENTER, "Dateiuebertragung");
    GD.cmd_text(240, 65, 26, OPT_CENTER, WifiConnection::status());
    GD.cmd_text(240, 95, 26, OPT_CENTER, WifiConnection::url());
    GD.cmd_text(240, 125, 26, OPT_CENTER, SdStorage::ready() ? "SD bereit" : "SD nicht verfuegbar");
    const auto progress = FileServer::progress();
    char text[64];
    snprintf(text, sizeof(text), "%s: %u / %u Bytes", progress.status,
        (unsigned)progress.completed, (unsigned)progress.total);
    GD.cmd_text(240, 155, 26, OPT_CENTER, text);
    const char* error = ModeSwitch::error();
    if (!error) error = FileServer::error();
    if (!error) error = SdStorage::error();
    if (error) GD.cmd_text(240, 190, 20, OPT_CENTER, error);
    GD.cmd_text(240, 235, 26, OPT_CENTER,
        ModeSwitch::pending() ? "Wechsel zum Spiel ..." : "KEY druecken/loslassen: Spiel");
    GD.swap(); GD.finish();
}
}
