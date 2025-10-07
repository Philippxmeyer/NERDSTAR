#include "display_menu.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Wire.h>
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#include "catalog.h"
#include "config.h"
#include "input.h"
#include "motion.h"
#include "planets.h"
#include "state.h"
#include "storage.h"

namespace display_menu {
namespace {

Adafruit_SSD1306 display(config::OLED_WIDTH, config::OLED_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
bool rtcAvailable = false;
bool sdAvailable = false;

enum class UiState {
  MainMenu,
  PolarAlign,
  SetupMenu,
  SetRtc,
  CatalogBrowser,
  AxisCalibration,
  GotoSpeed,
  BacklashCalibration,
};

UiState uiState = UiState::MainMenu;

struct RtcEditState {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
  int fieldIndex;
};

RtcEditState rtcEdit{2024, 1, 1, 0, 0, 0, 0};

struct AxisCalibrationState {
  int step;
  int64_t azZero;
  int64_t azReference;
  int64_t altZero;
  int64_t altReference;
};

AxisCalibrationState axisCal{0, 0, 0, 0, 0};

struct GotoProfileSteps {
  double maxSpeedAz;
  double accelerationAz;
  double decelerationAz;
  double maxSpeedAlt;
  double accelerationAlt;
  double decelerationAlt;
};

struct AxisGotoRuntime {
  int64_t finalTarget;
  int64_t compensatedTarget;
  double currentSpeed;
  int8_t desiredDirection;
  bool compensationPending;
  bool reachedFinalTarget;
};

struct GotoRuntimeState {
  bool active;
  AxisGotoRuntime az;
  AxisGotoRuntime alt;
  GotoProfileSteps profile;
  double estimatedDurationSec;
  uint32_t lastUpdateMs;
  DateTime startTime;
  double targetRaHours;
  double targetDecDegrees;
  int targetCatalogIndex;
};

struct TrackingState {
  bool active;
  double targetRaHours;
  double targetDecDegrees;
  int targetCatalogIndex;
  double offsetAzDeg;
  double offsetAltDeg;
  bool userAdjusting;
};

GotoRuntimeState gotoRuntime{false,
                             {0, 0, 0.0, 0, false, false},
                             {0, 0, 0.0, 0, false, false},
                             {0.0, 0.0, 0.0, 0.0, 0.0, 0.0},
                             0.0,
                             0,
                             DateTime(),
                             0.0,
                             0.0,
                             -1};

TrackingState tracking{false, 0.0, 0.0, -1, 0.0, 0.0, false};

struct GotoSpeedState {
  float maxSpeed;
  float acceleration;
  float deceleration;
  int fieldIndex;
};

struct BacklashCalibrationState {
  int step;
  int64_t azStart;
  int64_t azEnd;
  int64_t altStart;
  int64_t altEnd;
};

GotoSpeedState gotoSpeedState{0.0f, 0.0f, 0.0f, 0};
BacklashCalibrationState backlashState{0, 0, 0, 0, 0};

String selectedObjectName;
String gotoTargetName;

int mainMenuIndex = 0;
constexpr const char* kMainMenuItems[] = {
    "Status", "Polar Align", "Start Tracking", "Stop Tracking", "Catalog", "Goto Selected", "Setup"};
constexpr size_t kMainMenuCount = sizeof(kMainMenuItems) / sizeof(kMainMenuItems[0]);

int setupMenuIndex = 0;
constexpr const char* kSetupMenuItems[] = {
    "Set RTC", "Cal Joystick", "Cal Axes", "Goto Speed", "Cal Backlash", "Back"};
constexpr size_t kSetupMenuCount = sizeof(kSetupMenuItems) / sizeof(kSetupMenuItems[0]);

int catalogIndex = 0;

String infoMessage;
uint32_t infoUntil = 0;
portMUX_TYPE displayMux = portMUX_INITIALIZER_UNLOCKED;

float joystickScrollAccumulator = 0.0f;
uint32_t lastScrollUpdateMs = 0;
bool joystickRightLatched = false;
bool joystickLeftLatched = false;
bool joystickSelectEvent = false;
bool joystickBackEvent = false;

bool consumeJoystickSelectEvent() {
  bool triggered = joystickSelectEvent;
  joystickSelectEvent = false;
  return triggered;
}

bool consumeJoystickBackEvent() {
  bool triggered = joystickBackEvent;
  joystickBackEvent = false;
  return triggered;
}

void setUiState(UiState state) {
  uiState = state;
}

void drawHeader() {
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("NERDSTAR");
  if (rtcAvailable) {
    DateTime now = rtc.now();
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(config::OLED_WIDTH - w, 0);
    display.print(buffer);
  }
}

void formatRa(double hours, char* buffer, size_t length) {
  double normalized = fmod(hours, 24.0);
  if (normalized < 0.0) normalized += 24.0;
  int h = static_cast<int>(normalized);
  double minutesFloat = (normalized - h) * 60.0;
  int m = static_cast<int>(minutesFloat);
  int s = static_cast<int>((minutesFloat - m) * 60.0 + 0.5);
  if (s >= 60) {
    s -= 60;
    m += 1;
  }
  if (m >= 60) {
    m -= 60;
    h = (h + 1) % 24;
  }
  snprintf(buffer, length, "%02dh %02dm %02ds", h, m, s);
}

void formatDec(double degrees, char* buffer, size_t length) {
  char sign = degrees >= 0.0 ? '+' : '-';
  double absVal = fabs(degrees);
  int d = static_cast<int>(absVal);
  double minutesFloat = (absVal - d) * 60.0;
  int m = static_cast<int>(minutesFloat);
  int s = static_cast<int>((minutesFloat - m) * 60.0 + 0.5);
  if (s >= 60) {
    s -= 60;
    m += 1;
  }
  if (m >= 60) {
    m -= 60;
    d += 1;
  }
  snprintf(buffer, length, "%c%02d%c %02d' %02d\"", sign, d, 0xB0, m, s);
}

bool fetchInfoMessage(String& message) {
  bool active = false;
  portENTER_CRITICAL(&displayMux);
  if (!infoMessage.isEmpty()) {
    uint32_t now = millis();
    if (now <= infoUntil) {
      message = infoMessage;
      active = true;
    } else {
      infoMessage = "";
    }
  }
  portEXIT_CRITICAL(&displayMux);
  return active;
}

double degToRad(double degrees) { return degrees * DEG_TO_RAD; }

double radToDeg(double radians) { return radians * RAD_TO_DEG; }

double wrapAngle360(double degrees) {
  double wrapped = fmod(degrees, 360.0);
  if (wrapped < 0.0) wrapped += 360.0;
  return wrapped;
}

double wrapAngle180(double degrees) {
  double wrapped = fmod(degrees + 180.0, 360.0);
  if (wrapped < 0.0) wrapped += 360.0;
  return wrapped - 180.0;
}

double shortestAngularDistance(double from, double to) {
  double diff = wrapAngle180(to - from);
  return diff;
}

double applyAtmosphericRefraction(double geometricAltitudeDeg) {
  if (geometricAltitudeDeg < -1.0 || geometricAltitudeDeg > 90.0) {
    return geometricAltitudeDeg;
  }

  double altitudeWithOffset =
      geometricAltitudeDeg + 10.3 / (geometricAltitudeDeg + 5.11);
  double refractionArcMinutes =
      1.02 / tan(degToRad(altitudeWithOffset));
  return geometricAltitudeDeg + refractionArcMinutes / 60.0;
}

DateTime currentDateTime() {
  if (rtcAvailable) {
    return rtc.now();
  }
  if (storage::getConfig().lastRtcEpoch != 0) {
    return DateTime(storage::getConfig().lastRtcEpoch);
  }
  return DateTime(2024, 1, 1, 0, 0, 0);
}

double hourFraction(const DateTime& time) {
  return time.hour() + time.minute() / 60.0 + time.second() / 3600.0;
}

double localSiderealDegrees(const DateTime& time) {
  double jd = planets::julianDay(time.year(), time.month(), time.day(), hourFraction(time));
  double T = (jd - 2451545.0) / 36525.0;
  double lst = 280.46061837 + 360.98564736629 * (jd - 2451545.0) +
               0.000387933 * T * T - (T * T * T) / 38710000.0 + config::OBSERVER_LONGITUDE_DEG;
  return wrapAngle360(lst);
}

void getObjectRaDecAt(const CatalogObject& object,
                      const DateTime& when,
                      double secondsAhead,
                      double& raHours,
                      double& decDegrees,
                      DateTime* futureTime) {
  DateTime future = when + TimeSpan(0, 0, 0, static_cast<int32_t>(secondsAhead));
  double fractional = secondsAhead - floor(secondsAhead);
  double hour = future.hour() + future.minute() / 60.0 +
                (future.second() + fractional) / 3600.0;
  raHours = object.raHours;
  decDegrees = object.decDegrees;

  if (futureTime) {
    *futureTime = future;
  }

  PlanetId planetId;
  if (object.type.equalsIgnoreCase("planet") &&
      planets::planetFromString(object.name, planetId)) {
    double jd = planets::julianDay(future.year(), future.month(), future.day(), hour);
    PlanetPosition position;
    if (planets::computePlanet(planetId, jd, position)) {
      raHours = position.raHours;
      decDegrees = position.decDegrees;
    }
  }
}

bool raDecToAltAz(const DateTime& when,
                  double raHours,
                  double decDegrees,
                  double& azimuthDeg,
                  double& altitudeDeg) {
  double lstDeg = localSiderealDegrees(when);
  double raDeg = raHours * 15.0;
  double haDeg = wrapAngle180(lstDeg - raDeg);
  double latRad = degToRad(config::OBSERVER_LATITUDE_DEG);
  double haRad = degToRad(haDeg);
  double decRad = degToRad(decDegrees);

  double sinAlt = sin(decRad) * sin(latRad) + cos(decRad) * cos(latRad) * cos(haRad);
  sinAlt = std::clamp(sinAlt, -1.0, 1.0);
  double geometricAltitudeDeg = radToDeg(asin(sinAlt));

  double cosAz = (sin(decRad) - sinAlt * sin(latRad)) /
                 (cos(degToRad(geometricAltitudeDeg)) * cos(latRad));
  cosAz = std::clamp(cosAz, -1.0, 1.0);
  double azRad = acos(cosAz);
  if (sin(haRad) > 0) {
    azRad = 2 * PI - azRad;
  }
  azimuthDeg = wrapAngle360(radToDeg(azRad));
  altitudeDeg = applyAtmosphericRefraction(geometricAltitudeDeg);
  return altitudeDeg > -5.0;  // allow slight tolerance below horizon
}

GotoProfileSteps toProfileSteps(const GotoProfile& profile, const AxisCalibration& cal) {
  GotoProfileSteps result{};
  result.maxSpeedAz = profile.maxSpeedDegPerSec * cal.stepsPerDegreeAz;
  result.accelerationAz = profile.accelerationDegPerSec2 * cal.stepsPerDegreeAz;
  result.decelerationAz = profile.decelerationDegPerSec2 * cal.stepsPerDegreeAz;
  result.maxSpeedAlt = profile.maxSpeedDegPerSec * cal.stepsPerDegreeAlt;
  result.accelerationAlt = profile.accelerationDegPerSec2 * cal.stepsPerDegreeAlt;
  result.decelerationAlt = profile.decelerationDegPerSec2 * cal.stepsPerDegreeAlt;
  return result;
}

double computeTravelTimeSteps(double distanceSteps,
                              double maxSpeed,
                              double accel,
                              double decel) {
  double distance = fabs(distanceSteps);
  if (distance < 1.0) {
    return 0.0;
  }
  maxSpeed = std::max(maxSpeed, 1.0);
  accel = std::max(accel, 1.0);
  decel = std::max(decel, 1.0);
  double distAccel = (maxSpeed * maxSpeed) / (2.0 * accel);
  double distDecel = (maxSpeed * maxSpeed) / (2.0 * decel);
  if (distance >= distAccel + distDecel) {
    double cruise = distance - distAccel - distDecel;
    return maxSpeed / accel + maxSpeed / decel + cruise / maxSpeed;
  }
  double peakSpeed = sqrt((2.0 * distance * accel * decel) / (accel + decel));
  return peakSpeed / accel + peakSpeed / decel;
}

void drawStatus() {
  double azDeg = motion::stepsToAzDegrees(motion::getStepCount(Axis::Az));
  double altDeg = motion::stepsToAltDegrees(motion::getStepCount(Axis::Alt));
  char azBuffer[24];
  char altBuffer[24];
  snprintf(azBuffer, sizeof(azBuffer), "%06.2f%c", azDeg, 0xB0);
  formatDec(altDeg, altBuffer, sizeof(altBuffer));

  display.setCursor(0, 10);
  display.print("Az: ");
  display.print(azBuffer);
  display.setCursor(0, 18);
  display.print("Alt: ");
  display.print(altBuffer);

  display.setCursor(0, 26);
  display.print("Align: ");
  display.print(systemState.polarAligned ? "Yes" : "No");
  display.print("  Trk: ");
  display.print(systemState.trackingActive ? "On" : "Off");

  if (!selectedObjectName.isEmpty()) {
    display.setCursor(0, 34);
    display.print("Sel: ");
    display.print(selectedObjectName);
  }
  if (systemState.gotoActive) {
    display.setCursor(0, 42);
    display.print("Goto: ");
    display.print(gotoTargetName);
  } else if (tracking.active) {
    display.setCursor(0, 42);
    display.print("Track: ");
    display.print(gotoTargetName);
  }
}

void drawMainMenu() {
  for (size_t i = 0; i < kMainMenuCount; ++i) {
    int y = 50 + static_cast<int>(i) * 8;
    if (y >= config::OLED_HEIGHT) {
      break;
    }
    bool selected = static_cast<int>(i) == mainMenuIndex;
    if (selected) {
      display.fillRect(0, y, config::OLED_WIDTH, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, y);
    display.print(kMainMenuItems[i]);
    if (selected) {
      display.setTextColor(SSD1306_WHITE);
    }
  }
}

void drawSetupMenu() {
  display.setCursor(0, 16);
  display.print("Setup");
  for (size_t i = 0; i < kSetupMenuCount; ++i) {
    int y = 26 + static_cast<int>(i) * 8;
    bool selected = static_cast<int>(i) == setupMenuIndex;
    if (selected) {
      display.fillRect(0, y, config::OLED_WIDTH, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, y);
    display.print(kSetupMenuItems[i]);
    if (selected) {
      display.setTextColor(SSD1306_WHITE);
    }
  }
}

void drawRtcEditor() {
  display.setCursor(0, 12);
  display.print("RTC Setup");
  int y = 24;
  const char* labels[] = {"Year", "Month", "Day", "Hour", "Min", "Sec"};
  int values[] = {rtcEdit.year, rtcEdit.month, rtcEdit.day, rtcEdit.hour, rtcEdit.minute, rtcEdit.second};
  for (int i = 0; i < 6; ++i) {
    bool selected = rtcEdit.fieldIndex == i;
    if (selected) {
      display.fillRect(0, y, config::OLED_WIDTH, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, y);
    display.printf("%s: %02d", labels[i], values[i]);
    if (selected) {
      display.setTextColor(SSD1306_WHITE);
    }
    y += 8;
  }
  display.setCursor(0, 60);
  display.print("Press enc=Save");
}

void drawCatalog() {
  display.setCursor(0, 10);
  display.print("Catalog");
  if (catalog::size() == 0) {
    display.setCursor(0, 20);
    display.print(sdAvailable ? "No entries" : "SD missing");
    return;
  }
  if (catalogIndex < 0) catalogIndex = 0;
  if (catalogIndex >= static_cast<int>(catalog::size())) {
    catalogIndex = catalog::size() - 1;
  }
  const CatalogObject* object = catalog::get(static_cast<size_t>(catalogIndex));
  if (!object) {
    display.setCursor(0, 20);
    display.print("Invalid entry");
    return;
  }
  DateTime now = currentDateTime();
  double ra;
  double dec;
  getObjectRaDecAt(*object, now, 0.0, ra, dec, nullptr);
  char raBuffer[24];
  char decBuffer[24];
  formatRa(ra, raBuffer, sizeof(raBuffer));
  formatDec(dec, decBuffer, sizeof(decBuffer));
  double azDeg = 0.0;
  double altDeg = -90.0;
  bool above = raDecToAltAz(now, ra, dec, azDeg, altDeg);
  display.setCursor(0, 20);
  display.print(object->name);
  display.setCursor(0, 28);
  display.print(object->type);
  display.setCursor(0, 36);
  display.print("RA: ");
  display.print(raBuffer);
  display.setCursor(0, 44);
  display.print("Dec: ");
  display.print(decBuffer);
  display.setCursor(0, 52);
  display.printf("Alt: %+.1f%c Joy=Exit", altDeg, 0xB0);
  display.setCursor(0, 60);
  display.printf("Mag: %.1f %s", object->magnitude, above ? "" : "(below)");
  display.setCursor(78, 60);
  display.print("Enc=Go");
}

void drawAxisCalibration() {
  display.setCursor(0, 12);
  display.print("Axis Cal");
  const char* steps[] = {
      "Set Az 0deg, enc",
      "Rotate +90deg, enc",
      "Set Alt 0deg, enc",
      "Rotate +45deg, enc"};
  int index = std::min(axisCal.step, 3);
  display.setCursor(0, 24);
  display.print(steps[index]);
}

void drawGotoSpeedSetup() {
  display.setCursor(0, 12);
  display.print("Goto Speed");
  const char* labels[] = {"Max [deg/s]", "Accel [deg/s2]", "Decel [deg/s2]"};
  float values[] = {gotoSpeedState.maxSpeed, gotoSpeedState.acceleration, gotoSpeedState.deceleration};
  int y = 24;
  for (int i = 0; i < 3; ++i) {
    bool selected = gotoSpeedState.fieldIndex == i;
    if (selected) {
      display.fillRect(0, y, config::OLED_WIDTH, 8, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(0, y);
    display.printf("%s: %4.1f", labels[i], values[i]);
    if (selected) {
      display.setTextColor(SSD1306_WHITE);
    }
    y += 8;
  }
  display.setCursor(0, 60);
  display.print("Joy=Next Enc=Save");
}

void drawBacklashCalibration() {
  display.setCursor(0, 12);
  display.print("Backlash Cal");
  const char* prompts[] = {"Az fwd pos, enc", "Az reverse, enc", "Alt fwd pos, enc", "Alt reverse, enc", "Done"};
  int idx = std::min(backlashState.step, 4);
  display.setCursor(0, 24);
  display.print(prompts[idx]);
  display.setCursor(0, 40);
  display.print("Use joy to move");
  display.setCursor(0, 56);
  display.print("Joy btn = abort");
}

void enterGotoSpeedSetup() {
  const GotoProfile& profile = storage::getConfig().gotoProfile;
  gotoSpeedState.maxSpeed = profile.maxSpeedDegPerSec;
  gotoSpeedState.acceleration = profile.accelerationDegPerSec2;
  gotoSpeedState.deceleration = profile.decelerationDegPerSec2;
  gotoSpeedState.fieldIndex = 0;
  setUiState(UiState::GotoSpeed);
}

void handleGotoSpeedInput(int delta) {
  if (delta != 0) {
    constexpr float step = 0.1f;
    switch (gotoSpeedState.fieldIndex) {
      case 0:
        gotoSpeedState.maxSpeed = std::clamp(gotoSpeedState.maxSpeed + delta * step, 0.5f, 20.0f);
        break;
      case 1:
        gotoSpeedState.acceleration = std::clamp(gotoSpeedState.acceleration + delta * step, 0.1f, 20.0f);
        break;
      case 2:
        gotoSpeedState.deceleration = std::clamp(gotoSpeedState.deceleration + delta * step, 0.1f, 20.0f);
        break;
    }
  }
  if (input::consumeJoystickPress()) {
    gotoSpeedState.fieldIndex = (gotoSpeedState.fieldIndex + 1) % 3;
  }
  if (consumeJoystickBackEvent()) {
    setUiState(UiState::SetupMenu);
    return;
  }
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (select) {
    GotoProfile profile{gotoSpeedState.maxSpeed, gotoSpeedState.acceleration, gotoSpeedState.deceleration};
    storage::setGotoProfile(profile);
    showInfo("Goto saved");
    setUiState(UiState::SetupMenu);
  }
}

void startBacklashCalibration() {
  backlashState = {0, 0, 0, 0, 0};
  setUiState(UiState::BacklashCalibration);
  showInfo("Az fwd pos");
}

void completeBacklashCalibration() {
  int32_t azSteps = static_cast<int32_t>(std::min<int64_t>(llabs(backlashState.azEnd - backlashState.azStart), INT32_MAX));
  int32_t altSteps = static_cast<int32_t>(std::min<int64_t>(llabs(backlashState.altEnd - backlashState.altStart), INT32_MAX));
  BacklashConfig config{azSteps, altSteps};
  storage::setBacklash(config);
  motion::setBacklash(config);
  showInfo("Backlash saved");
  setUiState(UiState::SetupMenu);
}

void handleBacklashCalibrationInput() {
  if (input::consumeJoystickPress() || consumeJoystickBackEvent()) {
    setUiState(UiState::SetupMenu);
    showInfo("Cal aborted");
    return;
  }
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (!select) {
    return;
  }
  switch (backlashState.step) {
    case 0:
      backlashState.azStart = motion::getStepCount(Axis::Az);
      backlashState.step = 1;
      showInfo("Reverse AZ");
      break;
    case 1:
      backlashState.azEnd = motion::getStepCount(Axis::Az);
      backlashState.step = 2;
      showInfo("Set Alt pos");
      break;
    case 2:
      backlashState.altStart = motion::getStepCount(Axis::Alt);
      backlashState.step = 3;
      showInfo("Reverse ALT");
      break;
    case 3:
      backlashState.altEnd = motion::getStepCount(Axis::Alt);
      backlashState.step = 4;
      completeBacklashCalibration();
      break;
    default:
      setUiState(UiState::SetupMenu);
      break;
  }
}

void render() {
  display.clearDisplay();
  drawHeader();

  String message;
  if (fetchInfoMessage(message)) {
    display.setCursor(0, 12);
    display.print(message);
    display.display();
    return;
  }

  switch (uiState) {
    case UiState::MainMenu:
      drawStatus();
      drawMainMenu();
      break;
    case UiState::PolarAlign:
      drawStatus();
      display.setCursor(0, 36);
      display.print("Center Polaris");
      display.setCursor(0, 44);
      display.print("Enc=Confirm");
      display.setCursor(0, 52);
      display.print("Joy=Abort");
      break;
    case UiState::SetupMenu:
      drawSetupMenu();
      break;
    case UiState::SetRtc:
      drawRtcEditor();
      break;
    case UiState::CatalogBrowser:
      drawCatalog();
      break;
    case UiState::AxisCalibration:
      drawAxisCalibration();
      break;
    case UiState::GotoSpeed:
      drawGotoSpeedSetup();
      break;
    case UiState::BacklashCalibration:
      drawBacklashCalibration();
      break;
  }

  display.display();
}

void displayTask(void*) {
  for (;;) {
    render();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

void enterSetupMenu() {
  setupMenuIndex = 0;
  setUiState(UiState::SetupMenu);
}

void enterRtcEditor() {
  DateTime now;
  if (rtcAvailable) {
    now = rtc.now();
  } else if (storage::getConfig().lastRtcEpoch != 0) {
    time_t epoch = storage::getConfig().lastRtcEpoch;
    now = DateTime(epoch);
  } else {
    now = DateTime(2024, 1, 1, 0, 0, 0);
  }
  rtcEdit = {now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second(), 0};
  setUiState(UiState::SetRtc);
}

void applyRtcEdit() {
  DateTime updated(rtcEdit.year, rtcEdit.month, rtcEdit.day, rtcEdit.hour, rtcEdit.minute, rtcEdit.second);
  if (rtcAvailable) {
    rtc.adjust(updated);
  }
  storage::setRtcEpoch(updated.unixtime());
  showInfo("RTC updated");
  setUiState(UiState::SetupMenu);
}

void startJoystickCalibrationFlow() {
  showCalibrationStart();
  auto calibration = input::calibrateJoystick();
  input::setJoystickCalibration(calibration);
  storage::setJoystickCalibration(calibration);
  showCalibrationResult(calibration.centerX, calibration.centerY);
  setUiState(UiState::SetupMenu);
}

void resetAxisCalibrationState() {
  axisCal = {0, 0, 0, 0, 0};
  setUiState(UiState::AxisCalibration);
}

void completeAxisCalibration() {
  double azSpan = fabs(static_cast<double>(axisCal.azReference - axisCal.azZero));
  double altSpan = fabs(static_cast<double>(axisCal.altReference - axisCal.altZero));
  double stepsPerAzDegree = azSpan / 90.0;
  double stepsPerAltDegree = altSpan / 45.0;
  if (stepsPerAzDegree < 1.0 || stepsPerAltDegree < 1.0) {
    showInfo("Cal failed");
    resetAxisCalibrationState();
    return;
  }
  AxisCalibration calibration;
  calibration.stepsPerDegreeAz = stepsPerAzDegree;
  calibration.stepsPerDegreeAlt = stepsPerAltDegree;
  calibration.azHomeOffset = axisCal.azZero;
  calibration.altHomeOffset = axisCal.altZero;
  storage::setAxisCalibration(calibration);
  motion::applyCalibration(calibration);
  showInfo("Axes calibrated");
  setUiState(UiState::SetupMenu);
}

void handleAxisCalibrationClick() {
  switch (axisCal.step) {
    case 0:
      axisCal.azZero = motion::getStepCount(Axis::Az);
      axisCal.step = 1;
      showInfo("Rotate +90deg");
      break;
    case 1:
      axisCal.azReference = motion::getStepCount(Axis::Az);
      axisCal.step = 2;
      showInfo("Set Alt 0");
      break;
    case 2:
      axisCal.altZero = motion::getStepCount(Axis::Alt);
      axisCal.step = 3;
      showInfo("Rotate +45deg");
      break;
    case 3:
      axisCal.altReference = motion::getStepCount(Axis::Alt);
      axisCal.step = 4;
      completeAxisCalibration();
      break;
    default:
      break;
  }
}

AxisGotoRuntime initAxisRuntime(Axis axis, int64_t targetSteps) {
  AxisGotoRuntime runtime{};
  runtime.finalTarget = targetSteps;
  runtime.compensatedTarget = targetSteps;
  runtime.currentSpeed = 0.0;
  runtime.desiredDirection = 0;
  runtime.compensationPending = false;
  runtime.reachedFinalTarget = false;

  int64_t current = motion::getStepCount(axis);
  int64_t diff = targetSteps - current;
  if (diff == 0) {
    runtime.reachedFinalTarget = true;
    return runtime;
  }

  runtime.desiredDirection = diff >= 0 ? 1 : -1;
  int8_t lastDir = motion::getLastDirection(axis);
  int32_t backlash = motion::getBacklashSteps(axis);
  if (backlash > 0 && lastDir != 0 && lastDir != runtime.desiredDirection) {
    runtime.compensatedTarget = targetSteps + runtime.desiredDirection * backlash;
    runtime.compensationPending = true;
  }
  return runtime;
}

bool updateAxisGoto(Axis axis,
                    AxisGotoRuntime& runtime,
                    double dt,
                    const GotoProfileSteps& profile) {
  if (runtime.reachedFinalTarget) {
    motion::setGotoStepsPerSecond(axis, 0.0);
    return true;
  }

  int64_t current = motion::getStepCount(axis);
  int64_t error = runtime.compensatedTarget - current;
  double absError = fabs(static_cast<double>(error));
  double direction = (error >= 0) ? 1.0 : -1.0;

  double maxSpeed = std::max(axis == Axis::Az ? profile.maxSpeedAz : profile.maxSpeedAlt, 1.0);
  double accel = std::max(axis == Axis::Az ? profile.accelerationAz : profile.accelerationAlt, 1.0);
  double decel = std::max(axis == Axis::Az ? profile.decelerationAz : profile.decelerationAlt, 1.0);

  double speed = runtime.currentSpeed;
  double distanceToStop = (speed * speed) / (2.0 * decel);

  if (absError <= 1.0 && speed < 1.0) {
    motion::setGotoStepsPerSecond(axis, 0.0);
    if (runtime.compensationPending) {
      runtime.compensationPending = false;
      runtime.compensatedTarget = runtime.finalTarget;
      runtime.currentSpeed = 0.0;
      return false;
    }
    runtime.reachedFinalTarget = true;
    return true;
  }

  if (absError <= distanceToStop + 1.0) {
    speed -= decel * dt;
    if (speed < 0.0) speed = 0.0;
  } else {
    speed += accel * dt;
    if (speed > maxSpeed) speed = maxSpeed;
  }

  if (speed < 1.0 && absError > 2.0) {
    speed = std::min(maxSpeed, speed + accel * dt);
  }

  double commanded = speed * direction;
  motion::setGotoStepsPerSecond(axis, commanded);
  runtime.currentSpeed = speed;
  return false;
}

bool computeTargetAltAz(const CatalogObject& object,
                        const DateTime& start,
                        double secondsAhead,
                        double& raHours,
                        double& decDegrees,
                        double& azDeg,
                        double& altDeg,
                        DateTime& targetTime) {
  getObjectRaDecAt(object, start, secondsAhead, raHours, decDegrees, &targetTime);
  return raDecToAltAz(targetTime, raHours, decDegrees, azDeg, altDeg);
}

void finalizeTrackingTarget(int catalogIndex,
                            double raHours,
                            double decDegrees,
                            double azDeg,
                            double altDeg) {
  tracking.active = true;
  tracking.targetCatalogIndex = catalogIndex;
  tracking.targetRaHours = raHours;
  tracking.targetDecDegrees = decDegrees;
  tracking.offsetAzDeg = wrapAngle180(motion::stepsToAzDegrees(motion::getStepCount(Axis::Az)) - azDeg);
  tracking.offsetAltDeg = motion::stepsToAltDegrees(motion::getStepCount(Axis::Alt)) - altDeg;
  tracking.userAdjusting = false;
  systemState.trackingActive = true;
  motion::setTrackingEnabled(true);
}

void completeGotoSuccess() {
  motion::clearGotoRates();
  systemState.gotoActive = false;
  gotoRuntime.active = false;
  showInfo("Goto done");

  double azDeg = 0.0;
  double altDeg = 0.0;
  DateTime now = currentDateTime();
  double ra = gotoRuntime.targetRaHours;
  double dec = gotoRuntime.targetDecDegrees;
  raDecToAltAz(now, ra, dec, azDeg, altDeg);
  finalizeTrackingTarget(gotoRuntime.targetCatalogIndex, ra, dec, azDeg, altDeg);
}

void abortGoto() {
  motion::clearGotoRates();
  gotoRuntime.active = false;
  systemState.gotoActive = false;
  stopTracking();
}

void updateTracking() {
  if (gotoRuntime.active || systemState.gotoActive) {
    motion::setTrackingRates(0.0, 0.0);
    motion::setTrackingEnabled(false);
    return;
  }

  if (!tracking.active) {
    motion::setTrackingRates(0.0, 0.0);
    motion::setTrackingEnabled(false);
    systemState.trackingActive = false;
    return;
  }

  DateTime now = currentDateTime();
  double ra = tracking.targetRaHours;
  double dec = tracking.targetDecDegrees;
  if (tracking.targetCatalogIndex >= 0 &&
      tracking.targetCatalogIndex < static_cast<int>(catalog::size())) {
    const CatalogObject* object = catalog::get(static_cast<size_t>(tracking.targetCatalogIndex));
    if (object) {
      getObjectRaDecAt(*object, now, 0.0, ra, dec, nullptr);
    }
  }

  double azDeg = 0.0;
  double altDeg = 0.0;
  if (!raDecToAltAz(now, ra, dec, azDeg, altDeg)) {
    motion::setTrackingRates(0.0, 0.0);
    systemState.trackingActive = false;
    return;
  }

  double desiredAz = wrapAngle360(azDeg + tracking.offsetAzDeg);
  double desiredAlt = altDeg + tracking.offsetAltDeg;
  double currentAz = motion::stepsToAzDegrees(motion::getStepCount(Axis::Az));
  double currentAlt = motion::stepsToAltDegrees(motion::getStepCount(Axis::Alt));

  if (systemState.joystickActive) {
    tracking.userAdjusting = true;
    motion::setTrackingRates(0.0, 0.0);
    motion::setTrackingEnabled(false);
    systemState.trackingActive = false;
    return;
  }

  if (tracking.userAdjusting) {
    tracking.userAdjusting = false;
    tracking.offsetAzDeg = wrapAngle180(currentAz - azDeg);
    tracking.offsetAltDeg = currentAlt - altDeg;
    desiredAz = wrapAngle360(azDeg + tracking.offsetAzDeg);
    desiredAlt = altDeg + tracking.offsetAltDeg;
  }

  double errorAz = shortestAngularDistance(currentAz, desiredAz);
  double errorAlt = desiredAlt - currentAlt;
  constexpr double kTrackingGain = 0.4;
  constexpr double kMaxTrackingSpeed = 3.0;
  double azRate = std::clamp(errorAz * kTrackingGain, -kMaxTrackingSpeed, kMaxTrackingSpeed);
  double altRate = std::clamp(errorAlt * kTrackingGain, -kMaxTrackingSpeed, kMaxTrackingSpeed);

  motion::setTrackingRates(azRate, altRate);
  motion::setTrackingEnabled(true);
  systemState.trackingActive = true;
}

void updateGoto() {
  if (!gotoRuntime.active) {
    if (systemState.gotoActive) {
      abortGoto();
    }
    updateTracking();
    return;
  }

  if (!systemState.gotoActive) {
    abortGoto();
    updateTracking();
    return;
  }

  uint32_t nowMs = millis();
  double dt = (nowMs - gotoRuntime.lastUpdateMs) / 1000.0;
  gotoRuntime.lastUpdateMs = nowMs;
  if (dt <= 0.0) {
    return;
  }

  bool azDone = updateAxisGoto(Axis::Az, gotoRuntime.az, dt, gotoRuntime.profile);
  bool altDone = updateAxisGoto(Axis::Alt, gotoRuntime.alt, dt, gotoRuntime.profile);

  if (azDone && altDone) {
    completeGotoSuccess();
  }
}

bool startGotoToObject(const CatalogObject& object, int catalogIndex) {
  const AxisCalibration& cal = storage::getConfig().axisCalibration;
  if (cal.stepsPerDegreeAz <= 0.0 || cal.stepsPerDegreeAlt <= 0.0) {
    showInfo("Calibrate axes");
    return false;
  }

  if (gotoRuntime.active) {
    abortGoto();
  }

  DateTime now = currentDateTime();
  double currentAz = motion::stepsToAzDegrees(motion::getStepCount(Axis::Az));
  double currentAlt = motion::stepsToAltDegrees(motion::getStepCount(Axis::Alt));

  double raNow;
  double decNow;
  double azNow;
  double altNow;
  DateTime timeNow;
  if (!computeTargetAltAz(object, now, 0.0, raNow, decNow, azNow, altNow, timeNow) || altNow < 0.0) {
    showInfo("Below horizon");
    return false;
  }

  GotoProfileSteps profile = toProfileSteps(storage::getConfig().gotoProfile, cal);
  double azDiffNow = shortestAngularDistance(currentAz, azNow) * cal.stepsPerDegreeAz;
  double altDiffNow = (altNow - currentAlt) * cal.stepsPerDegreeAlt;
  double durationAz = computeTravelTimeSteps(azDiffNow, profile.maxSpeedAz, profile.accelerationAz, profile.decelerationAz);
  double durationAlt = computeTravelTimeSteps(altDiffNow, profile.maxSpeedAlt, profile.accelerationAlt, profile.decelerationAlt);
  double estimatedDuration = std::max(durationAz, durationAlt) + 1.0;

  double raFuture;
  double decFuture;
  double azFuture;
  double altFuture;
  DateTime arrivalTime;
  if (!computeTargetAltAz(object, now, estimatedDuration, raFuture, decFuture, azFuture, altFuture, arrivalTime) ||
      altFuture < 0.0) {
    showInfo("Below horizon");
    return false;
  }

  int64_t currentAzSteps = motion::getStepCount(Axis::Az);
  int64_t currentAltSteps = motion::getStepCount(Axis::Alt);
  int64_t targetAzSteps = currentAzSteps + static_cast<int64_t>(llround(shortestAngularDistance(currentAz, azFuture) * cal.stepsPerDegreeAz));
  int64_t targetAltSteps = currentAltSteps + static_cast<int64_t>(llround((altFuture - currentAlt) * cal.stepsPerDegreeAlt));

  gotoRuntime.active = true;
  gotoRuntime.az = initAxisRuntime(Axis::Az, targetAzSteps);
  gotoRuntime.alt = initAxisRuntime(Axis::Alt, targetAltSteps);
  gotoRuntime.profile = profile;
  gotoRuntime.estimatedDurationSec = estimatedDuration;
  gotoRuntime.lastUpdateMs = millis();
  gotoRuntime.startTime = now;
  gotoRuntime.targetRaHours = raFuture;
  gotoRuntime.targetDecDegrees = decFuture;
  gotoRuntime.targetCatalogIndex = catalogIndex;

  systemState.gotoActive = true;
  systemState.azGotoTarget = targetAzSteps;
  systemState.altGotoTarget = targetAltSteps;
  gotoTargetName = object.name;
  motion::clearGotoRates();
  stopTracking();
  showInfo("Goto started");
  return true;
}

void startGotoToSelected() {
  if (catalog::size() == 0 || systemState.selectedCatalogIndex < 0 ||
      systemState.selectedCatalogIndex >= static_cast<int>(catalog::size())) {
    showInfo("Select object");
    return;
  }
  const CatalogObject* object = catalog::get(static_cast<size_t>(systemState.selectedCatalogIndex));
  if (!object) {
    showInfo("Invalid object");
    return;
  }
  if (startGotoToObject(*object, systemState.selectedCatalogIndex)) {
    selectedObjectName = object->name;
    gotoTargetName = object->name;
  }
}

void stopTracking() {
  tracking.active = false;
  tracking.userAdjusting = false;
  systemState.trackingActive = false;
  motion::setTrackingEnabled(false);
  motion::setTrackingRates(0.0, 0.0);
}

void handleMainMenuInput(int delta) {
  if (delta != 0) {
    mainMenuIndex += delta;
    while (mainMenuIndex < 0) mainMenuIndex += static_cast<int>(kMainMenuCount);
    while (mainMenuIndex >= static_cast<int>(kMainMenuCount)) mainMenuIndex -= static_cast<int>(kMainMenuCount);
  }
  consumeJoystickBackEvent();
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (!select) {
    return;
  }
  switch (mainMenuIndex) {
    case 0:
      showInfo("Status ready", 1500);
      break;
    case 1:
      startPolarAlignment();
      break;
    case 2:
      if (!systemState.polarAligned) {
        showInfo("Align first");
      } else {
        if (!tracking.active && selectedObjectName.isEmpty() && tracking.targetCatalogIndex < 0) {
          showInfo("Goto first");
          break;
        }
        tracking.active = true;
        tracking.userAdjusting = false;
        systemState.trackingActive = true;
        showInfo("Tracking on");
      }
      break;
    case 3:
      stopTracking();
      showInfo("Tracking off");
      break;
    case 4:
      if (!sdAvailable || catalog::size() == 0) {
        showInfo("Catalog missing");
      } else {
        catalogIndex = systemState.selectedCatalogIndex;
        int total = static_cast<int>(catalog::size());
        if (catalogIndex < 0) catalogIndex = 0;
        if (catalogIndex >= total) catalogIndex = total - 1;
        setUiState(UiState::CatalogBrowser);
      }
      break;
    case 5:
      startGotoToSelected();
      break;
    case 6:
      enterSetupMenu();
      break;
    default:
      break;
  }
}

void handleSetupMenuInput(int delta) {
  if (delta != 0) {
    setupMenuIndex += delta;
    while (setupMenuIndex < 0) setupMenuIndex += static_cast<int>(kSetupMenuCount);
    while (setupMenuIndex >= static_cast<int>(kSetupMenuCount))
      setupMenuIndex -= static_cast<int>(kSetupMenuCount);
  }
  if (consumeJoystickBackEvent()) {
    setUiState(UiState::MainMenu);
    return;
  }
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (!select) {
    return;
  }
  switch (setupMenuIndex) {
    case 0:
      enterRtcEditor();
      break;
    case 1:
      startJoystickCalibrationFlow();
      break;
    case 2:
      resetAxisCalibrationState();
      showInfo("Set Az 0");
      break;
    case 3:
      enterGotoSpeedSetup();
      break;
    case 4:
      startBacklashCalibration();
      break;
    case 5:
      setUiState(UiState::MainMenu);
      break;
    default:
      break;
  }
}

void handleRtcInput(int delta) {
  if (delta != 0) {
    switch (rtcEdit.fieldIndex) {
      case 0:
        rtcEdit.year = std::clamp(rtcEdit.year + delta, 2020, 2100);
        break;
      case 1:
        rtcEdit.month += delta;
        if (rtcEdit.month < 1) rtcEdit.month = 12;
        if (rtcEdit.month > 12) rtcEdit.month = 1;
        break;
      case 2:
        rtcEdit.day += delta;
        if (rtcEdit.day < 1) rtcEdit.day = 31;
        if (rtcEdit.day > 31) rtcEdit.day = 1;
        break;
      case 3:
        rtcEdit.hour = (rtcEdit.hour + delta + 24) % 24;
        break;
      case 4:
        rtcEdit.minute = (rtcEdit.minute + delta + 60) % 60;
        break;
      case 5:
        rtcEdit.second = (rtcEdit.second + delta + 60) % 60;
        break;
    }
  }
  if (input::consumeJoystickPress()) {
    rtcEdit.fieldIndex = (rtcEdit.fieldIndex + 1) % 6;
  }
  if (consumeJoystickBackEvent()) {
    setUiState(UiState::SetupMenu);
    return;
  }
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (select) {
    applyRtcEdit();
  }
}

void handleCatalogInput(int delta) {
  if (catalog::size() == 0) {
    bool exit = input::consumeEncoderClick();
    if (!exit) {
      exit = input::consumeJoystickPress();
    }
    if (!exit) {
      exit = consumeJoystickSelectEvent();
    }
    if (!exit) {
      exit = consumeJoystickBackEvent();
    }
    if (exit) {
      setUiState(UiState::MainMenu);
    }
    return;
  }
  if (delta != 0) {
    catalogIndex += delta;
    int total = static_cast<int>(catalog::size());
    while (catalogIndex < 0) catalogIndex += total;
    while (catalogIndex >= total) catalogIndex -= total;
  }
  if (consumeJoystickBackEvent()) {
    setUiState(UiState::MainMenu);
    return;
  }
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (select) {
    systemState.selectedCatalogIndex = catalogIndex;
    const CatalogObject* object = catalog::get(static_cast<size_t>(catalogIndex));
    if (object) {
      if (startGotoToObject(*object, catalogIndex)) {
        selectedObjectName = object->name;
        gotoTargetName = object->name;
      }
    }
  }
  if (input::consumeJoystickPress()) {
    setUiState(UiState::MainMenu);
  }
}

void handlePolarAlignInput() {
  bool select = input::consumeEncoderClick();
  if (!select) {
    select = consumeJoystickSelectEvent();
  }
  if (select) {
    completePolarAlignment();
  }
  if (input::consumeJoystickPress() || consumeJoystickBackEvent()) {
    systemState.menuMode = MenuMode::Status;
    setUiState(UiState::MainMenu);
    showInfo("Align aborted");
  }
}

}  // namespace

void init() {
  Wire.begin(config::SDA_PIN, config::SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED init failed");
  }
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  rtcAvailable = rtc.begin();
  if (!rtcAvailable) {
    showInfo("RTC missing", 2000);
  }
}

void setSdAvailable(bool available) { sdAvailable = available; }

void showBootMessage() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("NERDSTAR booting...");
  display.display();
}

void showCalibrationStart() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Calibrating joystick");
  display.display();
}

void showCalibrationResult(int centerX, int centerY) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Calibration done");
  display.setCursor(0, 16);
  display.printf("CX=%d", centerX);
  display.setCursor(0, 24);
  display.printf("CY=%d", centerY);
  display.display();
  delay(1000);
}

void showReady() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("NERDSTAR ready");
  display.display();
}

void startTask() {
  xTaskCreatePinnedToCore(displayTask, "display", 4096, nullptr, 1, nullptr, 0);
}

void handleInput() {
  input::update();
  int delta = input::consumeEncoderDelta();
  float joyY = input::getJoystickNormalizedY();
  float joyX = input::getJoystickNormalizedX();
  uint32_t nowMs = millis();
  if (lastScrollUpdateMs == 0) {
    lastScrollUpdateMs = nowMs;
  }
  float dt = (nowMs - lastScrollUpdateMs) / 1000.0f;
  lastScrollUpdateMs = nowMs;
  constexpr float kItemsPerSecond = 6.0f;
  joystickScrollAccumulator += joyY * kItemsPerSecond * dt;
  int joystickSteps = 0;
  while (joystickScrollAccumulator >= 1.0f) {
    joystickSteps += 1;
    joystickScrollAccumulator -= 1.0f;
  }
  while (joystickScrollAccumulator <= -1.0f) {
    joystickSteps -= 1;
    joystickScrollAccumulator += 1.0f;
  }
  delta += joystickSteps;

  constexpr float kHorizontalThreshold = 0.6f;
  bool rightActive = joyX > kHorizontalThreshold;
  bool leftActive = joyX < -kHorizontalThreshold;
  if (rightActive) {
    if (!joystickRightLatched) {
      joystickRightLatched = true;
      joystickSelectEvent = true;
    }
  } else {
    joystickRightLatched = false;
  }
  if (leftActive) {
    if (!joystickLeftLatched) {
      joystickLeftLatched = true;
      joystickBackEvent = true;
    }
  } else {
    joystickLeftLatched = false;
  }
  switch (uiState) {
    case UiState::MainMenu:
      handleMainMenuInput(delta);
      break;
    case UiState::PolarAlign:
      handlePolarAlignInput();
      break;
    case UiState::SetupMenu:
      handleSetupMenuInput(delta);
      break;
    case UiState::SetRtc:
      handleRtcInput(delta);
      break;
    case UiState::CatalogBrowser:
      handleCatalogInput(delta);
      break;
    case UiState::AxisCalibration:
      if (consumeJoystickBackEvent() || input::consumeJoystickPress()) {
        setUiState(UiState::SetupMenu);
        break;
      }
      bool select = input::consumeEncoderClick();
      if (!select) {
        select = consumeJoystickSelectEvent();
      }
      if (select) {
        handleAxisCalibrationClick();
      }
      break;
    case UiState::GotoSpeed:
      handleGotoSpeedInput(delta);
      break;
    case UiState::BacklashCalibration:
      handleBacklashCalibrationInput();
      break;
  }
}

void showInfo(const String& message, uint32_t durationMs) {
  uint32_t until = millis() + durationMs;
  portENTER_CRITICAL(&displayMux);
  infoMessage = message;
  infoUntil = until;
  portEXIT_CRITICAL(&displayMux);
}

void completePolarAlignment() {
  systemState.menuMode = MenuMode::Status;
  systemState.polarAligned = true;
  systemState.trackingActive = false;
  systemState.gotoActive = false;
  stopTracking();
  double azDeg = 0.0;
  double altDeg = 0.0;
  DateTime now = currentDateTime();
  if (raDecToAltAz(now, config::POLARIS_RA_HOURS, config::POLARIS_DEC_DEGREES, azDeg, altDeg)) {
    motion::setStepCount(Axis::Az, motion::azDegreesToSteps(azDeg));
    motion::setStepCount(Axis::Alt, motion::altDegreesToSteps(altDeg));
  }
  storage::setPolarAligned(true);
  setUiState(UiState::MainMenu);
  showInfo("Polaris locked");
}

void startPolarAlignment() {
  systemState.menuMode = MenuMode::PolarAlign;
  systemState.polarAligned = false;
  systemState.trackingActive = false;
  systemState.gotoActive = false;
  stopTracking();
  setUiState(UiState::PolarAlign);
  showInfo("Use joystick", 2000);
}

void update() { updateGoto(); }

}  // namespace display_menu

