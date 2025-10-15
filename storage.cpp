#include "storage.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>
#include <string.h>

#include "config.h"

namespace {

constexpr uint32_t kConfigMagic = 0x4E455244;  // "NERD"
constexpr size_t kConfigStorageSize = 256;
bool eepromReady = false;

SystemConfig systemConfig{
    kConfigMagic,
    {2048, 2048},
    {0.0, 0.0, 0, 0},
    {0, 0},
    {3.0f, 1.0f, 1.0f},
    config::OBSERVER_LATITUDE_DEG,
    config::OBSERVER_LONGITUDE_DEG,
    60,
    DstMode::Auto,
    false,
    false,
    false,
    0,
    {3.0f, 1.0f, 1.0f}};

static_assert(sizeof(SystemConfig) <= kConfigStorageSize, "SystemConfig too large for config storage");

static_assert(sizeof(storage::CatalogEntry) == 12, "CatalogEntry packing mismatch");

// `catalog_data.inc` is a generated binary blob that contains the default catalog data.
// It is produced from `data/catalog.xml` via `tools/build_catalog.py` and embedded into
// the firmware image so we can seed the emulated EEPROM on first boot.
#include "catalog_data.inc"

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
  systemConfig.observerLatitudeDeg = config::OBSERVER_LATITUDE_DEG;
  systemConfig.observerLongitudeDeg = config::OBSERVER_LONGITUDE_DEG;
  systemConfig.timezoneOffsetMinutes = 60;
  systemConfig.dstMode = DstMode::Auto;
  systemConfig.joystickCalibrated = false;
  systemConfig.axisCalibrated = false;
  systemConfig.polarAligned = false;
  systemConfig.lastRtcEpoch = 0;
  systemConfig.panningProfile.maxSpeedDegPerSec = 3.0f;
  systemConfig.panningProfile.accelerationDegPerSec2 = 1.0f;
  systemConfig.panningProfile.decelerationDegPerSec2 = 1.0f;
}

bool profileIsInvalid(const GotoProfile& profile) {
  return !isfinite(profile.maxSpeedDegPerSec) || profile.maxSpeedDegPerSec <= 0.0f ||
         !isfinite(profile.accelerationDegPerSec2) || profile.accelerationDegPerSec2 <= 0.0f ||
         !isfinite(profile.decelerationDegPerSec2) || profile.decelerationDegPerSec2 <= 0.0f;
}

void saveConfigInternal() {
  if (!eepromReady) {
    return;
  }
  EEPROM.put(0, systemConfig);
  EEPROM.commit();
}

}  // namespace

namespace storage {

bool init() {
  eepromReady = EEPROM.begin(kConfigStorageSize);
  if (!eepromReady) {
    applyDefaults();
    return false;
  }
  EEPROM.get(0, systemConfig);
  if (systemConfig.magic != kConfigMagic || systemConfig.axisCalibration.stepsPerDegreeAz <= 0.0 ||
      systemConfig.axisCalibration.stepsPerDegreeAlt <= 0.0) {
    applyDefaults();
    saveConfigInternal();
  } else {
    if (profileIsInvalid(systemConfig.gotoProfile)) {
      systemConfig.gotoProfile.maxSpeedDegPerSec = 3.0f;
      systemConfig.gotoProfile.accelerationDegPerSec2 = 1.0f;
      systemConfig.gotoProfile.decelerationDegPerSec2 = 1.0f;
    }
    if (profileIsInvalid(systemConfig.panningProfile)) {
      systemConfig.panningProfile.maxSpeedDegPerSec = 3.0f;
      systemConfig.panningProfile.accelerationDegPerSec2 = 1.0f;
      systemConfig.panningProfile.decelerationDegPerSec2 = 1.0f;
    }
    if (systemConfig.backlash.azSteps < 0) systemConfig.backlash.azSteps = 0;
    if (systemConfig.backlash.altSteps < 0) systemConfig.backlash.altSteps = 0;
    if (!isfinite(systemConfig.observerLatitudeDeg) || systemConfig.observerLatitudeDeg < -90.0 ||
        systemConfig.observerLatitudeDeg > 90.0) {
      systemConfig.observerLatitudeDeg = config::OBSERVER_LATITUDE_DEG;
    }
    if (!isfinite(systemConfig.observerLongitudeDeg) || systemConfig.observerLongitudeDeg < -180.0 ||
        systemConfig.observerLongitudeDeg > 180.0) {
      systemConfig.observerLongitudeDeg = config::OBSERVER_LONGITUDE_DEG;
    }
    if (systemConfig.timezoneOffsetMinutes < -720 || systemConfig.timezoneOffsetMinutes > 840) {
      systemConfig.timezoneOffsetMinutes = 60;
    }
    if (static_cast<uint8_t>(systemConfig.dstMode) > static_cast<uint8_t>(DstMode::Auto)) {
      systemConfig.dstMode = DstMode::Auto;
    }
  }
  return true;
}

const SystemConfig& getConfig() { return systemConfig; }

void setJoystickCalibration(const JoystickCalibration& calibration) {
  systemConfig.joystickCalibration = calibration;
  systemConfig.joystickCalibrated = true;
  saveConfigInternal();
}

void setAxisCalibration(const AxisCalibration& calibration) {
  systemConfig.axisCalibration = calibration;
  systemConfig.axisCalibrated = true;
  saveConfigInternal();
}

void setBacklash(const BacklashConfig& backlash) {
  systemConfig.backlash = backlash;
  saveConfigInternal();
}

void setGotoProfile(const GotoProfile& profile) {
  systemConfig.gotoProfile = profile;
  saveConfigInternal();
}

void setPanningProfile(const GotoProfile& profile) {
  systemConfig.panningProfile = profile;
  saveConfigInternal();
}

void setPolarAligned(bool aligned) {
  systemConfig.polarAligned = aligned;
  saveConfigInternal();
}

void setRtcEpoch(uint32_t epoch) {
  systemConfig.lastRtcEpoch = epoch;
  saveConfigInternal();
}

void setObserverLocation(double latitudeDeg, double longitudeDeg, int32_t timezoneMinutes) {
  systemConfig.observerLatitudeDeg = latitudeDeg;
  systemConfig.observerLongitudeDeg = longitudeDeg;
  systemConfig.timezoneOffsetMinutes = timezoneMinutes;
  saveConfigInternal();
}

void setDstMode(DstMode mode) {
  if (mode == systemConfig.dstMode) {
    return;
  }
  systemConfig.dstMode = mode;
  saveConfigInternal();
}

void save() { saveConfigInternal(); }

size_t getCatalogEntryCount() { return kCatalogEntryCount; }

bool readCatalogEntry(size_t index, CatalogEntry& entry) {
  if (index >= kCatalogEntryCount) {
    return false;
  }
  entry = kCatalogEntries[index];
  return true;
}

bool readCatalogString(uint16_t offset, uint8_t length, char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) {
    return false;
  }
  if (static_cast<size_t>(offset) + length > kCatalogStringTableSize) {
    return false;
  }
  if (bufferSize <= length) {
    return false;
  }
  memcpy(buffer, &kCatalogStrings[offset], length);
  buffer[length] = '\0';
  return true;
}

}  // namespace storage
