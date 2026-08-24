#include "BLEControl.h"
#include "OBD2Control.h"
#include "IMUControl.h"
#include "config.h"
#include <esp_ota_ops.h>
#include <esp_partition.h>

const int MAX_GEAR = 4;
const int MIN_GEAR = 0;

NimBLECharacteristic* BLEControl::pCharacteristic = nullptr;
NimBLECharacteristic* BLEControl::pFileCharacteristic = nullptr;
NimBLECharacteristic* BLEControl::pOTACharacteristic = nullptr;
uint8_t BLEControl::pendingCommand = 0;
uint8_t BLEControl::commandData[512] = {0};
size_t BLEControl::commandDataLen = 0;
bool BLEControl::commandReady = false;
bool BLEControl::needsAdvertising = false;

BLEControl ble;

void BLEControl::ServerCallbacks::onConnect(NimBLEServer* pServer) {
  LOG("[BLE] Connected");
  NimBLEDevice::startAdvertising();
}

void BLEControl::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  LOG("[BLE] Disconnected");
  // NimBLEDevice::startAdvertising();
  BLEControl::needsAdvertising = true;
}

void BLEControl::CharacteristicCallbacks::onWrite(NimBLECharacteristic* pChar) {
  std::string rxValue = pChar->getValue();
  if (rxValue.size() == 0) return;

  // Dump semua bytes yang masuk
  String hexDump = "";
  for (size_t i = 0; i < rxValue.size(); i++) {
    char buf[6];
    snprintf(buf, sizeof(buf), "0x%02X ", (uint8_t)rxValue[i]);
    hexDump += buf;
  }
  LOG("[BLE] RX %d bytes: %s", rxValue.size(), hexDump.c_str());

  if (rxValue.size() < 4) {
    LOG("[BLE] Packet too short: %d bytes", rxValue.size());
    return;
  }

  const uint8_t* data = (const uint8_t*)rxValue.data();
  if (data[0] != 0xAA) {
    LOG("[BLE] Bad start byte: 0x%02X", data[0]);
    return;
  }

  uint8_t cmd = data[1];
  uint8_t val = data[2];
  uint8_t chk = data[3];
  if ((cmd ^ val) != chk) {
    LOG("[BLE] Checksum ERR: 0x%02X ^ 0x%02X = 0x%02X, expected 0x%02X", cmd, val, cmd ^ val, chk);
    return;
  }

  BLEControl::pendingCommand = cmd;
  BLEControl::commandDataLen = 1;
  BLEControl::commandData[0] = val;
  BLEControl::commandReady = true;
  LOG("[BLE] Command: 0x%02X, Val: %d", cmd, val);
}

BLEControl* BLEControl::instance = nullptr;

void BLEControl::CharacteristicCallbacks::onSubscribe(NimBLECharacteristic* pChar, ble_gap_conn_desc* desc, uint16_t subValue) {
  LOG("[BLE] Subscribe: subValue=%d (1=notify, 2=indicate, 0=unsubscribe)", subValue);
}

void BLEControl::FileCharacteristicCallbacks::onWrite(NimBLECharacteristic* pChar) {
  if (!BLEControl::instance) return;

  std::string value = pChar->getValue();
  if (value.length() == 0) return;

  LOG("[BLE] RX(file) %d bytes, cmd=0x%02X", value.length(), (uint8_t)value[0]);

  uint8_t cmd = value[0];
  if (cmd == CMD_FILE_START) {
    BLEControl::instance->startFileTransfer();
  } else if (cmd == CMD_FILE_DATA) {
    BLEControl::instance->writeFileData((uint8_t*)value.data() + 1, value.length() - 1);
  } else if (cmd == CMD_FILE_END) {
    BLEControl::instance->endFileTransfer();
  }
}

void BLEControl::begin() {
  instance = this;

  NimBLEDevice::init("QBOOM-Devices-2");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEDevice::setMTU(512);

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  pFileCharacteristic = pService->createCharacteristic(
      FILE_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE_NR |
      NIMBLE_PROPERTY::WRITE
  );

  pOTACharacteristic = pService->createCharacteristic(
      OTA_CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::WRITE_NR |
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::NOTIFY
  );
  pOTACharacteristic->setCallbacks(new OTACharacteristicCallbacks());

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::WRITE |
      NIMBLE_PROPERTY::NOTIFY
  );
  pCharacteristic->setCallbacks(new CharacteristicCallbacks());

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  if (pAdvertising->start()) {
    LOG("[BLE] BLE advertising started as 'QBOOM-Devices'");
    listAllAudioFiles();
  } else {
    LOG("[BLE] BLE advertising FAILED");
  }
}

