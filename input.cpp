#include "input.h"

#include <math.h>

#include "config.h"

namespace {

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
volatile int encoderTicks = 0;
volatile bool encoderClick = false;
volatile bool joystickClick = false;
volatile uint32_t lastEncoderEdgeUs = 0;

constexpr uint32_t ENCODER_DEBOUNCE_US = 300;

JoystickCalibration currentCalibration{2048, 2048};
bool lastJoystickState = false;

void IRAM_ATTR handleEncoderPinA() {
  const uint32_t now = micros();

  portENTER_CRITICAL_ISR(&encoderMux);
  if (now - lastEncoderEdgeUs < ENCODER_DEBOUNCE_US) {
    portEXIT_CRITICAL_ISR(&encoderMux);
    return;
  }
  lastEncoderEdgeUs = now;

  int a = digitalRead(config::ROT_A);
  int b = digitalRead(config::ROT_B);
  if (a == b) {
    encoderTicks++;
  } else if (encoderSubSteps <= -kEncoderStepsPerNotch) {
    encoderSubSteps += kEncoderStepsPerNotch;
    encoderTicks--;
  }

  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR handleEncoderPinA() {
  handleEncoderEdge();
}

void IRAM_ATTR handleEncoderPinB() {
  const uint32_t now = micros();

  portENTER_CRITICAL_ISR(&encoderMux);
  if (now - lastEncoderEdgeUs < ENCODER_DEBOUNCE_US) {
    portEXIT_CRITICAL_ISR(&encoderMux);
    return;
  }
  lastEncoderEdgeUs = now;

  int a = digitalRead(config::ROT_A);
  int b = digitalRead(config::ROT_B);
  if (a != b) {
    encoderTicks++;
  } else {
    encoderTicks--;
  }
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR handleEncoderButton() {
  if (digitalRead(config::ROT_BTN) == LOW) {
    encoderClick = true;
  }
}

void updateJoystickButton() {
  bool pressed = (digitalRead(config::JOY_BTN) == LOW);
  if (pressed && !lastJoystickState) {
    joystickClick = true;
  }
  lastJoystickState = pressed;
}

} // namespace

namespace input {

void init() {
  pinMode(config::JOY_BTN, INPUT_PULLUP);
  pinMode(config::ROT_A, INPUT_PULLUP);
  pinMode(config::ROT_B, INPUT_PULLUP);
  pinMode(config::ROT_BTN, INPUT_PULLUP);

  portENTER_CRITICAL(&encoderMux);
  bool levelA = digitalRead(config::ROT_A) == HIGH;
  bool levelB = digitalRead(config::ROT_B) == HIGH;
  encoderState = static_cast<uint8_t>(((levelA ? 1u : 0u) << 1) | (levelB ? 1u : 0u));
  encoderSubSteps = 0;
  encoderTicks = 0;
  portEXIT_CRITICAL(&encoderMux);

  attachInterrupt(digitalPinToInterrupt(config::ROT_A), handleEncoderPinA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(config::ROT_B), handleEncoderPinB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(config::ROT_BTN), handleEncoderButton, FALLING);

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

int consumeEncoderDelta() {
  portENTER_CRITICAL(&encoderMux);
  int delta = encoderTicks;
  encoderTicks = 0;
  portEXIT_CRITICAL(&encoderMux);
  return delta;
}

bool consumeEncoderClick() {
  bool clicked = encoderClick;
  encoderClick = false;
  return clicked;
}

int getJoystickCenterX() {
  return currentCalibration.centerX;
}

int getJoystickCenterY() {
  return currentCalibration.centerY;
}

void setJoystickCalibration(const JoystickCalibration& calibration) {
  currentCalibration = calibration;
}

} // namespace input

