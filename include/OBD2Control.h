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