// File: src/setup_led.h
#pragma once
#include <Arduino.h>

namespace setup_led {
  void begin();
  void show_led();
  void show_led_brightness();
  void on_encoder_turn(int8_t dir);
  void on_encoder_press();
  void on_toggle(int8_t dir);
  uint8_t get_current();
  uint8_t get_edit();
  void apply_pwm();
  uint8_t load_brightness();
  void     save_brightness(uint8_t value);
}