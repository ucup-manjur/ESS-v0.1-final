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
  // Auto-detect: Try HV EV first
  delay(2000);
  Serial.println("🔍 Auto-detecting vehicle type...");
  
  // Try HV EV Power
  requestHVBatteryPower();
  delay(200);
  
  if (connected) {
    useHVMode = true;
    Serial.println("✅ HV EV detected - using Power mode");
    // Read SoH once at startup for HV EV
    requestStateOfHealth();
    delay(200);
  } else {
    useHVMode = false;
    Serial.println("✅ ICE detected - using RPM mode");
  }
  
  startupComplete = true;
  Serial.println("✅ OBD2 startup complete");
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Request real-time data every 100ms
    if (currentTime - lastRealtimeRequest >= REALTIME_INTERVAL) {
      if (useHVMode) {
        requestHVBatteryPower();
      } else {
        requestRPM();
      }
      
      // Print all OBD2 data in one line
      Serial.printf("📊 OBD2: %s=%d | SoH=%d%% | Temp=%d°C | Steering=%d°\n", 
                   useHVMode ? "Power" : "RPM", 
                   useHVMode ? hvBatteryPower : obd2_rpm,
                   stateOfHealth, batteryTemp, steeringAngle);
      
      lastRealtimeRequest = currentTime;
    }
    
    // Request battery temp every 5s (only for HV EV)
    if (useHVMode && currentTime - lastBatteryTempRequest >= BATTERY_TEMP_INTERVAL) {
      requestBatteryTemp();
      lastBatteryTempRequest = currentTime;
    }
    
    // Request steering angle every 1s
    if (currentTime - lastSteeringRequest >= STEERING_INTERVAL) {
      requestSteeringAngle();
      lastSteeringRequest = currentTime;
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
    yield();
  }
}

void OBD2Control::requestHVBatteryPower() {
  CAN.beginPacket(CAN_HVEV.request);
  CAN.write(0x03);
  CAN.write(0x22);
  CAN.write(0x44);
  CAN.write(0x06);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.write(0x00);
  CAN.endPacket();
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_HVEV.response)) {
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x06) {
      int16_t powerRaw = (data[4] << 8) | data[5];
      hvBatteryPower = powerRaw;
    }
  }
}

void OBD2Control::requestRPM() {
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
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_RPM.response)) {
    if (data[1] == 0x62 && data[2] == 0x42 && data[3] == 0x03) {
      uint16_t rpm = (data[4] << 8) | data[5];
      obd2_rpm = constrain(rpm, 0, MAX_RPM);
    }
  }
}

void OBD2Control::requestStateOfHealth() {
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
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_SOH.response)) {
    stateOfHealth = data[4];
    stateOfHealth = constrain(stateOfHealth, 0, 100);
    sohRead = true;
    Serial.printf("🔋 Battery SoH: %d%%\n", stateOfHealth);
  } else {
    Serial.println("⚠️ Failed to read SoH");
  }
}

void OBD2Control::requestBatteryTemp() {
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
  
  uint8_t data[8];
  if (readCANResponse(data, 8, CAN_BATTERY_TEMP.response)) {
    if (data[1] == 0x62 && data[2] == 0x44 && data[3] == 0x0E) {
      uint8_t a = data[4];
      uint8_t b = data[5];
      batteryTemp = (int8_t)(((a * 256) + b) * 0.5) - 50;
      batteryTemp = constrain(batteryTemp, -40, 100);
    }
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