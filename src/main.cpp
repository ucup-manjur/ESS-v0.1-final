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
#include "config.h"
#include "AudioPlayer.h"
#include "SystemManager.h"
#include "OBD2Control.h"

// ============================================================================
// MODE SELECTION
// ============================================================================
// Pilih salah satu mode operasi:
// - DEV_MODE: Development mode dengan potentiometer untuk kontrol RPM
// - OBD_MODE: Production mode dengan OBD2 CAN bus untuk data kendaraan
//
// Cara switch mode:
// 1. Comment mode yang tidak digunakan dengan //
// 2. Uncomment mode yang akan digunakan
// 3. Upload ulang firmware ke ESP32
// ============================================================================

// #define DEV_MODE  // Use potentiometer for throttle
#define OBD_MODE  // Use OBD2 for throttle

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
AudioPlayer player;        // Audio playback engine
SystemManager sysManager;  // System control & coordination

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
  // DEV MODE: Development dengan potentiometer
  // - Audio player untuk playback
  // - System manager untuk kontrol button/LED/BLE
  // - Potentiometer di GPIO 34 untuk kontrol RPM
  Serial.println("🔧 DEV MODE - Using Potentiometer");
  player.begin();
  sysManager.begin(&player);
#endif

#ifdef OBD_MODE
  // OBD MODE: Production dengan OBD2 CAN bus
  // - Audio player untuk playback
  // - System manager untuk kontrol button/LED/BLE
  // - OBD2 control untuk baca data CAN bus (RPM, SOH, dll)
  // - Auto-retry 60s, fallback ke simulation mode
  Serial.println("🚗 OBD MODE - Using OBD2 Data");
  // Initialize OBD2 system
  player.begin();
  sysManager.begin(&player);
  obd2.begin();
  obd2.startTask();
  Serial.println("✅ OBD2 system initialized");
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
  for(;;) {
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
  // Main loop now just handles basic system tasks
  vTaskDelay(100 / portTICK_PERIOD_MS);
}

