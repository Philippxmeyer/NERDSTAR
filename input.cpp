#include "input.h"

#if defined(DEVICE_ROLE_HID)

#include <AiEsp32RotaryEncoder.h>
#include <math.h>

#include "config.h"

namespace {

constexpr int kEncoderStepsPerNotch = 4;
constexpr long kEncoderMinValue = -100000;
constexpr long kEncoderMaxValue = 100000;
constexpr uint32_t kAccelerationResetMs = 400;
constexpr int kMaxAcceleratedStep = 6;

AiEsp32RotaryEncoder rotaryEncoder(config::ROT_A, config::ROT_B, config::ROT_BTN, -1);

JoystickCalibration currentCalibration{2048, 2048};
bool joystickClick = false;
bool lastJoystickState = false;

long lastEncoderValue = 0;
uint32_t lastEncoderEventMs = 0;
float encoderAccelerationRemainder = 0.0f;

void IRAM_ATTR handleEncoderISR() { rotaryEncoder.readEncoder_ISR(); }

void IRAM_ATTR handleEncoderButtonISR() { rotaryEncoder.readButton_ISR(); }

void updateJoystickButton() {
  bool pressed = (digitalRead(config::JOY_BTN) == LOW);
  if (pressed && !lastJoystickState) {
    joystickClick = true;
  }
  lastJoystickState = pressed;
}

int applySoftAcceleration(int rawDelta) {
  if (rawDelta == 0) {
    return 0;
  }

  uint32_t now = millis();
  uint32_t elapsed = now - lastEncoderEventMs;
  lastEncoderEventMs = now;

  if (elapsed > kAccelerationResetMs) {
    encoderAccelerationRemainder = 0.0f;
  }

  float factor = 1.0f;
  if (elapsed <= 40) {
    factor = 2.0f;
  } else if (elapsed <= 100) {
    factor = 1.6f;
  } else if (elapsed <= 180) {
    factor = 1.3f;
  }

  float scaled = static_cast<float>(rawDelta) * factor + encoderAccelerationRemainder;
  int accelerated = static_cast<int>(scaled);
  encoderAccelerationRemainder = scaled - static_cast<float>(accelerated);

  if (accelerated == 0) {
    accelerated = (rawDelta > 0) ? 1 : -1;
    encoderAccelerationRemainder = scaled - static_cast<float>(accelerated);
  }

  if (accelerated > kMaxAcceleratedStep) {
    encoderAccelerationRemainder += static_cast<float>(accelerated - kMaxAcceleratedStep);
    accelerated = kMaxAcceleratedStep;
  } else if (accelerated < -kMaxAcceleratedStep) {
    encoderAccelerationRemainder += static_cast<float>(accelerated + kMaxAcceleratedStep);
    accelerated = -kMaxAcceleratedStep;
  }

  return accelerated;
}

}  // namespace

namespace input {

void init() {
  pinMode(config::JOY_BTN, INPUT_PULLUP);
  pinMode(config::ROT_A, INPUT_PULLUP);
  pinMode(config::ROT_B, INPUT_PULLUP);
  pinMode(config::ROT_BTN, INPUT_PULLUP);

  rotaryEncoder.begin();
  rotaryEncoder.setup(handleEncoderISR, handleEncoderButtonISR);
  rotaryEncoder.setEncoderStepsPerNotch(kEncoderStepsPerNotch);
  rotaryEncoder.setBoundaries(kEncoderMinValue, kEncoderMaxValue, true);
  rotaryEncoder.disableAcceleration();
  rotaryEncoder.reset(0);

  lastEncoderValue = rotaryEncoder.readEncoder();
  lastEncoderEventMs = millis();
  encoderAccelerationRemainder = 0.0f;

  analogReadResolution(12);
}

JoystickCalibration calibrateJoystick() {
  long sumX = 0;
  long sumY = 0;
  constexpr int samples = 100;
  for (int i = 0; i < samples; ++i) {
    sumX += analogRead(config::JOY_X);
    sumY += analogRead(config::JOY_Y);
    delay(5);
  }
  currentCalibration.centerX = sumX / samples;
  currentCalibration.centerY = sumY / samples;
  return currentCalibration;
}

void update() {
  rotaryEncoder.loop();
  updateJoystickButton();
}

float getJoystickNormalizedX() {
  int value = analogRead(config::JOY_X);
  float normalized = static_cast<float>(value - currentCalibration.centerX) / 2048.0f;
  if (fabs(normalized) < config::JOYSTICK_DEADZONE) {
    normalized = 0.0f;
  }
  if (normalized > 1.0f) normalized = 1.0f;
  if (normalized < -1.0f) normalized = -1.0f;
  return normalized;
}

float getJoystickNormalizedY() {
  int value = analogRead(config::JOY_Y);
  float normalized = static_cast<float>(value - currentCalibration.centerY) / 2048.0f;
  if (fabs(normalized) < config::JOYSTICK_DEADZONE) {
    normalized = 0.0f;
  }
  if (normalized > 1.0f) normalized = 1.0f;
  if (normalized < -1.0f) normalized = -1.0f;
  return normalized;
}

bool consumeJoystickPress() {
  bool clicked = joystickClick;
  joystickClick = false;
  return clicked;
}

bool isJoystickButtonPressed() { return lastJoystickState; }

int consumeEncoderDelta() {
  long current = rotaryEncoder.readEncoder();
  int rawDelta = static_cast<int>(current - lastEncoderValue);
  if (rawDelta == 0) {
    return 0;
  }
  lastEncoderValue = current;

  if (rawDelta > kMaxAcceleratedStep || rawDelta < -kMaxAcceleratedStep) {
    encoderAccelerationRemainder = 0.0f;
    return rawDelta;
  }

  int accelerated = applySoftAcceleration(rawDelta);
  return accelerated;
}

bool consumeEncoderClick() { return rotaryEncoder.isEncoderButtonClicked(); }

int getJoystickCenterX() { return currentCalibration.centerX; }

int getJoystickCenterY() { return currentCalibration.centerY; }

void setJoystickCalibration(const JoystickCalibration& calibration) {
  currentCalibration = calibration;
}

}  // namespace input

#endif  // DEVICE_ROLE_HID

