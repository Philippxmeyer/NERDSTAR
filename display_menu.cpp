#include "display_menu.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <Wire.h>
#include <algorithm>
#include <math.h>
#include <stdlib.h>

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
  int64_t raZero;
  int64_t raOneHour;
  int64_t decZero;
  int64_t decSpan;
};

AxisCalibrationState axisCal{0, 0, 0, 0, 0};

String selectedObjectName;
String gotoTargetName;

int mainMenuIndex = 0;
constexpr const char* kMainMenuItems[] = {
    "Status", "Polar Align", "Start Tracking", "Stop Tracking", "Catalog", "Goto Selected", "Setup"};
constexpr size_t kMainMenuCount = sizeof(kMainMenuItems) / sizeof(kMainMenuItems[0]);

int setupMenuIndex = 0;
constexpr const char* kSetupMenuItems[] = {"Set RTC", "Cal Joystick", "Cal Axes", "Back"};
constexpr size_t kSetupMenuCount = sizeof(kSetupMenuItems) / sizeof(kSetupMenuItems[0]);

int catalogIndex = 0;

String infoMessage;
uint32_t infoUntil = 0;
portMUX_TYPE displayMux = portMUX_INITIALIZER_UNLOCKED;

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

double getObjectCoordinates(const CatalogObject& object, double& dec) {
  double ra = object.raHours;
  dec = object.decDegrees;
  PlanetId planetId;
  if (object.type.equalsIgnoreCase("planet") && rtcAvailable &&
      planets::planetFromString(object.name, planetId)) {
    DateTime now = rtc.now();
    double hourFraction = now.hour() + now.minute() / 60.0 + now.second() / 3600.0;
    double jd = planets::julianDay(now.year(), now.month(), now.day(), hourFraction);
    PlanetPosition position;
    if (planets::computePlanet(planetId, jd, position)) {
      ra = position.raHours;
      dec = position.decDegrees;
    }
  }
  return ra;
}

void drawStatus() {
  char raBuffer[24];
  char decBuffer[24];
  double raHours = motion::stepsToRaHours(motion::getStepCount(Axis::RA));
  double decDeg = motion::stepsToDecDegrees(motion::getStepCount(Axis::DEC));
  formatRa(raHours, raBuffer, sizeof(raBuffer));
  formatDec(decDeg, decBuffer, sizeof(decBuffer));

  display.setCursor(0, 10);
  display.print("RA: ");
  display.print(raBuffer);
  display.setCursor(0, 18);
  display.print("Dec: ");
  display.print(decBuffer);

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
  double dec;
  double ra = getObjectCoordinates(*object, dec);
  char raBuffer[24];
  char decBuffer[24];
  formatRa(ra, raBuffer, sizeof(raBuffer));
  formatDec(dec, decBuffer, sizeof(decBuffer));
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
  display.print("Mag: ");
  display.print(object->magnitude, 1);
  display.setCursor(0, 60);
  display.print("Enc sel, joy=exit");
}

void drawAxisCalibration() {
  display.setCursor(0, 12);
  display.print("Axis Cal");
  const char* steps[] = {
      "Set RA 0h, enc",
      "Rotate +1h, enc",
      "Set Dec 0deg, enc",
      "Rotate +10deg, enc"};
  int index = std::min(axisCal.step, 3);
  display.setCursor(0, 24);
  display.print(steps[index]);
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
  double stepsPerHour = fabs(static_cast<double>(axisCal.raOneHour - axisCal.raZero));
  double stepsPerDegree = fabs(static_cast<double>(axisCal.decSpan - axisCal.decZero)) / 10.0;
  if (stepsPerHour < 1.0 || stepsPerDegree < 1.0) {
    showInfo("Cal failed");
    resetAxisCalibrationState();
    return;
  }
  AxisCalibration calibration;
  calibration.stepsPerHourRA = stepsPerHour;
  calibration.stepsPerDegreeDEC = stepsPerDegree;
  calibration.raHomeOffset = axisCal.raZero;
  calibration.decHomeOffset = axisCal.decZero;
  storage::setAxisCalibration(calibration);
  motion::applyCalibration(calibration);
  showInfo("Axes calibrated");
  setUiState(UiState::SetupMenu);
}

