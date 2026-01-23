#include "BLEControl.h"
#include "OBD2Control.h"

const int MAX_GEAR = 4;
const int MIN_GEAR = 0;

NimBLECharacteristic* BLEControl::pCharacteristic = nullptr;
NimBLECharacteristic* BLEControl::pFileCharacteristic = nullptr;
uint8_t BLEControl::pendingCommand = 0;
uint8_t BLEControl::commandData[512] = {0};
size_t BLEControl::commandDataLen = 0;
bool BLEControl::commandReady = false;

BLEControl ble;

void BLEControl::ServerCallbacks::onConnect(NimBLEServer* pServer) {
  Serial.println("📱 BLE Connected");
  NimBLEDevice::startAdvertising();

}

void BLEControl::ServerCallbacks::onDisconnect(NimBLEServer* pServer) {
  Serial.println("📱 BLE Disconnected");
  NimBLEDevice::startAdvertising();
}

void BLEControl::CharacteristicCallbacks::onWrite(NimBLECharacteristic* pChar) {
  std::string rxValue = pChar->getValue();
  if (rxValue.size() < 4) {
    Serial.println("⚠️ Data too short");
    return;
  }

  const uint8_t* data = (const uint8_t*)rxValue.data();
  if (data[0] != 0xAA) {
    Serial.printf("⚠️ Invalid start byte: 0x%02X\n", data[0]);
    return;
  }

  uint8_t cmd = data[1];
  uint8_t val = data[2];
  uint8_t chk = data[3];

  if ((cmd ^ val) != chk) {
    Serial.println("⚠️ BLE checksum gagal");
    return;
  }

  BLEControl::pendingCommand = cmd;
  BLEControl::commandDataLen = 1;
  BLEControl::commandData[0] = val;
  BLEControl::commandReady = true;
  
  Serial.printf("✅ BLE Command: 0x%02X, Val: %d\n", cmd, val);
}

BLEControl* BLEControl::instance = nullptr;

void BLEControl::FileCharacteristicCallbacks::onWrite(NimBLECharacteristic* pChar) {
  if (!BLEControl::instance) {
    Serial.println("⚠️ File callback: instance null!");
    return;
  }
  
  std::string value = pChar->getValue();
  if (value.length() > 0) {
    uint8_t cmd = value[0];
    Serial.printf("📥 File callback triggered - CMD: 0x%02X, Len: %d\n", cmd, value.length());
    
    if (cmd == CMD_FILE_START) {
      BLEControl::instance->startFileTransfer();
    } else if (cmd == CMD_FILE_DATA) {
      BLEControl::instance->writeFileData((uint8_t*)value.data() + 1, value.length() - 1);
    } else if (cmd == CMD_FILE_END) {
      BLEControl::instance->endFileTransfer();
    } else {
      Serial.printf("⚠️ Unknown file command: 0x%02X\n", cmd);
    }
  } else {
    Serial.println("⚠️ File callback: empty data");
  }
}

void BLEControl::begin() {
    instance = this;
    
    // Create audio folders
    // createAudioFolders();

    // --- Init BLE ---
    NimBLEDevice::init("QBOOM-Devices");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);  // power max biar sinyal mantap

    // NOTE: MTU paling stabil di-set setelah server dibuat
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEDevice::setMTU(512);   // biar file transfer bisa gede

    // --- Service ---
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // === Characteristic utama: command, status, notify ===
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );
    pCharacteristic->setCallbacks(new CharacteristicCallbacks());

    // === Characteristic kedua: file transfer ===
    //  PENTING: pakai WRITE_NR (write without response)
    //  biar transfer file nggak nge-lag
    pFileCharacteristic = pService->createCharacteristic(
        FILE_CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::WRITE_NR |   // <--- WAJIB kalau buat file transfer
        NIMBLE_PROPERTY::WRITE
    );
    // File transfer disabled by default - akan diaktifkan saat programming mode

    // --- Start service ---
    pService->start();

    // --- Advertising ---
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    // pAdvertising->setName("QBOOM-Devices");

    if (pAdvertising->start()) {
        Serial.println("✅ BLE Started: QBOOM-Devices");
        listAllAudioFiles();
    } else {
        Serial.println("❌ BLE Failed to start");
    }
}


void BLEControl::sendPacket(uint8_t cmd) {
  if (pCharacteristic) {
    pCharacteristic->setValue(&cmd, 1);
    pCharacteristic->notify();
  }
}

bool BLEControl::hasCommand() {
  return commandReady;
}

uint8_t BLEControl::getCommand() {
  commandReady = false;
  uint8_t cmd = pendingCommand;
  pendingCommand = 0;
  // Don't reset commandDataLen here - let SystemManager read it first
  return cmd;
}

uint8_t* BLEControl::getCommandData() {
  return commandData;
}

size_t BLEControl::getCommandDataLength() {
  return commandDataLen;
}

