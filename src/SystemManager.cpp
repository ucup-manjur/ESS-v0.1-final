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
  updateButtons();
  updateLEDs();
  updateBLE();
  updateIMU();
  applyIMUModifier();
}

void SystemManager::updateButtons() {
  buttons.update();
  
  if (isRevving || isRevDown) {
    updateRev();
  } else if (isShifting) {
    updateShift();
  }
  
  if (currentMode == MODE_NORMAL) {
    handleNormalMode();
  } else {
    handleProgrammingMode();
  }
}

void SystemManager::updateBLE() {
  ble.update();
  handleBLECommands();
}

void SystemManager::updateLEDs() {
  leds.update();
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
  
  static bool deleteExecuted = false;
  unsigned long cPressTime = buttons.getButtonCPressTime();
  
  if (cPressTime == 0) {
    deleteExecuted = false;
  } else if (cPressTime >= 3000 && !deleteExecuted) {
    deleteCurrentRegisterFile();
    deleteExecuted = true;
  } else if (cPressTime >= 1000) {
    leds.setAllBlink();
  }
}

void SystemManager::switchRegister() {
  currentRegister++;
  if (currentRegister > 4) currentRegister = 1;
  
  leds.setRegister(currentRegister);
  ble.setCurrentRegister(currentRegister);
  ble.sendCurrentPlaying();
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
  volumeControl.mute(false);
  Serial.println("🛠️ Programming Mode - Audio muted");
}

void SystemManager::exitProgrammingMode() {
  currentMode = MODE_NORMAL;
  leds.setBlinkMode(false);
  leds.setRegister(currentRegister);
  ble.enableFileTransfer(false);
  ble.sendStatus(currentMode);
  volumeControl.mute(false);
  Serial.println("🎮 Normal Mode - Audio restored");
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
          if (currentMode == MODE_NORMAL) {
            volumeControl.mute(false);
          }
          volumeControl.setVolume(data[0]);
          Serial.printf("📱 BLE Set Volume: %d%% (Mode: %s)\n", data[0],
                       currentMode == MODE_PROGRAMMING ? "Programming" : "Normal");
        }
      }
      break;
      
    case CMD_SET_AUDIO_PLAY:
      if (ble.getCommandDataLength() > 0 && data[0] >= 1 && data[0] <= 4) {
        currentRegister = data[0];
        leds.setRegister(currentRegister);
        ble.setCurrentRegister(currentRegister);
        ble.sendCurrentPlaying();
        if (isPlaying) {
          loadCurrentSound();
        } else {
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
        ble.replyFileList(0);
        Serial.println("📱 BLE Request All File Lists");
      }
      break;
      
    case CMD_DELETE_FILE:
      if (ble.getCommandDataLength() > 0) {
        const char* folders[] = {"", "/audio/engine", "/audio/shift", "/audio/effects"};
        if (data[0] >= 1 && data[0] <= 3) {
          ble.deleteFile((String(folders[data[0]]) + "/upload.tmp").c_str());
          ble.listAllAudioFiles();
          Serial.printf("📱 BLE Delete File: folder %d\n", data[0]);
        }
      }
      break;
      
    case CMD_REQ_STATUS:
      ble.sendStatus(currentMode);
      Serial.println("📱 BLE Status Request");
      break;
      
    case CMD_REQ_OBD2_STATUS:
      ble.sendOBD2Status();
      Serial.println("📱 BLE OBD2 Status Request");
      break;
      
    case CMD_REQ_OBD2_SOH:
      ble.sendOBD2SOH();
      Serial.println("📱 BLE OBD2 SoH Request");
      break;
      
    case CMD_REQ_OBD2_TEMP:
      ble.sendOBD2Temp();
      Serial.println("📱 BLE OBD2 Temp Request");
      break;
      
    case CMD_REQ_OBD2_STEERING:
      ble.sendOBD2Steering();
      Serial.println("📱 BLE OBD2 Steering Request");
      break;
      
    case CMD_REQ_OBD2_POWER:
      ble.sendOBD2Power();
      Serial.println("📱 BLE OBD2 Power Request");
      break;
      
    case CMD_IMU_TOGGLE:
      imuControl.setEnabled(!imuControl.isEnabled());
      ble.sendIMUStatus();
      Serial.printf("📱 IMU %s\n", imuControl.isEnabled() ? "ON" : "OFF");
      break;

    case CMD_REQ_IMU_STATUS:
      ble.sendIMUStatus();
      Serial.println("📱 IMU Status Request");
      break;

    case CMD_IMU_CALIBRATE:
      imuControl.calibrate();
      ble.sendIMUStatus(); // kirim status terbaru setelah kalibrasi
      Serial.println("📱 IMU Calibrate");
      break;
      
    default:
      Serial.printf("⚠️ Unknown BLE command: 0x%02X\n", cmd);
      break;
  }
  
  ble.getCommandData()[0] = 0;
}

