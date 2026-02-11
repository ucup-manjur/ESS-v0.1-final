#include "OBD2Control.h"

OBD2Control obd2;

OBD2Control::OBD2Control() {}

void OBD2Control::begin() {
  CAN.setPins(16, 17);
  
  if (!CAN.begin(500E3)) {
    Serial.println("❌ CAN bus initialization failed");
    connected = false;
    return;
  }
  
  Serial.println("✅ CAN bus initialized (RX=16, TX=17)");
  connected = true;
  // PID check will be done in task to avoid blocking
}

void OBD2Control::startTask() {
  if (obd2TaskHandle == nullptr) {
    xTaskCreatePinnedToCore(
      obd2TaskWrapper,
      "OBD2Task",
      4096,
      this,
      1,
      &obd2TaskHandle,
      1  // Core 1 to avoid conflict with ADC on Core 0
    );
    Serial.println("✅ OBD2 task started on Core 1");
  }
}

void OBD2Control::stopTask() {
  if (obd2TaskHandle != nullptr) {
    vTaskDelete(obd2TaskHandle);
    obd2TaskHandle = nullptr;
    Serial.println("⏹️ OBD2 task stopped");
  }
}

void OBD2Control::obd2TaskWrapper(void* pvParameters) {
  OBD2Control* instance = static_cast<OBD2Control*>(pvParameters);
  instance->obd2Task();
}

void OBD2Control::obd2Task() {
  // Wait for system to stabilize
  delay(1000);
  
  // Start connection retry process
  Serial.println("🔍 Starting OBD2 connection...");
  connectionRetryActive = true;
  connectionRetryStartTime = millis();
  lastConnectionRetry = 0;
  
  // Check PID support first
  Serial.println("👋 Checking PID supported...");
  sendTesterPresent();
  delay(500);
  
  // Auto-detect: Try HV EV first
  delay(1000);
  Serial.println("🔍 Auto-detecting vehicle type...");
  
  // Try HV EV Power
  requestBatteryVoltage();
  delay(200);
  requestBatteryCurrent();
  delay(200);
  
  if (connected) {
    // useHVMode = true;  // Uncomment to force HV mode for testing 
    useHVMode = false;    // Default to ICE for safety
    Serial.println("✅ HV EV detected - using Power mode");
    connectionRetryActive = false;  // Stop retry, connection successful
    // Start SOH request process
    Serial.println("🔍 Starting SOH request process...");
    sohRequestActive = true;
    sohRequestStartTime = millis();
    lastSohRequest = 0; // Force immediate first request
  } else {
    useHVMode = false;
    Serial.println("⚠️ No OBD2 response - starting retry process...");
    // Connection retry will continue in main loop
  }
  
  startupComplete = true;
  Serial.println("✅ OBD2 startup complete");
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Handle connection retry (continuous until connected or timeout)
    handleConnectionRetry();
    
    // Handle SOH requests (continuous until value received or timeout)
    handleSOHRequests();
    
    // Only request data if connected or in simulation mode
    if (connected || simulationMode) {
      // Request real-time data every 100ms
      if (currentTime - lastRealtimeRequest >= REALTIME_INTERVAL) {
        if (simulationMode) {
          // Simulation mode: generate fake data
          obd2_rpm = 1000 + (millis() / 100) % 6000;  // Simulate RPM 1000-7000
        } else if (useHVMode) {
          requestBatteryVoltage();
          delay(10);
          requestBatteryCurrent();
          delay(10);
          calculatePowerAndHP();
        } else {
          requestRPM();
        }
        
        // Print all OBD2 data in one line
        String modeDisplay = simulationMode ? "[SIM]" : "";
        String sohDisplay = (stateOfHealth == 255) ? "ERR" : 
                           (stateOfHealth == 0) ? "WAIT" : 
                           String(stateOfHealth) + "%";
        if (useHVMode) {
          Serial.printf("📊 OBD2%s: %.2fHP | SoH=%s | Temp=%d°C | Steering=%d°\n", 
                       modeDisplay.c_str(), batteryPowerHP, sohDisplay.c_str(), batteryTemp, steeringAngle);
        } else {
          Serial.printf("📊 OBD2%s: RPM=%d | SoH=%s | Temp=%d°C | Steering=%d°\n", 
                       modeDisplay.c_str(), obd2_rpm, sohDisplay.c_str(), batteryTemp, steeringAngle);
        }
        
        lastRealtimeRequest = currentTime;
      }
      
      // Request battery temp every 5s (only for HV EV)
      if (currentTime - lastBatteryTempRequest >= BATTERY_TEMP_INTERVAL && !simulationMode) {
        requestBatteryTemp();
        lastBatteryTempRequest = currentTime;
      }
      
      // Request steering angle every 1s
      if (currentTime - lastSteeringRequest >= STEERING_INTERVAL && !simulationMode) {
        requestSteeringAngle();
        lastSteeringRequest = currentTime;
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
    yield();
  }
}

void OBD2Control::requestBatteryVoltage() {
  CAN.beginPacket(CAN_BATTERY_VOLTAGE.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x44);
  CAN.write(0x04);  // Battery Voltage PID
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_BATTERY_VOLTAGE.response)) {
    Serial.printf("🔍 Voltage Raw: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x04) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      batteryVoltage = ((a * 256) + b) * 0.1;  // Convert to volts
      Serial.printf("✅ Voltage: %.1fV (a=%d, b=%d)\n", batteryVoltage, a, b);
    } else {
      Serial.println("⚠️ Voltage validation failed");
    }
  } else {
    Serial.println("⚠️ No voltage response");
  }
}

