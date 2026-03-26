#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "ICM20948.h"

enum SlopeCondition {
  SLOPE_FLAT     = 0,
  SLOPE_UPHILL   = 1,
  SLOPE_DOWNHILL = 2
};

class IMUControl {
public:
  bool begin(uint8_t sda = 21, uint8_t scl = 22);
  void update();

  SlopeCondition getCondition()      { return condition; }
  float          getPitchDeg()       { return pitchDeg; }
  bool           isEnabled()         { return enabled; }
  void           setEnabled(bool en) { enabled = en; }

  // Modifier ke sample rate: 0.88 (menanjak) ~ 1.12 (menurun), 1.0 (datar)
  float getSampleRateModifier();

  // Kalibrasi: simpan pitch saat posisi datar sebagai offset
  void calibrate();           // Panggil saat kendaraan datar
  float getCalibOffset()      { return pitchOffset; }

  // Threshold konfigurasi (derajat)
  float uphillThreshold   = 5.0f;
  float downhillThreshold = 5.0f;

private:
  ICM20948 icm{Wire, 0x69}; // AD0 HIGH = 0x69, AD0 LOW = 0x68
  bool  enabled     = false;
  bool  imuReady    = false;
  float pitchDeg    = 0.0f;  // pitch mentah
  float pitchOffset = 0.0f;  // offset kalibrasi
  SlopeCondition condition = SLOPE_FLAT;

  unsigned long lastUpdate = 0;
  const unsigned long UPDATE_INTERVAL = 200; // ms

  float readRawPitch(); // baca pitch tanpa offset
};

extern IMUControl imuControl;
