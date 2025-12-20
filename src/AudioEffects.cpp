#include "AudioEffects.h"

AudioEffects::AudioEffects(AudioPlayer* audioPlayer) : player(audioPlayer) {
  currentSampleRate = 8000;  // Start with idle RPM
  previousSampleRate = 8000;
  prevNormalRate = 8000;
}

void AudioEffects::update() {
  static unsigned long lastUpdate = 0;
  static unsigned long updateCount = 0;
  unsigned long now = millis();
  if (now - lastUpdate < 30) return;  // 33Hz updates
  lastUpdate = now;
  updateCount++;
  
  // Debug: Log update calls during rev
  if ((revActive || revStopping) && updateCount % 10 == 0) {
    Serial.printf("🔄 Update #%lu T=%lu, revActive=%s, revStopping=%s\n", 
                  updateCount, now, revActive ? "true" : "false", revStopping ? "true" : "false");
  }
  
  uint32_t newRate = currentSampleRate;
  
  // Handle revving effect - highest priority
  if (revActive) {
    unsigned long elapsed = millis() - revStartTime;
    if (elapsed < revRampDuration) {
      float progress = (float)elapsed / revRampDuration;
      newRate = prevNormalRate + (revTargetRate - prevNormalRate) * progress;
    } else {
      newRate = revTargetRate;
    }
  } 
  else if (revStopping) {
    unsigned long elapsed = millis() - revStopTime;
    Serial.printf("⛔ Ramp down: %lu/%lu ms\n", elapsed, revRampDownDuration);
    
    if (elapsed < revRampDownDuration) {
      float progress = (float)elapsed / revRampDownDuration;
      newRate = revTargetRate - (revTargetRate - prevNormalRate) * progress;
    } else {
      newRate = prevNormalRate;
      revStopping = false;
      Serial.printf("✅ Rev complete T=%lu, Final: %d Hz\n", millis(), newRate);
    }
  }
  // Handle shifting effect
  else if (shifting) {
    unsigned long elapsed = millis() - shiftStartTime;
    if (elapsed >= shiftDuration) {
      shifting = false;
      Serial.println("✅ Shift completed");
      // Effect finished - throttle will resume control
    } else {
      // Smooth RPM drop with sine curve for natural feel
      float progress = (float)elapsed / shiftDuration;
      float curve = sin(progress * PI * 0.5f);  // Smooth curve
      newRate = shiftStartRate - (shiftStartRate - shiftTargetRate) * curve;
    }
  }
  
  // Safety bounds
  if (newRate < 8000) newRate = 8000;
  if (newRate > 44100) newRate = 44100;
  
  if (newRate != currentSampleRate) {
    currentSampleRate = newRate;
    if (player) player->setSampleRate(currentSampleRate);
  }
}

void AudioEffects::triggerShift() {
  if (player && !shifting) {
    shiftStartRate = currentSampleRate;
    shiftTargetRate = (uint32_t)(currentSampleRate * 0.75f);  // Gentler 25% drop
    shiftStartTime = millis();
    shifting = true;
    Serial.printf("⚙️ Gear shift - RPM drop! (Gear %d)\n", currentGear);
  }
}

void AudioEffects::triggerGearUp() {
  if (currentGear < maxGear && !shifting) {
    currentGear++;
    triggerShift();
    Serial.printf("⬆️ Gear UP -> %d\n", currentGear);
  } else if (currentGear >= maxGear) {
    Serial.printf("⚠️ Max gear (%d)\n", maxGear);
  }
}

void AudioEffects::triggerGearDown() {
  if (currentGear > 1 && !shifting) {
    currentGear--;
    triggerShift();
    Serial.printf("⬇️ Gear DOWN -> %d\n", currentGear);
  } else if (currentGear <= 1) {
    Serial.println("⚠️ Min gear (1)");
  }
}

void AudioEffects::updateSampleRate(uint32_t targetRate) {
  throttleTargetRate = targetRate;
  
  if (!shifting && !ramping && !revActive && !revStopping) {
    // Direct update for normal throttle
    currentSampleRate = targetRate;
    if (player) player->setSampleRate(currentSampleRate);
  }
}

void AudioEffects::startRev() {
  unsigned long now = millis();
  if (!revActive && !shifting) {
    revActive = true;
    revStartTime = now;
    prevNormalRate = throttleTargetRate;  // Save current throttle position
    Serial.printf("🔊 Rev start! T=%lu, Saved rate: %d\n", now, prevNormalRate);
  } else {
    Serial.printf("⚠️ Rev blocked T=%lu - revActive: %s, shifting: %s\n", now, revActive ? "true" : "false", shifting ? "true" : "false");
  }
}

void AudioEffects::stopRev() {
  unsigned long now = millis();
  if (revActive) {
    unsigned long revDuration = now - revStartTime;
    revActive = false;
    revStopping = true;
    revStopTime = now;
    Serial.printf("⛔ Rev stop T=%lu, Duration=%lu ms\n", now, revDuration);
    
    // Immediate execution - don't wait for next update
    if (player) {
      uint32_t newRate = revTargetRate;  // Start ramp down from max
      player->setSampleRate(newRate);
      Serial.printf("⛔ Immediate ramp down start: %d Hz\n", newRate);
    }
  } else {
    Serial.printf("⚠️ Rev stop ignored T=%lu - not revving\n", now);
  }
}

void AudioEffects::setThrottleInput(uint32_t throttleRate) {
  throttleTargetRate = throttleRate;
  currentSampleRate = throttleRate;
  
  // Auto shift logic based on RPM
  if (autoShiftEnabled && !shifting) {
    uint32_t currentRPM = map(throttleRate, 8000, 44100, 1000, 18000);
    
    // Gear up logic
    if (currentGear < maxGear && currentRPM >= SHIFT_UP_RPM[currentGear - 1]) {
      currentGear++;
      triggerShift();
      Serial.printf("🔄 Auto Shift UP -> Gear %d (RPM: %d)\n", currentGear, currentRPM);
      return;
    }
    
    // Gear down logic
    if (currentGear > 1 && currentRPM <= SHIFT_DOWN_RPM[currentGear - 1]) {
      currentGear--;
      triggerShift();
      Serial.printf("🔄 Auto Shift DOWN -> Gear %d (RPM: %d)\n", currentGear, currentRPM);
      return;
    }
  }
  
  // Only apply throttle if no effects are active
  if (!shifting && !revActive && !revStopping && player) {
    player->setSampleRate(currentSampleRate);
  } else if (revActive || revStopping) {
    // During rev, don't update currentSampleRate from throttle
    return;
  }
}