void OBD2Control::requestBatteryCurrent() {
  CAN.beginPacket(CAN_BATTERY_CURRENT.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x44);
  CAN.write(0x06);  // Battery Current PID
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_BATTERY_CURRENT.response)) {
    Serial.printf("🔍 Current Raw: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x06) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      batteryCurrent = (((a * 256) + b) * 0.1) - 2047;  // Convert to amps
      Serial.printf("✅ Current: %.1fA (a=%d, b=%d)\n", batteryCurrent, a, b);
    } else {
      Serial.println("⚠️ Current validation failed");
    }
  } else {
    Serial.println("⚠️ No current response");
  }
}

void OBD2Control::calculatePowerAndHP() {
  batteryPowerWatts = batteryVoltage * batteryCurrent;
  batteryPowerHP = batteryPowerWatts / 745.7;  // Convert watts to horsepower (1 HP = 745.7 W)
  hvBatteryPower = (int32_t)batteryPowerWatts;  // Keep old variable for compatibility
  Serial.printf("📊 Power: %.1fV × %.1fA = %.1fW = %.2fHP\n", 
               batteryVoltage, batteryCurrent, batteryPowerWatts, batteryPowerHP);
}

void OBD2Control::requestRPM() {
  // Serial.println("🔍 Requesting RPM...");
  
  // Only request RPM if PID supported check passed
  if (!pidSupported) {
    // Serial.println("⚠️ Skipping RPM - PID not supported");
    return;
  }
  
  CAN.beginPacket(CAN_RPM.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x42);
  CAN.write(0x03);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  // Serial.printf("📤 RPM request sent to 0x%03X\n", CAN_RPM.request);
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_RPM.response)) {
    // Serial.printf("🔍 RPM Raw data: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //              data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    
    if (data[1] == 0x62 && data[2] == 0x42 && data[3] == 0x03) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      // Serial.printf("🔍 RPM calc: a=%d, b=%d, raw=%d\n", a, b, (a * 256) + b);
      int32_t rpmRaw = ((a * 256) + b) - 32767;
      // Serial.printf("🔍 RPM formula: ((%d*256)+%d)-32767 = %d RPM\n", a, b, rpmRaw);
      obd2_rpm = constrain(rpmRaw, 0, MAX_RPM);
      // Serial.printf("✅ RPM: %d (constrained)\n", obd2_rpm);
    } else {
      // Serial.printf("⚠️ RPM validation failed: %02X %02X %02X\n", data[1], data[2], data[3]);
    }
  } else {
    // Serial.printf("⚠️ Failed to read RPM from 0x%03X\n", CAN_RPM.response);
  }
}

