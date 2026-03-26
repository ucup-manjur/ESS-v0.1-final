/*
 * ============================================================================
 * ESS (Engine Sound Simulator) - Main Program
 * ============================================================================
 * 
 * DESKRIPSI:
 * Program utama untuk Engine Sound Simulator yang mensimulasikan suara mesin
 * dengan kontrol RPM dinamis menggunakan potentiometer atau data OBD2.
 * 
 * FITUR UTAMA:
 * - Dual Mode: DEV_MODE (potentiometer) atau OBD_MODE (OBD2 CAN bus)
 * - Audio playback dengan 4 register (folder audio berbeda)
 * - BLE control untuk remote operation
 * - Button control untuk switch register dan play/stop
 * - LED indicators untuk status
 * - Dual core architecture untuk performa optimal
 * 
 * MODE OPERASI:
 * 1. DEV_MODE:
 *    - Menggunakan potentiometer untuk kontrol RPM
 *    - ADC Task di Core 0 untuk baca potentiometer
 *    - BLE Task di Core 1 untuk komunikasi wireless
 *    - Cocok untuk development dan testing
 * 
 * 2. OBD_MODE:
 *    - Menggunakan data OBD2 dari CAN bus kendaraan
 *    - OBD2 Task di Core 1 untuk baca data CAN
 *    - BLE Task di Core 1 untuk komunikasi wireless
 *    - Auto-retry 1 menit, fallback ke simulation mode
 *    - Cocok untuk instalasi di kendaraan nyata
 * 
 * ARSITEKTUR DUAL CORE:
 * Core 0: ADC Task (hanya DEV_MODE)
 *         - Baca potentiometer setiap 30ms
 *         - Update button state setiap 5ms
 *         - Smooth ADC reading dengan slope limiting
 * 
 * Core 1: BLE Task (kedua mode)
 *         - Handle BLE commands
 *         - Update LED indicators
 *         - Update button state (OBD_MODE)
 *         OBD2 Task (hanya OBD_MODE)
 *         - Request data OBD2 setiap 100ms
 *         - Connection retry dengan timeout 60s
 *         - Fallback ke simulation mode
 * 
 * CARA MENGGUNAKAN:
 * 1. Pilih mode dengan uncomment salah satu:
 *    #define DEV_MODE  // Untuk development dengan potentiometer
 *    #define OBD_MODE  // Untuk production dengan OBD2
 * 
 * 2. Upload firmware ke ESP32
 * 
 * 3. Monitor serial untuk melihat status:
 *    - LittleFS initialization
 *    - Mode selection (DEV/OBD)
 *    - Task creation
 *    - OBD2 connection status (jika OBD_MODE)
 * 
 * 4. Kontrol via:
 *    - Button A: Switch register (1→2→3→4→1)
 *    - Button B: Play/Stop audio
 *    - Button B Long Press: Programming mode
 *    - Potentiometer: Kontrol RPM (DEV_MODE)
 *    - OBD2: Auto RPM dari kendaraan (OBD_MODE)
 *    - BLE: Remote control via aplikasi
 * 
 * HARDWARE REQUIREMENTS:
 * - ESP32 DevKit
 * - DAC output pin (GPIO 25)
 * - 3 buttons (GPIO defined in config.h)
 * - 3 LEDs (GPIO defined in config.h)
 * - Potentiometer (GPIO 34) untuk DEV_MODE
 * - CAN transceiver (RX=16, TX=17) untuk OBD_MODE
 * 
 * DEPENDENCIES:
 * - Arduino.h
 * - LittleFS.h (file system)
 * - config.h (pin definitions)
 * - AudioPlayer.h (audio playback)
 * - SystemManager.h (system control)
 * - OBD2Control.h (OBD2 integration)
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#include <Arduino.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "config.h"
#include "AudioPlayer.h"
#include "SystemManager.h"
#include "OBD2Control.h"
#include "IMUControl.h"

// ============================================================================
// MODE SELECTION
// ============================================================================

// #define DEV_MODE  // Use potentiometer for throttle
#define OBD_MODE  // Use OBD2 for throttle

// Dynamic Slope (0=DISABLED/Fixed, 1=ENABLED/Dynamic)
#define DYNAMIC_SLOPE 1

// IMU Enable (0=DISABLED, 1=ENABLED)
#define IMU_ENABLE 0

// IMU Debug (0=OFF, 1=ON)
#define IMU_DEBUG 0

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
AudioPlayer player;        // Audio playback engine
SystemManager sysManager;  // System control & coordination

// ============================================================================
// DYNAMIC SLOPE PARAMETERS (Configurable via BLE)
// ============================================================================
struct SlopeConfig {
  int lowRpmSlope = DEFAULT_LOW_RPM_SLOPE;
  int midRpmSlope = DEFAULT_MID_RPM_SLOPE;
  int highRpmSlope = DEFAULT_HIGH_RPM_SLOPE;
  float accelLag = DEFAULT_ACCEL_LAG;
  float decelLag = DEFAULT_DECEL_LAG;
  int aggressiveThreshold = DEFAULT_AGGRESSIVE_THRESH;
};

SlopeConfig slopeConfig;  // Global instance

// ============================================================================
// TASK HANDLES
// ============================================================================
// FreeRTOS task handles untuk dual core operation
TaskHandle_t ADCTaskHandle = NULL;  // Core 0: ADC + Button (DEV_MODE only)
TaskHandle_t BLETaskHandle = NULL;  // Core 1: BLE + LED + Button (both modes)

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void ADCTask(void* parameter);   // Task untuk baca ADC + button (Core 0)
void BLETask(void* parameter);   // Task untuk BLE + LED + button (Core 1)

// ============================================================================
// SETUP FUNCTION
// ============================================================================
// Dipanggil sekali saat ESP32 boot
// Fungsi:
// 1. Initialize serial communication (115200 baud)
// 2. Initialize LittleFS file system dengan retry
// 3. Initialize audio player dan system manager
// 4. Initialize OBD2 system (jika OBD_MODE)
// 5. Create FreeRTOS tasks untuk dual core operation
// ============================================================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(500);

  // ============================================================================
  // WATCHDOG TIMER INITIALIZATION
  // ============================================================================
  esp_task_wdt_init(30, true);  // 30s timeout, panic on timeout
  esp_task_wdt_add(NULL);       // Add current task (setup/loop)
  Serial.println("✅ Watchdog enabled (30s)");

  // ============================================================================
  // LITTLEFS INITIALIZATION
  // ============================================================================
  // LittleFS digunakan untuk menyimpan file audio (.raw)
  // Retry 3x jika gagal, kemudian restart ESP32
  // ============================================================================
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

// ============================================================================
// MODE-SPECIFIC INITIALIZATION
// ============================================================================
#ifdef DEV_MODE
  Serial.println("🔧 DEV MODE - Using Potentiometer");
  player.begin();
  sysManager.begin(&player);
  
#if IMU_ENABLE
  Serial.println("Starting IMU initialization...");
  delay(500);
  if (imuControl.begin()) {
    Serial.println("✅ IMU enabled");
  } else {
    Serial.println("⚠️ Continuing without IMU");
  }
#else
  Serial.println("⚠️ IMU disabled (IMU_ENABLE=0)");
#endif
#endif

#ifdef OBD_MODE
  Serial.println("🚗 OBD MODE - Using OBD2 Data");
  player.begin();
  sysManager.begin(&player);
  obd2.begin();
  obd2.startTask();
  Serial.println("✅ OBD2 system initialized");

#if IMU_ENABLE
  Serial.println("Starting IMU initialization...");
  delay(500);
  if (imuControl.begin()) {
    Serial.println("✅ IMU enabled");
  } else {
    Serial.println("⚠️ Continuing without IMU");
  }
#else
  Serial.println("⚠️ IMU disabled (IMU_ENABLE=0)");
#endif
#endif
  
  // ============================================================================
  // FREERTOS TASK CREATION
  // ============================================================================
  // BLE Task: Berjalan di kedua mode (DEV & OBD)
  // - Core 1, Priority 2
  // - Handle BLE communication
  // - Update LED indicators
  // - Update button state (untuk OBD_MODE)
  // ============================================================================
  xTaskCreatePinnedToCore(
    BLETask,           // Task function
    "BLE_Task",        // Task name
    8192,              // Stack size (larger for BLE)
    NULL,              // Parameters
    2,                 // Priority
    &BLETaskHandle,    // Task handle
    1                  // Core 1
  );
  
#ifdef DEV_MODE
  // ============================================================================
  // ADC TASK (DEV_MODE ONLY)
  // ============================================================================
  // - Core 0, Priority 1
  // - Baca potentiometer setiap 30ms
  // - Update button state setiap 5ms
  // - Smooth ADC reading dengan slope limiting
  // - Map ADC (0-4095) ke sample rate (8000-44100 Hz)
  // ============================================================================
  xTaskCreatePinnedToCore(
    ADCTask,           // Task function
    "ADC_Task",        // Task name
    4096,              // Stack size
    NULL,              // Parameters
    1,                 // Priority
    &ADCTaskHandle,    // Task handle
    0                  // Core 0
  );
  
  Serial.println("✅ Dual core tasks started");
  Serial.println("   ADC Task -> Core 0 (Priority 1)");
  Serial.println("   BLE Task -> Core 1 (Priority 2)");
#else
  Serial.println("✅ BLE Task started on Core 1");
#endif
}

// ============================================================================
// ADC TASK (CORE 0 - DEV_MODE ONLY)
// ============================================================================
// FUNGSI:
// Task ini berjalan di Core 0 untuk membaca potentiometer dan button
// 
// CARA KERJA:
// 1. Baca button state setiap 5ms (prioritas tinggi)
// 2. Baca ADC potentiometer setiap 30ms
// 3. Smooth ADC reading dengan slope limiting (max 200 per update)
// 4. Map ADC value (0-4095) ke sample rate (8000-44100 Hz)
// 5. Update audio player sample rate jika tidak ada rev/shift effect
// 
// PARAMETER:
// - adcSlopeLimit: 200 (max perubahan ADC per update)
// - Update interval: 30ms untuk ADC, 5ms untuk button
// 
// PENGGUNAAN:
// - Putar potentiometer untuk kontrol RPM
// - Semakin tinggi nilai ADC, semakin tinggi sample rate (RPM)
// - Smooth transition untuk menghindari audio glitch
// ============================================================================
void ADCTask(void* parameter) {
  static unsigned long lastUpdate = 0;
  static int lastRaw = 0;
  static int smoothedRaw = 0;
  static int targetRaw = 0;  // Target dari input
  
  esp_task_wdt_add(NULL);  // Add ADC task to watchdog
  
  for(;;) {
    esp_task_wdt_reset();  // Feed watchdog
    unsigned long now = millis();
    sysManager.updateButtons();
    
    if (now - lastUpdate >= 30) {
      int raw = analogRead(THROTTLE_ADC_PIN);
      
#if DYNAMIC_SLOPE
      // Update target
      targetRaw = raw;
      
      int diff = targetRaw - smoothedRaw;
      float throttlePercent = (float)smoothedRaw / 4095.0f;
      int slope;
      
      // Base slope by RPM zone
      if (throttlePercent < 0.25f) {
        slope = slopeConfig.lowRpmSlope;   // Low RPM
      } else if (throttlePercent < 0.75f) {
        slope = slopeConfig.midRpmSlope;   // Mid RPM
      } else {
        slope = slopeConfig.highRpmSlope;  // High RPM
      }
      
      // Acceleration vs Deceleration
      if (diff > 0) {
        // Accelerating: Apply lag
        // Aggressive input detection (sudden throttle)
        if (abs(diff) > slopeConfig.aggressiveThreshold) {
          slope = (int)(slope * slopeConfig.accelLag);  // Slower on sudden gas
        }
      } else {
        // Decelerating: Apply lag (engine inertia)
        slope = (int)(slope * slopeConfig.decelLag);  // Slower on decel too
      }
      
      // Apply slope limiting
      if (abs(diff) > slope) {
        smoothedRaw += (diff > 0) ? slope : -slope;
      } else {
        smoothedRaw = targetRaw;
      }
#else
      // Fixed slope (original)
      int diff = raw - smoothedRaw;
      int slope = 200;
      if (abs(diff) > slope) {
        smoothedRaw += (diff > 0) ? slope : -slope;
      } else {
        smoothedRaw = raw;
      }
#endif
      
      int modifiedRaw = smoothedRaw;
      
      if (abs(modifiedRaw - lastRaw) > 10) {
        uint32_t rate = map(modifiedRaw, 0, 4095, 8000, 44100);
        Serial.printf("🎯 ADC: %d -> %d Hz (slope: %d)\n", modifiedRaw, rate, slope);
        lastRaw = modifiedRaw;
      }
      
      uint32_t throttleRate = map(modifiedRaw, 0, 4095, 8000, 44100);
      sysManager.setCurrentThrottleRate(throttleRate);
      if (!sysManager.isRevActive() && !sysManager.isShiftActive()) {
        player.updateSampleRateFromADC(modifiedRaw);
      }
      lastUpdate = now;
    }
    
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// BLE TASK (CORE 1 - BOTH MODES)
// ============================================================================
// FUNGSI:
// Task ini berjalan di Core 1 untuk handle BLE, LED, dan button
// 
// CARA KERJA:
// 1. Update button state setiap 10ms
//    - Button A: Switch register (1→2→3→4→1)
//    - Button B: Play/Stop, Long press = Programming mode
//    - Button C: Delete file (programming mode only)
// 
// 2. Update BLE communication
//    - Terima command dari aplikasi BLE
//    - Process command (volume, register, rev, gear, OBD2 request)
//    - Kirim response/notification ke aplikasi
// 
// 3. Update LED indicators
//    - LED sesuai register aktif (1-4)
//    - LED berkedip saat programming mode
//    - LED mati saat stop
// 
// UPDATE INTERVAL: 10ms
// 
// PENGGUNAAN:
// - Task ini selalu berjalan di kedua mode (DEV & OBD)
// - Di DEV_MODE: Button dihandle juga di ADC Task (lebih responsif)
// - Di OBD_MODE: Button hanya dihandle di BLE Task
// ============================================================================
void BLETask(void* parameter) {
  esp_task_wdt_add(NULL);  // Add BLE task to watchdog
  
  for(;;) {
    esp_task_wdt_reset();  // Feed watchdog
    // Handle buttons, BLE and LED updates
    sysManager.updateButtons();
    sysManager.updateBLE();
    sysManager.updateLEDs();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// ============================================================================
// MAIN LOOP
// ============================================================================
// FUNGSI:
// Main loop Arduino - tidak digunakan karena semua logic ada di tasks
// 
// CARA KERJA:
// - Hanya delay 100ms untuk yield ke FreeRTOS scheduler
// - Semua operasi dilakukan di ADC Task dan BLE Task
// 
// CATATAN:
// - Jangan tambahkan logic di sini
// - Gunakan tasks untuk operasi yang membutuhkan real-time response
// ============================================================================
void loop() {
  esp_task_wdt_reset();  // Feed watchdog
  // Main loop now just handles basic system tasks
  vTaskDelay(100 / portTICK_PERIOD_MS);
}
