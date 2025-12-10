#pragma once
#include <Arduino.h>

class LEDManager {
public:
  LEDManager();
  void begin();
  void update();
  
  void setRegister(uint8_t reg);
  void setAllOff();
  void setBlinkMode(bool enable);
  
private:
  uint8_t currentRegister = 1;
  bool blinkMode = false;
  bool blinkState = false;
  unsigned long lastBlink = 0;
  const unsigned long BLINK_INTERVAL = 500;
};
