#include <Arduino.h>
#include "src/module_manager.h"
#include "src/display_module.h"

void setup() {
  Serial.begin(115200);
  delay(200);

  display_module::earlyInit();      // correct early SPI
  module_manager::begin_all();      // now safely triggers setup_module::begin()
}

void loop() {
  module_manager::update_all();
  delay(1);
}
