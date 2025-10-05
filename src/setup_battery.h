// =============================
// File: src/setup_battery.h
// =============================
#pragma once
#include <stdint.h>

namespace setup_battery {
  void begin();               // init I2C + MAX17048 and draw static label
  void update();              // draw % + bar if value changed (and charging blink)
  uint8_t readBatteryPercent();
  bool isCharging();          // true when VBUS sensed (if enabled)

  // Utilities for diagnostics / one-time reseed
  void oneShotQuickStart();   // call once (ideally off USB) to reseed gauge
  float readVoltage();        // convenience: MAX17048 cell voltage (V)
}