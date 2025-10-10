#include "motion.h"

#if defined(DEVICE_ROLE_MAIN)

#include "motion.h"

#if defined(DEVICE_ROLE_MAIN)

#include <algorithm>
#include <cmath>
#include <limits>

#include <TMCStepper.h>
#include <esp32-hal-gpio.h>
#include <esp_rom_sys.h>
#include <esp_timer.h>

#include "config.h"

namespace {

constexpr double kStepsPerAxisRev =
    config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO;
constexpr double kMinActiveStepsPerSecond = 0.1;
constexpr uint32_t kStepPulseWidthUs = 3;

struct AxisState {
  uint8_t enPin;
  uint8_t dirPin;
  uint8_t stepPin;
  portMUX_TYPE mux;
  volatile int64_t stepCounter;
  double userStepsPerSecond;
  double gotoStepsPerSecond;
  double trackingStepsPerSecond;
  int8_t lastDirection;
  uint64_t nextStepDueUs;
};

AxisState axisAz{config::EN_RA,
                 config::DIR_RA,
                 config::STEP_RA,
                 portMUX_INITIALIZER_UNLOCKED,
                 0,
                 0.0,
                 0.0,
                 0.0,
                 1,
                 0};

AxisState axisAlt{config::EN_DEC,
                  config::DIR_DEC,
                  config::STEP_DEC,
                  portMUX_INITIALIZER_UNLOCKED,
                  0,
                  0.0,
                  0.0,
                  0.0,
                  1,
                  0};

AxisCalibration calibration{
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 360.0,
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 360.0,
    0,
    0};

BacklashConfig backlash{0, 0};

portMUX_TYPE trackingMux = portMUX_INITIALIZER_UNLOCKED;
bool trackingEnabled = false;

TMC2209Stepper driverAz(&Serial2, config::R_SENSE, config::DRIVER_ADDR_RA);
TMC2209Stepper driverAlt(&Serial1, config::R_SENSE, config::DRIVER_ADDR_DEC);

AxisState& getAxisState(Axis axis) {
  return (axis == Axis::Az) ? axisAz : axisAlt;
}

bool isTrackingEnabled() {
  portENTER_CRITICAL(&trackingMux);
  bool enabled = trackingEnabled;
  portEXIT_CRITICAL(&trackingMux);
  return enabled;
}

void updateNextStep(AxisState& axis, uint64_t next) {
  portENTER_CRITICAL(&axis.mux);
  axis.nextStepDueUs = next;
  portEXIT_CRITICAL(&axis.mux);
}

void setAxisCounter(AxisState& axis, int64_t value) {
  portENTER_CRITICAL(&axis.mux);
  axis.stepCounter = value;
  portEXIT_CRITICAL(&axis.mux);
}

int64_t getAxisCounter(const AxisState& axis) {
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  int64_t value = axis.stepCounter;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  return value;
}

double getAxisUserContribution(const AxisState& axis) {
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  double value = axis.userStepsPerSecond;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  return value;
}

double getAxisGotoContribution(const AxisState& axis) {
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  double value = axis.gotoStepsPerSecond;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  return value;
}

double getAxisTrackingContribution(const AxisState& axis) {
  portENTER_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  double value = axis.trackingStepsPerSecond;
  portEXIT_CRITICAL(const_cast<portMUX_TYPE*>(&axis.mux));
  return value;
}

void setAxisUserContribution(AxisState& axis, double value) {
  portENTER_CRITICAL(&axis.mux);
  axis.userStepsPerSecond = value;
  portEXIT_CRITICAL(&axis.mux);
}

void setAxisGotoContribution(AxisState& axis, double value) {
  portENTER_CRITICAL(&axis.mux);
  axis.gotoStepsPerSecond = value;
  portEXIT_CRITICAL(&axis.mux);
}

void setAxisTrackingContribution(AxisState& axis, double value) {
  portENTER_CRITICAL(&axis.mux);
  axis.trackingStepsPerSecond = value;
  portEXIT_CRITICAL(&axis.mux);
}

void applyStep(AxisState& axis, int8_t direction, uint64_t nextDue) {
  digitalWrite(axis.dirPin, direction > 0 ? HIGH : LOW);
  digitalWrite(axis.stepPin, HIGH);
  esp_rom_delay_us(kStepPulseWidthUs);
  digitalWrite(axis.stepPin, LOW);

  portENTER_CRITICAL(&axis.mux);
  axis.stepCounter += direction;
  axis.lastDirection = direction;
  axis.nextStepDueUs = nextDue;
  portEXIT_CRITICAL(&axis.mux);
}

uint64_t updateAxis(AxisState& axis, uint64_t nowUs) {
  double user = getAxisUserContribution(axis);
  double gotoRate = getAxisGotoContribution(axis);
  double trackingRate = isTrackingEnabled() ? getAxisTrackingContribution(axis)
                                            : 0.0;
  double total = user + gotoRate + trackingRate;
  if (std::fabs(total) < kMinActiveStepsPerSecond) {
    updateNextStep(axis, 0);
    digitalWrite(axis.stepPin, LOW);
    return std::numeric_limits<uint64_t>::max();
  }

  int8_t direction = (total >= 0.0) ? 1 : -1;
  total = std::fabs(total);
  if (total < kMinActiveStepsPerSecond) {
    total = kMinActiveStepsPerSecond;
  }
  uint64_t nextDue = nowUs;
  {
    portENTER_CRITICAL(&axis.mux);
    if (axis.nextStepDueUs != 0) {
      nextDue = axis.nextStepDueUs;
    }
    portEXIT_CRITICAL(&axis.mux);
  }

  if (nowUs + 1 >= nextDue) {
    double intervalUs = 1000000.0 / (total * 1.0);
    if (intervalUs < 50.0) {
      intervalUs = 50.0;
    }
    uint64_t scheduled = nowUs + static_cast<uint64_t>(intervalUs);
    applyStep(axis, direction, scheduled);
    return scheduled;
  }

  return nextDue;
}

void configureDriver(TMC2209Stepper& driver) {
  driver.begin();
  delay(50);
  driver.pdn_disable(true);
  driver.en_spreadCycle(false);
  driver.rms_current(config::DRIVER_CURRENT_MA);
  driver.microsteps(static_cast<uint16_t>(config::MICROSTEPS));
}

}  // namespace

