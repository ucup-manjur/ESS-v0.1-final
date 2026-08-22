#include "SystemManager.h"
#include "VolumeControl.h"
#include "config.h"

SystemManager::SystemManager() {}

void SystemManager::begin(AudioPlayer* audioPlayer) {
  player = audioPlayer;
  buttons.begin();
  leds.begin();
  ble.begin();
  
  leds.setRegister(currentRegister);
  ble.setCurrentRegister(currentRegister);
  LOG("System OK");
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
    // format only in programming mode
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
  LOG("🎵 Register switched to: %d", currentRegister);
  
  if (currentMode == MODE_NORMAL && isPlaying) {
    loadCurrentSound();
  }
}

void SystemManager::togglePlayback() {
  isPlaying = !isPlaying;
  
  if (isPlaying) {
    leds.setRegister(currentRegister);
    loadCurrentSound();
    LOG("▶️ Playback started (Register %d)", currentRegister);
  } else {
    leds.setAllOff();
    if (player) player->stopPlayback();
    LOG("⏹️ Playback stopped");
  }
}

void SystemManager::enterProgrammingMode() {
  currentMode = MODE_PROGRAMMING;
  ble.setCurrentMode(currentMode);
  leds.setBlinkMode(true);
  ble.enableFileTransfer(true);
  ble.sendStatus(currentMode, currentRegister, isPlaying);
  volumeControl.mute(false);
  LOG("🔧 Programming mode ON (Register %d)", currentRegister);
}

void SystemManager::exitProgrammingMode() {
  currentMode = MODE_NORMAL;
  ble.setCurrentMode(currentMode);
  leds.setBlinkMode(false);
  leds.setRegister(currentRegister);
  ble.enableFileTransfer(false);
  ble.sendStatus(currentMode, currentRegister, isPlaying);
  volumeControl.mute(false);
  LOG("✅ Programming mode OFF");
}

void SystemManager::handleBLECommands() {
  if (!ble.hasCommand()) return;
  
  uint8_t cmd = ble.getCommand();
  uint8_t* data = ble.getCommandData();
  
  LOG("🔍 Processing BLE command: 0x%02X", cmd);
  switch(cmd) {
    case CMD_GEAR_UP:
      LOG("⬆️ BLE Gear Up → Gear %d", currentGear + 1);
      triggerGearUp();
      break;
      
    case CMD_GEAR_DOWN:
      LOG("⬇️ BLE Gear Down → Gear %d", currentGear - 1);
      triggerGearDown();
      break;
      
    case CMD_REV_START:
      LOG("🔴 BLE Rev Start");
      startRev();
      break;
      
    case CMD_REV_STOP:
      LOG("🟢 BLE Rev Stop");
      stopRev();
      break;
      
    case CMD_VOL:
      if (ble.getCommandDataLength() > 0) {
        if (data[0] == 0) {
          volumeControl.toggleMute();
          LOG("🔇 BLE Toggle Mute");
        } else {
          if (currentMode == MODE_NORMAL) volumeControl.mute(false);
          volumeControl.setVolume(data[0]);
          LOG("🔊 BLE Volume: %d", data[0]);
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
        LOG("📱 BLE Set Audio Play: Register %d", currentRegister);
      }
      break;
      
    case CMD_TOGGLE_AUTO_SHIFT:
      break;
      
    case CMD_REQ_FILE_INFO:
      LOG("📱 BLE Request File Info");
      ble.sendCurrentPlaying();
      break;
      
    case CMD_REQ_FILE_LIST:
      LOG("📱 BLE Request All File Lists");
      if (ble.getCommandDataLength() > 0 && data[0] >= 1 && data[0] <= 4) {
        ble.replyFileList(data[0]);
      } else {
        ble.replyFileList(0);
      }
      break;
      
    case CMD_DELETE_FILE:
      if (ble.getCommandDataLength() > 0) {
        const char* folders[] = {"", "/audio/engine", "/audio/shift", "/audio/effects"};
        if (data[0] >= 1 && data[0] <= 3) {
          ble.deleteFile((String(folders[data[0]]) + "/upload.tmp").c_str());
          ble.listAllAudioFiles();
        }
      }
      break;
      
    case CMD_REQ_STATUS:
      LOG("[BLE] Status requested");
      ble.sendStatus(currentMode, currentRegister, isPlaying);
      break;
      
    case CMD_REQ_OBD2_STATUS:
      ble.sendOBD2Status();
      break;
    case CMD_REQ_OBD2_SOH:
      ble.sendOBD2SOH();
      break;
    case CMD_REQ_OBD2_TEMP:
      ble.sendOBD2Temp();
      break;
    case CMD_REQ_OBD2_STEERING:
      ble.sendOBD2Steering();
      break;
    case CMD_REQ_OBD2_POWER:
      ble.sendOBD2Power();
      break;
      
    case CMD_IMU_TOGGLE:
      imuControl.setEnabled(!imuControl.isEnabled());
      LOG("🧭 IMU %s", imuControl.isEnabled() ? "ON" : "OFF");
      ble.sendIMUStatus();
      break;
    case CMD_REQ_IMU_STATUS:
      LOG("🧭 IMU status requested");
      ble.sendIMUStatus();
      break;
    case CMD_IMU_CALIBRATE:
      LOG("🧭 IMU calibrate");
      imuControl.calibrate();
      ble.sendIMUStatus();
      break;
    case CMD_REQ_FW_VER:
      LOG("📦 FW version requested");
      ble.sendFWVersion();
      break;
      
    default:
      LOG("CMD unknown:0x%02X", cmd);
      break;
  }
  
  ble.getCommandData()[0] = 0;
}

void SystemManager::loadCurrentSound() {
  if (!player) return;
  
  if (currentMode == MODE_PROGRAMMING) return;
  
  String folderPath;
  if (currentRegister == 1) {
    folderPath = "/Audio";
  } else {
    folderPath = "/Audio" + String(currentRegister - 1);
  }
  
  LOG("🔍 Loading sound from: %s", folderPath.c_str());
  
  String filename = "";
  File dir = LittleFS.open(folderPath);
  if (dir && dir.isDirectory()) {
    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".raw")) {
        filename = String(file.name());
        break;
      }
      file = dir.openNextFile();
    }
    dir.close();
  }
  
  if (filename != "" && player->loadFile(filename.c_str())) {
    player->startPlayback();
    LOG("✅ Loaded: %s", filename.c_str());
  } else {
    LOG("❌ No file in: %s", folderPath.c_str());
  }
}

