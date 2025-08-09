// File: display_module.cpp
#include "display_module.h"
#include "pinmap_module.h"
#include <SPI.h>

Adafruit_ST7789 display_module::tft(pinmap::TFT_CS, pinmap::TFT_DC, pinmap::TFT_RST);

void display_module::earlyInit() {
  Serial.println("Early display init");
  SPI.begin(pinmap::TFT_SCK, -1, pinmap::TFT_MOSI, -1);
  tft.init(135, 240);
  tft.setRotation(3);
}

void display_module::begin() {
  Serial.println("Display begin");
  pinMode(pinmap::TFT_BL_PWM, OUTPUT);
  analogWrite(pinmap::TFT_BL_PWM, 255);
}

void display_module::update() {
  // Intentionally left blank
}
