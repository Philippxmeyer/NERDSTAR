#include "input.h"

#if defined(DEVICE_ROLE_HID)

#include <math.h>
#include <stdint.h>

#include "config.h"

namespace {

portMUX_TYPE encoderMux = portMUX_INITIALIZER_UNLOCKED;
volatile int encoderTicks = 0;
volatile uint8_t encoderState = 0;
volatile int8_t encoderStepAccumulator = 0;
volatile bool encoderClick = false;
volatile bool joystickClick = false;
volatile uint32_t lastEncoderEdgeUs = 0;

constexpr uint32_t ENCODER_DEBOUNCE_US = 250;
constexpr int kEncoderStepsPerNotch = 4;

constexpr int8_t kTransitionTable[4][4] = {
    {0, -1, 1, 0},
    {1, 0, 0, -1},
    {-1, 0, 0, 1},
    {0, 1, -1, 0},
};

JoystickCalibration currentCalibration{2048, 2048};
bool lastJoystickState = false;

constexpr int8_t kQuadratureTable[16] = {
    0,  -1, 1,  0,   // 00 -> 00/01/10/11
    1,  0,  0,  -1,  // 01 -> 00/01/10/11
    -1, 0,  0,  1,   // 10 -> 00/01/10/11
    0,  1,  -1, 0};  // 11 -> 00/01/10/11

void IRAM_ATTR updateEncoderState() {
  uint8_t a = static_cast<uint8_t>(digitalRead(config::ROT_A));
  uint8_t b = static_cast<uint8_t>(digitalRead(config::ROT_B));
  uint8_t current = static_cast<uint8_t>((a << 1) | b);
  uint8_t previous = encoderState & 0x03;
  uint8_t index = static_cast<uint8_t>((previous << 2) | current);
  encoderState = current;
  int8_t movement = kQuadratureTable[index];
  encoderStepAccumulator += movement;
  if (encoderStepAccumulator >= 4) {
    encoderTicks++;
    encoderStepAccumulator = 0;
  } else if (encoderStepAccumulator <= -4) {
    encoderTicks--;
    encoderStepAccumulator = 0;
  }
}

void IRAM_ATTR handleEncoderPinA() {
  portENTER_CRITICAL_ISR(&encoderMux);
  updateEncoderState();
  portEXIT_CRITICAL_ISR(&encoderMux);
}

void IRAM_ATTR handleEncoderPinB() {
  portENTER_CRITICAL_ISR(&encoderMux);
  updateEncoderState();
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

  encoderState = static_cast<uint8_t>((digitalRead(config::ROT_A) << 1) |
                                      digitalRead(config::ROT_B));
  encoderStepAccumulator = 0;

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

}  // namespace input

#endif  // DEVICE_ROLE_HID

