#pragma once

#include "role_config.h"

#if defined(DEVICE_ROLE_HID)

#include <Arduino.h>

#include "calibration.h"

namespace input {

void init();
JoystickCalibration calibrateJoystick();
void update();
float getJoystickNormalizedX();
float getJoystickNormalizedY();
bool consumeJoystickPress();
bool isJoystickButtonPressed();
int consumeEncoderDelta();
bool consumeEncoderClick();
int getJoystickCenterX();
int getJoystickCenterY();
void setJoystickCalibration(const JoystickCalibration& calibration);

} // namespace input

#endif  // DEVICE_ROLE_HID

