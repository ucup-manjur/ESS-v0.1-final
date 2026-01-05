#pragma once
#include <Arduino.h>
#include "AudioPlayer.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include "BLEControl.h"

enum SystemMode {
  MODE_NORMAL,
  MODE_PROGRAMMING
};

class SystemManager {
public:
  SystemManager();
  void begin(AudioPlayer* audioPlayer = nullptr);
  void update();
  void updateButtons();
  void updateBLE();
  void updateLEDs();
  void setCurrentThrottleRate(uint32_t rate) { currentThrottleRate = rate; }
  bool isRevActive() { return isRevving || isRevDown; }
  bool isShiftActive() { return isShifting; }
  
private:
  AudioPlayer* player;
  
  // Simple rev variables
  bool isRevving = false;
  bool isRevDown = false;
  unsigned long revStartTime = 0;
  unsigned long revDownStartTime = 0;
  uint32_t prevNormalRate = 8000;
  uint32_t currentThrottleRate = 8000;  // Track current throttle
  const uint32_t revTargetRate = 39000;  // Match original
  const unsigned long revRampDuration = 300;  // Match original
  const unsigned long revDownDuration = 400;
  
  // Simple shift variables
  bool isShifting = false;
  unsigned long shiftStartTime = 0;
  uint32_t shiftBaseRate = 8000;
  uint32_t shiftTargetRate = 8000;
  uint8_t shiftPhase = 0;  // 0=drop, 1=recovery
  
  uint8_t currentGear = 0;
  const uint8_t maxGear = 4;
  ButtonManager buttons;
  LEDManager leds; 
  BLEControl ble;
  
  SystemMode currentMode = MODE_NORMAL;
  uint8_t currentRegister = 1;
  bool isPlaying = false;
  
  void handleNormalMode();
  void handleProgrammingMode();
  void handleBLECommands();
  void switchRegister();
  void togglePlayback();
  void enterProgrammingMode();
  void exitProgrammingMode();
  void loadCurrentSound();
  void formatLittleFS();
  void deleteCurrentRegisterFile();
  void deleteAllFiles();
  void startRev();
  void stopRev();
  void updateRev();
  void triggerShift();
  void triggerGearUp();
  void triggerGearDown();
  void updateShift();

};