#include "ButtonManager.h"
#include "config.h"

ButtonManager::ButtonManager() {
  flagMutex = portMUX_INITIALIZER_UNLOCKED;
}

void ButtonManager::begin() {
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  pinMode(BUTTON_C_PIN, INPUT_PULLUP);
}

bool ButtonManager::debounceButton(int pin, bool &lastState, unsigned long &lastChange) {
  // Pin validation
  if (pin < 0 || pin > 39) return false;
  
  bool currentState = digitalRead(pin);
  unsigned long now = millis();
  
  if (currentState != lastState) {
    // Handle millis overflow safely
    unsigned long elapsed = (now >= lastChange) ? (now - lastChange) : (ULONG_MAX - lastChange + now + 1);
    
    if (elapsed >= DEBOUNCE_MS) {
      lastState = currentState;
      lastChange = now;
      return true;
    }
  }
  return false;
}

void ButtonManager::update() {
  unsigned long now = millis();
  
  // Button A - Simple press
  if (debounceButton(BUTTON_A_PIN, btnA_lastState, btnA_lastChange)) {
    if (btnA_lastState == LOW) {
      portENTER_CRITICAL(&flagMutex);
      btnA_pressed = true;
      portEXIT_CRITICAL(&flagMutex);
    }
  }
  
  // Button B - Short and Long press
  if (debounceButton(BUTTON_B_PIN, btnB_lastState, btnB_lastChange)) {
    if (btnB_lastState == LOW) {
      btnB_pressStart = now;
      btnB_longTriggered = false;
    } else {
      unsigned long pressDuration = (now >= btnB_pressStart) ? (now - btnB_pressStart) : (ULONG_MAX - btnB_pressStart + now + 1);
      if (!btnB_longTriggered && pressDuration < LONG_PRESS_MS) {
        portENTER_CRITICAL(&flagMutex);
        btnB_shortPressed = true;
        portEXIT_CRITICAL(&flagMutex);
      }
    }
  }
  
  if (btnB_lastState == LOW && !btnB_longTriggered) {
    unsigned long elapsed = (now >= btnB_pressStart) ? (now - btnB_pressStart) : (ULONG_MAX - btnB_pressStart + now + 1);
    if (elapsed >= LONG_PRESS_MS) {
      portENTER_CRITICAL(&flagMutex);
      btnB_longPressed = true;
      portEXIT_CRITICAL(&flagMutex);
      btnB_longTriggered = true;
    }
  }
  
  // Button C - Short and Long press (5 sec for format)
  if (debounceButton(BUTTON_C_PIN, btnC_lastState, btnC_lastChange)) {
    if (btnC_lastState == LOW) {
      btnC_pressStart = now;
      btnC_longTriggered = false;
      Serial.println("🔴 Button C pressed - starting timer");
    } else {
      unsigned long pressDuration = (now >= btnC_pressStart) ? (now - btnC_pressStart) : (ULONG_MAX - btnC_pressStart + now + 1);
      Serial.printf("🔵 Button C released after %lu ms\n", pressDuration);
      if (!btnC_longTriggered && pressDuration < FORMAT_PRESS_MS) {
        portENTER_CRITICAL(&flagMutex);
        btnC_pressed = true;
        portEXIT_CRITICAL(&flagMutex);
        Serial.println("✅ Button C short press detected");
      }
    }
  }
  
  if (btnC_lastState == LOW && !btnC_longTriggered) {
    unsigned long elapsed = (now >= btnC_pressStart) ? (now - btnC_pressStart) : (ULONG_MAX - btnC_pressStart + now + 1);
    if (elapsed >= FORMAT_PRESS_MS) {
      portENTER_CRITICAL(&flagMutex);
      btnC_longPressed = true;
      portEXIT_CRITICAL(&flagMutex);
      btnC_longTriggered = true;
      Serial.println("❗ Button C LONG PRESS (5s) detected - FORMAT TRIGGERED!");
    }
  }
}

bool ButtonManager::getAndClearFlag(volatile bool &flag) {
  // Atomic read-and-clear operation
  portENTER_CRITICAL(&flagMutex);
  bool result = flag;
  if (result) {
    flag = false;
  }
  portEXIT_CRITICAL(&flagMutex);
  
  return result;
}

bool ButtonManager::isButtonAPressed() {
  return getAndClearFlag(btnA_pressed);
}

bool ButtonManager::isButtonBPressed() {
  return getAndClearFlag(btnB_shortPressed);
}

bool ButtonManager::isButtonCPressed() {
  return getAndClearFlag(btnC_pressed);
}

bool ButtonManager::isButtonBLongPress() {
  return getAndClearFlag(btnB_longPressed);
}

bool ButtonManager::isButtonCLongPress() {
  return getAndClearFlag(btnC_longPressed);
}

unsigned long ButtonManager::getButtonCPressTime() {
  if (btnC_lastState == LOW) {
    unsigned long now = millis();
    return (now >= btnC_pressStart) ? (now - btnC_pressStart) : (ULONG_MAX - btnC_pressStart + now + 1);
  }
  return 0;
}