void SystemManager::loadCurrentSound() {
  if (!player) return;
  
  if (currentMode == MODE_PROGRAMMING) {
    Serial.println("⚠️ Cannot load sound in programming mode");
    return;
  }
  
  String folderPath;
  if (currentRegister == 1) {
    folderPath = "/Audio";
  } else {
    folderPath = "/Audio" + String(currentRegister - 1);
  }
  
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

void SystemManager::deleteCurrentRegisterFile() {
  if (player) {
    player->stopPlayback();
  }
  
  String folderPath;
  if (currentRegister == 1) {
    folderPath = "/Audio";
  } else {
    folderPath = "/Audio" + String(currentRegister - 1);
  }
  
  File dir = LittleFS.open(folderPath);
  if (dir && dir.isDirectory()) {
    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String filePath = folderPath + "/" + file.name();
        file.close();
        LittleFS.remove(filePath);
        Serial.printf("🗑️ Deleted: %s\n", filePath.c_str());
      }
      file = dir.openNextFile();
    }
    dir.close();
  }
  
  leds.setFastBlink(2000);
  Serial.printf("✅ Register %d files deleted\n", currentRegister);
}

void SystemManager::updateIMU() {
  imuControl.update();
}

void SystemManager::applyIMUModifier() {
  if (!player || !imuControl.isEnabled()) {
    currentIMUModifier = 1.0f; // reset saat IMU off
    return;
  }
  if (isRevving || isRevDown || isShifting) return;

  float targetModifier = imuControl.getSampleRateModifier();

  // Lerp modifier perlahan menuju target
  currentIMUModifier += (targetModifier - currentIMUModifier) * IMU_LERP_SPEED;

  // Snap ke target jika sudah sangat dekat (hindari infinite approach)
  if (fabsf(currentIMUModifier - targetModifier) < 0.001f) {
    currentIMUModifier = targetModifier;
  }

  // Hanya apply jika ada perubahan berarti dari 1.0
  if (fabsf(currentIMUModifier - 1.0f) < 0.001f) return;

  uint32_t modifiedRate = (uint32_t)(currentThrottleRate * currentIMUModifier);
  modifiedRate = constrain(modifiedRate, 8000, 44100);
  player->setSampleRate(modifiedRate);
}

// void SystemManager::applyIMUModifier() {
//   if (!player || !imuControl.isEnabled()) return;
//   if (isRevving || isRevDown || isShifting) return;

//   float modifier = imuControl.getSampleRateModifier();
//   if (modifier == 1.0f) return;

//   uint32_t modifiedRate = (uint32_t)(currentThrottleRate * modifier);
//   modifiedRate = constrain(modifiedRate, 8000, 44100);
//   player->setSampleRate(modifiedRate);
// }

void SystemManager::startRev() {
  if (!isRevving && player) {
    isRevving = true;
    revStartTime = millis();
    prevNormalRate = currentThrottleRate;
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
    shiftTargetRate = (uint32_t)(currentThrottleRate * 1.3f);
    shiftStartTime = millis();
    isShifting = true;
    shiftPhase = 0;
    Serial.printf("⚙️ Gear shift start! %d -> %d Hz (30%% up)\n", shiftBaseRate, shiftTargetRate);
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
  
  if (shiftPhase == 0) {
    if (elapsed < 150) {
      float progress = (float)elapsed / 150.0f;
      newRate = shiftBaseRate + (shiftTargetRate - shiftBaseRate) * progress;
    } else {
      shiftPhase = 1;
      shiftStartTime = millis();
      newRate = shiftTargetRate;
    }
  } else if (shiftPhase == 1) {
    if (elapsed < 200) {
      float progress = (float)elapsed / 200.0f;
      newRate = shiftTargetRate - (shiftTargetRate - shiftBaseRate) * progress;
    } else {
      isShifting = false;
      newRate = shiftBaseRate;
      Serial.println("✅ Shift complete");
    }
  }
  
  player->setSampleRate(newRate);
}
