// motion.cpp — ESP32 Core 3.x only (uses esp_timer with TASK dispatch)

#include "motion.h"

#include <math.h>
#include <Arduino.h>
#include <TMCStepper.h>

#include <esp32-hal.h>   // pinMode/digitalWrite, portMUX*
#include <esp_timer.h>   // esp_timer (Task-Dispatch)

#ifndef ESP_ARDUINO_VERSION_VAL
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) \
  ((major << 16) | (minor << 8) | (patch))
#endif

#if !defined(ESP_ARDUINO_VERSION) || (ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0))
#error "This motion.cpp requires Arduino-ESP32 Core >= 3.0.0"
#endif

#include "calibration.h"

namespace {

constexpr double kStepsPerAxisRev =
    config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO;

AxisCalibration calibration{
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 360.0,
    (config::FULLSTEPS_PER_REV * config::MICROSTEPS * config::GEAR_RATIO) / 360.0,
    0,
    0};

BacklashConfig backlash{0, 0};

bool trackingEnabled = false;

struct AxisState {
  uint8_t enPin;
  uint8_t dirPin;
  uint8_t stepPin;
  esp_timer_handle_t timer;       // Core 3.x: esp_timer (TASK dispatch)
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

// Achtung: Wir laufen im TASK-Kontext (nicht ISR). Daher _ohne_ ISR-Makros.
void handleAxisStep(AxisState& axis) {
  portENTER_CRITICAL(&axis.mux);
  axis.stepState = !axis.stepState;
  digitalWrite(axis.stepPin, axis.stepState);
  if (axis.stepState) {
    axis.stepCounter += axis.direction;
  }
  portEXIT_CRITICAL(&axis.mux);
}

void onStepAz() { handleAxisStep(axisAz); }
void onStepAlt() { handleAxisStep(axisAlt); }

// esp_timer-Callback trampolinen wir auf obige Funktionen
void timer_trampoline(void* arg) {
  auto fn = reinterpret_cast<void (*)()>(arg);
  fn();
}

AxisState& getAxisState(Axis axis) {
  return (axis == Axis::Az) ? axisAz : axisAlt;
}

void configureTimer(AxisState& axis, uint8_t /*timerIndex*/, void (*isr)()) {
  esp_timer_create_args_t args{};
  args.callback = &timer_trampoline;
  args.arg = reinterpret_cast<void*>(isr);
  args.dispatch_method = ESP_TIMER_TASK;  // kein ISR-Dispatch in deinem Core
  args.name = "axis";
  ESP_ERROR_CHECK(esp_timer_create(&args, &axis.timer));
  // Start/Periode erfolgt in applyAxisCommand()
}

void applyAxisCommand(AxisState& axis) {
  double trackingContribution = trackingEnabled ? axis.trackingStepsPerSecond : 0.0;
  double totalSteps = axis.userStepsPerSecond + axis.gotoStepsPerSecond + trackingContribution;

  if (fabs(totalSteps) < 0.1) {
    (void)esp_timer_stop(axis.timer);   // ignoriert Fehler, falls schon gestoppt
    portENTER_CRITICAL(&axis.mux);
    axis.stepState = false;
    portEXIT_CRITICAL(&axis.mux);
    digitalWrite(axis.stepPin, LOW);
    return;
  }

  axis.direction = (totalSteps >= 0.0) ? 1 : -1;
  axis.lastDirection = axis.direction;
  digitalWrite(axis.dirPin, axis.direction > 0 ? HIGH : LOW);

  // Toggle-Frequenz (HIGH/LOW) -> Periodendauer in µs
  double freq = fabs(totalSteps) * 2.0;
  double periodUs = 1000000.0 / freq;
  if (periodUs < 50.0) {
    periodUs = 50.0;  // CPU-Load/Jitter begrenzen
  }

  // esp_timer hat kein „SetOverflow“ -> stop + start_periodic
  (void)esp_timer_stop(axis.timer);
  ESP_ERROR_CHECK(esp_timer_start_periodic(axis.timer, static_cast<uint64_t>(periodUs)));
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

  (void)esp_timer_stop(axisAz.timer);
  (void)esp_timer_stop(axisAlt.timer);

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
