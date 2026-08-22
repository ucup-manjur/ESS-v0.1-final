#include "IMUControl.h"
#include <math.h>

IMUControl imuControl;

bool IMUControl::begin(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl);

  int status = icm.begin();
  if (status < 0) {
    IMU_LOG("ICM-20948 tidak ditemukan! Status: %d", status);
    return false;
  }

  // Config accel: range ±2G, DLPF 111Hz
  icm.configAccel(ICM20948::ACCEL_RANGE_2G, ICM20948::ACCEL_DLPF_BANDWIDTH_111HZ);

  imuReady = true;
  IMU_LOG("ICM-20948 ready");
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
    IMU_LOG("UPHILL   pitch=%.2f offset=%.2f", pitchDeg, pitchOffset);
  } else if (pitchDeg < -downhillThreshold) {
    condition = SLOPE_DOWNHILL;
    IMU_LOG("DOWNHILL pitch=%.2f offset=%.2f", pitchDeg, pitchOffset);
  } else {
    condition = SLOPE_FLAT;
    IMU_LOG("FLAT     pitch=%.2f offset=%.2f", pitchDeg, pitchOffset);
  }
}

float IMUControl::readRawPitch() {
  icm.readSensor();
  float ax = icm.getAccelX_mss() / 9.807f;
  float ay = icm.getAccelY_mss() / 9.807f;
  float az = icm.getAccelZ_mss() / 9.807f;
  // Pakai ay sebagai axis utama (roll) karena IMU dipasang menyamping
  return atan2(ay, sqrt(ax * ax + az * az)) * 180.0f / M_PI;
}

void IMUControl::calibrate() {
  if (!imuReady) {
    IMU_LOG("belum siap, kalibrasi dibatalkan");
    return;
  }
  // Rata-rata 10 sampel untuk hasil lebih stabil
  float sum = 0.0f;
  for (int i = 0; i < 10; i++) {
    sum += readRawPitch();
    delay(20);
  }
  pitchOffset = sum / 10.0f;
  IMU_LOG("kalibrasi selesai, offset=%.2f", pitchOffset);
}

float IMUControl::getSampleRateModifier() {
  if (!enabled) return 1.0f;
  switch (condition) {
    case SLOPE_UPHILL:   return 0.78f; // Engine lebih berat, RPM turun ~12%
    case SLOPE_DOWNHILL: return 1.12f; // Engine lebih ringan, RPM naik ~12%
    default:             return 1.0f;
  }
}
