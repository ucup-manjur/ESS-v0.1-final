/*
 * ============================================================================
 * VolumeControl.h - Digital Volume Control
 * ============================================================================
 * 
 * DESKRIPSI:
 * Class untuk kontrol volume digital audio dengan lookup table (LUT).
 * Menyediakan volume adjustment dan mute function tanpa audio glitch.
 * 
 * FITUR UTAMA:
 * 1. Volume Control:
 *    - Range: 0-100%
 *    - Lookup Table untuk fast processing
 *    - Linear volume scaling
 *    - No audio artifacts
 * 
 * 2. Mute Function:
 *    - Instant mute/unmute
 *    - Toggle mute
 *    - Preserve volume level
 * 
 * 3. Real-time Processing:
 *    - Process setiap audio sample
 *    - Minimal CPU overhead
 *    - ISR-safe (dipanggil dari timer ISR)
 * 
 * CARA MENGGUNAKAN:
 * 
 * 1. Inisialisasi:
 *    ```cpp
 *    VolumeControl volumeControl;
 *    volumeControl.begin();  // Build LUT
 *    ```
 * 
 * 2. Set Volume:
 *    ```cpp
 *    volumeControl.setVolume(50);   // 50%
 *    volumeControl.setVolume(100);  // 100% (max)
 *    volumeControl.setVolume(0);    // 0% (silent)
 *    ```
 * 
 * 3. Mute Control:
 *    ```cpp
 *    volumeControl.mute(true);   // Mute
 *    volumeControl.mute(false);  // Unmute
 *    volumeControl.toggleMute(); // Toggle
 *    ```
 * 
 * 4. Process Audio (dipanggil dari ISR):
 *    ```cpp
 *    uint8_t sample = audioBuffer[index];
 *    uint8_t processed = volumeControl.processAudioSample(sample);
 *    dacWrite(DAC_PIN, processed);
 *    ```
 * 
 * 5. Check Status:
 *    ```cpp
 *    if (volumeControl.isMuted()) {
 *        // Audio is muted
 *    }
 *    uint8_t level = volumeControl.getVolume();
 *    ```
 * 
 * VOLUME CALCULATION:
 * - Input: 0-100 (percentage)
 * - Multiplier: 0.0-1.0 (linear)
 * - LUT: Pre-calculated untuk 256 sample values
 * - Output: Scaled sample value
 * 
 * MUTE BEHAVIOR:
 * - Muted: Output 128 (DC offset, silent)
 * - Unmuted: Normal volume processing
 * - Volume level preserved saat mute
 * 
 * PERFORMANCE:
 * - LUT lookup: O(1) constant time
 * - No floating point di ISR
 * - Minimal CPU overhead
 * - ISR-safe operation
 * 
 * DEFAULT VALUES:
 * - Volume: 50%
 * - Muted: false
 * - Sample center: 128 (8-bit unsigned)
 * 
 * AUTHOR: Amazon Q
 * DATE: 10 Februari 2026
 * VERSION: 1.0
 * ============================================================================
 */

#pragma once
#include <Arduino.h>

class VolumeControl {
public:
  VolumeControl();
  
  void begin();
  void setVolume(uint8_t level);  // 0-100
  void mute(bool enable);
  void toggleMute();
  
  uint8_t processAudioSample(uint8_t sample);
  bool isMuted() { return muted; }
  uint8_t getVolume() { return currentVolume; }
  
private:
  void buildVolumeLUT();
  
  uint8_t volumeLUT[256];
  uint8_t currentVolume = 50;  // Default 50%
  bool muted = false;
  float volumeMultiplier = 0.5f;
};

extern VolumeControl volumeControl;