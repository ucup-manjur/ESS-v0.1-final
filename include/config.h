/*
 * ============================================================================
 * config.h - Hardware Pin Configuration
 * ============================================================================
 * 
 * DESKRIPSI:
 * File konfigurasi untuk pin mapping hardware ESP32. Semua pin definitions
 * untuk audio, button, LED, dan ADC didefinisikan di sini.
 * 
 * CARA MENGGUNAKAN:
 * 1. Sesuaikan pin numbers dengan hardware schematic
 * 2. Uncomment/comment pin configuration yang sesuai
 * 3. Recompile dan upload firmware
 * 
 * PIN MAPPING:
 * 
 * AUDIO OUTPUT:
 * - DAC Pin: GPIO 25 (ESP32 internal DAC1)
 *   - Output: 0-3.3V analog
 *   - Resolution: 8-bit (0-255)
 *   - Impedance: Connect ke audio amplifier
 * 
 * ANALOG INPUT:
 * - Throttle ADC: GPIO 32 (ADC1_CH4)
 *   - Input: 0-3.3V
 *   - Resolution: 12-bit (0-4095)
 *   - Untuk potentiometer throttle (DEV_MODE)
 * 
 * DIGITAL INPUT (Buttons):
 * - Button A: GPIO 33 (INPUT_PULLUP)
 *   - Function: Switch register
 *   - Active: LOW (pressed)
 * - Button B: GPIO 34 (INPUT_PULLUP)
 *   - Function: Play/Stop, Programming mode
 *   - Active: LOW (pressed)
 * - Button C: GPIO 35 (INPUT_PULLUP)
 *   - Function: Delete file
 *   - Active: LOW (pressed)
 * 
 * DIGITAL OUTPUT (LEDs):
 * - LED 1: GPIO 4
 *   - Indicates: Register 1
 *   - Active: HIGH (ON)
 * - LED 2: GPIO 13
 *   - Indicates: Register 2
 *   - Active: HIGH (ON)
 * - LED 3: GPIO 14
 *   - Indicates: Register 3
 *   - Active: HIGH (ON)
 * 
 * CAN BUS (OBD2):
 * - CAN RX: GPIO 16 (defined in OBD2Control.cpp)
 * - CAN TX: GPIO 17 (defined in OBD2Control.cpp)
 *   - Speed: 500 kbps
 *   - Requires: External CAN transceiver (e.g., MCP2551)
 * 
 * MEMORY CONFIGURATION:
 * - Audio Ring Buffer: 32KB
 *   - Trade-off: Memory vs performance
 *   - Larger = smoother playback, more RAM usage
 *   - Smaller = less RAM, possible audio glitches
 * 
 * - Audio Header: 512 bytes
 *   - Max JSON metadata size
 *   - Contains: title, volume, gearCount, maxRPM, etc.
 * 
 * TIMER CONFIGURATION:
 * - Timer Group: 0
 * - Timer Index: 0
 * - Used for: Audio sample rate timing (ISR)
 * 
 * CATATAN PENTING:
 * 1. GPIO 34-39 adalah input-only (no pullup internal)
 * 2. GPIO 6-11 digunakan untuk flash, jangan dipakai
 * 3. GPIO 0, 2, 15 mempengaruhi boot mode
 * 4. DAC hanya tersedia di GPIO 25 (DAC1) dan GPIO 26 (DAC2)
 * 5. ADC2 (GPIO 0, 2, 4, 12-15, 25-27) conflict dengan WiFi
 * 
 * ALTERNATIVE PIN CONFIGURATION:
 * Uncomment section "LAWAS" jika menggunakan hardware versi lama:
 * - THROTTLE_ADC_PIN: 34
 * - BUTTON_A_PIN: 33
 * - BUTTON_B_PIN: 32
 * - BUTTON_C_PIN: 35
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>

// Hardware
#define AUDIO_DAC_PIN         25   // DAC1 (GPIO25)

// #define THROTTLE_ADC_PIN      32   // example ADC pin (modify)
// #define BUTTON_A_PIN          33
// #define BUTTON_B_PIN          34
// #define BUTTON_C_PIN          35

#define ADC_MIN_VALUE    880    // ADC saat throttle idle (ukur aktual)
#define ADC_MAX_VALUE    3000   // ADC saat throttle full (ukur aktual)

#define THROTTLE_ADC_PIN      34   // example ADC pin LAWAS
#define BUTTON_A_PIN          33
#define BUTTON_B_PIN          32
#define BUTTON_C_PIN          35

#define LED_1_PIN             4
#define LED_2_PIN             13
#define LED_3_PIN             14


// Playback buffer
#define AUDIO_RING_CAPACITY   (32*1024) // 32KB ring buffer - adjust memory vs performance

// Header JSON max size
#define AUDIO_HEADER_MAXLEN   512

// Timer for ISR (uses timer0)
#define TIMER_GROUP           0
#define TIMER_INDEX           0

// ============================================================================
// DYNAMIC SLOPE CONFIGURATION
// ============================================================================
// Default values for throttle response (can be changed via BLE)
#define DEFAULT_LOW_RPM_SLOPE      80    // Low RPM zone (0-25%)
#define DEFAULT_MID_RPM_SLOPE      250   // Mid RPM zone (25-75%)
#define DEFAULT_HIGH_RPM_SLOPE     150   // High RPM zone (75-100%)
#define DEFAULT_ACCEL_LAG          0.7f  // Acceleration lag (0.5=slow, 1.0=instant)
#define DEFAULT_DECEL_LAG          0.6f  // Deceleration lag (0.5=slow, 1.0=instant)
#define DEFAULT_AGGRESSIVE_THRESH  1000  // Threshold for sudden throttle detection

// Firmware version
#define FW_VERSION  "1.0.0"

// Logging (0=OFF production, 1=ON development)
#define ESS_LOG_ENABLE 1
#if ESS_LOG_ENABLE
  #define LOG(fmt, ...) Serial.printf(fmt "\n", ##__VA_ARGS__)
#else
  #define LOG(fmt, ...)
#endif
