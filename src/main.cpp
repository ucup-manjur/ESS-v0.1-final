#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "AudioPlayer.h"
#include "SystemManager.h"
#include "OBD2Control.h"

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
  
  // Initialize OBD2 system
  // obd2.begin();
  // obd2.startTask();
  // Serial.println("✅ OBD2 system initialized");
  
  // Create tasks with adjusted priorities
  xTaskCreatePinnedToCore(
    ADCTask,           // Task function
    "ADC_Task",        // Task name
    4096,              // Stack size
    NULL,              // Parameters
    1,                 // Priority (reduced from 2)
    &ADCTaskHandle,    // Task handle
    0                  // Core 0
  );
  
  xTaskCreatePinnedToCore(
    BLETask,           // Task function
    "BLE_Task",        // Task name
    8192,              // Stack size (larger for BLE)
    NULL,              // Parameters
    2,                 // Priority (increased from 1)
    &BLETaskHandle,    // Task handle
    1                  // Core 1
  );
  
  Serial.println("✅ Dual core tasks started");
  Serial.println("   ADC Task -> Core 0 (Priority 1)");
  Serial.println("   BLE Task -> Core 1 (Priority 2)");
}

// ADC + Button Task - Core 0
void ADCTask(void* parameter) {
  static unsigned long lastUpdate = 0;
  static int lastRaw = 0;
  static int smoothedRaw = 0;
  
  // Configurable ADC slope limiting
  static int adcSlopeLimit = 200;
  
  for(;;) {
    unsigned long now = millis();
    
    // Handle buttons first (higher priority)
    sysManager.updateButtons();
    
    // Throttle input update every 30ms for smoother response
    if (now - lastUpdate >= 30) {
      int raw = analogRead(THROTTLE_ADC_PIN);
      
      // Smooth the ADC reading with configurable slope limiting
      int diff = raw - smoothedRaw;
      if (abs(diff) > adcSlopeLimit) {
        // Limit big jumps - apply slope
        smoothedRaw += (diff > 0) ? adcSlopeLimit : -adcSlopeLimit;
      } else {
        smoothedRaw = raw;
      }
      
      // Debug ADC values with smaller threshold
      if (abs(smoothedRaw - lastRaw) > 10) {
        uint32_t rate = map(smoothedRaw, 0, 4095, 8000, 44100);
        Serial.printf("🎯 ADC: %d (smooth: %d) -> Rate: %d Hz\n", raw, smoothedRaw, rate);
        lastRaw = smoothedRaw;
      }
      
      // Use ADC as throttle input - back to 44.1kHz
      uint32_t throttleRate = map(smoothedRaw, 0, 4095, 8000, 44100);
      sysManager.setCurrentThrottleRate(throttleRate);
      if (!sysManager.isRevActive() && !sysManager.isShiftActive()) {
        player.updateSampleRateFromADC(smoothedRaw);
      }
      lastUpdate = now;
    }
    
    vTaskDelay(5 / portTICK_PERIOD_MS);  // Reduced delay for better button response
  }
}

void BLETask(void* parameter) {
  for(;;) {
    // Only handle BLE and LED updates
    sysManager.updateBLE();
    sysManager.updateLEDs();
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Increased delay since buttons moved to Core 0
  }
}

void loop() {
  // Main loop now just handles basic system tasks
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

