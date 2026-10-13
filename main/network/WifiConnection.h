#pragma once
namespace WifiConnection {
void begin();
void update();
void stop();
bool connected();
const char* status();
const char* url();
}
