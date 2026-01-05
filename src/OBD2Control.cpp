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
  // Read SoH once at startup
  delay(2000); // Wait for CAN bus to stabilize
  requestStateOfHealth();
  startupComplete = true;
  Serial.println("✅ OBD2 startup complete");
  
  while (true) {
    unsigned long currentTime = millis();
    
    // Request real-time data every 100ms
    if (currentTime - lastRealtimeRequest >= REALTIME_INTERVAL) {
#ifdef USE_HV_BATTERY_POWER
      requestHVBatteryPower();
#else
      requestRPM();
#endif
      lastRealtimeRequest = currentTime;
    }
    
    // Request steering angle every 1s
    if (currentTime - lastSteeringRequest >= STEERING_INTERVAL) {
      requestSteeringAngle();
      lastSteeringRequest = currentTime;
    }
    
    // Request battery temp every 5s
    if (currentTime - lastBatteryTempRequest >= BATTERY_TEMP_INTERVAL) {
      requestBatteryTemp();
      lastBatteryTempRequest = currentTime;
    }
    
    // Give other tasks a chance
    vTaskDelay(pdMS_TO_TICKS(50));
    yield();
  }
}

#ifdef USE_HV_BATTERY_POWER
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
    int16_t powerRaw = (data[4] << 8) | data[5];
    hvBatteryPower = abs(powerRaw);
    hvBatteryPower = constrain(hvBatteryPower, 0, MAX_HV_POWER);
  }
}
#else
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
    if (data[1] == 0x41 && data[2] == 0x0C) {
      uint16_t rpm = ((data[3] * 256) + data[4]) / 4;
      obd2_rpm = constrain(rpm, 0, MAX_RPM);
    }
  }
}
#endif

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
    batteryTemp = (int8_t)data[4] - 40;
    batteryTemp = constrain(batteryTemp, -40, 100);
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
    int16_t angleRaw = (data[4] << 8) | data[5];
    steeringAngle = angleRaw / 10;
    steeringAngle = constrain(steeringAngle, -720, 720);
  }
}

bool OBD2Control::readCANResponse(uint8_t* data, size_t maxLen, uint16_t expectedResponseId) {
  unsigned long timeout = millis() + 10;
  
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
    delayMicroseconds(100);
  }
  
  connected = false;
  return false;
}