// =============================
// File: src/pinmap_module.cpp
// =============================
#include "pinmap_module.h"


// =============================
// File: src/mux_module.h
// =============================
#pragma once
#include <Arduino.h>

namespace mux_module {
  // Logical inputs that live on the 4051. Use these in higher-level code.
  enum class MuxInput : uint8_t {
    Stomp1,
    Stomp2,
    Stomp3,
    Stomp4,
    EncoderPush,
    ToggleUp,
    ToggleDown,
    Mirror,          // keep for compatibility; ignore if not wired
    COUNT
  };

  void begin();                          // Setup MUX control & COM pins
  void update();                         // Poll all channels; prints press events

  bool read_channel(uint8_t channel);    // raw physical (0..7), active LOW
  bool read_input(MuxInput input);       // logical, preferred

  void debug_scan_once();                // debug: print all 8 channel states (LOW=pressed)
}
