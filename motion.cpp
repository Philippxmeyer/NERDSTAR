#include "motion.h"

#include <math.h>

#include <TMCStepper.h>

#include "calibration.h"

namespace {

AxisCalibration calibration{
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 24.0,
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 360.0,
    0,
    0};

double siderealStepsPerSecond =
    (calibration.stepsPerHourRA * 24.0) / config::SIDEREAL_DAY_SECONDS;

struct AxisState {
  uint8_t enPin;
  uint8_t dirPin;
  uint8_t stepPin;
  hw_timer_t* timer;
  portMUX_TYPE mux;
  volatile bool stepState;
  volatile int64_t stepCounter;
  volatile int8_t direction;
  double manualStepsPerSecond;
  double trackingStepsPerSecond;
};

AxisState axisRA{
    config::EN_RA,
    config::DIR_RA,
    config::STEP_RA,
    nullptr,
    portMUX_INITIALIZER_UNLOCKED,
    false,
    0,
    1,
    0.0,
    0.0};

AxisState axisDEC{
    config::EN_DEC,
    config::DIR_DEC,
    config::STEP_DEC,
    nullptr,
    portMUX_INITIALIZER_UNLOCKED,
    false,
    0,
    1,
    0.0,
    0.0};

TMC2209Stepper driverRA(&Serial2, config::R_SENSE, config::DRIVER_ADDR_RA);
TMC2209Stepper driverDEC(&Serial1, config::R_SENSE, config::DRIVER_ADDR_DEC);

void IRAM_ATTR handleAxisStep(AxisState& axis) {
  portENTER_CRITICAL_ISR(&axis.mux);
  axis.stepState = !axis.stepState;
  digitalWrite(axis.stepPin, axis.stepState);
  if (axis.stepState) {
    axis.stepCounter += axis.direction;
  }
  portEXIT_CRITICAL_ISR(&axis.mux);
}

void IRAM_ATTR onStepRA() { handleAxisStep(axisRA); }
void IRAM_ATTR onStepDEC() { handleAxisStep(axisDEC); }

AxisState& getAxisState(Axis axis) {
  return (axis == Axis::RA) ? axisRA : axisDEC;
}

void configureTimer(AxisState& axis, uint8_t timerIndex, void (*isr)()) {
  axis.timer = timerBegin(timerIndex, 80, true); // 80MHz / 80 = 1 MHz base
  timerAttachInterrupt(axis.timer, isr, true);
  timerAlarmWrite(axis.timer, 1000, true);
  timerAlarmDisable(axis.timer);
}

void applyAxisCommand(AxisState& axis) {
  double totalSteps = axis.manualStepsPerSecond + axis.trackingStepsPerSecond;
  if (fabs(totalSteps) < 0.1) {
    timerAlarmDisable(axis.timer);
    portENTER_CRITICAL(&axis.mux);
    axis.stepState = false;
    portEXIT_CRITICAL(&axis.mux);
    digitalWrite(axis.stepPin, LOW);
    return;
  }

  axis.direction = (totalSteps >= 0.0) ? 1 : -1;
  digitalWrite(axis.dirPin, axis.direction > 0 ? HIGH : LOW);

  double freq = fabs(totalSteps) * 2.0; // toggle high/low
  double periodUs = 1000000.0 / freq;
  if (periodUs < 50.0) {
    periodUs = 50.0; // limit to avoid overwhelming timer
  }
  timerAlarmWrite(axis.timer, static_cast<uint64_t>(periodUs), true);
  timerAlarmEnable(axis.timer);
}

} // namespace

namespace motion {

void init() {
  pinMode(axisRA.enPin, OUTPUT);
  pinMode(axisRA.dirPin, OUTPUT);
  pinMode(axisRA.stepPin, OUTPUT);
  pinMode(axisDEC.enPin, OUTPUT);
  pinMode(axisDEC.dirPin, OUTPUT);
  pinMode(axisDEC.stepPin, OUTPUT);

  digitalWrite(axisRA.enPin, HIGH);
  digitalWrite(axisDEC.enPin, HIGH);
  digitalWrite(axisRA.stepPin, LOW);
  digitalWrite(axisDEC.stepPin, LOW);

  Serial2.begin(115200, SERIAL_8N1, config::UART_RA_RX, config::UART_RA_TX);
  driverRA.begin();
  delay(50);
  driverRA.pdn_disable(true);
  driverRA.en_spreadCycle(false);
  driverRA.rms_current(config::DRIVER_CURRENT_MA);
  driverRA.microsteps(static_cast<uint16_t>(config::MICROSTEPS));
  digitalWrite(axisRA.enPin, LOW);

  Serial1.begin(115200, SERIAL_8N1, config::UART_DEC_RX, config::UART_DEC_TX);
  driverDEC.begin();
  delay(50);
  driverDEC.pdn_disable(true);
  driverDEC.en_spreadCycle(false);
  driverDEC.rms_current(config::DRIVER_CURRENT_MA);
  driverDEC.microsteps(static_cast<uint16_t>(config::MICROSTEPS));
  digitalWrite(axisDEC.enPin, LOW);

  configureTimer(axisRA, 0, onStepRA);
  configureTimer(axisDEC, 1, onStepDEC);

  stopAll();
}

void setManualRate(Axis axis, float rpm) {
  AxisState& state = getAxisState(axis);
  state.manualStepsPerSecond = (static_cast<double>(rpm) * kStepsPerAxisRev) / 60.0;
  applyAxisCommand(state);
}

void stopAll() {
  axisRA.manualStepsPerSecond = 0.0;
  axisDEC.manualStepsPerSecond = 0.0;
  axisRA.trackingStepsPerSecond = 0.0;
  axisDEC.trackingStepsPerSecond = 0.0;
  timerAlarmDisable(axisRA.timer);
  timerAlarmDisable(axisDEC.timer);
  digitalWrite(axisRA.stepPin, LOW);
  digitalWrite(axisDEC.stepPin, LOW);
}

void setTrackingEnabled(bool enabled) {
  axisRA.trackingStepsPerSecond = enabled ? siderealStepsPerSecond : 0.0;
  applyAxisCommand(axisRA);
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

double stepsToRaHours(int64_t steps) {
  double adjusted = static_cast<double>(steps - calibration.raHomeOffset);
  double hours = fmod(adjusted / calibration.stepsPerHourRA, 24.0);
  if (hours < 0.0) {
    hours += 24.0;
  }
  return hours;
}

double stepsToDecDegrees(int64_t steps) {
  double adjusted = static_cast<double>(steps - calibration.decHomeOffset);
  double degrees = adjusted / calibration.stepsPerDegreeDEC;
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

int64_t raHoursToSteps(double hours) {
  double normalized = fmod(hours, 24.0);
  if (normalized < 0.0) {
    normalized += 24.0;
  }
  double steps = normalized * calibration.stepsPerHourRA + calibration.raHomeOffset;
  return static_cast<int64_t>(llround(steps));
}

int64_t decDegreesToSteps(double degrees) {
  double steps = degrees * calibration.stepsPerDegreeDEC + calibration.decHomeOffset;
  return static_cast<int64_t>(llround(steps));
}

void applyCalibration(const AxisCalibration& newCalibration) {
  calibration = newCalibration;
  siderealStepsPerSecond = (calibration.stepsPerHourRA * 24.0) / config::SIDEREAL_DAY_SECONDS;
  applyAxisCommand(axisRA);
  applyAxisCommand(axisDEC);
}

} // namespace motion

