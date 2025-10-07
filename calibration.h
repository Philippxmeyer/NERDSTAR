#pragma once

#include <Arduino.h>

struct JoystickCalibration {
  int centerX;
  int centerY;
};

struct AxisCalibration {
  double stepsPerHourRA;
  double stepsPerDegreeDEC;
  int64_t raHomeOffset;
  int64_t decHomeOffset;
};