void BLEControl::sendPacket(uint8_t cmd) {
  if (pCharacteristic) {
    pCharacteristic->setValue(&cmd, 1);
    pCharacteristic->notify();
  }
}

bool BLEControl::hasCommand() { return commandReady; }

uint8_t BLEControl::getCommand() {
  commandReady = false;
  uint8_t cmd = pendingCommand;
  pendingCommand = 0;
  return cmd;
}

uint8_t* BLEControl::getCommandData() { return commandData; }
size_t BLEControl::getCommandDataLength() { return commandDataLen; }

void BLEControl::startFileTransfer(String filename) {
  if (fileReceiving) cancelFileTransfer();

  String folderPath = getCurrentRegisterFolder();
  originalFilename = "engine.raw";

  if (!LittleFS.exists(folderPath)) {
    LittleFS.mkdir(folderPath);
    LOG("[BLE] Created folder: %s", folderPath.c_str());
  }
  currentFilename = folderPath + "/upload.tmp";
  tmpFile = LittleFS.open(currentFilename, "w");

  if (tmpFile) {
    fileReceiving = true;
    receivedBytes = 0;
    LOG("[BLE] File transfer started → %s", currentFilename.c_str());
  } else {
    LOG("[BLE] File open error: %s", currentFilename.c_str());
  }
}

void BLEControl::writeFileData(const uint8_t* data, size_t len) {
  if (fileReceiving && tmpFile) {
    tmpFile.write(data, len);
    receivedBytes += len;
    LOG("[BLE] File data: %d bytes (total: %d)", len, receivedBytes);
  }
}

void BLEControl::endFileTransfer() {
  if (!fileReceiving) return;

  if (tmpFile) tmpFile.close();
  fileReceiving = false;

  if (receivedBytes > 0) {
    String folderPath = getCurrentRegisterFolder();
    String finalFilename = folderPath + "/" + originalFilename;
    if (LittleFS.exists(finalFilename)) LittleFS.remove(finalFilename);
    if (LittleFS.rename(currentFilename, finalFilename)) {
      LOG("[BLE] File saved: %s (%d bytes)", finalFilename.c_str(), receivedBytes);
    } else {
      LOG("[BLE] File rename failed: %s → %s", currentFilename.c_str(), finalFilename.c_str());
    }
  } else {
    if (LittleFS.exists(currentFilename)) LittleFS.remove(currentFilename);
  }
  listAllAudioFiles();
}

void BLEControl::cancelFileTransfer() {
  if (tmpFile) tmpFile.close();
  if (LittleFS.exists(currentFilename)) LittleFS.remove(currentFilename);
  fileReceiving = false;
  receivedBytes = 0;
}

void BLEControl::sendCurrentPlaying() {
  String folderPath = getCurrentRegisterFolder();
  String title = "empty";

  File dir = LittleFS.open(folderPath);
  if (dir && dir.isDirectory()) {
    File file = dir.openNextFile();
    while (file) {
      if (!file.isDirectory() && String(file.name()).endsWith(".raw")) {
        title = file.name();
        break;
      }
      file = dir.openNextFile();
    }
    dir.close();
  }
  String response = "0xAA," + title;
  sendBLEResponse(response);
  LOG("[BLE] Current playing: %s", title.c_str());
}

void BLEControl::replyFileList(uint8_t registerNum) {
  for (int i = 0; i < 4; i++) {
    String folderPath = (i == 0) ? "/Audio" : "/Audio" + String(i);
    String title = "empty";

    File dir = LittleFS.open(folderPath);
    if (dir && dir.isDirectory()) {
      File file = dir.openNextFile();
      while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".raw")) {
          title = file.name();
          break;
        }
        file = dir.openNextFile();
      }
      dir.close();
    }

    String response = "0xAA,reg" + String(i + 1) + ":" + title;
    sendBLEResponse(response);
    LOG("[BLE] File list sent: reg%d = %s", i + 1, title.c_str());
    delay(50);
  }
}

