#include <math.h>

#include "config.h"
#include "catalog.h"
#include "display_menu.h"
#include "input.h"
#include "motion.h"
#include "state.h"
#include "storage.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  storage::init();
  systemState.polarAligned = storage::getConfig().polarAligned;
  systemState.sdAvailable = storage::isSdAvailable();
  systemState.selectedCatalogIndex = -1;

  display_menu::init();
  display_menu::showBootMessage();
  display_menu::setSdAvailable(systemState.sdAvailable);

  input::init();
  if (storage::getConfig().joystickCalibrated) {
    input::setJoystickCalibration(storage::getConfig().joystickCalibration);
  } else {
    display_menu::showCalibrationStart();
    JoystickCalibration calibration = input::calibrateJoystick();
    input::setJoystickCalibration(calibration);
    storage::setJoystickCalibration(calibration);
    display_menu::showCalibrationResult(calibration.centerX, calibration.centerY);
  }

  motion::init();
  motion::applyCalibration(storage::getConfig().axisCalibration);
  motion::setBacklash(storage::getConfig().backlash);
  display_menu::showReady();
  display_menu::startTask();

  if (systemState.sdAvailable) {
    if (!catalog::init()) {
      Serial.println("Catalog load failed");
      systemState.sdAvailable = false;
      display_menu::setSdAvailable(false);
      display_menu::showInfo("Catalog missing", 2000);
    }
  }
}

void loop() {
  display_menu::update();
  display_menu::handleInput();

  float azInput = input::getJoystickNormalizedX();
  float altInput = input::getJoystickNormalizedY();
  systemState.joystickActive = (fabs(azInput) > config::JOYSTICK_DEADZONE ||
                                fabs(altInput) > config::JOYSTICK_DEADZONE);
  motion::setManualRate(Axis::Az, azInput * config::MAX_RPM_MANUAL);
  motion::setManualRate(Axis::Alt, altInput * config::MAX_RPM_MANUAL);

  if (systemState.gotoActive) {
    if (input::consumeJoystickPress()) {
      systemState.gotoActive = false;
      motion::clearGotoRates();
      display_menu::showInfo("Goto aborted", 2000);
    }
  }

  if (input::consumeJoystickPress() && !systemState.gotoActive) {
    motion::stopAll();
    display_menu::stopTracking();
    display_menu::showInfo("Motion stopped", 2000);
  }

  delay(20);
}

