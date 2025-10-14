#pragma once

#include <Arduino.h>

namespace wifi_ota {

void init();
void setEnabled(bool enabled);
bool isEnabled();
const char* hostname();
bool credentialsConfigured();
bool isConnected();
const char* ssid();
void update();

}  // namespace wifi_ota

