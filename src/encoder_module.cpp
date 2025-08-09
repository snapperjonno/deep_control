// File: encoder_module.cpp
//
// encoder_module.cpp: Handles quadrature encoder reading and value updates.
// IMPORTANT: Do NOT modify this file.

#include "encoder_module.h"
#include <Arduino.h>
#include "pinmap_module.h"  // Use central pinmap

static long encoder_value = 0;
static int last_encoded = 0;
static long prev_display_value = LONG_MIN;

void encoder_module::begin() {
  pinMode(pinmap::ENC_PIN_A, INPUT_PULLUP);
  pinMode(pinmap::ENC_PIN_B, INPUT_PULLUP);
  last_encoded = (digitalRead(pinmap::ENC_PIN_A) << 1) | digitalRead(pinmap::ENC_PIN_B);
  encoder_value = 0;
  prev_display_value = LONG_MIN;
}

void encoder_module::update() {
  int msb = digitalRead(pinmap::ENC_PIN_A);
  int lsb = digitalRead(pinmap::ENC_PIN_B);
  int encoded = (msb << 1) | lsb;
  int sum = (last_encoded << 2) | encoded;
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    encoder_value++;
  } else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    encoder_value--;
  }
  last_encoded = encoded;

  long display_value = encoder_value / 4;
  if (display_value != prev_display_value) {
    Serial.print("Encoder value: ");
    Serial.println(display_value);
    prev_display_value = display_value;
  }
}
