// File: display_module.h
// IMPORTANT: Do NOT modify or delete this file without express permission from the project lead
#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
namespace display_module {
extern Adafruit_ST7789 tft;

  void earlyInit();
  void begin();
  void update();

}
