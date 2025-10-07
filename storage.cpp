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
  config.axisCalibration.stepsPerHourRA = stepsPerAxisRev / 24.0;
  config.axisCalibration.stepsPerDegreeDEC = stepsPerAxisRev / 360.0;
  config.axisCalibration.raHomeOffset = 0;
  config.axisCalibration.decHomeOffset = 0;
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
  if (config.magic != kConfigMagic || config.axisCalibration.stepsPerHourRA <= 0.0 ||
      config.axisCalibration.stepsPerDegreeDEC <= 0.0) {
    applyDefaults();
    saveConfig();
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