void BLEControl::startFileTransfer(String filename) {
  if (fileReceiving) {
    cancelFileTransfer();
  }
  
  // Get current register from SystemManager to determine target folder
  String folderPath = getCurrentRegisterFolder();
  
  // Use generic filename - same for all registers
  originalFilename = "engine.raw";
  
  // Create folder if not exists
  if (!LittleFS.exists(folderPath)) {
    LittleFS.mkdir(folderPath);
    Serial.printf("📁 Created folder: %s\n", folderPath.c_str());
  }
  
  currentFilename = folderPath + "/upload.tmp";
  tmpFile = LittleFS.open(currentFilename, "w");
  
  if (tmpFile) {
    fileReceiving = true;
    receivedBytes = 0;
    Serial.printf("📥 File transfer started -> %s (will save as %s)\n", currentFilename.c_str(), originalFilename.c_str());
  } else {
    Serial.printf("❌ Failed to create temp file: %s\n", currentFilename.c_str());
  }
}

void BLEControl::writeFileData(const uint8_t* data, size_t len) {
  if (fileReceiving && tmpFile) {
    tmpFile.write(data, len);
    receivedBytes += len;
  }
}

void BLEControl::endFileTransfer() {
  if (fileReceiving) {
    if (tmpFile) {
      tmpFile.close();
    }
    fileReceiving = false;
    
    if (receivedBytes > 0) {
      // Use original filename
      String folderPath = getCurrentRegisterFolder();
      String finalFilename = folderPath + "/" + originalFilename;
      
      // Remove old file if exists
      if (LittleFS.exists(finalFilename)) {
        LittleFS.remove(finalFilename);
      }
      
      // Rename temp file to final name
      if (LittleFS.rename(currentFilename, finalFilename)) {
        Serial.printf("✅ File saved: %s (%d bytes)\n", finalFilename.c_str(), receivedBytes);
      } else {
        Serial.printf("❌ Failed to rename: %s -> %s\n", currentFilename.c_str(), finalFilename.c_str());
      }
    } else {
      Serial.println("❌ No data received, removing temp file");
      if (LittleFS.exists(currentFilename)) {
        LittleFS.remove(currentFilename);
      }
    }
    
    listAllAudioFiles();
  } else {
    Serial.println("⚠️ endFileTransfer called but not receiving");
  }
}

void BLEControl::cancelFileTransfer() {
  if (tmpFile) {
    tmpFile.close();
  }
  if (LittleFS.exists(currentFilename)) {
    LittleFS.remove(currentFilename);
  }
  fileReceiving = false;
  receivedBytes = 0;
  Serial.println("❌ File transfer cancelled");
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
  Serial.printf("📡 Current playing: %s\n", title.c_str());
}

void BLEControl::replyFileList(uint8_t registerNum) {
  // Send each register separately to avoid MTU issues
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
    delay(50);  // Small delay between packets
    Serial.printf("📡 File list sent: reg%d = %s\n", i + 1, title.c_str());
  }
}

void BLEControl::setActiveFile(uint8_t index) {
  // TODO: Implement set active file
}

void BLEControl::update() {
  // Reserved for future use
}

void BLEControl::createAudioFolders() {
  bool wasEmpty = !LittleFS.exists("/audio");
  
  if (!LittleFS.exists("/audio")) {
    LittleFS.mkdir("/audio");
    Serial.println("📁 Created /audio folder");
  }
  
  if (!LittleFS.exists("/audio/engine")) {
    LittleFS.mkdir("/audio/engine");
    Serial.println("📁 Created /audio/engine folder");
  }
  
  if (!LittleFS.exists("/audio/shift")) {
    LittleFS.mkdir("/audio/shift");
    Serial.println("📁 Created /audio/shift folder");
  }
  
  if (!LittleFS.exists("/audio/effects")) {
    LittleFS.mkdir("/audio/effects");
    Serial.println("📁 Created /audio/effects folder");
  }
  
  if (wasEmpty) {
    Serial.println("✨ LittleFS has been reset - All folders recreated");
  }
}

void BLEControl::listRawFiles(const char* folder) {
  File dir = LittleFS.open(folder);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("⚠️ Folder not found: %s\n", folder);
    return;
  }
  
  Serial.printf("📁 %s:\n", folder);
  File file = dir.openNextFile();
  int count = 0;
  
  while (file) {
    if (!file.isDirectory()) {
      String filename = file.name();
      if (filename.endsWith(".raw")) {
        Serial.printf("  🎧 %s (%d bytes)\n", filename.c_str(), file.size());
        count++;
      }
    }
    file = dir.openNextFile();
  }
  
  if (count == 0) {
    Serial.println("  💭 No .raw files found");
  }
  
  dir.close();
}

void BLEControl::listAllAudioFiles() {
  Serial.println("📊 Audio Files:");
  listRawFiles("/Audio");
  listRawFiles("/Audio1");
  listRawFiles("/Audio2");
  listRawFiles("/Audio3");
  Serial.println();
}

void BLEControl::deleteFile(const char* filepath) {
  if (LittleFS.exists(filepath)) {
    if (LittleFS.remove(filepath)) {
      Serial.printf("✅ Deleted file: %s\n", filepath);
    } else {
      Serial.printf("❌ Failed to delete file: %s\n", filepath);
    }
  } else {
    Serial.printf("⚠️ File not found: %s\n", filepath);
  }
}

