#include "SystemManager.h"

SystemManager::SystemManager() {}

void SystemManager::begin(AudioPlayer* audioPlayer) {
  player = audioPlayer;
  buttons.begin();
  leds.begin();
  ble.begin();
  
  leds.setRegister(currentRegister);
  Serial.println("✅ System ready");
}

void SystemManager::update() {
  buttons.update();
  leds.update();
  ble.update();
  
  handleBLECommands();
  
  if (currentMode == MODE_NORMAL) {
    handleNormalMode();
  } else {
    handleProgrammingMode();
  }
}

void SystemManager::handleNormalMode() {
  if (buttons.isButtonAPressed()) {
    switchRegister();
  }
  
  if (buttons.isButtonBPressed()) {
    togglePlayback();
  }
  
  if (buttons.isButtonBLongPress()) {
    enterProgrammingMode();
  }
}

void SystemManager::handleProgrammingMode() {
  if (buttons.isButtonAPressed()) {
    switchRegister();
  }
  
  if (buttons.isButtonBLongPress()) {
    exitProgrammingMode();
  }
}

void SystemManager::switchRegister() {
  currentRegister++;
  if (currentRegister > 3) currentRegister = 1;
  
  leds.setRegister(currentRegister);
  Serial.printf("📍 Register: %d\n", currentRegister);
  
  if (currentMode == MODE_NORMAL && isPlaying) {
    loadCurrentSound();
  }
}

void SystemManager::togglePlayback() {
  isPlaying = !isPlaying;
  
  if (isPlaying) {
    // leds.setRegister(currentRegister);
    loadCurrentSound();
    Serial.println("▶️ Play");
  } else {
    leds.setAllOff();
    if (player) player->stopPlayback();
    Serial.println("⏸️ Stop");
  }
}

void SystemManager::enterProgrammingMode() {
  currentMode = MODE_PROGRAMMING;
  leds.setBlinkMode(true);
  Serial.println("🛠️ Programming Mode");
}

void SystemManager::exitProgrammingMode() {
  currentMode = MODE_NORMAL;
  leds.setBlinkMode(false);
  leds.setRegister(currentRegister);
  Serial.println("🎮 Normal Mode");
}

void SystemManager::handleBLECommands() {
  if (!ble.hasCommand()) return;
  
  uint8_t cmd = ble.getCommand();
  uint8_t* data = ble.getCommandData();
  
  switch(cmd) {
    case CMD_REV_START:
      Serial.println("📱 BLE Rev Start");
      break;
      
    case CMD_REV_STOP:
      Serial.println("📱 BLE Rev Stop");
      break;
      
    case CMD_VOL:
      if (player && ble.getCommandDataLength() > 0) {
        player->toggleMute();
        Serial.printf("📱 BLE Volume: %d\n", data[0]);
      }
      break;
      
    case CMD_SET_AUDIO_PLAY:
      if (ble.getCommandDataLength() > 0 && data[0] >= 1 && data[0] <= 3) {
        currentRegister = data[0];
        leds.setRegister(currentRegister);
        if (isPlaying) loadCurrentSound();
        Serial.printf("📱 BLE Set Audio: %d\n", currentRegister);
      }
      break;
      
    case CMD_REQ_FILE_INFO:
      ble.replyCurrentFile();
      break;
      
    case CMD_REQ_FILE_LIST:
      ble.replyFileList();
      break;
  }
}

void SystemManager::loadCurrentSound() {
  if (!player) return;
  
  String filename = "/sound" + String(currentRegister) + ".raw";
  if (player->loadFile(filename.c_str())) {
    player->startPlayback();
  } else {
    Serial.printf("⚠️ File tidak ada: %s\n", filename.c_str());
  }
}
