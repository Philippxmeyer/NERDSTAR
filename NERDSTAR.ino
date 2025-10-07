/*
  ============================================================
   🤓✨ NERDSTAR
   Navigation, Elevation & Rotation Drive System
   for Telescope Alt/Az Regulation
   ------------------------------------------------------------
   Because even the cosmos deserves a little nerd power.
   ------------------------------------------------------------
   Features:
   - Dual TMC2209 stepper drivers (Azimuth + Altitude)
   - Hardware timers for µs-precise stepping
   - OLED Display (SSD1306)
   - RTC DS3231 for real-time clock base
   - Joystick control for manual movement
   - Center calibration at startup
   - Ready for coordinate math (Alt/Az tracking)
   ------------------------------------------------------------
   Author: Philipp
   License: GNU General Public License v3.0
  ============================================================
*/

#include <TMCStepper.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>

// ----------------- Pin Definitions -----------------
#define EN_AZ      27
#define DIR_AZ     26
#define STEP_AZ    25
#define UART_AZ_TX 17
#define UART_AZ_RX 16

#define EN_ALT     14
#define DIR_ALT    12
#define STEP_ALT   13
#define UART_ALT_TX 5
#define UART_ALT_RX 4

// Joystick KY-023
#define JOY_X 34     // X = Azimuth
#define JOY_Y 35     // Y = Altitude
#define JOY_BTN 32   // Taster (LOW aktiv)

// OLED + RTC I2C
#define SDA_PIN 21
#define SCL_PIN 22

// --------------- Constants -----------------
#define R_SENSE 0.11f
#define DRIVER_ADDR_AZ 0b00
#define DRIVER_ADDR_ALT 0b01

#define FULLSTEPS_PER_REV  (32 * 64)   // 2048
#define MICROSTEPS         16
#define DRIVER_CURRENT     600         // mA
#define MAX_RPM_MANUAL     3.0f        // max speed per joystick

// --------------- Globals -----------------
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RTC_DS3231 rtc;

TMC2209Stepper driverAZ(&Serial2, R_SENSE, DRIVER_ADDR_AZ);
TMC2209Stepper driverALT(&Serial1, R_SENSE, DRIVER_ADDR_ALT);

hw_timer_t *timerAZ = nullptr;
hw_timer_t *timerALT = nullptr;

portMUX_TYPE muxAZ = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE muxALT = portMUX_INITIALIZER_UNLOCKED;

volatile bool stepStateAZ = false;
volatile bool stepStateALT = false;

volatile float rpmAZ = 0.0f;
volatile float rpmALT = 0.0f;

int joyCenterX = 2048;
int joyCenterY = 2048;

// ----------------- ISR -----------------
void IRAM_ATTR onStepAZ() {
  portENTER_CRITICAL_ISR(&muxAZ);
  digitalWrite(STEP_AZ, stepStateAZ);
  stepStateAZ = !stepStateAZ;
  portEXIT_CRITICAL_ISR(&muxAZ);
}

void IRAM_ATTR onStepALT() {
  portENTER_CRITICAL_ISR(&muxALT);
  digitalWrite(STEP_ALT, stepStateALT);
  stepStateALT = !stepStateALT;
  portEXIT_CRITICAL_ISR(&muxALT);
}