void SystemManager::formatLittleFS() {
  LOG("FORMAT FS");
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
        // String filePath = folderPath + "/" + file.name();
        String filePath = String(file.name());
        file.close();
        LittleFS.remove(filePath);
        LOG("DEL:%s", filePath.c_str());
      }
      file = dir.openNextFile();
    }
    dir.close();
  }
  
  leds.setFastBlink(2000);
  LOG("DEL reg%d", currentRegister);
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

void SystemManager::startRev() {
  if (!isRevving && player) {
    isRevving = true;
    revStartTime = millis();
    prevNormalRate = currentThrottleRate;
    LOG("🔴 Rev start (base rate: %lu)", prevNormalRate);
  }
}

void SystemManager::stopRev() {
  if (isRevving) {
    isRevving = false;
    isRevDown = true;
    revDownStartTime = millis();
    LOG("🟢 Rev stop → rev down");
  }
}

void SystemManager::triggerShift() {
  if (!isShifting && !isRevving && player) {
    shiftBaseRate = currentThrottleRate;
    shiftTargetRate = (uint32_t)(currentThrottleRate * 1.3f);
    shiftStartTime = millis();
    isShifting = true;
    shiftPhase = 0;
  }
}

void SystemManager::triggerGearUp() {
  if (currentGear < maxGear && !isShifting && !isRevving) {
    currentGear++;
    LOG("⬆️ Gear Up → %d", currentGear);
    triggerShift();
  } else if (currentGear >= maxGear) {
    LOG("⚠️ Already at max gear (%d)", maxGear);
  }
}

void SystemManager::triggerGearDown() {
  if (currentGear > 1 && !isShifting && !isRevving) {
    currentGear--;
    LOG("⬇️ Gear Down → %d", currentGear);
    triggerShift();
  } else if (currentGear <= 1) {
    LOG("⚠️ Already at min gear");
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
    }
  }
  
  player->setSampleRate(newRate);
}
