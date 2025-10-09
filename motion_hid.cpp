#include "motion.h"

#if defined(DEVICE_ROLE_HID)

#include <cstdlib>
#include <initializer_list>
#include <vector>

#include "comm.h"

namespace {

const char* axisToString(Axis axis) {
  return (axis == Axis::Az) ? "AZ" : "ALT";
}

String formatFloat(double value) { return String(value, 6); }

int64_t parseInt64(const String& value) {
  return static_cast<int64_t>(strtoll(value.c_str(), nullptr, 10));
}

double parseDouble(const String& value) {
  return strtod(value.c_str(), nullptr);
}

}  // namespace

namespace motion {

void init() {
  // Ensure the link is initialised; the HID controller waits for the main
  // controller to announce readiness in the Arduino sketch before invoking
  // motion functions.
}

void setManualRate(Axis axis, float rpm) {
  comm::call("SET_MANUAL_RPM", {String(axisToString(axis)), formatFloat(rpm)});
}

void setManualStepsPerSecond(Axis axis, double stepsPerSecond) {
  comm::call("SET_MANUAL_SPS",
             {String(axisToString(axis)), formatFloat(stepsPerSecond)});
}

void setGotoStepsPerSecond(Axis axis, double stepsPerSecond) {
  comm::call("SET_GOTO_SPS",
             {String(axisToString(axis)), formatFloat(stepsPerSecond)});
}

void clearGotoRates() { comm::call("CLEAR_GOTO", {}); }

void stopAll() { comm::call("STOP_ALL", {}); }

void setTrackingEnabled(bool enabled) {
  comm::call("SET_TRACKING_ENABLED", {enabled ? "1" : "0"});
}

void setTrackingRates(double azDegPerSec, double altDegPerSec) {
  comm::call("SET_TRACKING_RATES",
             {formatFloat(azDegPerSec), formatFloat(altDegPerSec)});
}

int64_t getStepCount(Axis axis) {
  std::vector<String> payload;
  if (!comm::call("GET_STEP_COUNT", {String(axisToString(axis))}, &payload)) {
    return 0;
  }
  if (payload.empty()) {
    return 0;
  }
  return parseInt64(payload.front());
}

void setStepCount(Axis axis, int64_t value) {
  comm::call("SET_STEP_COUNT",
             {String(axisToString(axis)), String(value)});
}

double stepsToAzDegrees(int64_t steps) {
  std::vector<String> payload;
  if (!comm::call("STEPS_TO_AZ", {String(steps)}, &payload)) {
    return 0.0;
  }
  if (payload.empty()) {
    return 0.0;
  }
  return parseDouble(payload.front());
}

double stepsToAltDegrees(int64_t steps) {
  std::vector<String> payload;
  if (!comm::call("STEPS_TO_ALT", {String(steps)}, &payload)) {
    return 0.0;
  }
  if (payload.empty()) {
    return 0.0;
  }
  return parseDouble(payload.front());
}

int64_t azDegreesToSteps(double degrees) {
  std::vector<String> payload;
  if (!comm::call("AZ_TO_STEPS", {formatFloat(degrees)}, &payload)) {
    return 0;
  }
  if (payload.empty()) {
    return 0;
  }
  return parseInt64(payload.front());
}

int64_t altDegreesToSteps(double degrees) {
  std::vector<String> payload;
  if (!comm::call("ALT_TO_STEPS", {formatFloat(degrees)}, &payload)) {
    return 0;
  }
  if (payload.empty()) {
    return 0;
  }
  return parseInt64(payload.front());
}

void applyCalibration(const AxisCalibration& calibration) {
  comm::call("APPLY_CALIBRATION",
             {formatFloat(calibration.stepsPerDegreeAz),
              formatFloat(calibration.stepsPerDegreeAlt),
              String(calibration.azHomeOffset),
              String(calibration.altHomeOffset)});
}

void setBacklash(const BacklashConfig& backlash) {
  comm::call("SET_BACKLASH",
             {String(backlash.azSteps), String(backlash.altSteps)});
}

int32_t getBacklashSteps(Axis axis) {
  std::vector<String> payload;
  if (!comm::call("GET_BACKLASH", {String(axisToString(axis))}, &payload)) {
    return 0;
  }
  if (payload.empty()) {
    return 0;
  }
  return static_cast<int32_t>(payload.front().toInt());
}

int8_t getLastDirection(Axis axis) {
  std::vector<String> payload;
  if (!comm::call("GET_LAST_DIR", {String(axisToString(axis))}, &payload)) {
    return 0;
  }
  if (payload.empty()) {
    return 0;
  }
  return static_cast<int8_t>(payload.front().toInt());
}

}  // namespace motion

#endif  // DEVICE_ROLE_HID

