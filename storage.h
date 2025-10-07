#pragma once

#include <Arduino.h>

#include "calibration.h"

struct SystemConfig {
  uint32_t magic;
  JoystickCalibration joystickCalibration;
  AxisCalibration axisCalibration;
  bool joystickCalibrated;
  bool axisCalibrated;
  bool polarAligned;
  uint32_t lastRtcEpoch;
};

namespace storage {

bool init();
const SystemConfig& getConfig();
void setJoystickCalibration(const JoystickCalibration& calibration);
void setAxisCalibration(const AxisCalibration& calibration);
void setPolarAligned(bool aligned);
void setRtcEpoch(uint32_t epoch);
void save();
bool isSdAvailable();

}  // namespace storage