void BLEControl::setActiveFile(uint8_t index) {}
// void BLEControl::update() {}
void BLEControl::update() {
  // Jika bendera terangkat, nyalakan advertising secara aman
  if (BLEControl::needsAdvertising) {
    delay(100); // Beri jeda 100ms agar OS membersihkan memori koneksi lama
    NimBLEDevice::startAdvertising();
    LOG("[BLE] Restarting Advertising...");
    BLEControl::needsAdvertising = false;
  }
}

void BLEControl::createAudioFolders() {
  if (!LittleFS.exists("/Audio"))         LittleFS.mkdir("/Audio");
  if (!LittleFS.exists("/Audio/engine"))  LittleFS.mkdir("/Audio/engine");
  if (!LittleFS.exists("/Audio/shift"))   LittleFS.mkdir("/Audio/shift");
  if (!LittleFS.exists("/Audio/effects")) LittleFS.mkdir("/Audio/effects");
}

void BLEControl::listRawFiles(const char* folder) {
  File dir = LittleFS.open(folder);
  if (!dir || !dir.isDirectory()) {
    LOG("[BLE] [%s] folder not found or empty", folder);
    return;
  }

  bool found = false;
  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory() && String(file.name()).endsWith(".raw")) {
      LOG("[BLE] [%s] %s (%d bytes)", folder, file.name(), file.size());
      found = true;
    }
    file = dir.openNextFile();
  }
  if (!found) LOG("[BLE] [%s] no .raw files", folder);
  dir.close();
}

void BLEControl::listAllAudioFiles() {
  LOG("[BLE] === Audio Files ===");
  listRawFiles("/Audio");
  listRawFiles("/Audio1");
  listRawFiles("/Audio2");
  listRawFiles("/Audio3");
}

void BLEControl::deleteFile(const char* filepath) {
  if (LittleFS.exists(filepath)) {
    LittleFS.remove(filepath);
    LOG("[BLE] DEL:%s", filepath);
  }
}

void BLEControl::deleteFolder(const char* folderpath) {
  File dir = LittleFS.open(folderpath);
  if (!dir || !dir.isDirectory()) return;

  File file = dir.openNextFile();
  while (file) {
    if (!file.isDirectory()) LittleFS.remove(String(file.name()));
    file = dir.openNextFile();
  }
  dir.close();
  LittleFS.rmdir(folderpath);
}

String BLEControl::getCurrentRegisterFolder() {
  if (currentRegister == 1) return "/Audio";
  if (currentRegister == 2) return "/Audio1";
  if (currentRegister == 3) return "/Audio2";
  if (currentRegister == 4) return "/Audio3";
  return "/Audio";
}

void BLEControl::setCurrentRegister(uint8_t reg) { currentRegister = reg; }
void BLEControl::setCurrentMode(uint8_t mode) { currentMode = mode; }

void BLEControl::sendStatus(uint8_t mode, uint8_t reg, bool playing) {
  if (!pCharacteristic) return;
  // [0xAA, 0xFF, mode|reg<<2|playing<<3, chk]
  uint8_t val = (mode & 0x03) | ((reg & 0x0F) << 2) | (playing ? 0x40 : 0x00);
  uint8_t buf[4] = {0xAA, 0xFF, val, (uint8_t)(0xFF ^ val)};
  pCharacteristic->setValue(buf, 4);
  pCharacteristic->notify();
  LOG("[BLE] Status sent: mode=%d reg=%d playing=%d", mode, reg, playing);
}

void BLEControl::sendBLEResponse(String response) {
  if (pCharacteristic) {
    pCharacteristic->setValue((uint8_t*)response.c_str(), response.length());
    pCharacteristic->notify();
    LOG("[BLE] Response (%d bytes): %s", response.length(), response.c_str());
  }
}

void BLEControl::enableFileTransfer(bool enable) {
  if (!pFileCharacteristic) {
    LOG("[BLE] File characteristic not initialized");
    return;
  }
  if (enable) {
    pFileCharacteristic->setCallbacks(new FileCharacteristicCallbacks());
  } else {
    pFileCharacteristic->setCallbacks(nullptr);
    if (fileReceiving) cancelFileTransfer();
  }
}

