// File: mirror_module.h
//
// mirror_module.h: Declares mirror module interface.
// IMPORTANT: Do NOT modify this file.
#pragma once

namespace mirror_module {
  void begin();  // Setup mirror button parameters
  void update(); // Poll mirror button for short/long press

  // Returns true if a long press (>= LONG_PRESS_MS) was detected
  bool pressedLong();
};