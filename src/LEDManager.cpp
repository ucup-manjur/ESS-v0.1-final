#include "LEDManager.h"
#include "config.h"

LEDManager::LEDManager() : blinkMode(false), blinkState(false), lastBlink(0), currentRegister(0) {}

// void LEDManager::begin() {
//   pinMode(LED_1_PIN, OUTPUT);
//   pinMode(LED_2_PIN, OUTPUT);
//   pinMode(LED_3_PIN, OUTPUT);
//   setAllOff();
// }
void LEDManager::begin() {
  // Validasi pin
  if (LED_1_PIN < 0 || LED_1_PIN > 39 ||
      LED_2_PIN < 0 || LED_2_PIN > 39 ||
      LED_3_PIN < 0 || LED_3_PIN > 39) {
    LOG("[LED] pins invalid!");
    return;
  }
  
  // Cek pin conflict
  if (LED_1_PIN == LED_2_PIN || LED_1_PIN == LED_3_PIN || LED_2_PIN == LED_3_PIN) {
    LOG("[LED] pins conflict!");
    return;
  }
  
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);
  setAllOff();
  
  LOG("[LED] Manager initialized");
}


void LEDManager::update() {
  unsigned long now = millis();
  
  // Handle fast blink mode (priority over normal blink)
  if (fastBlinkMode) {
    if (now - fastBlinkStart >= fastBlinkDuration) {
      // Fast blink duration expired, return to normal mode
      fastBlinkMode = false;
      if (blinkMode) {
        // Return to programming mode blink
        setBlinkMode(true);
      } else {
        // Return to register display
        setRegister(currentRegister);
      }
    } else {
      // Fast blink active
      if (now - lastBlink >= FAST_BLINK_INTERVAL) {
        blinkState = !blinkState;
        lastBlink = now;
        
        if (blinkState) {
          setAllOn();
        } else {
          setAllOff();
        }
      }
    }
    return;
  }
  
  // Normal blink mode (programming mode)
  if (blinkMode) {
    if (now - lastBlink >= BLINK_INTERVAL) {
      blinkState = !blinkState;
      lastBlink = now;
      
      if (blinkState) {
        if (currentRegister == 4) {
          digitalWrite(LED_1_PIN, LOW);
          digitalWrite(LED_2_PIN, LOW);
          digitalWrite(LED_3_PIN, LOW);
        } else {
          digitalWrite(LED_1_PIN, currentRegister == 1 ? LOW : HIGH);
          digitalWrite(LED_2_PIN, currentRegister == 2 ? LOW : HIGH);
          digitalWrite(LED_3_PIN, currentRegister == 3 ? LOW : HIGH);
        }
      } else {
        setAllOff();
      }
    }
  }
}

void LEDManager::setRegister(uint8_t reg) {
  currentRegister = reg;
  if (!blinkMode) {
    if (reg == 4) {
      // Register 4: semua LED nyala
      digitalWrite(LED_1_PIN, LOW);
      digitalWrite(LED_2_PIN, LOW);
      digitalWrite(LED_3_PIN, LOW);
    } else {
      digitalWrite(LED_1_PIN, reg == 1 ? LOW : HIGH);
      digitalWrite(LED_2_PIN, reg == 2 ? LOW : HIGH);
      digitalWrite(LED_3_PIN, reg == 3 ? LOW : HIGH);
    }
  }
}

void LEDManager::setAllOff() {
  digitalWrite(LED_1_PIN, HIGH);
  digitalWrite(LED_2_PIN, HIGH);
  digitalWrite(LED_3_PIN, HIGH);
}

void LEDManager::setAllOn() {
  digitalWrite(LED_1_PIN, LOW);
  digitalWrite(LED_2_PIN, LOW);
  digitalWrite(LED_3_PIN, LOW);
}

void LEDManager::setAllBlink() {
  blinkMode = true;
  currentRegister = 4;  // Use register 4 pattern for all blink
}

void LEDManager::setFastBlink(unsigned long duration) {
  fastBlinkMode = true;
  fastBlinkStart = millis();
  fastBlinkDuration = duration;
  blinkState = false;
  lastBlink = millis();
}

void LEDManager::setBlinkMode(bool enable) {
  blinkMode = enable;
  if (!enable) {
    setRegister(currentRegister);
  }
}
