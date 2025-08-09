// File: mirror_module.cpp (updated)
#include "mirror_module.h"
#include <Arduino.h>
#include "mux_module.h"     // needed for read_channel

static const unsigned long LONG_PRESS_MS = 1000;
static bool prev_state = false;
static unsigned long press_time = 0;
static bool long_press_detected = false;
static bool long_press_consumed = false; // track if detection has been reported


void mirror_module::begin() {
  // Initialize previous state and reset flags
  prev_state = mux_module::read_channel(1);
  long_press_detected = false;
  long_press_consumed = false;
}

void mirror_module::update() {
  bool state = mux_module::read_channel(1);
  unsigned long now = millis();
  if (state && !prev_state) {
    // Button pressed: reset detection flags
    press_time = now;
    long_press_detected = false;
    long_press_consumed = false;
  } else if (state && !long_press_detected && (now - press_time >= LONG_PRESS_MS)) {
    // Long press detected once per press
    long_press_detected = true;
  } else if (!state && prev_state) {
    // Button released
    if (!long_press_detected) {
      Serial.println("Mirror short press");
    }
    // no further action; flags will reset on next press
  }
  prev_state = state;
}

bool mirror_module::pressedLong() {
  // Return and clear the long press detection once
  if (long_press_detected && !long_press_consumed) {
    long_press_consumed = true;
    return true;
  }
  return false;
}