namespace motion {

void init() {
  pinMode(axisAz.enPin, OUTPUT);
  pinMode(axisAz.dirPin, OUTPUT);
  pinMode(axisAz.stepPin, OUTPUT);
  pinMode(axisAlt.enPin, OUTPUT);
  pinMode(axisAlt.dirPin, OUTPUT);
  pinMode(axisAlt.stepPin, OUTPUT);

  digitalWrite(axisAz.enPin, HIGH);
  digitalWrite(axisAlt.enPin, HIGH);
  digitalWrite(axisAz.stepPin, LOW);
  digitalWrite(axisAlt.stepPin, LOW);

  Serial2.begin(115200, SERIAL_8N1, config::UART_RA_RX, config::UART_RA_TX);
  Serial1.begin(115200, SERIAL_8N1, config::UART_DEC_RX, config::UART_DEC_TX);

  configureDriver(driverAz);
  configureDriver(driverAlt);

  digitalWrite(axisAz.enPin, LOW);
  digitalWrite(axisAlt.enPin, LOW);

  updateNextStep(axisAz, 0);
  updateNextStep(axisAlt, 0);
}

void motorTaskLoop() {
  while (true) {
    uint64_t now = esp_timer_get_time();
    uint64_t nextAz = updateAxis(axisAz, now);
    uint64_t nextAlt = updateAxis(axisAlt, now);
    uint64_t nextWake = std::min(nextAz, nextAlt);

    if (nextWake == std::numeric_limits<uint64_t>::max()) {
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }

    uint64_t nowAfter = esp_timer_get_time();
    if (nextWake <= nowAfter) {
      taskYIELD();
      continue;
    }

    uint64_t delta = nextWake - nowAfter;
    if (delta > 2000) {
      vTaskDelay(pdMS_TO_TICKS(delta / 1000));
    } else {
      esp_rom_delay_us(delta);
    }
  }
}

void setManualRate(Axis axis, float rpm) {
  double stepsPerSecond = (static_cast<double>(rpm) * kStepsPerAxisRev) / 60.0;
  setManualStepsPerSecond(axis, stepsPerSecond);
}

void setManualStepsPerSecond(Axis axis, double stepsPerSecond) {
  setAxisUserContribution(getAxisState(axis), stepsPerSecond);
}

void setGotoStepsPerSecond(Axis axis, double stepsPerSecond) {
  setAxisGotoContribution(getAxisState(axis), stepsPerSecond);
}

void clearGotoRates() {
  setAxisGotoContribution(axisAz, 0.0);
  setAxisGotoContribution(axisAlt, 0.0);
}

void stopAll() {
  setAxisUserContribution(axisAz, 0.0);
  setAxisUserContribution(axisAlt, 0.0);
  setAxisGotoContribution(axisAz, 0.0);
  setAxisGotoContribution(axisAlt, 0.0);
  setAxisTrackingContribution(axisAz, 0.0);
  setAxisTrackingContribution(axisAlt, 0.0);
  portENTER_CRITICAL(&trackingMux);
  trackingEnabled = false;
  portEXIT_CRITICAL(&trackingMux);
}

void setTrackingEnabled(bool enabled) {
  portENTER_CRITICAL(&trackingMux);
  trackingEnabled = enabled;
  portEXIT_CRITICAL(&trackingMux);
}

void setTrackingRates(double azDegPerSec, double altDegPerSec) {
  double azSteps = azDegPerSec * calibration.stepsPerDegreeAz;
  double altSteps = altDegPerSec * calibration.stepsPerDegreeAlt;
  setAxisTrackingContribution(axisAz, azSteps);
  setAxisTrackingContribution(axisAlt, altSteps);
}

int64_t getStepCount(Axis axis) { return getAxisCounter(getAxisState(axis)); }

void setStepCount(Axis axis, int64_t value) {
  setAxisCounter(getAxisState(axis), value);
}

double stepsToAzDegrees(int64_t steps) {
  double adjusted = static_cast<double>(steps - calibration.azHomeOffset);
  double degrees = adjusted / calibration.stepsPerDegreeAz;
  degrees = fmod(degrees, 360.0);
  if (degrees < 0.0) {
    degrees += 360.0;
  }
  return degrees;
}

double stepsToAltDegrees(int64_t steps) {
  double adjusted = static_cast<double>(steps - calibration.altHomeOffset);
  double degrees = adjusted / calibration.stepsPerDegreeAlt;
  if (degrees > 180.0 || degrees < -180.0) {
    degrees = fmod(degrees, 360.0);
    if (degrees > 180.0) {
      degrees -= 360.0;
    } else if (degrees < -180.0) {
      degrees += 360.0;
    }
  }
  return degrees;
}

int64_t azDegreesToSteps(double degrees) {
  double wrapped = fmod(degrees, 360.0);
  if (wrapped < 0.0) {
    wrapped += 360.0;
  }
  double steps = wrapped * calibration.stepsPerDegreeAz + calibration.azHomeOffset;
  return static_cast<int64_t>(llround(steps));
}

int64_t altDegreesToSteps(double degrees) {
  double steps = degrees * calibration.stepsPerDegreeAlt + calibration.altHomeOffset;
  return static_cast<int64_t>(llround(steps));
}

void applyCalibration(const AxisCalibration& newCalibration) {
  calibration = newCalibration;
}

void setBacklash(const BacklashConfig& newBacklash) { backlash = newBacklash; }

int32_t getBacklashSteps(Axis axis) {
  return (axis == Axis::Az) ? backlash.azSteps : backlash.altSteps;
}

int8_t getLastDirection(Axis axis) {
  AxisState& state = getAxisState(axis);
  portENTER_CRITICAL(&state.mux);
  int8_t direction = state.lastDirection;
  portEXIT_CRITICAL(&state.mux);
  return direction;
}

}  // namespace motion

#endif  // DEVICE_ROLE_MAIN
