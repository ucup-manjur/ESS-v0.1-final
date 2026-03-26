/*
 * ============================================================================
 * BLEControl.h - Bluetooth Low Energy Communication
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk komunikasi wireless via BLE menggunakan NimBLE library.
 * Menangani command control, file transfer, dan OBD2 data request.
 * 
 * FITUR UTAMA:
 * 1. Command Control:
 *    - Audio control (volume, register, play/stop)
 *    - Audio effects (rev, gear shift)
 *    - System control (status, mode)
 *    - OBD2 data request
 * 
 * 2. File Transfer:
 *    - Upload audio file via BLE
 *    - Max MTU: 512 bytes
 *    - Progress tracking
 *    - Auto-rename ke engine.raw
 *    - Register-aware (upload ke folder sesuai register)
 * 
 * 3. File Management:
 *    - List files per register
 *    - Delete file/folder
 *    - Format LittleFS
 *    - Get file info
 * 
 * 4. OBD2 Integration:
 *    - Request OBD2 status
 *    - Request SOH, temp, steering, power
 *    - Send response via BLE notify
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    BLEControl ble;
 *    ble.begin();  // Initialize BLE server
 *    ```
 * 
 * 2. Check Command:
 *    ```cpp
 *    if (ble.hasCommand()) {
 *        uint8_t cmd = ble.getCommand();
 *        uint8_t* data = ble.getCommandData();
 *        // Process command
 *    }
 *    ```
 * 
 * 3. Send Response:
 *    ```cpp
 *    ble.sendBLEResponse("0xAA,data");
 *    ble.sendStatus(MODE_NORMAL);
 *    ble.sendOBD2SOH();
 *    ```
 * 
 * 4. File Transfer:
 *    ```cpp
 *    // Enable saat programming mode
 *    ble.enableFileTransfer(true);
 *    
 *    // Aplikasi kirim:
 *    // 1. CMD_FILE_START (0x20)
 *    // 2. CMD_FILE_DATA (0x21) + data chunks
 *    // 3. CMD_FILE_END (0x22)
 *    
 *    // Disable saat keluar programming mode
 *    ble.enableFileTransfer(false);
 *    ```
 * 
 * 5. File Management:
 *    ```cpp
 *    ble.replyFileList(1);  // List files di register 1
 *    ble.sendCurrentPlaying();  // Send current file info
 *    ble.deleteFile("/Audio/engine.raw");
 *    ```
 * 
 * PROTOCOL FORMAT:
 * Command: 0xAA [CMD] [VAL] [CHECKSUM]
 * - Start byte: 0xAA
 * - Command: 1 byte (lihat command definitions)
 * - Value: 1 byte (parameter)
 * - Checksum: CMD XOR VAL
 * 
 * Response: String format
 * - File list: "0xAA,reg1:title,reg2:title,..."
 * - File info: "0xAA,current_title"
 * - Status: Binary format
 * 
 * COMMAND DEFINITIONS:
 * Audio Control:
 * - 0x01: Gear Up
 * - 0x02: Gear Down
 * - 0x03: Rev Start
 * - 0x04: Rev Stop
 * - 0x11: Volume/Mute (val=0:toggle, val>0:set volume)
 * - 0x15: Set Audio Register (val=1-4)
 * - 0x16: Toggle Auto Shift
 * 
 * File Management:
 * - 0x13: Request File List (val=0:all, val=1-4:specific register)
 * - 0x14: Request File Info (current playing)
 * - 0x20: File Start (begin upload)
 * - 0x21: File Data (data chunks)
 * - 0x22: File End (finish upload)
 * - 0x23: Delete File
 * - 0x24: Delete Folder
 * 
 * OBD2 Commands:
 * - 0x30: Request OBD2 Status (connected/disconnected)
 * - 0x31: Request SOH (State of Health)
 * - 0x32: Request Battery Temperature
 * - 0x33: Request Steering Angle
 * - 0x34: Request Power/RPM
 * 
 * System:
 * - 0xFF: Request Status (mode, register, playing)
 * 
 * BLE CONFIGURATION:
 * - Device Name: "QBOOM-Devices"
 * - Service UUID: 12345678-1234-1234-1234-1234567890ab
 * - Characteristic UUID: 87654321-4321-4321-4321-0987654321ab
 * - File Characteristic UUID: 87654321-4321-4321-4321-0987654321cd
 * - MTU: 512 bytes
 * - Power: ESP_PWR_LVL_P9 (max)
 * 
 * FILE TRANSFER:
 * - Max file size: 1MB
 * - Format: .raw (PCM 8-bit)
 * - Temp file: upload.tmp
 * - Final file: engine.raw
 * - Auto-normalization: 19-237 range
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <NimBLEDevice.h>
#include <LittleFS.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-0987654321ab"
#define FILE_CHARACTERISTIC_UUID "87654321-4321-4321-4321-0987654321cd"

// Command definitions untuk kontrol
#define CMD_GEAR_UP          0x01
#define CMD_GEAR_DOWN        0x02
#define CMD_REV_START        0x03
#define CMD_REV_STOP         0x04
#define CMD_VOL              0x11
#define CMD_REQ_FILE_LIST    0x13
#define CMD_REQ_FILE_INFO    0x14
#define CMD_SET_AUDIO_PLAY   0x15
#define CMD_TOGGLE_AUTO_SHIFT 0x16
#define CMD_REQ_STATUS       0xFF

// OBD2 command definitions
#define CMD_REQ_OBD2_STATUS  0x30
#define CMD_REQ_OBD2_SOH     0x31
#define CMD_REQ_OBD2_TEMP    0x32
#define CMD_REQ_OBD2_STEERING 0x33
#define CMD_REQ_OBD2_POWER   0x34

// Command definitions untuk file transfer audio
#define CMD_FILE_START       0x20
#define CMD_FILE_DATA        0x21
#define CMD_FILE_END         0x22
#define CMD_DELETE_FILE      0x23
#define CMD_DELETE_FOLDER    0x24

// Slope config command definitions
#define CMD_SET_SLOPE_LOW    0x50
#define CMD_SET_SLOPE_MID    0x51
#define CMD_SET_SLOPE_HIGH   0x52
#define CMD_SET_SLOPE_DECEL  0x53
#define CMD_SET_SLOPE_LAG    0x54
#define CMD_SET_SLOPE_THRESH 0x55
#define CMD_REQ_SLOPE_CONFIG 0x56

// IMU commands
#define CMD_IMU_TOGGLE     0x60
#define CMD_REQ_IMU_STATUS 0x61
#define CMD_IMU_CALIBRATE  0x62

extern const int MAX_GEAR;
extern const int MIN_GEAR;

class BLEControl {
public:
  void begin();
  void sendPacket(uint8_t cmd);
  bool isReceivingFile() { return fileReceiving; }
  String getCurrentFilename() { return currentFilename; }
  size_t getReceivedBytes() { return receivedBytes; }
  
  bool hasCommand();
  uint8_t getCommand();
  uint8_t* getCommandData();
  size_t getCommandDataLength();
  
  void startFileTransfer(String filename = "");
  void writeFileData(const uint8_t* data, size_t len);
  void endFileTransfer();
  void cancelFileTransfer();
  void sendCurrentPlaying();
  void replyFileList(uint8_t registerNum = 0);
  void setActiveFile(uint8_t index);
  void update();
  void enableFileTransfer(bool enable);
  void createAudioFolders();
  void listRawFiles(const char* folder);
  void listAllAudioFiles();
  void deleteFile(const char* filepath);
  void deleteFolder(const char* folderpath);
  void formatLittleFS();
  void setCurrentRegister(uint8_t reg);
  String getCurrentRegisterFolder();
  void sendStatus(uint8_t mode, uint8_t reg = 0, bool playing = false);
  void sendBLEResponse(String response);
  void sendOBD2Status();
  void sendOBD2SOH();
  void sendOBD2Temp();
  void sendOBD2Steering();
  void sendOBD2Power();
  void sendIMUStatus();
  
private:
  static NimBLECharacteristic* pCharacteristic;
  static NimBLECharacteristic* pFileCharacteristic;
  static uint8_t pendingCommand;
  static uint8_t commandData[512];
  static size_t commandDataLen;
  static bool commandReady;
  static BLEControl* instance;
  
  bool fileReceiving = false;
  size_t receivedBytes = 0;
  String currentFilename;
  String originalFilename;
  File tmpFile;
  uint8_t currentRegister = 1;
  
  class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer);
    void onDisconnect(NimBLEServer* pServer);
  };
  
  class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic);
  };
  
  class FileCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic);
  };
};

extern BLEControl ble;
