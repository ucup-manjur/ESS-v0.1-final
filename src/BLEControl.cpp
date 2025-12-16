#include "BLEControl.h"

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
    NimBLEDevice::init("ESP32_ESS_SERVER");
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
    pAdvertising->setName("ESP32_ESS_SERVER");

    if (pAdvertising->start()) {
        Serial.println("✅ BLE Started: ESP32_ESS_SERVER");
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
  commandDataLen = 0;
  return cmd;
}

uint8_t* BLEControl::getCommandData() {
  return commandData;
}

size_t BLEControl::getCommandDataLength() {
  return commandDataLen;
}

void BLEControl::startFileTransfer() {
  if (fileReceiving) {
    cancelFileTransfer();
  }
  
  currentFilename = "/audio/engine/upload.tmp";
  tmpFile = LittleFS.open(currentFilename, "w");
  
  if (tmpFile) {
    fileReceiving = true;
    receivedBytes = 0;
    Serial.println("📥 File transfer started");
  } else {
    Serial.println("❌ Failed to create temp file");
  }
}

void BLEControl::writeFileData(const uint8_t* data, size_t len) {
  if (fileReceiving && tmpFile) {
    tmpFile.write(data, len);
    receivedBytes += len;
  }
}

void BLEControl::endFileTransfer() {
  if (fileReceiving && tmpFile) {
    tmpFile.close();
    fileReceiving = false;
    Serial.printf("✅ File transfer complete: %d bytes\n", receivedBytes);
    listAllAudioFiles();
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

void BLEControl::replyCurrentFile() {
  // TODO: Implement current file info reply
}

void BLEControl::replyFileList() {
  // TODO: Implement file list reply
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
