#include "storage.h"

#include <Arduino.h>
#include <EEPROM.h>
#include <math.h>

#include "config.h"

namespace {

constexpr uint32_t kConfigMagic = 0x4E455244;  // "NERD"
constexpr size_t kConfigStorageSize = 256;
constexpr uint32_t kCatalogMagic = 0x4E434154;  // "NCAT"
constexpr uint16_t kCatalogVersion = 1;

struct __attribute__((packed)) CatalogHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint16_t entriesOffset;
  uint16_t namesOffset;
  uint16_t namesLength;
};

SystemConfig systemConfig{
    kConfigMagic,
    {2048, 2048},
    {0.0, 0.0, 0, 0},
    {0, 0},
    {3.0f, 1.0f, 1.0f},
    config::OBSERVER_LATITUDE_DEG,
    config::OBSERVER_LONGITUDE_DEG,
    60,
    false,
    false,
    false,
    0,
    {3.0f, 1.0f, 1.0f}};

static_assert(sizeof(storage::CatalogEntry) == 9, "CatalogEntry packing mismatch");
static_assert(sizeof(CatalogHeader) == 14, "CatalogHeader packing mismatch");

#include "catalog_data.inc"

constexpr size_t kCatalogHeaderOffset = kConfigStorageSize;
constexpr size_t kCatalogEntriesOffset = kCatalogHeaderOffset + sizeof(CatalogHeader);
constexpr size_t kCatalogNamesOffset =
    kCatalogEntriesOffset + kCatalogEntryCount * sizeof(storage::CatalogEntry);
constexpr size_t kCatalogStorageSize = sizeof(CatalogHeader) +
                                       kCatalogEntryCount * sizeof(storage::CatalogEntry) +
                                       kCatalogNameTableSize;
constexpr size_t kEepromSize = kConfigStorageSize + kCatalogStorageSize;

static_assert(kCatalogNamesOffset + kCatalogNameTableSize <= kEepromSize,
              "EEPROM size insufficient for catalog data");

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
  systemConfig.joystickCalibrated = false;
  systemConfig.axisCalibrated = false;
  systemConfig.polarAligned = false;
  systemConfig.lastRtcEpoch = 0;
  systemConfig.panningProfile.maxSpeedDegPerSec = 3.0f;
  systemConfig.panningProfile.accelerationDegPerSec2 = 1.0f;
  systemConfig.panningProfile.decelerationDegPerSec2 = 1.0f;
}

void saveConfigInternal() {
  EEPROM.put(0, systemConfig);
  EEPROM.commit();
}

bool catalogDataIsValid() {
  CatalogHeader header{};
  EEPROM.get(kCatalogHeaderOffset, header);
  return header.magic == kCatalogMagic && header.version == kCatalogVersion &&
         header.count == kCatalogEntryCount && header.entriesOffset == kCatalogEntriesOffset &&
         header.namesOffset == kCatalogNamesOffset && header.namesLength == kCatalogNameTableSize;
}

void writeCatalogToEeprom() {
  CatalogHeader header{kCatalogMagic,
                       kCatalogVersion,
                       static_cast<uint16_t>(kCatalogEntryCount),
                       static_cast<uint16_t>(kCatalogEntriesOffset),
                       static_cast<uint16_t>(kCatalogNamesOffset),
                       static_cast<uint16_t>(kCatalogNameTableSize)};
  EEPROM.put(kCatalogHeaderOffset, header);
  for (size_t i = 0; i < kCatalogEntryCount; ++i) {
    const auto& entry = kCatalogEntries[i];
    EEPROM.put(kCatalogEntriesOffset + i * sizeof(storage::CatalogEntry), entry);
  }
  for (size_t i = 0; i < kCatalogNameTableSize; ++i) {
    EEPROM.write(kCatalogNamesOffset + i, static_cast<uint8_t>(kCatalogNames[i]));
  }
  EEPROM.commit();
}

void ensureCatalogData() {
  if (!catalogDataIsValid()) {
    writeCatalogToEeprom();
  }
}

}  // namespace

namespace storage {

bool init() {
  EEPROM.begin(kEepromSize);
  EEPROM.get(0, systemConfig);
  if (systemConfig.magic != kConfigMagic || systemConfig.axisCalibration.stepsPerDegreeAz <= 0.0 ||
      systemConfig.axisCalibration.stepsPerDegreeAlt <= 0.0) {
    applyDefaults();
    saveConfigInternal();
  } else {
    if (systemConfig.gotoProfile.maxSpeedDegPerSec <= 0.0f ||
        systemConfig.gotoProfile.accelerationDegPerSec2 <= 0.0f ||
        systemConfig.gotoProfile.decelerationDegPerSec2 <= 0.0f) {
      systemConfig.gotoProfile.maxSpeedDegPerSec = 3.0f;
      systemConfig.gotoProfile.accelerationDegPerSec2 = 1.0f;
      systemConfig.gotoProfile.decelerationDegPerSec2 = 1.0f;
    }
    if (systemConfig.panningProfile.maxSpeedDegPerSec <= 0.0f ||
        systemConfig.panningProfile.accelerationDegPerSec2 <= 0.0f ||
        systemConfig.panningProfile.decelerationDegPerSec2 <= 0.0f) {
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
  }

  ensureCatalogData();
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

void save() { saveConfigInternal(); }

size_t getCatalogEntryCount() { return kCatalogEntryCount; }

bool readCatalogEntry(size_t index, CatalogEntry& entry) {
  if (index >= kCatalogEntryCount) {
    return false;
  }
  size_t address = kCatalogEntriesOffset + index * sizeof(CatalogEntry);
  EEPROM.get(address, entry);
  return true;
}

bool readCatalogName(uint16_t offset, uint8_t length, char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) {
    return false;
  }
  if (static_cast<size_t>(offset) + length > kCatalogNameTableSize) {
    return false;
  }
  if (bufferSize <= length) {
    return false;
  }
  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = static_cast<char>(EEPROM.read(kCatalogNamesOffset + offset + i));
  }
  buffer[length] = '\0';
  return true;
}

}  // namespace storage