void OBD2Control::requestStateOfHealth() {
  Serial.println("🔍 Requesting SoH...");
  
  CAN.beginPacket(CAN_SOH.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x10);
  CAN.write(0x48);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  Serial.printf("📤 SoH request sent to 0x%03X\n", CAN_SOH.request);
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_SOH.response)) {
    Serial.printf("🔍 SoH Raw data: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x06) {
      // This is actually HV Power response, not SoH
      Serial.println("⚠️ Got HV Power response instead of SoH");
      uint8_t a = data[4];
      uint8_t b = data[5];
      uint16_t rawSoH = (a * 256) + b;
      Serial.printf("🔍 SoH calc: a=%d, b=%d, raw=%d\n", a, b, rawSoH);
      
      // Validate SoH range
      if (rawSoH > 0 && rawSoH <= 100) {
        stateOfHealth = rawSoH;
        sohRead = true;
        sohRequestActive = false; // Stop requesting
        Serial.printf("✅ Battery SoH: %d%% - Stopping requests\n", stateOfHealth);
      } else {
        Serial.printf("⚠️ SoH out of range: %d (expected 1-100%%)\n", rawSoH);
      }
    } else if (data[1] == 0x62 && data[2] == 0x10 && data[3] == 0x48) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      uint16_t rawSoH = (a * 256) + b;
      Serial.printf("🔍 SoH calc: a=%d, b=%d, raw=%d\n", a, b, rawSoH);
      
      // Validate SoH range and check for non-zero value
      if (rawSoH > 0 && rawSoH <= 100) {
        stateOfHealth = rawSoH;
        sohRead = true;
        sohRequestActive = false; // Stop requesting
        Serial.printf("✅ Battery SoH: %d%% - Stopping requests\n", stateOfHealth);
      } else {
        Serial.printf("⚠️ SoH out of range: %d (expected 1-100%%)\n", rawSoH);
      }
    } else {
      Serial.printf("⚠️ SoH validation failed: %02X %02X %02X\n", data[1], data[2], data[3]);
    }
  } else {
    Serial.printf("⚠️ Failed to read SoH from 0x%03X\n", CAN_SOH.response);
  }
}

void OBD2Control::handleSOHRequests() {
  if (!sohRequestActive) return;
  
  unsigned long currentTime = millis();
  
  // Check timeout
  if (currentTime - sohRequestStartTime >= SOH_REQUEST_TIMEOUT) {
    Serial.printf("⏰ SOH request timeout after %d seconds\n", SOH_REQUEST_TIMEOUT / 1000);
    sohRequestActive = false;
    stateOfHealth = 255; // Error indicator
    sohRead = false;
    return;
  }
  
  // Request SOH at intervals
  if (currentTime - lastSohRequest >= SOH_REQUEST_INTERVAL) {
    requestStateOfHealth();
    lastSohRequest = currentTime;
    
    unsigned long elapsed = (currentTime - sohRequestStartTime) / 1000;
    Serial.printf("🔄 SOH request #%d (elapsed: %ds)\n", 
                 (int)((currentTime - sohRequestStartTime) / SOH_REQUEST_INTERVAL) + 1, 
                 (int)elapsed);
  }
}

void OBD2Control::requestBatteryTemp() {
  // Serial.println("🔍 Requesting Battery Temp...");
  
  CAN.beginPacket(CAN_BATTERY_TEMP.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x44);
  CAN.write(0x0E);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  // Serial.printf("📤 Battery Temp request sent to 0x%03X\n", CAN_BATTERY_TEMP.request);
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_BATTERY_TEMP.response)) {
    // Serial.printf("🔍 Battery Temp Raw data: %02X %02X %02X %02X %02X %02X %02X %02X\n", 
    //              data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x0E) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      // Serial.printf("🔍 Temp calc: a=%d, b=%d, raw=%d\n", a, b, (a * 256) + b);
      int32_t tempRaw = (((a * 256) + b) * 0.5) - 50;
      // Serial.printf("🔍 Temp formula: (((%d*256)+%d)*0.5)-50 = %d°C\n", a, b, tempRaw);
      batteryTemp = tempRaw;
      // Serial.printf("✅ Battery Temp: %d°C\n", batteryTemp);
    } else {
      // Serial.printf("⚠️ Battery Temp validation failed: %02X %02X %02X\n", data[1], data[2], data[3]);
    }
  } else {
    // Serial.printf("⚠️ Failed to read Battery Temp from 0x%03X\n", CAN_BATTERY_TEMP.response);
  }
}

