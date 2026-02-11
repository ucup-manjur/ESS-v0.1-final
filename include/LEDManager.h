/*
 * ============================================================================
 * LEDManager.h - LED Status Indicators
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk mengontrol 3 LED sebagai indikator visual status sistem.
 * Menampilkan register aktif, mode operasi, dan notifikasi.
 * 
 * FITUR UTAMA:
 * 1. Register Indication:
 *    - Register 1: LED 1 ON
 *    - Register 2: LED 2 ON
 *    - Register 3: LED 3 ON
 *    - Register 4: All LEDs ON
 * 
 * 2. Mode Indication:
 *    - Normal Mode: Solid LED
 *    - Programming Mode: Blinking LED (500ms interval)
 *    - Stop: All LEDs OFF
 * 
 * 3. Notification:
 *    - Fast Blink: 150ms interval untuk notifikasi
 *    - Configurable duration
 *    - Auto-return ke mode sebelumnya
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    LEDManager leds;
 *    leds.begin();  // Initialize pins
 *    ```
 * 
 * 2. Update Loop:
 *    ```cpp
 *    void loop() {
 *        leds.update();  // Call setiap 10ms
 *    }
 *    ```
 * 
 * 3. Set Register:
 *    ```cpp
 *    leds.setRegister(1);  // Show register 1
 *    leds.setRegister(4);  // Show register 4 (all LEDs)
 *    ```
 * 
 * 4. Set Mode:
 *    ```cpp
 *    leds.setBlinkMode(true);   // Programming mode
 *    leds.setBlinkMode(false);  // Normal mode
 *    leds.setAllOff();          // Stop mode
 *    ```
 * 
 * 5. Notification:
 *    ```cpp
 *    leds.setFastBlink(2000);  // Fast blink for 2 seconds
 *    ```
 * 
 * LED PATTERNS:
 * Register 1: ● ○ ○
 * Register 2: ○ ● ○
 * Register 3: ○ ○ ●
 * Register 4: ● ● ●
 * Programming: ◐ ◐ ◐ (blinking)
 * Stop: ○ ○ ○
 * 
 * TIMING:
 * - Normal Blink: 500ms interval
 * - Fast Blink: 150ms interval
 * - Update Rate: 10ms recommended
 * 
 * PIN CONFIGURATION:
 * Defined in config.h:
 * - LED_1_PIN
 * - LED_2_PIN
 * - LED_3_PIN
 * - OUTPUT mode
 * - Active HIGH (ON = HIGH)
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>

class LEDManager {
public:
  LEDManager();
  void begin();
  void update();
  
  void setRegister(uint8_t reg);
  void setAllOff();
  void setAllOn();
  void setAllBlink();
  void setBlinkMode(bool enable);
  void setFastBlink(unsigned long duration);
  
private:
  uint8_t currentRegister = 1;
  bool blinkMode = false;
  bool blinkState = false;
  unsigned long lastBlink = 0;
  const unsigned long BLINK_INTERVAL = 500;
  
  // Fast blink for notifications
  bool fastBlinkMode = false;
  unsigned long fastBlinkStart = 0;
  unsigned long fastBlinkDuration = 0;
  const unsigned long FAST_BLINK_INTERVAL = 150;
};
