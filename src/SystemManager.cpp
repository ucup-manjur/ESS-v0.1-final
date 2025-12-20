#include "SystemManager.h"
#include "VolumeControl.h"

SystemManager::SystemManager() {}

void SystemManager::begin(AudioPlayer* audioPlayer) {
  player = audioPlayer;
  buttons.begin();
  leds.begin();
  ble.begin();
  
  leds.setRegister(currentRegister);
  ble.setCurrentRegister(currentRegister);
  Serial.println("✅ System ready");
}

void SystemManager::update() {
  buttons.update();
  leds.update();
  ble.update();
  
  // Only update effects when needed
  if (isRevving || isRevDown) {
    updateRev();
  } else if (isShifting) {
    updateShift();
  }
  
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
    Serial.println("⚠️ Format hanya bisa dalam Programming Mode!");
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
  ble.setCurrentRegister(currentRegister);
  ble.sendCurrentPlaying();  // Auto-send current file info
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
  ble.sendStatus(currentMode);
  Serial.println("🛠️ Programming Mode");
}

void SystemManager::exitProgrammingMode() {
  currentMode = MODE_NORMAL;
  leds.setBlinkMode(false);
  leds.setRegister(currentRegister);
  ble.enableFileTransfer(false);
  ble.sendStatus(currentMode);
  Serial.println("🎮 Normal Mode");
}

void SystemManager::handleBLECommands() {
  if (!ble.hasCommand()) return;
  
  uint8_t cmd = ble.getCommand();
  uint8_t* data = ble.getCommandData();
  
  Serial.printf("🔍 Processing BLE command: 0x%02X\n", cmd);
  
  switch(cmd) {
    case CMD_GEAR_UP:
      triggerGearUp();
      Serial.println("📱 BLE Gear Up");
      break;
      
    case CMD_GEAR_DOWN:
      triggerGearDown();
      Serial.println("📱 BLE Gear Down");
      break;
      
    case CMD_REV_START:
      startRev();
      Serial.println("📱 BLE Rev Start");
      break;
      
    case CMD_REV_STOP:
      stopRev();
      Serial.println("📱 BLE Rev Stop");
      break;
      
    case CMD_VOL:
      Serial.printf("🔊 CMD_VOL received, data length: %d, value: %d\n", ble.getCommandDataLength(), data[0]);
      if (ble.getCommandDataLength() > 0) {
        if (data[0] == 0) {
          volumeControl.toggleMute();
          Serial.println("📱 BLE Toggle Mute");
        } else {
          volumeControl.mute(false);  // Unmute when setting volume
          volumeControl.setVolume(data[0]);
          Serial.printf("📱 BLE Set Volume: %d%%\n", data[0]);
        }
      }
      break;
      
    case CMD_SET_AUDIO_PLAY:
      if (ble.getCommandDataLength() > 0 && data[0] >= 1 && data[0] <= 4) {
        currentRegister = data[0];
        leds.setRegister(currentRegister);
        ble.setCurrentRegister(currentRegister);
        ble.sendCurrentPlaying();  // Auto-send current file info
        if (isPlaying) {
          loadCurrentSound();
        } else {
          // Start playing the selected register
          isPlaying = true;
          leds.setRegister(currentRegister);
          loadCurrentSound();
        }
        Serial.printf("📱 BLE Set Audio Play: Register %d\n", currentRegister);
      }
      break;
      
    case CMD_TOGGLE_AUTO_SHIFT:
      Serial.println("📱 Auto Shift (disabled)");
      break;
      
    case CMD_REQ_FILE_INFO:
      ble.sendCurrentPlaying();
      Serial.println("📱 BLE Request File Info");
      break;
      
    case CMD_REQ_FILE_LIST:
      if (ble.getCommandDataLength() > 0 && data[0] >= 1 && data[0] <= 4) {
        ble.replyFileList(data[0]);
        Serial.printf("📱 BLE Request File List: Register %d\n", data[0]);
      } else {
        ble.replyFileList(0);  // All folders
        Serial.println("📱 BLE Request All File Lists");
      }
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
      
    case CMD_REQ_STATUS:
      ble.sendStatus(currentMode);
      Serial.println("📱 BLE Status Request");
      break;
      
    default:
      Serial.printf("⚠️ Unknown BLE command: 0x%02X\n", cmd);
      break;
  }
  
  // Reset command data after processing
  ble.getCommandData()[0] = 0;
  // Note: commandDataLen will be reset on next command
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
  
  // Find first .raw file in folder
  String filename = "";
  File dir = LittleFS.open(folderPath);
  if (dir && dir.isDirectory()) {
    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".raw")) {
        filename = folderPath + "/" + file.name();
        break;
      }
      file = dir.openNextFile();
    }
    dir.close();
  }
  
  if (filename != "" && player->loadFile(filename.c_str())) {
    player->startPlayback();
    Serial.printf("✅ Loaded: %s\n", filename.c_str());
  } else {
    Serial.printf("⚠️ File tidak ada di: %s\n", folderPath.c_str());
  }
}

