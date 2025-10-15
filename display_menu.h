#pragma once

#include "role_config.h"

#if defined(DEVICE_ROLE_HID)

#include <Arduino.h>
#include <RTClib.h>
#include <time.h>

namespace display_menu {

void init();
void showBootMessage();
void showCalibrationStart();
void showCalibrationResult(int centerX, int centerY);
void showReady();
void startTask();
void prepareStartupLockPrompt(bool hasSavedLock);
void setOrientationKnown(bool known);
void handleInput();
void showInfo(const String& message, uint32_t durationMs = 3000);
void completePolarAlignment();
void startPolarAlignment();
void update();
void setSdAvailable(bool available);
void stopTracking();
void applyNetworkTime(time_t utcEpoch);

} // namespace display_menu

#endif  // DEVICE_ROLE_HID

