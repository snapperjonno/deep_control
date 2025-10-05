// =============================
// File: src/mux_module.h
// =============================
#pragma once
#include <Arduino.h>

namespace mux_module {
  enum class MuxInput : uint8_t {
    Stomp1,
    Stomp2,
    Stomp3,
    Stomp4,
    EncoderPush,
    ToggleUp,
    ToggleDown,
    Mirror,
    COUNT
  };

  void begin();
  void update();

  bool read_channel(uint8_t channel);  // raw physical (0..7), active LOW
  bool read_input(MuxInput input);     // logical, preferred
  void debug_scan_once();              // debug: print all 8 channel states (LOW=pressed)
}
