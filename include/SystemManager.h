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
  
private:
  AudioPlayer* player;
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

};