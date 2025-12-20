#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "AudioPlayer.h"
#include "SystemManager.h"

AudioPlayer player;
SystemManager sysManager;

// Task handles
TaskHandle_t ADCTaskHandle = NULL;
TaskHandle_t BLETaskHandle = NULL;

// Forward declarations
void ADCTask(void* parameter);
void BLETask(void* parameter);

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
  
  // Create tasks on different cores
  xTaskCreatePinnedToCore(
    ADCTask,           // Task function
    "ADC_Task",        // Task name
    4096,              // Stack size
    NULL,              // Parameters
    2,                 // Priority (higher = more priority)
    &ADCTaskHandle,    // Task handle
    0                  // Core 0
  );
  
  xTaskCreatePinnedToCore(
    BLETask,           // Task function
    "BLE_Task",        // Task name
    8192,              // Stack size (larger for BLE)
    NULL,              // Parameters
    1,                 // Priority (lower than ADC)
    &BLETaskHandle,    // Task handle
    1                  // Core 1
  );
  
  Serial.println("✅ Dual core tasks started");
  Serial.println("   ADC Task -> Core 0 (Priority 2)");
  Serial.println("   BLE Task -> Core 1 (Priority 1)");
}

// ADC Task - Core 0
void ADCTask(void* parameter) {
  static unsigned long lastUpdate = 0;
  static int lastRaw = 0;
  static int smoothedRaw = 0;
  
  for(;;) {
    unsigned long now = millis();
    
    // Throttle input update every 30ms for smoother response
    if (now - lastUpdate >= 30) {
      int raw = analogRead(THROTTLE_ADC_PIN);
      
      // Smooth the ADC reading with slope limiting
      int diff = raw - smoothedRaw;
      if (abs(diff) > 50) {
        // Limit big jumps - apply slope
        smoothedRaw += (diff > 0) ? 50 : -50;
      } else {
        smoothedRaw = raw;
      }
      
      // Debug ADC values with smaller threshold
      if (abs(smoothedRaw - lastRaw) > 10) {
        uint32_t rate = map(smoothedRaw, 0, 4095, 8000, 44100);
        Serial.printf("🎯 ADC: %d (smooth: %d) -> Rate: %d Hz\n", raw, smoothedRaw, rate);
        lastRaw = smoothedRaw;
      }
      
      // Always update throttle rate for rev system
      uint32_t throttleRate = map(smoothedRaw, 0, 4095, 8000, 44100);
      sysManager.setCurrentThrottleRate(throttleRate);
      
      // Direct player control - but not during rev or shift
      if (!sysManager.isRevActive() && !sysManager.isShiftActive()) {
        player.updateSampleRateFromADC(smoothedRaw);
      }
      lastUpdate = now;
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);  // 10ms delay
  }
}

// BLE Task - Core 1
void BLETask(void* parameter) {
  for(;;) {
    sysManager.update();
    vTaskDelay(1 / portTICK_PERIOD_MS);  // 1ms delay for fastest response
  }
}

void loop() {
  // Main loop now just handles basic system tasks
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

