#include "WifiConnection.h"
#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>
#if __has_include("config/WifiCredentials.h")
#include "config/WifiCredentials.h"
#else
#include "config/WifiCredentials.example.h"
#endif
namespace WifiConnection {
namespace {
constexpr uint32_t attemptMs = 10000, backoffMs = 5000;
constexpr unsigned maxAttempts = 3;
unsigned attempts = 0;
bool configured = false, online = false, trying = false;
uint32_t since = 0;
char address[48] = "";
const char* message = "WLAN nicht konfiguriert";
void attempt() {
    WiFi.begin(WifiCredentials::ssid, WifiCredentials::password);
    ++attempts; since = millis(); trying = true;
    message = "WLAN wird verbunden ...";
}
}
void begin() {
    configured = WifiCredentials::ssid[0] != '\0';
    if (!configured) return;
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    attempt();
}
void update() {
    if (!configured) return;
    if (WiFi.status() == WL_CONNECTED) {
        if (!online) {
            const auto ip = WiFi.localIP();
            snprintf(address, sizeof(address), "http://%u.%u.%u.%u/", ip[0], ip[1], ip[2], ip[3]);
            Serial.printf("Transfer: %s\n", address);
        }
        online = true; trying = false; attempts = 0;
        message = "WLAN verbunden"; return;
    }
    if (online) {
        online = false; address[0] = '\0'; since = millis();
        message = "WLAN-Verbindung verloren";
    }
    const uint32_t now = millis();
    if (trying && now - since >= attemptMs) {
        WiFi.disconnect(false, false); trying = false; since = now;
        message = "WLAN fehlgeschlagen; neuer Versuch";
    }
    if (!trying && attempts < maxAttempts && now - since >= backoffMs) attempt();
    if (!trying && attempts >= maxAttempts) message = "WLAN fehlgeschlagen (3 Versuche)";
}
void stop() { if (configured) { WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF); } online = false; }
bool connected() { return online; }
const char* status() { return message; }
const char* url() { return address; }
}
