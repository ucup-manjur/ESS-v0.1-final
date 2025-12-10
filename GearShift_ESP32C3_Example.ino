// Contoh kode untuk ESP32C3 Gear Shift Controller
// Upload ke ESP32C3 yang terhubung dengan gear shift

#include <NimBLEDevice.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-0987654321ab"

#define CMD_GEAR_UP          0x01
#define CMD_GEAR_DOWN        0x02
#define CMD_TOGGLE_PLAY      0x03
#define CMD_MUTE             0x04

#define GEAR_UP_PIN     2
#define GEAR_DOWN_PIN   3
#define PLAY_BTN_PIN    4

NimBLEClient* pClient = nullptr;
NimBLERemoteCharacteristic* pCharacteristic = nullptr;
bool connected = false;

void setup() {
  Serial.begin(115200);
  
  pinMode(GEAR_UP_PIN, INPUT_PULLUP);
  pinMode(GEAR_DOWN_PIN, INPUT_PULLUP);
  pinMode(PLAY_BTN_PIN, INPUT_PULLUP);
  
  NimBLEDevice::init("GearShift");
  
  Serial.println("Scanning for ESS-Audio...");
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setActiveScan(true);
  NimBLEScanResults results = pScan->start(5);
  
  for(int i = 0; i < results.getCount(); i++) {
    NimBLEAdvertisedDevice device = results.getDevice(i);
    if(device.getName() == "ESS-Audio") {
      Serial.println("Found ESS-Audio!");
      connectToServer(device.getAddress());
      break;
    }
  }
}

void connectToServer(NimBLEAddress address) {
  pClient = NimBLEDevice::createClient();
  
  if(pClient->connect(address)) {
    Serial.println("Connected!");
    
    NimBLERemoteService* pService = pClient->getService(SERVICE_UUID);
    if(pService) {
      pCharacteristic = pService->getCharacteristic(CHARACTERISTIC_UUID);
      if(pCharacteristic) {
        connected = true;
        Serial.println("Ready to send commands");
      }
    }
  }
}

void sendCommand(uint8_t cmd) {
  if(connected && pCharacteristic) {
    pCharacteristic->writeValue(&cmd, 1);
    Serial.printf("Sent: 0x%02X\n", cmd);
  }
}

void loop() {
  static bool lastGearUp = HIGH;
  static bool lastGearDown = HIGH;
  static bool lastPlay = HIGH;
  
  bool gearUp = digitalRead(GEAR_UP_PIN);
  bool gearDown = digitalRead(GEAR_DOWN_PIN);
  bool play = digitalRead(PLAY_BTN_PIN);
  
  if(gearUp == LOW && lastGearUp == HIGH) {
    sendCommand(CMD_GEAR_UP);
    delay(200);
  }
  
  if(gearDown == LOW && lastGearDown == HIGH) {
    sendCommand(CMD_GEAR_DOWN);
    delay(200);
  }
  
  if(play == LOW && lastPlay == HIGH) {
    sendCommand(CMD_TOGGLE_PLAY);
    delay(200);
  }
  
  lastGearUp = gearUp;
  lastGearDown = gearDown;
  lastPlay = play;
  
  if(!connected && millis() % 5000 == 0) {
    Serial.println("Reconnecting...");
    ESP.restart();
  }
}