void handleAxisCalibrationClick() {
  switch (axisCal.step) {
    case 0:
      axisCal.raZero = motion::getStepCount(Axis::RA);
      axisCal.step = 1;
      showInfo("Rotate +1h");
      break;
    case 1:
      axisCal.raOneHour = motion::getStepCount(Axis::RA);
      axisCal.step = 2;
      showInfo("Set Dec 0");
      break;
    case 2:
      axisCal.decZero = motion::getStepCount(Axis::DEC);
      axisCal.step = 3;
      showInfo("Rotate +10deg");
      break;
    case 3:
      axisCal.decSpan = motion::getStepCount(Axis::DEC);
      axisCal.step = 4;
      completeAxisCalibration();
      break;
    default:
      break;
  }
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
  double dec;
  double ra = getObjectCoordinates(*object, dec);
  systemState.raGotoTarget = motion::raHoursToSteps(ra);
  systemState.decGotoTarget = motion::decDegreesToSteps(dec);
  systemState.gotoActive = true;
  gotoTargetName = object->name;
  motion::setTrackingEnabled(false);
  systemState.trackingActive = false;
  showInfo("Goto started");
}

void updateGoto() {
  if (!systemState.gotoActive) {
    return;
  }
  constexpr double maxRpm = config::MAX_RPM_MANUAL * 0.8f;
  bool raDone = false;
  bool decDone = false;

  int64_t currentRa = motion::getStepCount(Axis::RA);
  int64_t currentDec = motion::getStepCount(Axis::DEC);
  int64_t diffRa = systemState.raGotoTarget - currentRa;
  int64_t diffDec = systemState.decGotoTarget - currentDec;

  if (llabs(diffRa) < 10) {
    motion::setManualRate(Axis::RA, 0.0f);
    raDone = true;
  } else {
    float rpm = diffRa > 0 ? maxRpm : -maxRpm;
    motion::setManualRate(Axis::RA, rpm);
  }

  if (llabs(diffDec) < 10) {
    motion::setManualRate(Axis::DEC, 0.0f);
    decDone = true;
  } else {
    float rpm = diffDec > 0 ? maxRpm : -maxRpm;
    motion::setManualRate(Axis::DEC, rpm);
  }

  if (raDone && decDone) {
    systemState.gotoActive = false;
    motion::setManualRate(Axis::RA, 0.0f);
    motion::setManualRate(Axis::DEC, 0.0f);
    showInfo("Goto done");
  }
}

void handleMainMenuInput(int delta) {
  if (delta != 0) {
    mainMenuIndex += delta;
    while (mainMenuIndex < 0) mainMenuIndex += static_cast<int>(kMainMenuCount);
    while (mainMenuIndex >= static_cast<int>(kMainMenuCount)) mainMenuIndex -= static_cast<int>(kMainMenuCount);
  }
  if (!input::consumeEncoderClick()) {
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
        motion::setTrackingEnabled(true);
        systemState.trackingActive = true;
        showInfo("Tracking on");
      }
      break;
    case 3:
      motion::setTrackingEnabled(false);
      systemState.trackingActive = false;
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
  if (!input::consumeEncoderClick()) {
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
      showInfo("Set RA 0h");
      break;
    case 3:
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
  if (input::consumeEncoderClick()) {
    applyRtcEdit();
  }
}

void handleCatalogInput(int delta) {
  if (catalog::size() == 0) {
    if (input::consumeEncoderClick() || input::consumeJoystickPress()) {
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
  if (input::consumeEncoderClick()) {
    systemState.selectedCatalogIndex = catalogIndex;
    const CatalogObject* object = catalog::get(static_cast<size_t>(catalogIndex));
    if (object) {
      selectedObjectName = object->name;
      showInfo("Target selected");
    }
  }
  if (input::consumeJoystickPress()) {
    setUiState(UiState::MainMenu);
  }
}

void handlePolarAlignInput() {
  if (input::consumeEncoderClick()) {
    completePolarAlignment();
  }
  if (input::consumeJoystickPress()) {
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
      if (input::consumeEncoderClick()) {
        handleAxisCalibrationClick();
      }
      if (input::consumeJoystickPress()) {
        setUiState(UiState::SetupMenu);
      }
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
  motion::setTrackingEnabled(false);
  motion::setStepCount(Axis::RA, motion::raHoursToSteps(config::POLARIS_RA_HOURS));
  motion::setStepCount(Axis::DEC, motion::decDegreesToSteps(config::POLARIS_DEC_DEGREES));
  storage::setPolarAligned(true);
  setUiState(UiState::MainMenu);
  showInfo("Polaris locked");
}

void startPolarAlignment() {
  systemState.menuMode = MenuMode::PolarAlign;
  systemState.polarAligned = false;
  systemState.trackingActive = false;
  systemState.gotoActive = false;
  motion::setTrackingEnabled(false);
  setUiState(UiState::PolarAlign);
  showInfo("Use joystick", 2000);
}

void update() { updateGoto(); }

}  // namespace display_menu

