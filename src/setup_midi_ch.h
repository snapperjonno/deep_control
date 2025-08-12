// =============================
// File: src/setup_midi_ch.h  (v3)
// =============================
#pragma once
#include <Arduino.h>

namespace setup_midi_ch {
  void begin();
  void show_midi_ch();               // view both values (triangles + "MIDI channel" or shortened)
  void show_midi_ch_select();        // choose BLE/DIN (both label+value green when selected)
  void show_midi_ch_confirmation();  // edit chosen output's channel (big red 01..16, lifted triangles)

  void on_encoder_turn(int8_t dir);
  void on_encoder_press();
  void on_toggle(int8_t dir);
}