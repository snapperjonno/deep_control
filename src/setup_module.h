// File: setup_module.h
#pragma once

#include <Arduino.h>  // for analogReadMilliVolts

namespace setup_module {
  // Total number of setup menu items
  static constexpr int MENU_ITEM_COUNT = 10;

  // Layout constants for battery screen
  static constexpr int SCREEN_W        = 240;
  static constexpr int HEADER_Y        = 20;
  static constexpr int LINE_MARGIN_X   = 10;
  static constexpr int LINE_Y          = 30;
  static constexpr int LINE_LENGTH     = 50;
  static constexpr int LINE_THICKNESS  = 2;
  static constexpr int BAR_X           = 20;
  static constexpr int BAR_Y           = 50;
  static constexpr int BAR_WIDTH       = 200;
  static constexpr int BAR_HEIGHT      = 20;
  static constexpr int SECTIONS        = 5;
  static constexpr int BOTTOM_Y        = 100;
  static constexpr int TRI_MARGIN_L    = 15;
  static constexpr int TRI_MARGIN_R    = 15;
  static constexpr int TRI_Y           = 110;
  static constexpr int TRI_SIDE        = 10;
  static constexpr int NUM_X           = 110;
  static constexpr int NUM_Y           = 80;

// Battery voltage thresholds in volts (for smoothed battery reading)
static constexpr float BATT_EMPTY_V = 3.45f;
static constexpr float BATT_FULL_V  = 4.05f;

  // Returns battery percent (0-100)
  uint8_t readBatteryPercent();

  // Index of the current menu item
  extern int currentMenuIndex;

  // Called once when entering setup mode
  void begin();

  // Called each loop to update the setup screen
  void update();
}