#include "VolumeControl.h"

VolumeControl volumeControl;

VolumeControl::VolumeControl() {}

void VolumeControl::begin() {
  buildVolumeLUT();
  Serial.println("✅ Volume Control initialized");
}

void VolumeControl::setVolume(uint8_t level) {
  if (level > 100) level = 100;
  
  currentVolume = level;
  volumeMultiplier = level / 100.0f;
  
  // Anti clipping - max 90%
  if (volumeMultiplier > 0.9f) volumeMultiplier = 0.9f;
  
  buildVolumeLUT();
  Serial.printf("🔊 Volume: %d%%\n", level);
}

void VolumeControl::mute(bool enable) {
  muted = enable;
  Serial.printf("🔇 Mute: %s\n", muted ? "ON" : "OFF");
}

void VolumeControl::toggleMute() {
  mute(!muted);
}

void VolumeControl::buildVolumeLUT() {
  for (int i = 0; i < 256; i++) {
    int val = (int)(i * volumeMultiplier);
    if (val > 255) val = 255;
    volumeLUT[i] = (uint8_t)val;
  }
}

uint8_t VolumeControl::processAudioSample(uint8_t sample) {
  if (muted) return 128;  // Silent (center)
  return volumeLUT[sample];
}