void BLEControl::formatLittleFS() {
  if (LittleFS.format()) {
    LOG("[BLE] FS formatted, restart");
    delay(1000);
    ESP.restart();
  } else {
    LOG("[BLE] FS format ERR");
  }
}

void BLEControl::sendOBD2Status() {
  if (!pCharacteristic) return;
  uint8_t val = obd2.isConnected() ? 1 : 0;
  uint8_t buf[4] = {0xAA, 0x30, val, (uint8_t)(0x30 ^ val)};
  pCharacteristic->setValue(buf, 4);
  pCharacteristic->notify();
  LOG("[BLE] OBD2 status: %s", val ? "connected" : "disconnected");
}

void BLEControl::sendOBD2SOH() {
  if (!pCharacteristic) return;
  obd2.refreshStateOfHealth();
  delay(100);
  uint8_t val = (uint8_t)obd2.getStateOfHealth();
  uint8_t buf[4] = {0xAA, 0x31, val, (uint8_t)(0x31 ^ val)};
  pCharacteristic->setValue(buf, 4);
  pCharacteristic->notify();
  LOG("[BLE] OBD2 SOH: %d", val);
}

void BLEControl::sendOBD2Temp() {
  if (!pCharacteristic) return;
  uint8_t val = (uint8_t)(int8_t)obd2.getBatteryTemp();
  uint8_t buf[4] = {0xAA, 0x32, val, (uint8_t)(0x32 ^ val)};
  pCharacteristic->setValue(buf, 4);
  pCharacteristic->notify();
  LOG("[BLE] OBD2 Temp: %d°C", (int8_t)val);
}

void BLEControl::sendOBD2Steering() {
  if (!pCharacteristic) return;
  int16_t steering = obd2.getSteeringAngle();
  uint8_t lo = steering & 0xFF;
  uint8_t hi = (steering >> 8) & 0xFF;
  uint8_t buf[5] = {0xAA, 0x33, lo, hi, (uint8_t)(0x33 ^ lo ^ hi)};
  pCharacteristic->setValue(buf, 5);
  pCharacteristic->notify();
  LOG("[BLE] OBD2 Steering: %d", steering);
}

void BLEControl::sendOBD2Power() {
  if (!pCharacteristic) return;
#ifdef USE_HV_BATTERY_POWER
  uint16_t val = obd2.getHVBatteryPower();
  LOG("[BLE] OBD2 Power: %dW", val);
#else
  uint16_t val = obd2.getRPM();
  LOG("[BLE] OBD2 RPM: %d", val);
#endif
  uint8_t lo = val & 0xFF;
  uint8_t hi = (val >> 8) & 0xFF;
  uint8_t buf[5] = {0xAA, 0x34, lo, hi, (uint8_t)(0x34 ^ lo ^ hi)};
  pCharacteristic->setValue(buf, 5);
  pCharacteristic->notify();
}

void BLEControl::sendIMUStatus() {
  if (!pCharacteristic) return;
  uint8_t buf[7];
  buf[0] = 0xAA;
  buf[1] = 0x61;
  buf[2] = imuControl.isEnabled() ? 1 : 0;
  buf[3] = (uint8_t)imuControl.getCondition();
  buf[4] = (uint8_t)(int8_t)imuControl.getPitchDeg();
  buf[5] = (uint8_t)(int8_t)imuControl.getCalibOffset();
  buf[6] = buf[1] ^ buf[2] ^ buf[3] ^ buf[4] ^ buf[5];
  pCharacteristic->setValue(buf, 7);
  pCharacteristic->notify();
  LOG("[BLE] IMU status sent");
}

// ============================================================================
// OTA FUNCTIONS
// ============================================================================

void BLEControl::sendFWVersion() {
  if (!pCharacteristic) return;
  String ver = FW_VERSION;
  uint8_t len = ver.length();
  uint8_t buf[len + 4];
  buf[0] = 0xAA;
  buf[1] = 0x74;
  buf[2] = len;
  memcpy(&buf[3], ver.c_str(), len);
  uint8_t chk = 0x74 ^ len;
  for (uint8_t i = 0; i < len; i++) chk ^= ver[i];
  buf[3 + len] = chk;
  pCharacteristic->setValue(buf, 4 + len);
  pCharacteristic->notify();
  LOG("[BLE] FW:%s", FW_VERSION);
}

