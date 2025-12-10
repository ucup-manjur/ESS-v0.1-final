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
    Serial.println("❌ LED pins invalid!");
    return;
  }
  
  // Cek pin conflict
  if (LED_1_PIN == LED_2_PIN || LED_1_PIN == LED_3_PIN || LED_2_PIN == LED_3_PIN) {
    Serial.println("❌ LED pins conflict!");
    return;
  }
  
  pinMode(LED_1_PIN, OUTPUT);
  pinMode(LED_2_PIN, OUTPUT);
  pinMode(LED_3_PIN, OUTPUT);
  setAllOff();
  
  Serial.println("✅ LED Manager initialized");
}


void LEDManager::update() {
  if (blinkMode) {
    unsigned long now = millis();
    if (now - lastBlink >= BLINK_INTERVAL) {
      blinkState = !blinkState;
      lastBlink = now;
      
      if (blinkState) {
        digitalWrite(LED_1_PIN, currentRegister == 1 ? LOW : HIGH);
        digitalWrite(LED_2_PIN, currentRegister == 2 ? LOW : HIGH);
        digitalWrite(LED_3_PIN, currentRegister == 3 ? LOW : HIGH);
      } else {
        setAllOff();
      }
    }
  }
}

void LEDManager::setRegister(uint8_t reg) {
  currentRegister = reg;
  if (!blinkMode) {
    digitalWrite(LED_1_PIN, reg == 1 ? LOW : HIGH);
    digitalWrite(LED_2_PIN, reg == 2 ? LOW : HIGH);
    digitalWrite(LED_3_PIN, reg == 3 ? LOW : HIGH);
  }
}

void LEDManager::setAllOff() {
  digitalWrite(LED_1_PIN, HIGH);
  digitalWrite(LED_2_PIN, HIGH);
  digitalWrite(LED_3_PIN, HIGH);
}

void LEDManager::setBlinkMode(bool enable) {
  blinkMode = enable;
  if (!enable) {
    setRegister(currentRegister);
  }
}
