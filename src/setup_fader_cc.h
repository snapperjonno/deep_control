// =============================
// File: src/setup_fader_cc.h  (v1.0)
// =============================
#pragma once
#include <Arduino.h>

namespace setup_fader_cc {
  void begin();

  // Screens
  void show_fader_cc();           // Root view: F1..F4 labels on one row, values below
  void show_fader_cc_select();    // Select which fader (F1..F4); may auto-fallback to single-item if too tight
  void show_fader_cc_edit();      // Edit selected fader CC (big red 0..127)

  // Input
  void on_encoder_turn(int8_t dir);
  void on_encoder_press();
  void on_toggle(int8_t dir);
}