void BLEControl::startOTA(uint32_t fileSize) {
  if (otaReceiving) abortOTA();

  const esp_partition_t* otaPartition = esp_ota_get_next_update_partition(NULL);
  if (!otaPartition) {
    LOG("[BLE] OTA: no update partition found");
    sendBLEResponse("OTA:ERR:NO_PARTITION");
    return;
  }

  esp_err_t err = esp_ota_begin(otaPartition, fileSize, &otaHandle);
  if (err != ESP_OK) {
    LOG("[BLE] OTA begin failed: %s", esp_err_to_name(err));
    sendBLEResponse("OTA:ERR:BEGIN_FAIL");
    return;
  }

  otaReceiving = true;
  otaTotalSize = fileSize;
  otaBytesWritten = 0;
  LOG("[BLE] OTA started, expected size: %lu bytes", fileSize);
  sendBLEResponse("OTA:OK:START");
}

void BLEControl::writeOTAData(const uint8_t* data, size_t len) {
  if (!otaReceiving) return;

  esp_err_t err = esp_ota_write(otaHandle, data, len);
  if (err != ESP_OK) {
    LOG("[BLE] OTA write ERR:%s", esp_err_to_name(err));
    sendBLEResponse("OTA:ERR:WRITE_FAIL");
    abortOTA();
    return;
  }

  otaBytesWritten += len;

  if (otaTotalSize > 0 && (otaBytesWritten % 10240) < len) {
    uint8_t progress = (uint8_t)((otaBytesWritten * 100) / otaTotalSize);
    LOG("[BLE] OTA progress: %d%% (%lu/%lu bytes)", progress, otaBytesWritten, otaTotalSize);
    char buf[20];
    snprintf(buf, sizeof(buf), "OTA:PROG:%d", progress);
    sendBLEResponse(String(buf));
  }
}

void BLEControl::endOTA() {
  if (!otaReceiving) {
    sendBLEResponse("OTA:ERR:NOT_STARTED");
    return;
  }

  esp_err_t err = esp_ota_end(otaHandle);
  if (err != ESP_OK) {
    LOG("[BLE] OTA end ERR:%s", esp_err_to_name(err));
    sendBLEResponse("OTA:ERR:END_FAIL");
    otaReceiving = false;
    return;
  }

  const esp_partition_t* otaPartition = esp_ota_get_next_update_partition(NULL);
  err = esp_ota_set_boot_partition(otaPartition);
  if (err != ESP_OK) {
    LOG("[BLE] OTA boot ERR:%s", esp_err_to_name(err));
    sendBLEResponse("OTA:ERR:BOOT_FAIL");
    otaReceiving = false;
    return;
  }

  otaReceiving = false;
  LOG("[BLE] OTA complete: %lu bytes written, restarting...", otaBytesWritten);
  sendBLEResponse("OTA:OK:DONE");
  delay(500);
  ESP.restart();
}

void BLEControl::abortOTA() {
  if (!otaReceiving) return;
  esp_ota_abort(otaHandle);
  otaReceiving = false;
  otaBytesWritten = 0;
  otaTotalSize = 0;
  LOG("[BLE] OTA aborted");
  sendBLEResponse("OTA:ERR:ABORTED");
}

void BLEControl::OTACharacteristicCallbacks::onWrite(NimBLECharacteristic* pChar) {
  if (!BLEControl::instance) return;

  std::string value = pChar->getValue();
  if (value.length() == 0) return;
  LOG("[DEBUG] Ada data masuk ke kamar OTA: %d bytes", value.length());
  uint8_t cmd = value[0];

  if (cmd == CMD_OTA_START) {
    if (value.length() < 5) {
      BLEControl::instance->sendBLEResponse("OTA:ERR:INVALID_START");
      return;
    }
    uint32_t fileSize = ((uint8_t)value[1]) |
                        ((uint8_t)value[2] << 8) |
                        ((uint8_t)value[3] << 16) |
                        ((uint8_t)value[4] << 24);
    BLEControl::instance->startOTA(fileSize);
  } else if (cmd == CMD_OTA_DATA) {
    BLEControl::instance->writeOTAData((uint8_t*)value.data() + 1, value.length() - 1);
  } else if (cmd == CMD_OTA_END) {
    BLEControl::instance->endOTA();
  } else if (cmd == CMD_OTA_ABORT) {
    BLEControl::instance->abortOTA();
  }
}
