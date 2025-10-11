#pragma once

#include <Arduino.h>
#include "driver/uart.h"

#include "role_config.h"

namespace config {

// Role-specific pin mappings -------------------------------------------------

#if defined(DEVICE_ROLE_MAIN)

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

// Inter-board communication (UART1 on classic ESP32)
constexpr uart_port_t COMM_UART_NUM = UART_NUM_1;
constexpr uint8_t COMM_TX_PIN = 33;
constexpr uint8_t COMM_RX_PIN = 32;

#elif defined(DEVICE_ROLE_HID)

// Joystick KY-023 pins (ESP32-C3 SuperMini)
constexpr uint8_t JOY_X = 0;
constexpr uint8_t JOY_Y = 1;
constexpr uint8_t JOY_BTN = 7;  // LOW active, use external pull-up if module lacks one

// Rotary encoder pins (ESP32-C3 SuperMini)
constexpr uint8_t ROT_A = 3;
constexpr uint8_t ROT_B = 4;
constexpr uint8_t ROT_BTN = 5;

// OLED + RTC I2C pins (ESP32-C3 SuperMini default)
constexpr uint8_t SDA_PIN = 8;
constexpr uint8_t SCL_PIN = 9;

// Inter-board communication (UART1 on ESP32-C3)
constexpr uart_port_t COMM_UART_NUM = UART_NUM_1;
constexpr uint8_t COMM_TX_PIN = 21;
constexpr uint8_t COMM_RX_PIN = 20;

#else
#error "Unsupported device role"
#endif

constexpr uint32_t COMM_BAUD = 115200;
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

// WiFi / OTA configuration
constexpr const char* WIFI_AP_SSID_PREFIX = "NERDSTAR";
constexpr const char* WIFI_AP_PASSWORD = "nerdstar";
constexpr const char* WIFI_HOSTNAME_PREFIX = "nerdstar";

// Astronomy constants
constexpr double SIDEREAL_DAY_SECONDS = 86164.0905;
constexpr double POLARIS_RA_HOURS = 2.530301;     // 02h 31m 49s
constexpr double POLARIS_DEC_DEGREES = 89.2641;   // +89° 15' 50"
constexpr double OBSERVER_LATITUDE_DEG = 52.5200;  // Default: Berlin
constexpr double OBSERVER_LONGITUDE_DEG = 13.4050; // East positive

} // namespace config

