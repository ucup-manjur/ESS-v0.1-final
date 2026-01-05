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
