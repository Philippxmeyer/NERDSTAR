#pragma once

#include <Arduino.h>

#include "calibration.h"

namespace input {

void init();
JoystickCalibration calibrateJoystick();
void update();
float getJoystickNormalizedX();
float getJoystickNormalizedY();
bool consumeJoystickPress();
int consumeEncoderDelta();
bool consumeEncoderClick();
int getJoystickCenterX();
int getJoystickCenterY();
void setJoystickCalibration(const JoystickCalibration& calibration);

} // namespace input

