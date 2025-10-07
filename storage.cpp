#include "storage.h"

#include <EEPROM.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"

namespace {

constexpr uint32_t kConfigMagic = 0x4E455244;  // "NERD"
constexpr size_t kEepromSize = 256;

SystemConfig config{
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
  config.magic = kConfigMagic;
  config.joystickCalibration = {2048, 2048};
  config.axisCalibration.stepsPerDegreeAz = stepsPerAxisRev / 360.0;
  config.axisCalibration.stepsPerDegreeAlt = stepsPerAxisRev / 360.0;
  config.axisCalibration.azHomeOffset = 0;
  config.axisCalibration.altHomeOffset = 0;
  config.backlash = {0, 0};
  config.gotoProfile.maxSpeedDegPerSec = 3.0f;
  config.gotoProfile.accelerationDegPerSec2 = 1.0f;
  config.gotoProfile.decelerationDegPerSec2 = 1.0f;
  config.joystickCalibrated = false;
  config.axisCalibrated = false;
  config.polarAligned = false;
  config.lastRtcEpoch = 0;
}

void saveConfig() {
  EEPROM.put(0, config);
  EEPROM.commit();
}

}  // namespace

namespace storage {

bool init() {
  EEPROM.begin(kEepromSize);
  EEPROM.get(0, config);
  if (config.magic != kConfigMagic || config.axisCalibration.stepsPerDegreeAz <= 0.0 ||
      config.axisCalibration.stepsPerDegreeAlt <= 0.0) {
    applyDefaults();
    saveConfig();
  } else {
    if (config.gotoProfile.maxSpeedDegPerSec <= 0.0f || config.gotoProfile.accelerationDegPerSec2 <= 0.0f ||
        config.gotoProfile.decelerationDegPerSec2 <= 0.0f) {
      config.gotoProfile.maxSpeedDegPerSec = 3.0f;
      config.gotoProfile.accelerationDegPerSec2 = 1.0f;
      config.gotoProfile.decelerationDegPerSec2 = 1.0f;
    }
    if (config.backlash.azSteps < 0) config.backlash.azSteps = 0;
    if (config.backlash.altSteps < 0) config.backlash.altSteps = 0;
  }

  sdAvailable = SD.begin(config::SD_CS_PIN);
  return sdAvailable;
}

const SystemConfig& getConfig() { return config; }

void setJoystickCalibration(const JoystickCalibration& calibration) {
  config.joystickCalibration = calibration;
  config.joystickCalibrated = true;
  saveConfig();
}

void setAxisCalibration(const AxisCalibration& calibration) {
  config.axisCalibration = calibration;
  config.axisCalibrated = true;
  saveConfig();
}

void setBacklash(const BacklashConfig& backlash) {
  config.backlash = backlash;
  saveConfig();
}

void setGotoProfile(const GotoProfile& profile) {
  config.gotoProfile = profile;
  saveConfig();
}

void setPolarAligned(bool aligned) {
  config.polarAligned = aligned;
  saveConfig();
}

void setRtcEpoch(uint32_t epoch) {
  config.lastRtcEpoch = epoch;
  saveConfig();
}

void save() { saveConfig(); }

bool isSdAvailable() { return sdAvailable; }

}  // namespace storage

