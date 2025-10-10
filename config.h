#pragma once

#include <Arduino.h>

#include "role_config.h"

namespace config {

// Stepper driver pins for Azimuth axis
constexpr uint8_t EN_RA = 27;
constexpr uint8_t DIR_RA = 26;
constexpr uint8_t STEP_RA = 25;
constexpr uint8_t UART_RA_TX = 17;
constexpr uint8_t UART_RA_RX = 16;

// Stepper driver pins for Altitude axis
constexpr uint8_t EN_DEC = 14;
constexpr uint8_t DIR_DEC = 12;
constexpr uint8_t STEP_DEC = 13;
constexpr uint8_t UART_DEC_TX = 5;
constexpr uint8_t UART_DEC_RX = 4;

// Joystick KY-023 pins
constexpr uint8_t JOY_X = 34;
constexpr uint8_t JOY_Y = 35;
constexpr uint8_t JOY_BTN = 32; // LOW active

// Rotary encoder pins  (weg von VSPI!)
constexpr uint8_t ROT_A   = 36; // SENSOR_VP, input-only, ext. Pullup nötig wenn "nackter" Encoder
constexpr uint8_t ROT_B   = 39; // SENSOR_VN, input-only, ext. Pullup nötig wenn "nackter" Encoder
constexpr uint8_t ROT_BTN = 33; // mit INPUT_PULLUP betreiben


// OLED + RTC I2C pins
constexpr uint8_t SDA_PIN = 21;
constexpr uint8_t SCL_PIN = 22;

// SD card chip select
constexpr uint8_t SD_CS_PIN = 15;

// SD card SPI pins/frequency (VSPI default)
constexpr uint8_t SD_SCK_PIN = 18;
constexpr uint8_t SD_MISO_PIN = 19;
constexpr uint8_t SD_MOSI_PIN = 23;
constexpr uint32_t SD_SPI_FREQUENCY_HZ = 8000000;

// SD card initialization behavior
constexpr uint32_t SD_INIT_TIMEOUT_MS = 1500;
constexpr uint16_t SD_INIT_RETRY_DELAY_MS = 100;

// Inter-board communication (UART0 by default, pins 1/3)
constexpr uint8_t COMM_TX_PIN = 1;
constexpr uint8_t COMM_RX_PIN = 3;
constexpr uint32_t COMM_BAUDRATE = 115200;
constexpr uint32_t COMM_RESPONSE_TIMEOUT_MS = 200;

// Driver configuration
constexpr float R_SENSE = 0.11f;
constexpr uint8_t DRIVER_ADDR_RA = 0b00;
constexpr uint8_t DRIVER_ADDR_DEC = 0b01;

// Motion configuration
constexpr double FULLSTEPS_PER_REV = 32.0 * 64.0; // 2048
constexpr double MICROSTEPS = 16.0;
constexpr float DRIVER_CURRENT_MA = 600.0f;
constexpr float MAX_RPM_MANUAL = 3.0f;
constexpr float GEAR_RATIO = 4.0f; // 1:4 reduction motor:telescope
constexpr float JOYSTICK_DEADZONE = 0.05f;

// Display configuration
constexpr uint8_t OLED_WIDTH = 128;
constexpr uint8_t OLED_HEIGHT = 64;

// Astronomy constants
constexpr double SIDEREAL_DAY_SECONDS = 86164.0905;
constexpr double POLARIS_RA_HOURS = 2.530301;     // 02h 31m 49s
constexpr double POLARIS_DEC_DEGREES = 89.2641;   // +89° 15' 50"
constexpr double OBSERVER_LATITUDE_DEG = 52.5200;  // Default: Berlin
constexpr double OBSERVER_LONGITUDE_DEG = 13.4050; // East positive

} // namespace config

