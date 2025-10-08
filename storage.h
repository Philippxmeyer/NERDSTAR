#pragma once

#include <Arduino.h>

#include "calibration.h"

struct SystemConfig {
  uint32_t magic;
  JoystickCalibration joystickCalibration;
  AxisCalibration axisCalibration;
  BacklashConfig backlash;
  GotoProfile gotoProfile;
  double observerLatitudeDeg;
  double observerLongitudeDeg;
  int32_t timezoneOffsetMinutes;
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
void setBacklash(const BacklashConfig& backlash);
void setGotoProfile(const GotoProfile& profile);
void setPolarAligned(bool aligned);
void setRtcEpoch(uint32_t epoch);
void setObserverLocation(double latitudeDeg, double longitudeDeg, int32_t timezoneMinutes);
void save();
bool isSdAvailable();

}  // namespace storage

