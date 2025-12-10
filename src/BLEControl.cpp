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
  if (!BLEControl::instance) return;
  
  std::string value = pChar->getValue();
  if (value.length() > 0) {
    uint8_t cmd = value[0];
    
    if (cmd == CMD_FILE_START) {
      BLEControl::instance->startFileTransfer();
    } else if (cmd == CMD_FILE_DATA) {
      BLEControl::instance->writeFileData((uint8_t*)value.data() + 1, value.length() - 1);
    } else if (cmd == CMD_FILE_END) {
      BLEControl::instance->endFileTransfer();
    }
  }
}

void BLEControl::begin() {
    instance = this;

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
    pFileCharacteristic->setCallbacks(new FileCharacteristicCallbacks());

    // --- Start service ---
    pService->start();

    // --- Advertising ---
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setName("ESP32_ESS_SERVER");

    if (pAdvertising->start()) {
        Serial.println("✅ BLE Started: ESP32_ESS_SERVER");
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
  
  currentFilename = "/upload.tmp";
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
