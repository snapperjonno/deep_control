// =============================
// File: src/setup_module.h (updated for TFT)
// =============================
#pragma once

#include <Arduino.h>

namespace setup_module {
  // Public state
  extern int currentMenuIndex; // 0 = setup_battery, 1 = setup_led, 2 = setup_led_brightness, 3 = setup_tft, 4 = setup_tft_brightness

  // Lifecycle
  void begin();   // called once when entering Setup mode
  void update();  // called regularly while in Setup mode

  // Input hooks for Setup mode (to be called by your input/router)
  void onEncoderTurn(int8_t dir);   // +1 / -1
  void onEncoderPress();            // press/release debounced in caller
  void onToggle(int8_t dir);        // treat like encoder turn (+1/-1)

  // Clear a horizontal band between the inner edges of the bottom triangles
  // without touching the triangles themselves. Y coordinates are inclusive.
  void clearBetweenTriangles(int16_t yTop, int16_t yBottom);
}

