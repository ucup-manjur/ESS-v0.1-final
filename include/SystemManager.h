/*
 * ============================================================================
 * SystemManager.h - System Coordination & Control
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class utama untuk koordinasi semua subsystem (Audio, Button, LED, BLE).
 * Mengelola mode operasi, register audio, dan audio effects (rev, shift).
 * 
 * FITUR UTAMA:
 * 1. Mode Management:
 *    - Normal Mode: Operasi normal dengan button control
 *    - Programming Mode: File upload via BLE
 *    - Auto mode switching dengan security
 * 
 * 2. Register System:
 *    - 4 register audio (folder berbeda)
 *    - Switch register via button atau BLE
 *    - Auto-load audio saat register berubah
 * 
 * 3. Audio Effects:
 *    - Rev Limiter: Smooth ramp up/down
 *    - Gear Shift: RPM drop dan recovery
 *    - Configurable duration dan target rate
 * 
 * 4. Subsystem Integration:
 *    - AudioPlayer: Playback control
 *    - ButtonManager: Physical button input
 *    - LEDManager: Visual indicators
 *    - BLEControl: Wireless communication
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    SystemManager sysManager;
 *    AudioPlayer player;
 *    
 *    player.begin();
 *    sysManager.begin(&player);
 *    ```
 * 
 * 2. Update Loop (di FreeRTOS task):
 *    ```cpp
 *    void BLETask(void* param) {
 *        for(;;) {
 *            sysManager.updateButtons();  // Update button state
 *            sysManager.updateBLE();      // Handle BLE commands
 *            sysManager.updateLEDs();     // Update LED indicators
 *            vTaskDelay(10);
 *        }
 *    }
 *    ```
 * 
 * 3. Audio Effects:
 *    ```cpp
 *    // Rev limiter (via BLE command)
 *    // CMD_REV_START (0x03) -> startRev()
 *    // CMD_REV_STOP (0x04) -> stopRev()
 *    
 *    // Gear shift (via BLE command)
 *    // CMD_GEAR_UP (0x01) -> triggerGearUp()
 *    // CMD_GEAR_DOWN (0x02) -> triggerGearDown()
 *    ```
 * 
 * 4. Register Control:
 *    ```cpp
 *    // Via button: Press Button A
 *    // Via BLE: CMD_SET_AUDIO_PLAY (0x15) + register number
 *    ```
 * 
 * 5. Programming Mode:
 *    ```cpp
 *    // Enter: Button B long press (3s)
 *    // Exit: Button B long press (3s) lagi
 *    // File transfer: Via BLE saat programming mode
 *    ```
 * 
 * BUTTON MAPPING:
 * - Button A: Switch register (1→2→3→4→1)
 * - Button B: Play/Stop (short press)
 * - Button B Long: Programming mode (3s)
 * - Button C Long: Delete current register file (3s, programming mode only)
 * 
 * BLE COMMANDS:
 * - 0x01: Gear Up
 * - 0x02: Gear Down
 * - 0x03: Rev Start
 * - 0x04: Rev Stop
 * - 0x11: Volume/Mute
 * - 0x13: Request File List
 * - 0x14: Request File Info
 * - 0x15: Set Audio Register
 * - 0x16: Toggle Auto Shift
 * - 0x30-0x34: OBD2 Commands
 * - 0xFF: Request Status
 * 
 * AUDIO EFFECTS PARAMETERS:
 * - Rev Target Rate: 39000 Hz
 * - Rev Ramp Duration: 300ms
 * - Rev Down Duration: 400ms
 * - Shift Rise: 150ms (130% of base rate)
 * - Shift Recovery: 200ms
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>
#include "AudioPlayer.h"
#include "ButtonManager.h"
#include "LEDManager.h"
#include "BLEControl.h"
#include "IMUControl.h"

enum SystemMode {
  MODE_NORMAL,
  MODE_PROGRAMMING
};

class SystemManager {
public:
  SystemManager();
  void begin(AudioPlayer* audioPlayer = nullptr);
  void update();
  void updateButtons();
  void updateBLE();
  void updateLEDs();
  void setCurrentThrottleRate(uint32_t rate) { currentThrottleRate = rate; }
  bool isRevActive()   { return isRevving || isRevDown; }
  bool isShiftActive() { return isShifting; }
  void updateIMU();
  void applyIMUModifier();
  
private:
  AudioPlayer* player;

  // IMU modifier smoothing
  float currentIMUModifier = 1.0f;
  const float IMU_LERP_SPEED = 0.05f; // 0.01=sangat lambat, 0.1=cepat
  
  // Simple rev variables
  bool isRevving = false;
  bool isRevDown = false;
  unsigned long revStartTime = 0;
  unsigned long revDownStartTime = 0;
  uint32_t prevNormalRate = 8000;
  uint32_t currentThrottleRate = 8000;  // Track current throttle
  const uint32_t revTargetRate = 39000;  // Match original
  const unsigned long revRampDuration = 300;  // Match original
  const unsigned long revDownDuration = 400;
  
  // Simple shift variables
  bool isShifting = false;
  unsigned long shiftStartTime = 0;
  uint32_t shiftBaseRate = 8000;
  uint32_t shiftTargetRate = 8000;
  uint8_t shiftPhase = 0;  // 0=drop, 1=recovery
  
  uint8_t currentGear = 0;
  const uint8_t maxGear = 4;
  ButtonManager buttons;
  LEDManager leds; 
  BLEControl ble;
  
  SystemMode currentMode = MODE_NORMAL;
  uint8_t currentRegister = 1;
  bool isPlaying = false;
  
  void handleNormalMode();
  void handleProgrammingMode();
  void handleBLECommands();
  void switchRegister();
  void togglePlayback();
  void enterProgrammingMode();
  void exitProgrammingMode();
  void loadCurrentSound();
  void formatLittleFS();
  void deleteCurrentRegisterFile();
  void startRev();
  void stopRev();
  void updateRev();
  void triggerShift();
  void triggerGearUp();
  void triggerGearDown();
  void updateShift();

};