#include "motion.h"

#include <math.h>

#include <TMCStepper.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <esp32-hal.h>
#include <esp32-hal-timer.h>
#endif

#ifndef ESP_ARDUINO_VERSION_VAL
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) \
  ((major << 16) | (minor << 8) | (patch))
#endif

#if defined(ESP_ARDUINO_VERSION) && \
    (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0))
#define MOTION_HAS_NEW_TIMER_API 1
#else
#define MOTION_HAS_NEW_TIMER_API 0
#endif

#include "calibration.h"

namespace {

constexpr double kStepsPerAxisRev =
    config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO;

AxisCalibration calibration{(config::FULLSTEPS_PER_REV * config::MICROSTEPS *
                             config::GEAR_RATIO) /
                                360.0,
                             (config::FULLSTEPS_PER_REV * config::MICROSTEPS *
                              config::GEAR_RATIO) /
                                360.0,
                             0,
                             0};

BacklashConfig backlash{0, 0};

bool trackingEnabled = false;

struct AxisState {
  uint8_t enPin;
  uint8_t dirPin;
  uint8_t stepPin;
  hw_timer_t* timer;
  portMUX_TYPE mux;
  volatile bool stepState;
  volatile int64_t stepCounter;
  volatile int8_t direction;
  double userStepsPerSecond;
  double gotoStepsPerSecond;
  double trackingStepsPerSecond;
  int8_t lastDirection;
};

AxisState axisAz{config::EN_RA,
                 config::DIR_RA,
                 config::STEP_RA,
                 nullptr,
                 portMUX_INITIALIZER_UNLOCKED,
                 false,
                 0,
                 1,
                 0.0,
                 0.0,
                 0.0,
                 0};

AxisState axisAlt{config::EN_DEC,
                  config::DIR_DEC,
                  config::STEP_DEC,
                  nullptr,
                  portMUX_INITIALIZER_UNLOCKED,
                  false,
                  0,
                  1,
                  0.0,
                  0.0,
                  0.0,
                  0};

TMC2209Stepper driverAz(&Serial2, config::R_SENSE, config::DRIVER_ADDR_RA);
TMC2209Stepper driverAlt(&Serial1, config::R_SENSE, config::DRIVER_ADDR_DEC);

void IRAM_ATTR handleAxisStep(AxisState& axis) {
  portENTER_CRITICAL_ISR(&axis.mux);
  axis.stepState = !axis.stepState;
  digitalWrite(axis.stepPin, axis.stepState);
  if (axis.stepState) {
    axis.stepCounter += axis.direction;
  }
  portEXIT_CRITICAL_ISR(&axis.mux);
}

void IRAM_ATTR onStepAz() { handleAxisStep(axisAz); }
void IRAM_ATTR onStepAlt() { handleAxisStep(axisAlt); }

AxisState& getAxisState(Axis axis) {
  return (axis == Axis::Az) ? axisAz : axisAlt;
}

void configureTimer(AxisState& axis, uint8_t timerIndex, void (*isr)()) {
#if MOTION_HAS_NEW_TIMER_API
  (void)timerIndex;
  constexpr uint32_t kTimerBaseHz = 1000000;  // 1 MHz tick frequency
  axis.timer = timerBegin(kTimerBaseHz);
  timerAttachInterrupt(axis.timer, isr);
  timerSetAutoReload(axis.timer, true);
  timerSetOverflow(axis.timer, 1000);
  timerStop(axis.timer);
#else
  axis.timer = timerBegin(timerIndex, 80, true);  // 80MHz / 80 = 1 MHz base
  timerAttachInterrupt(axis.timer, isr, true);
  timerAlarmWrite(axis.timer, 1000, true);
  timerAlarmDisable(axis.timer);
#endif
}

void applyAxisCommand(AxisState& axis) {
  double trackingContribution = trackingEnabled ? axis.trackingStepsPerSecond : 0.0;
  double totalSteps = axis.userStepsPerSecond + axis.gotoStepsPerSecond + trackingContribution;
  if (fabs(totalSteps) < 0.1) {
#if MOTION_HAS_NEW_TIMER_API
    timerStop(axis.timer);
#else
    timerAlarmDisable(axis.timer);
#endif
    portENTER_CRITICAL(&axis.mux);
    axis.stepState = false;
    portEXIT_CRITICAL(&axis.mux);
    digitalWrite(axis.stepPin, LOW);
    return;
  }

  axis.direction = (totalSteps >= 0.0) ? 1 : -1;
  axis.lastDirection = axis.direction;
  digitalWrite(axis.dirPin, axis.direction > 0 ? HIGH : LOW);

  double freq = fabs(totalSteps) * 2.0;  // toggle high/low
  double periodUs = 1000000.0 / freq;
  if (periodUs < 50.0) {
    periodUs = 50.0;  // limit to avoid overwhelming timer
  }
#if MOTION_HAS_NEW_TIMER_API
  timerSetOverflow(axis.timer, static_cast<uint64_t>(periodUs));
  timerWrite(axis.timer, 0);
  timerStart(axis.timer);
#else
  timerAlarmWrite(axis.timer, static_cast<uint64_t>(periodUs), true);
  timerAlarmEnable(axis.timer);
#endif
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
  driverAz.begin();
  delay(50);
  driverAz.pdn_disable(true);
  driverAz.en_spreadCycle(false);
  driverAz.rms_current(config::DRIVER_CURRENT_MA);
  driverAz.microsteps(static_cast<uint16_t>(config::MICROSTEPS));
  digitalWrite(axisAz.enPin, LOW);

  Serial1.begin(115200, SERIAL_8N1, config::UART_DEC_RX, config::UART_DEC_TX);
  driverAlt.begin();
  delay(50);
  driverAlt.pdn_disable(true);
  driverAlt.en_spreadCycle(false);
  driverAlt.rms_current(config::DRIVER_CURRENT_MA);
  driverAlt.microsteps(static_cast<uint16_t>(config::MICROSTEPS));
  digitalWrite(axisAlt.enPin, LOW);

  configureTimer(axisAz, 0, onStepAz);
  configureTimer(axisAlt, 1, onStepAlt);

  stopAll();
}

void setManualRate(Axis axis, float rpm) {
  double stepsPerSecond = (static_cast<double>(rpm) * kStepsPerAxisRev) / 60.0;
  setManualStepsPerSecond(axis, stepsPerSecond);
}

void setManualStepsPerSecond(Axis axis, double stepsPerSecond) {
  AxisState& state = getAxisState(axis);
  state.userStepsPerSecond = stepsPerSecond;
  applyAxisCommand(state);
}

void setGotoStepsPerSecond(Axis axis, double stepsPerSecond) {
  AxisState& state = getAxisState(axis);
  state.gotoStepsPerSecond = stepsPerSecond;
  applyAxisCommand(state);
}

void clearGotoRates() {
  axisAz.gotoStepsPerSecond = 0.0;
  axisAlt.gotoStepsPerSecond = 0.0;
  applyAxisCommand(axisAz);
  applyAxisCommand(axisAlt);
}

void stopAll() {
  axisAz.userStepsPerSecond = 0.0;
  axisAlt.userStepsPerSecond = 0.0;
  axisAz.gotoStepsPerSecond = 0.0;
  axisAlt.gotoStepsPerSecond = 0.0;
  axisAz.trackingStepsPerSecond = 0.0;
  axisAlt.trackingStepsPerSecond = 0.0;
  trackingEnabled = false;
#if MOTION_HAS_NEW_TIMER_API
  timerStop(axisAz.timer);
  timerStop(axisAlt.timer);
#else
  timerAlarmDisable(axisAz.timer);
  timerAlarmDisable(axisAlt.timer);
#endif
  digitalWrite(axisAz.stepPin, LOW);
  digitalWrite(axisAlt.stepPin, LOW);
}

void setTrackingEnabled(bool enabled) {
  trackingEnabled = enabled;
  applyAxisCommand(axisAz);
  applyAxisCommand(axisAlt);
}

void setTrackingRates(double azDegPerSec, double altDegPerSec) {
  axisAz.trackingStepsPerSecond = azDegPerSec * calibration.stepsPerDegreeAz;
  axisAlt.trackingStepsPerSecond = altDegPerSec * calibration.stepsPerDegreeAlt;
  if (trackingEnabled) {
    applyAxisCommand(axisAz);
    applyAxisCommand(axisAlt);
  }
}

int64_t getStepCount(Axis axis) {
  AxisState& state = getAxisState(axis);
  portENTER_CRITICAL(&state.mux);
  int64_t steps = state.stepCounter;
  portEXIT_CRITICAL(&state.mux);
  return steps;
}

void setStepCount(Axis axis, int64_t value) {
  AxisState& state = getAxisState(axis);
  portENTER_CRITICAL(&state.mux);
  state.stepCounter = value;
  portEXIT_CRITICAL(&state.mux);
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
  applyAxisCommand(axisAz);
  applyAxisCommand(axisAlt);
}

void setBacklash(const BacklashConfig& newBacklash) { backlash = newBacklash; }

int32_t getBacklashSteps(Axis axis) {
  return (axis == Axis::Az) ? backlash.azSteps : backlash.altSteps;
}

int8_t getLastDirection(Axis axis) {
  AxisState& state = getAxisState(axis);
  return state.lastDirection;
}

}  // namespace motion

