#pragma once

#include <Arduino.h>

#include "calibration.h"

enum class DstMode : uint8_t { Off = 0, On = 1, Auto = 2 };

struct SystemConfig {
  uint32_t magic;
  JoystickCalibration joystickCalibration;
  AxisCalibration axisCalibration;
  BacklashConfig backlash;
  GotoProfile gotoProfile;
  double observerLatitudeDeg;
  double observerLongitudeDeg;
  int32_t timezoneOffsetMinutes;
  DstMode dstMode;
  bool joystickCalibrated;
  bool axisCalibrated;
  bool polarAligned;
  uint32_t lastRtcEpoch;
  GotoProfile panningProfile;
  char wifiSsid[33];
  char wifiPassword[65];
};

namespace storage {

struct __attribute__((packed)) CatalogEntry {
  uint16_t nameOffset;
  uint8_t nameLength;
  uint16_t codeOffset;
  uint8_t codeLength;
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
void setDstMode(DstMode mode);
bool hasWifiCredentials();
void setWifiCredentials(const char* ssid, const char* password);
const char* wifiSsid();
const char* wifiPassword();
void save();
size_t getCatalogEntryCount();
bool readCatalogEntry(size_t index, CatalogEntry& entry);
bool readCatalogString(uint16_t offset, uint8_t length, char* buffer, size_t bufferSize);

}  // namespace storage