void SystemManager::formatLittleFS() {
  Serial.println("🚨 TOMBOL 3 DITEKAN 5 DETIK - FORMAT LITTLEFS!");
  ble.formatLittleFS();
}

void SystemManager::startRev() {
  if (!isRevving && player) {
    isRevving = true;
    revStartTime = millis();
    prevNormalRate = currentThrottleRate;  // Save current throttle position
    Serial.printf("🔊 Rev start! T=%lu, From: %d Hz\n", revStartTime, prevNormalRate);
  }
}

void SystemManager::stopRev() {
  if (isRevving) {
    isRevving = false;
    isRevDown = true;
    revDownStartTime = millis();
    Serial.printf("⛔ Rev down start: %d -> %d Hz\n", revTargetRate, prevNormalRate);
  }
}

void SystemManager::triggerShift() {
  if (!isShifting && !isRevving && player) {
    shiftBaseRate = currentThrottleRate;
    shiftPhase = 0;  // Start phase 0: turun sedikit
    shiftStartTime = millis();
    isShifting = true;
    Serial.printf("⚙️ Gear shift start! Base: %d Hz\n", shiftBaseRate);
  }
}

void SystemManager::triggerGearUp() {
  if (currentGear < maxGear && !isShifting && !isRevving) {
    currentGear++;
    triggerShift();
    Serial.printf("⬆️ Gear UP -> %d\n", currentGear);
  } else if (currentGear >= maxGear) {
    Serial.printf("⚠️ Max gear (%d)\n", maxGear);
  }
}

void SystemManager::triggerGearDown() {
  if (currentGear > 1 && !isShifting && !isRevving) {
    currentGear--;
    triggerShift();
    Serial.printf("⬇️ Gear DOWN -> %d\n", currentGear);
  } else if (currentGear <= 1) {
    Serial.println("⚠️ Min gear (1)");
  }
}

void SystemManager::updateRev() {
  if (!player) return;
  
  if (isRevving) {
    unsigned long elapsed = millis() - revStartTime;
    if (elapsed >= revRampDuration) {
      player->setSampleRate(revTargetRate);
    } else {
      float progress = (float)elapsed / revRampDuration;
      uint32_t newRate = prevNormalRate + (revTargetRate - prevNormalRate) * progress;
      player->setSampleRate(newRate);
    }
  } else if (isRevDown) {
    unsigned long elapsed = millis() - revDownStartTime;
    if (elapsed < revDownDuration) {
      float progress = (float)elapsed / revDownDuration;
      uint32_t blendedRate = revTargetRate - (revTargetRate - prevNormalRate) * progress;
      player->setSampleRate(blendedRate);
    } else {
      player->setSampleRate(prevNormalRate);
      isRevDown = false;
      Serial.println("✅ Turun selesai, balik idle");
    }
  }
}

void SystemManager::updateShift() {
  if (!player || !isShifting) return;
  
  unsigned long elapsed = millis() - shiftStartTime;
  uint32_t newRate = shiftBaseRate;
  
  if (shiftPhase == 0) {  // Turun sedikit
    newRate = shiftBaseRate - 1500;
    if (elapsed >= 150) {
      shiftPhase = 1;
      shiftStartTime = millis();
    }
  } else if (shiftPhase == 1) {  // Naik mengejar throttle
    float progress = (float)elapsed / 200.0f;
    if (progress >= 1.0f) {
      isShifting = false;
      newRate = currentThrottleRate;
      Serial.println("✅ Shift complete");
    } else {
      newRate = (shiftBaseRate - 1500) + ((currentThrottleRate - (shiftBaseRate - 1500)) * progress);
    }
  }
  
  player->setSampleRate(newRate);
}
