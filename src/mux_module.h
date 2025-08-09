// File: mux_module.h
//
// mux_module.h: Declares multiplexer module interface.
// IMPORTANT: Do NOT modify this file.
#pragma once
#include <Arduino.h>

namespace mux_module {
  void begin();    // Setup MUX pins
  void update();   // Poll MUX and emit Serial events
  bool read_channel(uint8_t ch);  // Public access for other modules
}