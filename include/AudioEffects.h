#pragma once
#include <Arduino.h>
#include "AudioPlayer.h"

class AudioEffects {
public:
  AudioEffects(AudioPlayer* audioPlayer);
  
  void update();
  void startRev();
  void stopRev();
  void triggerShift();
  void triggerGearUp();
  void triggerGearDown();
  void updateSampleRate(uint32_t targetRate);
  void setThrottleInput(uint32_t throttleRate);
  uint8_t getCurrentGear() { return currentGear; }
  void setAutoShift(bool enabled) { autoShiftEnabled = enabled; }
  bool isAutoShiftEnabled() { return autoShiftEnabled; }
  bool isRevving() { return revActive; }
  bool isShifting() { return shifting; }
  
private:
  AudioPlayer* player;
  
  // Rev variables
  bool revActive = false;
  bool revStopping = false;
  unsigned long revStartTime = 0;
  unsigned long revStopTime = 0;
  uint32_t revTargetRate = 44100;
  uint32_t prevNormalRate = 8000;
  
  // Shift variables
  bool shifting = false;
  bool ramping = false;
  unsigned long shiftStartTime = 0;
  unsigned long rampStartTime = 0;
  uint32_t shiftStartRate = 8000;
  uint32_t shiftTargetRate = 5200;
  uint32_t rampOriginRate = 8000;
  uint32_t rampTargetRate = 8000;
  uint32_t currentSampleRate = 8000;
  uint32_t previousSampleRate = 8000;
  uint32_t throttleTargetRate = 8000;
  
  // Gear system
  uint8_t currentGear = 1;
  const uint8_t maxGear = 4;
  
  // Auto shift RPM limits
  const uint32_t SHIFT_UP_RPM[6] = {5000, 9000, 12000, 15000, 18000, 22000};
  const uint32_t SHIFT_DOWN_RPM[6] = {0, 3000, 6000, 9000, 12000, 15000};
  bool autoShiftEnabled = false;
  
  const unsigned long revRampDuration = 500;   // Much faster up
  const unsigned long revRampDownDuration = 500; // Much faster down
  const unsigned long shiftDuration = 1500;  // Much slower shift
  const unsigned long rampDuration = 1000;
};