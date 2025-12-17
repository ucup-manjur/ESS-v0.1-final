#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "AudioPlayer.h"
#include "SystemManager.h"

AudioPlayer player;
SystemManager sysManager;

void setup() {
  Serial.begin(115200);
  delay(500);

  uint8_t retries = 3;
  while (!LittleFS.begin(true) && retries > 0) {
    // amazonq-ignore-next-line
    Serial.printf("❌ LittleFS gagal, retry %d/3...\n", 4 - retries);
    delay(1000);
    retries--;
  }
  
  if (retries == 0) {
    Serial.println("❌ LittleFS gagal setelah 3x retry, restart ESP...");
    delay(2000);
    ESP.restart();
  }
  Serial.println("✅ LittleFS OK");

  player.begin();
  sysManager.begin(&player);
}

void loop() {
  sysManager.update();
  
  static unsigned long lastUpdate = 0;
  static unsigned long lastRPMLog = 0;
  static int lastRaw = 0;
  unsigned long now = millis();
  
  // Throttle input update
  if (now - lastUpdate >= 50) {
    int raw = analogRead(THROTTLE_ADC_PIN);
    
    if (abs(raw - lastRaw) > 50) {
      player.updateSampleRateFromADC(raw);
      lastRaw = raw;
    }
    
    lastUpdate = now;
  }
  
  // RPM logging only when sample rate changes
  static uint32_t lastRPM = 0;
  uint32_t currentSampleRate = player.getSampleRate();
  uint32_t currentRPM = map(currentSampleRate, 8000, 44100, 1000, 18000);
  
  if (currentRPM != lastRPM) {
    Serial.printf("📊 RPM: %d | Rate: %d Hz\n", currentRPM, currentSampleRate);
    lastRPM = currentRPM;
  }
}

