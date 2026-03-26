#include "IMUControl.h"
#include <math.h>

IMUControl imuControl;

bool IMUControl::begin(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);

  int status = icm.begin();
  if (status < 0) {
    Serial.printf("❌ IMU ICM-20948 tidak ditemukan! Status: %d\n", status);
    return false;
  }

  // Config accel: range ±2G, DLPF 111Hz
  icm.configAccel(ICM20948::ACCEL_RANGE_2G, ICM20948::ACCEL_DLPF_BANDWIDTH_111HZ);

  imuReady = true;
  Serial.println("✅ IMU ICM-20948 ready");
  return true;
}

void IMUControl::update() {
  if (!enabled || !imuReady) return;

  unsigned long now = millis();
  if (now - lastUpdate < UPDATE_INTERVAL) return;
  lastUpdate = now;

  // pitch sudah dikurangi offset kalibrasi
  pitchDeg = readRawPitch() - pitchOffset;

  if (pitchDeg > uphillThreshold) {
    condition = SLOPE_UPHILL;
  } else if (pitchDeg < -downhillThreshold) {
    condition = SLOPE_DOWNHILL;
  } else {
    condition = SLOPE_FLAT;
  }
}

float IMUControl::readRawPitch() {
  icm.readSensor();
  float ax = icm.getAccelX_mss() / 9.807f;
  float ay = icm.getAccelY_mss() / 9.807f;
  float az = icm.getAccelZ_mss() / 9.807f;
  return atan2(ax, sqrt(ay * ay + az * az)) * 180.0f / M_PI;
}

void IMUControl::calibrate() {
  if (!imuReady) {
    Serial.println("⚠️ IMU belum siap, kalibrasi dibatalkan");
    return;
  }
  // Rata-rata 10 sampel untuk hasil lebih stabil
  float sum = 0.0f;
  for (int i = 0; i < 10; i++) {
    sum += readRawPitch();
    delay(20);
  }
  pitchOffset = sum / 10.0f;
  Serial.printf("✅ IMU kalibrasi selesai, offset = %.2f°\n", pitchOffset);
}

float IMUControl::getSampleRateModifier() {
  if (!enabled) return 1.0f;
  switch (condition) {
    case SLOPE_UPHILL:   return 0.88f; // Engine lebih berat, RPM turun ~12%
    case SLOPE_DOWNHILL: return 1.12f; // Engine lebih ringan, RPM naik ~12%
    default:             return 1.0f;
  }
}
