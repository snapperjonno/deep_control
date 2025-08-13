// =============================
// File: src/setup_mirror_delay.h
// =============================
#pragma once
#include <Arduino.h>

namespace setup_mirror_delay {
  void begin();                  // load from settings and init state
  void show_mirror();            // view current delay (green, bottom triangles at default)
  void show_mirror_select();     // edit delay (red, triangles raised to value centre)

  void on_encoder_turn(int8_t dir);
  void on_encoder_press();       // confirm/save when in edit screen
  void on_toggle(int8_t dir);    // alias of on_encoder_turn

  // Expose current/edit values in tenths for testing/telemetry (1..30)
  uint8_t get_current_tenths();
  uint8_t get_edit_tenths();
}
