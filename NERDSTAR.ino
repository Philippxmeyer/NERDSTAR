#include "role_config.h"

#include "wifi_ota.h"

#if defined(DEVICE_ROLE_HID)

#include <math.h>

#include "catalog.h"
#include "comm.h"
#include "config.h"
#include "display_menu.h"
#include "input.h"
#include "motion.h"
#include "state.h"
#include "storage.h"

namespace {
bool g_mountLinkReady = false;

void initDebugSerial() {
  Serial.begin(config::USB_DEBUG_BAUD);
  delay(50);
  Serial.println("[HID] Boot");
}
}

void setup() {
  initDebugSerial();
  wifi_ota::init();
  comm::initLink();

  display_menu::init();
  display_menu::showBootMessage();

  storage::init();
  systemState.polarAligned = storage::getConfig().polarAligned;
  systemState.selectedCatalogIndex = -1;
  systemState.selectedCatalogTypeIndex = -1;
  systemState.mountLinkReady = false;
  systemState.manualCommandOk = false;

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

  g_mountLinkReady = comm::waitForReady(5000);
  if (Serial) {
    Serial.println(g_mountLinkReady ? "[HID] Mount link ready"
                                    : "[HID] Mount link offline");
  }
  if (!g_mountLinkReady) {
    display_menu::showInfo("Mount link offline", 2000);
  }

  if (!catalog::init()) {
    display_menu::showInfo("Catalog missing", 2000);
  }
}

void loop() {
  display_menu::update();
  display_menu::handleInput();

  float azInput = input::getJoystickNormalizedX();
  float altInput = input::getJoystickNormalizedY();
  systemState.joystickX = azInput;
  systemState.joystickY = altInput;
  systemState.joystickButtonPressed = input::isJoystickButtonPressed();
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

  if (!g_mountLinkReady) {
    if (comm::waitForReady(1)) {
      g_mountLinkReady = true;
      systemState.manualCommandOk = true;
      display_menu::showInfo("Mount link ready", 2000);
      if (Serial) {
        Serial.println("[HID] Mount link re-established");
      }
    }
  }
  systemState.mountLinkReady = g_mountLinkReady;

  wifi_ota::update();
  delay(20);
}

#elif defined(DEVICE_ROLE_MAIN)

#include <Arduino.h>
#include <cmath>
#include <cstdlib>

#include "comm.h"
#include "config.h"
#include "motion.h"
#include "state.h"
#include "storage.h"

