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
  
  if (buttons.isButtonCPressed()) {
    // Handle button C short press
  }
  
  if (buttons.isButtonCLongPress()) {
    formatLittleFS();
  }
}

void SystemManager::handleProgrammingMode() {
  if (buttons.isButtonAPressed()) {
    switchRegister();
  }
  
  if (buttons.isButtonBLongPress()) {
    exitProgrammingMode();
  }
  
  if (buttons.isButtonCPressed()) {
    // Handle button C short press in programming mode
  }
  
  if (buttons.isButtonCLongPress()) {
    formatLittleFS();
  }
}

void SystemManager::switchRegister() {
  currentRegister++;
  if (currentRegister > 4) currentRegister = 1;
  
  leds.setRegister(currentRegister);
  Serial.printf("📍 Register: %d\n", currentRegister);
  
  if (currentMode == MODE_NORMAL && isPlaying) {
    loadCurrentSound();
  }
}

void SystemManager::togglePlayback() {
  isPlaying = !isPlaying;
  
  if (isPlaying) {
    leds.setRegister(currentRegister);
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
  ble.enableFileTransfer(true);
  Serial.println("🛠️ Programming Mode");
}

void SystemManager::exitProgrammingMode() {
  currentMode = MODE_NORMAL;
  leds.setBlinkMode(false);
  leds.setRegister(currentRegister);
  ble.enableFileTransfer(false);
  Serial.println("🎮 Normal Mode");
}

void SystemManager::handleBLECommands() {
  if (!ble.hasCommand()) return;
  
  uint8_t cmd = ble.getCommand();
  uint8_t* data = ble.getCommandData();
  
  switch(cmd) {
    case CMD_GEAR_UP:
      // if (currentGear < 4) {
      //   currentGear++;
      //   Serial.printf("📱 Manual Gear Up: %d\n", currentGear);
      // }
        Serial.printf("📱 Manual Gear Up");
      break;
      
    case CMD_GEAR_DOWN:
      // if (currentGear > 0) {
      //   currentGear--;
      //   Serial.printf("📱 Manual Gear Down: %d\n", currentGear);
      // }
        Serial.printf("📱 Manual Gear Down");
      break;
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
      
    case CMD_DELETE_FILE:
      if (ble.getCommandDataLength() > 0) {
        // Value 1=engine, 2=shift, 3=effects folder
        const char* folders[] = {"", "/audio/engine", "/audio/shift", "/audio/effects"};
        if (data[0] >= 1 && data[0] <= 3) {
          ble.deleteFile((String(folders[data[0]]) + "/upload.tmp").c_str());
          ble.listAllAudioFiles();
          Serial.printf("📱 BLE Delete File: folder %d\n", data[0]);
        }
      }
      break;
      
    case CMD_DELETE_FOLDER:
      if (ble.getCommandDataLength() > 0) {
        const char* folders[] = {"", "/audio/engine", "/audio/shift", "/audio/effects"};
        if (data[0] >= 1 && data[0] <= 3) {
          ble.deleteFolder(folders[data[0]]);
          // ble.createAudioFolders(); // Recreate empty folder
          ble.listAllAudioFiles();
          Serial.printf("📱 BLE Delete Folder: %d\n", data[0]);
        }
      }
      break;
  }
}

void SystemManager::loadCurrentSound() {
  if (!player) return;
  
  // Mapping register ke folder: 1=/Audio, 2=/Audio1, 3=/Audio2, 4=/Audio3
  String folderPath;
  if (currentRegister == 1) {
    folderPath = "/Audio";
  } else {
    folderPath = "/Audio" + String(currentRegister - 1);
  }
  
  File dir = LittleFS.open(folderPath);
  
  if (!dir || !dir.isDirectory()) {
    Serial.printf("⚠️ Folder tidak ada: %s\n", folderPath.c_str());
    return;
  }
  
  File file = dir.openNextFile();
  String foundFile = "";
  
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".raw")) {
        foundFile = folderPath + "/" + filename;
        break;
      }
    }
    file = dir.openNextFile();
  }
  dir.close();
  
  if (foundFile.length() > 0) {
    if (player->loadFile(foundFile.c_str())) {
      player->startPlayback();
      Serial.printf("✅ Loaded: %s\n", foundFile.c_str());
    } else {
      Serial.printf("❌ Failed to load: %s\n", foundFile.c_str());
    }
  } else {
    Serial.printf("⚠️ No .raw files in: %s\n", folderPath.c_str());
  }
}

void SystemManager::formatLittleFS() {
  Serial.println("🚨 TOMBOL 3 DITEKAN 5 DETIK - FORMAT LITTLEFS!");
  ble.formatLittleFS();
}