// ----------------- Display Task -----------------
void displayTask(void *pvParams) {
  for (;;) {
    DateTime now = rtc.now();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("NERDSTAR v1.0");
    display.print("AZ: "); display.println(rpmAZ, 2);
    display.print("ALT: "); display.println(rpmALT, 2);
    display.print("Time: ");
    display.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
    display.display();

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// ----------------- Calibration -----------------
void calibrateJoystickCenter() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Calibrating joystick...");
  display.display();

  long sumX = 0, sumY = 0;
  for (int i = 0; i < 100; i++) {
    sumX += analogRead(JOY_X);
    sumY += analogRead(JOY_Y);
    delay(5);
  }
  joyCenterX = sumX / 100;
  joyCenterY = sumY / 100;

  display.clearDisplay();
  display.println("Center calibrated!");
  display.printf("CX=%d  CY=%d\n", joyCenterX, joyCenterY);
  display.display();
  delay(1000);
}

// ----------------- Setup -----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(EN_AZ, OUTPUT); pinMode(DIR_AZ, OUTPUT); pinMode(STEP_AZ, OUTPUT);
  pinMode(EN_ALT, OUTPUT); pinMode(DIR_ALT, OUTPUT); pinMode(STEP_ALT, OUTPUT);
  digitalWrite(EN_AZ, HIGH); digitalWrite(EN_ALT, HIGH);
  digitalWrite(STEP_AZ, LOW); digitalWrite(STEP_ALT, LOW);

  pinMode(JOY_BTN, INPUT_PULLUP);
  analogReadResolution(12);

  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (!rtc.begin()) {
    display.println("RTC not found!");
    display.display();
    while (true);
  }

  calibrateJoystickCenter();

  // --- Setup TMC2209 (AZ) ---
  Serial2.begin(115200, SERIAL_8N1, UART_AZ_RX, UART_AZ_TX);
  driverAZ.begin(); delay(50);
  driverAZ.pdn_disable(true);
  driverAZ.en_spreadCycle(false);
  driverAZ.rms_current(DRIVER_CURRENT);
  driverAZ.microsteps(MICROSTEPS);
  digitalWrite(EN_AZ, LOW);

  // --- Setup TMC2209 (ALT) ---
  Serial1.begin(115200, SERIAL_8N1, UART_ALT_RX, UART_ALT_TX);
  driverALT.begin(); delay(50);
  driverALT.pdn_disable(true);
  driverALT.en_spreadCycle(false);
  driverALT.rms_current(DRIVER_CURRENT);
  driverALT.microsteps(MICROSTEPS);
  digitalWrite(EN_ALT, LOW);

  // --- Timer AZ ---
  timerAZ = timerBegin(1e6);
  timerAttachInterrupt(timerAZ, &onStepAZ);
  timerAlarm(timerAZ, 1000, true, 0);
  timerStart(timerAZ);

  // --- Timer ALT ---
  timerALT = timerBegin(1e6);
  timerAttachInterrupt(timerALT, &onStepALT);
  timerAlarm(timerALT, 1000, true, 0);
  timerStart(timerALT);

  // --- OLED Task (Core 0) ---
  xTaskCreatePinnedToCore(displayTask, "display", 4096, nullptr, 1, nullptr, 0);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("NERDSTAR ready.");
  display.display();
}

// ----------------- Loop -----------------
void loop() {
  int joyX = analogRead(JOY_X);
  int joyY = analogRead(JOY_Y);
  bool pressed = (digitalRead(JOY_BTN) == LOW);

  // --- AZ (X-Achse) ---
  float posX = (joyX - joyCenterX) / 2048.0f;
  if (fabs(posX) < 0.05f) posX = 0.0f;
  rpmAZ = posX * MAX_RPM_MANUAL;

  // --- ALT (Y-Achse) ---
  float posY = (joyY - joyCenterY) / 2048.0f;
  if (fabs(posY) < 0.05f) posY = 0.0f;
  rpmALT = posY * MAX_RPM_MANUAL;

  // Richtungen
  digitalWrite(DIR_AZ, rpmAZ >= 0.0f ? HIGH : LOW);
  digitalWrite(DIR_ALT, rpmALT >= 0.0f ? HIGH : LOW);

  // Schrittfrequenzen berechnen
  const double steps_per_rev = FULLSTEPS_PER_REV * (double)MICROSTEPS;
  const double stepsAZ = (steps_per_rev * fabs(rpmAZ)) / 60.0;
  const double stepsALT = (steps_per_rev * fabs(rpmALT)) / 60.0;

  const double freqAZ = stepsAZ * 2.0;
  const double freqALT = stepsALT * 2.0;

  // Timer anpassen
  if (freqAZ > 1.0) { uint64_t p = (uint64_t)(1e6 / freqAZ); timerAlarm(timerAZ, p, true, 0); timerStart(timerAZ); }
  else timerStop(timerAZ);

  if (freqALT > 1.0) { uint64_t p = (uint64_t)(1e6 / freqALT); timerAlarm(timerALT, p, true, 0); timerStart(timerALT); }
  else timerStop(timerALT);

  if (pressed) { timerStop(timerAZ); timerStop(timerALT); }

  vTaskDelay(pdMS_TO_TICKS(50));
}
