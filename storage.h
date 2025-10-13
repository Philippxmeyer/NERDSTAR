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
  GotoProfile panningProfile;
};

namespace storage {

struct __attribute__((packed)) CatalogEntry {
  uint16_t nameOffset;
  uint8_t nameLength;
  uint8_t typeIndex;
  uint16_t raHoursTimes1000;
  int16_t decDegreesTimes100;
  int8_t magnitudeTimes10;
};

bool init();
const SystemConfig& getConfig();
void setJoystickCalibration(const JoystickCalibration& calibration);
void setAxisCalibration(const AxisCalibration& calibration);
void setBacklash(const BacklashConfig& backlash);
void setGotoProfile(const GotoProfile& profile);
void setPanningProfile(const GotoProfile& profile);
void setPolarAligned(bool aligned);
void setRtcEpoch(uint32_t epoch);
void setObserverLocation(double latitudeDeg, double longitudeDeg, int32_t timezoneMinutes);
void save();
size_t getCatalogEntryCount();
bool readCatalogEntry(size_t index, CatalogEntry& entry);
bool readCatalogName(uint16_t offset, uint8_t length, char* buffer, size_t bufferSize);

}  // namespace storage

