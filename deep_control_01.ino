// File: deep_control_01.ino
#include <Arduino.h>
#include "src/module_manager.h"
#include "src/display_module.h"
#include "src/settings_module.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  settings_module::begin();   // <-- initialize NVS and load saved values (must be before any getters)
  mux_module::begin();
  mux_module::debug_scan_once();
  settings_module::dumpToSerial();
  display_module::earlyInit();
  module_manager::begin_all();

  // Debug: dump settings after modules have initialized preferences
  settings_module::dumpToSerial();
}

void loop() {
  module_manager::update_all();
  delay(1);
}