void OBD2Control::requestSteeringAngle() {
  CAN.beginPacket(CAN_STEERING.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x30);
  CAN.write(0x0C);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_STEERING.response)) {
    if (data[1] == 0x62 && data[2] == 0x30 && data[3] == 0x0C) {
      int16_t angleRaw = (data[4] << 8) | data[5];
      steeringAngle = angleRaw / 10;
      steeringAngle = constrain(steeringAngle, -720, 720);
    }
  }
}

bool OBD2Control::readCANResponse(uint8_t* data, size_t maxLen, uint16_t expectedResponseId) {
  unsigned long timeout = millis() + 50;
  
  while (millis() < timeout) {
    int packetSize = CAN.parsePacket();
    if (packetSize && CAN.packetId() == expectedResponseId) {
      size_t readLen = min((size_t)packetSize, maxLen);
      for (size_t i = 0; i < readLen; i++) {
        data[i] = CAN.read();
      }
      connected = true;
      return true;
    }
    delayMicroseconds(500);
  }
  
  connected = false;
  return false;
}

void OBD2Control::sendTesterPresent() {
  Serial.println("👋 Checking PID supported...");
  
  // Send PID Supported query (0x01 0x00)
  CAN.beginPacket(0x7DF);
  CAN.write(0x02);  // Length
  CAN.write(0x01);  // Show current data
  CAN.write(0x00);  // PID 00 - Supported PIDs 01-20
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  // Wait for response: 7E8 06 41 00 98 18 00 01 00
  uint8_t data[8];
  if (readCANResponse(data, 8, 0x7E8)) {
    if (data[1] == 0x41 && data[2] == 0x00) {
      Serial.printf("✅ PID Supported: %02X %02X %02X %02X\n", 
                   data[3], data[4], data[5], data[6]);
      pidSupported = true;
    }
  } else {
    Serial.println("⚠️ No PID supported response");
    pidSupported = false;
  }
  
  Serial.println("✅ PID check complete");
}

void OBD2Control::refreshStateOfHealth() {
  Serial.println("📱 App requested SoH refresh");
  // Restart SOH request process
  sohRequestActive = true;
  sohRequestStartTime = millis();
  lastSohRequest = 0; // Force immediate request
  stateOfHealth = 0;  // Reset to 0
  sohRead = false;
  Serial.println("🔄 SOH request process restarted");
}

void OBD2Control::handleConnectionRetry() {
  if (!connectionRetryActive) return;
  
  unsigned long currentTime = millis();
  
  // Check timeout - enter simulation mode after 1 minute
  if (currentTime - connectionRetryStartTime >= CONNECTION_RETRY_TIMEOUT) {
    Serial.printf("⏰ OBD2 connection timeout after %d seconds\n", CONNECTION_RETRY_TIMEOUT / 1000);
    connectionRetryActive = false;
    enterSimulationMode();
    return;
  }
  
  // Retry connection at intervals
  if (currentTime - lastConnectionRetry >= CONNECTION_RETRY_INTERVAL) {
    Serial.println("🔄 Retrying OBD2 connection...");
    
    // Try PID check
    sendTesterPresent();
    delay(200);
    
    // Try data request
    if (useHVMode) {
      requestBatteryVoltage();
      delay(100);
    } else {
      requestRPM();
      delay(100);
    }
    
    if (connected) {
      Serial.println("✅ OBD2 connected successfully!");
      connectionRetryActive = false;
      // Start SOH request
      sohRequestActive = true;
      sohRequestStartTime = millis();
      lastSohRequest = 0;
    } else {
      unsigned long elapsed = (currentTime - connectionRetryStartTime) / 1000;
      Serial.printf("⚠️ OBD2 not responding (elapsed: %ds/%ds)\n", 
                   (int)elapsed, CONNECTION_RETRY_TIMEOUT / 1000);
    }
    
    lastConnectionRetry = currentTime;
  }
}

void OBD2Control::enterSimulationMode() {
  Serial.println("🎮 Entering SIMULATION MODE - OBD2 not detected");
  Serial.println("📡 System will use simulated RPM data");
  Serial.println("🔌 You can still use potentiometer/BLE for control");
  
  simulationMode = true;
  connected = false;
  useHVMode = false;
  
  // Set default values
  obd2_rpm = 1000;
  stateOfHealth = 0;  // Unknown
  batteryTemp = 25;   // Default temp
  steeringAngle = 0;  // Straight
  
  Serial.println("✅ Simulation mode active - system operational");
}