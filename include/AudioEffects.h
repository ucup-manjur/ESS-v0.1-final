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
  void updateSampleRate(uint32_t targetRate);
  void setThrottleInput(uint32_t throttleRate);
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
  
  const unsigned long revRampDuration = 1000;
  const unsigned long revRampDownDuration = 2000;
  const unsigned long shiftDuration = 300;
  const unsigned long rampDuration = 800;
};