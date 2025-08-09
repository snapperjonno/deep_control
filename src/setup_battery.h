// =============================
// File: src/setup_battery.h
// =============================
#pragma once
#include <stdint.h>

namespace setup_battery {
  void begin();               // Called once when entering the battery screen
  void update();              // Called each loop to update the battery screen
  uint8_t readBatteryPercent();
  bool isCharging();
}