#pragma once

#include <Arduino.h>

#include "calibration.h"
#include "config.h"

enum class Axis {
  RA,
  DEC
};

namespace motion {

void init();
void setManualRate(Axis axis, float rpm);
void stopAll();
void setTrackingEnabled(bool enabled);
int64_t getStepCount(Axis axis);
void setStepCount(Axis axis, int64_t value);
double stepsToRaHours(int64_t steps);
double stepsToDecDegrees(int64_t steps);
int64_t raHoursToSteps(double hours);
int64_t decDegreesToSteps(double degrees);
void applyCalibration(const AxisCalibration& calibration);

} // namespace motion

