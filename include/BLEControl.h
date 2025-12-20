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

// Command definitions untuk file transfer audio
#define CMD_FILE_START       0x20
#define CMD_FILE_DATA        0x21
#define CMD_FILE_END         0x22
#define CMD_DELETE_FILE      0x23
#define CMD_DELETE_FOLDER    0x24

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
