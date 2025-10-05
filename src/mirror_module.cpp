// =============================
// File: src/mirror_module.cpp
// =============================
#include "mirror_module.h"
#include <Arduino.h>
#include "mux_module.h"

// Detect short/long presses on the logical "Mirror" input from the 4051.
// NOTE: This now uses the logical map (MuxInput::Mirror) rather than a hardcoded channel.

namespace {
  static const unsigned long LONG_PRESS_MS = 1000; // 1s long press threshold
  static bool prev_state = false;                  // last sampled level (active LOW via mux scanner)
  static unsigned long press_time = 0;             // millis at press edge
  static bool long_press_detected = false;         // latched when threshold crossed
  static bool long_press_consumed = false;         // cleared by pressedLong()

  inline bool mirror_now() {
    // Read the logical Mirror input (active LOW inside mux_module)
    return mux_module::read_input(mux_module::MuxInput::Mirror);
  }
}

void mirror_module::begin() {
  prev_state = mirror_now();
  long_press_detected = false;
  long_press_consumed = false;
  press_time = 0;
}

void mirror_module::update() {
  const bool state = mirror_now();
  const unsigned long now = millis();

  // Rising edge (pressed)
  if (state && !prev_state) {
    press_time = now;
    long_press_detected = false;
    long_press_consumed = false;
  }

  // While held, mark a long press once
  if (state && !long_press_detected) {
    if (press_time && (now - press_time >= LONG_PRESS_MS)) {
      long_press_detected = true;
      // (No Serial print here by design; app layer may act on pressedLong())
    }
  }

  // Falling edge (released)
  if (!state && prev_state) {
    if (!long_press_detected) {
      // Debug-only: short press intent on Mirror button
      Serial.println(F("Mirror short press"));
    }
    // If it was a long press, app can query pressedLong() once; we keep the flag
    // until consumed by pressedLong().
  }

  prev_state = state;
}

bool mirror_module::pressedLong() {
  if (long_press_detected && !long_press_consumed) {
    long_press_consumed = true;
    return true;
  }
  return false;
}
