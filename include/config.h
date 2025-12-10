#pragma once
#include <Arduino.h>

// Hardware
#define AUDIO_DAC_PIN         25   // DAC1 (GPIO25)

#define THROTTLE_ADC_PIN      32   // example ADC pin (modify)
#define BUTTON_A_PIN          33
#define BUTTON_B_PIN          34
#define BUTTON_C_PIN          35

#define LED_1_PIN             4
#define LED_2_PIN             13
#define LED_3_PIN             14


// Playback buffer
#define AUDIO_RING_CAPACITY   (32*1024) // 32KB ring buffer - adjust memory vs performance

// Header JSON max size
#define AUDIO_HEADER_MAXLEN   512

// Timer for ISR (uses timer0)
#define TIMER_GROUP           0
#define TIMER_INDEX           0
