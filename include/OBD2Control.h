/*
 * ============================================================================
 * OBD2Control.h - OBD2 CAN Bus Integration
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk integrasi dengan OBD2 CAN bus kendaraan. Membaca data real-time
 * seperti RPM, HV Battery Power, State of Health, Battery Temperature, dan
 * Steering Angle dari ECU kendaraan.
 * 
 * FITUR UTAMA:
 * 1. Multi-Data Support:
 *    - RPM (untuk ICE vehicles)
 *    - HV Battery Power (untuk EV vehicles)
 *    - State of Health (SOH) - battery health
 *    - Battery Temperature
 *    - Steering Angle
 * 
 * 2. Connection Retry System:
 *    - Auto-retry setiap 2 detik
 *    - Timeout 60 detik (1 menit)
 *    - Fallback ke simulation mode jika gagal
 * 
 * 3. SOH Request System:
 *    - Continuous request sampai nilai valid diterima
 *    - Timeout 30 detik
 *    - Configurable interval (500ms)
 * 
 * 4. Simulation Mode:
 *    - Auto-enable jika OBD2 tidak terdeteksi
 *    - Generate simulated RPM data
 *    - System tetap operational
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    obd2.begin();        // Initialize CAN bus
 *    obd2.startTask();    // Start OBD2 task di Core 1
 *    ```
 * 
 * 2. Baca Data:
 *    ```cpp
 *    uint16_t rpm = obd2.getRPM();
 *    int32_t power = obd2.getHVBatteryPower();
 *    uint16_t soh = obd2.getStateOfHealth();
 *    int8_t temp = obd2.getBatteryTemp();
 *    int16_t steering = obd2.getSteeringAngle();
 *    ```
 * 
 * 3. Cek Status:
 *    ```cpp
 *    if (obd2.isConnected()) {
 *        // OBD2 connected
 *    }
 *    if (obd2.isSimulationMode()) {
 *        // Running in simulation mode
 *    }
 *    ```
 * 
 * 4. Refresh SOH:
 *    ```cpp
 *    obd2.refreshStateOfHealth();  // Restart SOH request
 *    ```
 * 
 * CAN BUS CONFIGURATION:
 * - RX Pin: GPIO 16
 * - TX Pin: GPIO 17
 * - Speed: 500 kbps (standard automotive)
 * - Protocol: ISO 15765-4 (CAN)
 * 
 * CAN ID MAPPING:
 * - RPM: Request 0x7E3, Response 0x7EB
 * - HV Power: Request 0x7E5, Response 0x7ED
 * - SOH: Request 0x7E5, Response 0x7ED
 * - Battery Temp: Request 0x7E5, Response 0x7ED
 * - Steering: Request 0x720, Response 0x730
 * 
 * TIMING CONFIGURATION:
 * - Real-time data (RPM/Power): 100ms interval
 * - Battery temperature: 5s interval
 * - Steering angle: 1s interval
 * - SOH request: 500ms interval, 30s timeout
 * - Connection retry: 2s interval, 60s timeout
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>
#include <CAN.h>

// Configuration - pilih salah satu untuk real-time input
// #define USE_HV_BATTERY_POWER  // Comment this to use RPM instead
// #define USE_RPM_INPUT

#define MAX_RPM 8000
#define MAX_HV_POWER 200000  // 200kW max

// OBD2 CAN IDs - different for each data type
// CAN ID mapping untuk berbagai data type
struct CANIDs {
  uint16_t request;
  uint16_t response;
};

const CANIDs CAN_RPM = {0x7E3, 0x7EB};
const CANIDs CAN_HVEV = {0x7E5, 0x7ED};
const CANIDs CAN_BATTERY_VOLTAGE = {0x7E5, 0x7ED};
const CANIDs CAN_BATTERY_CURRENT = {0x7E5, 0x7ED};
const CANIDs CAN_SOH = {0x7E5, 0x7ED};
const CANIDs CAN_BATTERY_TEMP = {0x7E5, 0x7ED};
const CANIDs CAN_STEERING = {0x720, 0x730};

class OBD2Control {
public:
  OBD2Control();
  void begin();
  void startTask();
  void stopTask();
  
  // Real-time data (updated frequently ~100ms)
  int32_t getHVBatteryPower() { return hvBatteryPower; }  // Watts (can be negative)
  uint16_t getRPM() { return obd2_rpm; }
  bool isUsingHVMode() { return useHVMode; }
  bool isConnected() { return connected; }
  bool isSimulationMode() { return simulationMode; }  // Check if in simulation mode
  
  // One-time data (read at startup)
  uint16_t getStateOfHealth() { return stateOfHealth; }  // Raw value
  bool isSOHRead() { return sohRead; }
  
  // Medium frequency data (~5s interval)
  int8_t getBatteryTemp() { return batteryTemp; }      // Celsius
  
  // High frequency data (~1s interval)
  int16_t getSteeringAngle() { return steeringAngle; }  // Degrees
  
  // Task control
  static void obd2TaskWrapper(void* pvParameters);
  
private:
  // Real-time variables
  volatile int32_t hvBatteryPower = 0;
  volatile uint16_t obd2_rpm = 1000;
  volatile bool connected = false;
  volatile bool useHVMode = false;
  volatile bool pidSupported = false;
  
  // HV Battery data
  volatile float batteryVoltage = 0.0;  // Volts
  volatile float batteryCurrent = 0.0;  // Amps
  volatile float batteryPowerWatts = 0.0;  // Watts
  volatile float batteryPowerHP = 0.0;     // Horsepower
  
  // One-time variables
  uint16_t stateOfHealth = 0;
  bool sohRead = false;
  
  // SOH request control
  bool sohRequestActive = false;
  unsigned long sohRequestStartTime = 0;
  unsigned long lastSohRequest = 0;
  const unsigned long SOH_REQUEST_INTERVAL = 500;  // 500ms between requests
  const unsigned long SOH_REQUEST_TIMEOUT = 30000; // 30s timeout
  
  // OBD2 connection retry control
  bool connectionRetryActive = false;
  unsigned long connectionRetryStartTime = 0;
  unsigned long lastConnectionRetry = 0;
  const unsigned long CONNECTION_RETRY_INTERVAL = 2000;  // 2s between retries
  const unsigned long CONNECTION_RETRY_TIMEOUT = 60000;  // 60s timeout (1 minute)
  bool simulationMode = false;  // Fallback to simulation if OBD not detected
  
  // Medium/High frequency variables
  int8_t batteryTemp = 25;        // Default 25°C
  int16_t steeringAngle = 0;      // Default straight
  
  // Task handle
  TaskHandle_t obd2TaskHandle = nullptr;
  
  // Internal methods
  void obd2Task();
  void requestHVBatteryPower();
  void requestBatteryVoltage();
  void requestBatteryCurrent();
  void requestRPM();
  void requestStateOfHealth();
  void requestBatteryTemp();
  void requestSteeringAngle();
  bool readCANResponse(uint8_t* data, size_t maxLen, uint16_t expectedResponseId);
  void sendTesterPresent();
  void calculatePowerAndHP();
  void handleSOHRequests(); // New method for SOH request logic
  void handleConnectionRetry(); // New method for connection retry logic
  void enterSimulationMode(); // Fallback to simulation mode
  
public:
  // Method untuk request SoH dari aplikasi
  void refreshStateOfHealth();
  
  // Timing control
  unsigned long lastRealtimeRequest = 0;
  unsigned long lastBatteryTempRequest = 0;
  unsigned long lastSteeringRequest = 0;
  bool startupComplete = false;
  
  const unsigned long REALTIME_INTERVAL = 100;     // 100ms for power/rpm
  const unsigned long BATTERY_TEMP_INTERVAL = 5000; // 5s for battery temp
  const unsigned long STEERING_INTERVAL = 1000;     // 1s for steering
};

extern OBD2Control obd2;