void BLEControl::deleteFolder(const char* folderpath) {
  File dir = LittleFS.open(folderpath);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("⚠️ Folder not found: %s\n", folderpath);
    return;
  }
  
  // Delete all files in folder first
  File file = dir.openNextFile();
  while (file) {
    String fullPath = String(folderpath) + "/" + file.name();
    if (!file.isDirectory()) {
      LittleFS.remove(fullPath);
      Serial.printf("🗑️ Deleted: %s\n", fullPath.c_str());
    }
    file = dir.openNextFile();
  }
  dir.close();
  
  // Delete the folder itself
  if (LittleFS.rmdir(folderpath)) {
    Serial.printf("✅ Deleted folder: %s\n", folderpath);
  } else {
    Serial.printf("❌ Failed to delete folder: %s\n", folderpath);
  }
}

String BLEControl::getCurrentRegisterFolder() {
  // Get current register from external source (will be set by SystemManager)
  if (currentRegister == 1) return "/Audio";
  if (currentRegister == 2) return "/Audio1";
  if (currentRegister == 3) return "/Audio2";
  if (currentRegister == 4) return "/Audio3";
  return "/Audio"; // default
}

void BLEControl::setCurrentRegister(uint8_t reg) {
  currentRegister = reg;
}

void BLEControl::sendStatus(uint8_t mode, uint8_t reg, bool playing) {
  if (pCharacteristic) {
    uint8_t status[4];
    status[0] = 0xAA;  // Same as command protocol
    status[1] = 0xFF;  // Status command
    status[2] = mode;  // 0=Normal, 1=Programming
    status[3] = status[1] ^ status[2];  // Checksum
    
    pCharacteristic->setValue(status, 4);
    pCharacteristic->notify();
    Serial.printf("📡 Status sent: Mode=%d\n", mode);
  }
}

void BLEControl::sendBLEResponse(String response) {
  if (pCharacteristic) {
    pCharacteristic->setValue((uint8_t*)response.c_str(), response.length());
    pCharacteristic->notify();
    Serial.printf("📡 BLE Response (%d bytes): %s\n", response.length(), response.c_str());
  }
}

void BLEControl::enableFileTransfer(bool enable) {
  if (!pFileCharacteristic) {
    Serial.println("⚠️ File characteristic not found!");
    return;
  }
  
  if (enable) {
    Serial.println("🔄 Enabling file transfer characteristic...");
    pFileCharacteristic->setCallbacks(new FileCharacteristicCallbacks());
    Serial.printf("✅ File transfer ENABLED - UUID: %s\n", FILE_CHARACTERISTIC_UUID);
  } else {
    Serial.println("🔄 Disabling file transfer characteristic...");
    pFileCharacteristic->setCallbacks(nullptr);
    if (fileReceiving) {
      Serial.println("📥 Cancelling active file transfer...");
      cancelFileTransfer();
    }
    Serial.printf("❌ File transfer DISABLED - UUID: %s\n", FILE_CHARACTERISTIC_UUID);
  }
}

void BLEControl::formatLittleFS() {
  Serial.println("⚠️ FORMATTING LittleFS - ALL DATA WILL BE LOST!");
  
  if (LittleFS.format()) {
    Serial.println("✅ LittleFS formatted successfully");
    Serial.println("🔄 Restarting ESP32...");
    delay(1000);
    ESP.restart();
  } else {
    Serial.println("❌ Failed to format LittleFS");
  }
}

void BLEControl::sendOBD2Status() {
  if (!pCharacteristic) return;
  uint8_t status = obd2.isConnected() ? 1 : 0;
  pCharacteristic->setValue(&status, 1);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 Status: %d\n", status);
}

void BLEControl::sendOBD2SOH() {
  if (!pCharacteristic) return;
  uint8_t soh = obd2.getStateOfHealth();
  pCharacteristic->setValue(&soh, 1);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 SoH: %d%%\n", soh);
}

void BLEControl::sendOBD2Temp() {
  if (!pCharacteristic) return;
  int8_t temp = obd2.getBatteryTemp();
  pCharacteristic->setValue((uint8_t*)&temp, 1);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 Temp: %d°C\n", temp);
}

void BLEControl::sendOBD2Steering() {
  if (!pCharacteristic) return;
  int16_t steering = obd2.getSteeringAngle();
  pCharacteristic->setValue((uint8_t*)&steering, 2);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 Steering: %d°\n", steering);
}

void BLEControl::sendOBD2Power() {
  if (!pCharacteristic) return;
#ifdef USE_HV_BATTERY_POWER
  uint16_t power = obd2.getHVBatteryPower();
  pCharacteristic->setValue((uint8_t*)&power, 2);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 Power: %d W\n", power);
#else
  uint16_t rpm = obd2.getRPM();
  pCharacteristic->setValue((uint8_t*)&rpm, 2);
  pCharacteristic->notify();
  Serial.printf("📡 OBD2 RPM: %d\n", rpm);
#endif
}