namespace {

TaskHandle_t motorTaskHandle = nullptr;
TaskHandle_t commandTaskHandle = nullptr;

void initDebugSerial() {
  Serial.begin(config::USB_DEBUG_BAUD);
  delay(50);
  Serial.println("[MAIN] Boot");
}

bool parseAxis(const String& value, Axis& outAxis) {
  if (value.equalsIgnoreCase("AZ")) {
    outAxis = Axis::Az;
    return true;
  }
  if (value.equalsIgnoreCase("ALT")) {
    outAxis = Axis::Alt;
    return true;
  }
  return false;
}

String formatDouble(double value) { return String(value, 6); }

void handleRequest(const comm::Request& request) {
  const String& cmd = request.command;
  auto paramCount = request.params.size();

  auto requireParams = [&](size_t expected) {
    if (paramCount < expected) {
      comm::sendError(request.id, "Missing params");
      return false;
    }
    return true;
  };

  auto parseAxisParam = [&](size_t index, Axis& axis) {
    if (!parseAxis(request.params[index], axis)) {
      comm::sendError(request.id, "Invalid axis");
      return false;
    }
    return true;
  };

  if (cmd == "SET_MANUAL_RPM") {
    if (!requireParams(2)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    float rpm = request.params[1].toFloat();
    motion::setManualRate(axis, rpm);
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_MANUAL_SPS") {
    if (!requireParams(2)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    double sps = strtod(request.params[1].c_str(), nullptr);
    motion::setManualStepsPerSecond(axis, sps);
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_GOTO_SPS") {
    if (!requireParams(2)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    double sps = strtod(request.params[1].c_str(), nullptr);
    motion::setGotoStepsPerSecond(axis, sps);
    comm::sendOk(request.id, {});
  } else if (cmd == "CLEAR_GOTO") {
    motion::clearGotoRates();
    comm::sendOk(request.id, {});
  } else if (cmd == "STOP_ALL") {
    motion::stopAll();
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_TRACKING_ENABLED") {
    if (!requireParams(1)) return;
    bool enabled = request.params[0] == "1";
    motion::setTrackingEnabled(enabled);
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_TRACKING_RATES") {
    if (!requireParams(2)) return;
    double az = strtod(request.params[0].c_str(), nullptr);
    double alt = strtod(request.params[1].c_str(), nullptr);
    motion::setTrackingRates(az, alt);
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_WIFI_ENABLED") {
    if (!requireParams(1)) return;
    bool enabled = request.params[0] == "1";
    wifi_ota::setEnabled(enabled);
    comm::sendOk(request.id, {});
  } else if (cmd == "GET_STEP_COUNT") {
    if (!requireParams(1)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    int64_t count = motion::getStepCount(axis);
    comm::sendOk(request.id, {String(count)});
  } else if (cmd == "SET_STEP_COUNT") {
    if (!requireParams(2)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    int64_t value = strtoll(request.params[1].c_str(), nullptr, 10);
    motion::setStepCount(axis, value);
    comm::sendOk(request.id, {});
  } else if (cmd == "STEPS_TO_AZ") {
    if (!requireParams(1)) return;
    int64_t steps = strtoll(request.params[0].c_str(), nullptr, 10);
    double degrees = motion::stepsToAzDegrees(steps);
    comm::sendOk(request.id, {formatDouble(degrees)});
  } else if (cmd == "STEPS_TO_ALT") {
    if (!requireParams(1)) return;
    int64_t steps = strtoll(request.params[0].c_str(), nullptr, 10);
    double degrees = motion::stepsToAltDegrees(steps);
    comm::sendOk(request.id, {formatDouble(degrees)});
  } else if (cmd == "AZ_TO_STEPS") {
    if (!requireParams(1)) return;
    double degrees = strtod(request.params[0].c_str(), nullptr);
    int64_t steps = motion::azDegreesToSteps(degrees);
    comm::sendOk(request.id, {String(steps)});
  } else if (cmd == "ALT_TO_STEPS") {
    if (!requireParams(1)) return;
    double degrees = strtod(request.params[0].c_str(), nullptr);
    int64_t steps = motion::altDegreesToSteps(degrees);
    comm::sendOk(request.id, {String(steps)});
  } else if (cmd == "APPLY_CALIBRATION") {
    if (!requireParams(4)) return;
    AxisCalibration calib{};
    calib.stepsPerDegreeAz = strtod(request.params[0].c_str(), nullptr);
    calib.stepsPerDegreeAlt = strtod(request.params[1].c_str(), nullptr);
    calib.azHomeOffset = strtoll(request.params[2].c_str(), nullptr, 10);
    calib.altHomeOffset = strtoll(request.params[3].c_str(), nullptr, 10);
    motion::applyCalibration(calib);
    comm::sendOk(request.id, {});
  } else if (cmd == "SET_BACKLASH") {
    if (!requireParams(2)) return;
    BacklashConfig config{};
    config.azSteps = static_cast<int32_t>(request.params[0].toInt());
    config.altSteps = static_cast<int32_t>(request.params[1].toInt());
    motion::setBacklash(config);
    comm::sendOk(request.id, {});
  } else if (cmd == "GET_BACKLASH") {
    if (!requireParams(1)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    comm::sendOk(request.id, {String(motion::getBacklashSteps(axis))});
  } else if (cmd == "GET_LAST_DIR") {
    if (!requireParams(1)) return;
    Axis axis;
    if (!parseAxisParam(0, axis)) return;
    comm::sendOk(request.id, {String(motion::getLastDirection(axis))});
  } else {
    comm::sendError(request.id, "Unknown command");
  }
}

void commandTask(void*) {
  constexpr uint32_t kReadyIntervalMs = 500;
  uint32_t lastReadyMs = 0;
  comm::announceReady();
  lastReadyMs = millis();
  while (true) {
    comm::Request request;
    if (comm::readRequest(request, 100)) {
      handleRequest(request);
      lastReadyMs = millis();
    } else {
      uint32_t now = millis();
      if ((now - lastReadyMs) >= kReadyIntervalMs) {
        comm::announceReady();
        lastReadyMs = now;
      }
    }
  }
}

void motorTask(void*) {
  motion::motorTaskLoop();
}

}  // namespace

void setup() {
  initDebugSerial();
  wifi_ota::init();
  comm::initLink();
  storage::init();
  motion::init();
  motion::applyCalibration(storage::getConfig().axisCalibration);
  motion::setBacklash(storage::getConfig().backlash);

  xTaskCreatePinnedToCore(motorTask, "motor", 4096, nullptr, 2, &motorTaskHandle,
                          1);
  xTaskCreatePinnedToCore(commandTask, "cmd", 8192, nullptr, 2,
                          &commandTaskHandle, 0);
  if (Serial) {
    Serial.println("[MAIN] Tasks started");
  }
}

void loop() {
  wifi_ota::update();
  vTaskDelay(pdMS_TO_TICKS(20));
}

#endif

