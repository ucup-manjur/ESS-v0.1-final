/*
 * ============================================================================
 * ButtonManager.h - Physical Button Input Handler
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk menangani input dari 3 button fisik dengan debouncing dan
 * long press detection. Thread-safe dengan mutex untuk dual core operation.
 * 
 * FITUR UTAMA:
 * 1. Debouncing:
 *    - Software debouncing 50ms
 *    - Eliminasi false trigger dari noise
 *    - Stable button reading
 * 
 * 2. Long Press Detection:
 *    - Button B: 3 detik untuk programming mode
 *    - Button C: 3 detik untuk delete file
 *    - Configurable duration
 * 
 * 3. Thread-Safe:
 *    - Mutex protection untuk flag access
 *    - Safe untuk dual core operation
 *    - Atomic flag operations
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    ButtonManager buttons;
 *    buttons.begin();  // Initialize pins
 *    ```
 * 
 * 2. Update Loop:
 *    ```cpp
 *    void loop() {
 *        buttons.update();  // Call setiap 5-10ms
 *        
 *        if (buttons.isButtonAPressed()) {
 *            // Button A pressed
 *        }
 *        if (buttons.isButtonBLongPress()) {
 *            // Button B long press (3s)
 *        }
 *    }
 *    ```
 * 
 * 3. Check Press Time:
 *    ```cpp
 *    unsigned long pressTime = buttons.getButtonCPressTime();
 *    if (pressTime >= 3000) {
 *        // Button C held for 3+ seconds
 *    }
 *    ```
 * 
 * BUTTON MAPPING:
 * - Button A: Short press only (switch register)
 * - Button B: Short press (play/stop) + Long press (programming mode)
 * - Button C: Short press + Long press (delete file)
 * 
 * TIMING:
 * - Debounce: 50ms
 * - Long Press: 3000ms (3 detik)
 * - Update Rate: 5-10ms recommended
 * 
 * PIN CONFIGURATION:
 * Defined in config.h:
 * - BUTTON_A_PIN
 * - BUTTON_B_PIN
 * - BUTTON_C_PIN
 * - INPUT_PULLUP mode
 * - Active LOW (pressed = LOW)
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>

class ButtonManager {
public:
  ButtonManager();
  void begin();
  void update();
  
  bool isButtonAPressed();
  bool isButtonBPressed();
  bool isButtonCPressed();
  bool isButtonBLongPress();
  bool isButtonCLongPress();
  unsigned long getButtonCPressTime();
  
private:
  bool getAndClearFlag(volatile bool &flag);
  
private:
  unsigned long btnA_lastChange = 0;
  unsigned long btnB_lastChange = 0;
  unsigned long btnC_lastChange = 0;
  unsigned long btnB_pressStart = 0;
  unsigned long btnC_pressStart = 0;
  
  bool btnA_lastState = HIGH;
  bool btnB_lastState = HIGH;
  bool btnC_lastState = HIGH;
  
  volatile bool btnA_pressed = false;
  volatile bool btnB_shortPressed = false;
  volatile bool btnB_longPressed = false;
  volatile bool btnC_pressed = false;
  volatile bool btnC_longPressed = false;
  
  bool btnB_longTriggered = false;
  bool btnC_longTriggered = false;
  
  portMUX_TYPE flagMutex;
  
  static const unsigned long DEBOUNCE_MS = 50;
  static const unsigned long LONG_PRESS_MS = 3000;
  static const unsigned long FORMAT_PRESS_MS = 5000;
  
  bool debounceButton(int pin, bool &lastState, unsigned long &lastChange);
};
