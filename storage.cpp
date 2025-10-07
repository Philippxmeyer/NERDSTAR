#include "storage.h"

#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"

namespace {

constexpr uint32_t kConfigMagic = 0x4E455244;  // "NERD"
constexpr size_t kEepromSize = 256;

SystemConfig systemConfig{
    kConfigMagic,
    {2048, 2048},
    {0.0, 0.0, 0, 0},
    {0, 0},
    {3.0f, 1.0f, 1.0f},
    false,
    false,
    false,
    0};

bool sdAvailable = false;

void applyDefaults() {
  constexpr double stepsPerMotorRev = config::FULLSTEPS_PER_REV * config::MICROSTEPS;
  constexpr double stepsPerAxisRev = stepsPerMotorRev * config::GEAR_RATIO;
  systemConfig.magic = kConfigMagic;
  systemConfig.joystickCalibration = {2048, 2048};
  systemConfig.axisCalibration.stepsPerDegreeAz = stepsPerAxisRev / 360.0;
  systemConfig.axisCalibration.stepsPerDegreeAlt = stepsPerAxisRev / 360.0;
  systemConfig.axisCalibration.azHomeOffset = 0;
  systemConfig.axisCalibration.altHomeOffset = 0;
  systemConfig.backlash = {0, 0};
  systemConfig.gotoProfile.maxSpeedDegPerSec = 3.0f;
  systemConfig.gotoProfile.accelerationDegPerSec2 = 1.0f;
  systemConfig.gotoProfile.decelerationDegPerSec2 = 1.0f;
  systemConfig.joystickCalibrated = false;
  systemConfig.axisCalibrated = false;
  systemConfig.polarAligned = false;
  systemConfig.lastRtcEpoch = 0;
}

void saveConfig() {
  EEPROM.put(0, systemConfig);
  EEPROM.commit();
}

}  // namespace

namespace storage {

bool init() {
  EEPROM.begin(kEepromSize);
  EEPROM.get(0, systemConfig);
  if (systemConfig.magic != kConfigMagic || systemConfig.axisCalibration.stepsPerDegreeAz <= 0.0 ||
      systemConfig.axisCalibration.stepsPerDegreeAlt <= 0.0) {
    applyDefaults();
    saveConfig();
  } else {
    if (systemConfig.gotoProfile.maxSpeedDegPerSec <= 0.0f ||
        systemConfig.gotoProfile.accelerationDegPerSec2 <= 0.0f ||
        systemConfig.gotoProfile.decelerationDegPerSec2 <= 0.0f) {
      systemConfig.gotoProfile.maxSpeedDegPerSec = 3.0f;
      systemConfig.gotoProfile.accelerationDegPerSec2 = 1.0f;
      systemConfig.gotoProfile.decelerationDegPerSec2 = 1.0f;
    }
    if (systemConfig.backlash.azSteps < 0) systemConfig.backlash.azSteps = 0;
    if (systemConfig.backlash.altSteps < 0) systemConfig.backlash.altSteps = 0;
  }

  sdAvailable = SD.begin(config::SD_CS_PIN);
  return sdAvailable;
}

const SystemConfig& getConfig() { return systemConfig; }

void setJoystickCalibration(const JoystickCalibration& calibration) {
  systemConfig.joystickCalibration = calibration;
  systemConfig.joystickCalibrated = true;
  saveConfig();
}

void setAxisCalibration(const AxisCalibration& calibration) {
  systemConfig.axisCalibration = calibration;
  systemConfig.axisCalibrated = true;
  saveConfig();
}

void setBacklash(const BacklashConfig& backlash) {
  systemConfig.backlash = backlash;
  saveConfig();
}

void setGotoProfile(const GotoProfile& profile) {
  systemConfig.gotoProfile = profile;
  saveConfig();
}

void setPolarAligned(bool aligned) {
  systemConfig.polarAligned = aligned;
  saveConfig();
}

void setRtcEpoch(uint32_t epoch) {
  systemConfig.lastRtcEpoch = epoch;
  saveConfig();
}

void save() { saveConfig(); }

bool isSdAvailable() { return sdAvailable; }

}  // namespace storage

