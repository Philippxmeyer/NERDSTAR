#pragma once

#include <Arduino.h>

namespace wifi_ota {

void init();
void setEnabled(bool enabled);
bool isEnabled();
const char* accessPointSsid();
const char* hostname();
void update();

}  // namespace wifi_ota

