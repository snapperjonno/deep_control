// =============================
// File: src/setup_stomp_cc.h  (v1.1)
// =============================
#pragma once
#include <Arduino.h>

namespace setup_stomp_cc {
  void begin();

  // Screens
  void show_stomp_cc();           // Root view: S1..S4 labels on one row, values below
  void show_stomp_cc_select();    // Select which stomp (S1..S4)
  void show_stomp_cc_edit();      // Edit selected stomp CC (big red 0..127)

  // Input
  void on_encoder_turn(int8_t dir);
  void on_encoder_press();
  void on_toggle(int8_t dir);
}