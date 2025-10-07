#pragma once

#include <Arduino.h>

struct JoystickCalibration {
  int centerX;
  int centerY;
};

struct AxisCalibration {
  double stepsPerDegreeAz;
  double stepsPerDegreeAlt;
  int64_t azHomeOffset;
  int64_t altHomeOffset;
};

struct BacklashConfig {
  int32_t azSteps;
  int32_t altSteps;
};

struct GotoProfile {
  float maxSpeedDegPerSec;
  float accelerationDegPerSec2;
  float decelerationDegPerSec2;
};

