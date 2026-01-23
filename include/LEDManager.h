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
