#pragma once
#include <Arduino.h>

class VolumeControl {
public:
  VolumeControl();
  
  void begin();
  void setVolume(uint8_t level);  // 0-100
  void mute(bool enable);
  void toggleMute();
  
  uint8_t processAudioSample(uint8_t sample);
  bool isMuted() { return muted; }
  uint8_t getVolume() { return currentVolume; }
  
private:
  void buildVolumeLUT();
  
  uint8_t volumeLUT[256];
  uint8_t currentVolume = 50;  // Default 50%
  bool muted = false;
  float volumeMultiplier = 0.5f;
};

extern VolumeControl volumeControl;