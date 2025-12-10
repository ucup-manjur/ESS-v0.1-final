#include "AudioEffects.h"

AudioEffects::AudioEffects(AudioPlayer* audioPlayer) : player(audioPlayer) {
  currentSampleRate = 8000;  // Start with idle RPM
  previousSampleRate = 8000;
  prevNormalRate = 8000;
}

void AudioEffects::update() {
  if (revActive) {
    unsigned long elapsed = millis() - revStartTime;
    if (elapsed < revRampDuration) {
      float progress = (float)elapsed / revRampDuration;
      float curve = sin(progress * PI * 0.5f);
      currentSampleRate = prevNormalRate + (revTargetRate - prevNormalRate) * curve;
    } else {
      currentSampleRate = revTargetRate;
    }
  } 
  else if (revStopping) {
    unsigned long elapsed = millis() - revStopTime;
    if (elapsed < revRampDownDuration) {
      float progress = (float)elapsed / revRampDownDuration;
      float curve = 1.0f - sin(progress * PI * 0.5f);
      currentSampleRate = prevNormalRate + (revTargetRate - prevNormalRate) * curve;
    } else {
      currentSampleRate = prevNormalRate;
      revStopping = false;
      Serial.println("✅ Turun selesai, balik idle");
    }
  }
  
  if (currentSampleRate != previousSampleRate) {
    previousSampleRate = currentSampleRate;
    if (player) player->setSampleRate(currentSampleRate);
  }
}

void AudioEffects::triggerShift() {
  if (player) {
    shiftStartRate = currentSampleRate;
    shiftTargetRate = (uint32_t)(currentSampleRate * 0.65f);
    shiftStartTime = millis();
    shifting = true;
    ramping = false;
    Serial.println("⚙️ Gear shift - RPM drop!");
  }
}

void AudioEffects::updateSampleRate(uint32_t targetRate) {
  if (shifting) {
    unsigned long elapsed = millis() - shiftStartTime;
    if (elapsed >= shiftDuration) {
      currentSampleRate = shiftTargetRate;
      shifting = false;
      rampOriginRate = currentSampleRate;
      rampTargetRate = targetRate;
      rampStartTime = millis();
      ramping = true;
      Serial.println("⬆️ Ramping up to new gear");
    } else {
      float progress = (float)elapsed / shiftDuration;
      float curve = sin(progress * PI * 0.5f);
      currentSampleRate = shiftStartRate + (shiftTargetRate - shiftStartRate) * curve;
    }
  } else if (ramping) {
    unsigned long elapsed = millis() - rampStartTime;
    if (elapsed >= rampDuration) {
      currentSampleRate = rampTargetRate;
      ramping = false;
    } else {
      float progress = (float)elapsed / rampDuration;
      float curve = 0.5f - 0.5f * cos(progress * PI);
      currentSampleRate = rampOriginRate + (rampTargetRate - rampOriginRate) * curve;
    }
  } else {
    int32_t diff = targetRate - currentSampleRate;
    currentSampleRate += (uint32_t)(diff * 0.15f);
  }
}

void AudioEffects::startRev() {
  if (!revActive && !shifting) {
    revActive = true;
    revStartTime = millis();
    prevNormalRate = currentSampleRate;
    Serial.println("🔊 Rev start!");
  }
}

void AudioEffects::stopRev() {
  if (revActive) {
    revActive = false;
    revStopping = true;
    revStopTime = millis();
    Serial.println("⛔ Rev stop, mulai turun...");
  }
}

void AudioEffects::setThrottleInput(uint32_t throttleRate) {
  throttleTargetRate = throttleRate;
  // Continuously update sample rate based on throttle
  if (!revActive && !revStopping) {
    updateSampleRate(throttleTargetRate);
  }
}