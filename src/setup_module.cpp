// =============================
// File: src/setup_module.cpp
// =============================
#include "setup_module.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>                    // IMPORTANT: include before font
#include "display_module.h"
#include "mirror_module.h"
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"  // Uses GFXglyph; safe after Adafruit_GFX
#include "pinmap_module.h"
#include "layout_constants.h"
#include "setup_battery.h"

#define COLOR_BG    ST77XX_BLACK
#define COLOR_TEXT  ST77XX_WHITE
#define COLOR_LINE  ST77XX_WHITE
#define COLOR_TRI   ST77XX_WHITE
#define HEADER_Y       20   // unchanged
#define LINE_Y         13   // match original
#define LINE_THICKNESS 2    // match original

namespace setup_module {

int currentMenuIndex = 0; // 0 = setup_battery

static void drawSetupHeaderAndTriangles() {
  display_module::tft.setFont(&OpenSans_SemiBold14pt7b);
  display_module::tft.setTextColor(COLOR_TEXT, COLOR_BG);

  const char* HEADER_TEXT = "setup";
  int16_t tx, ty; uint16_t tw, th;
  display_module::tft.getTextBounds(HEADER_TEXT, 0, HEADER_Y, &tx, &ty, &tw, &th);
  int16_t hx = (SCREEN_W - (int16_t)tw) / 2;

  display_module::tft.fillScreen(COLOR_BG);
  display_module::tft.fillRect(LINE_MARGIN_X, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);
  display_module::tft.fillRect(SCREEN_W - LINE_MARGIN_X - LINE_LENGTH, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);

  display_module::tft.setCursor(hx, HEADER_Y);
  display_module::tft.print(HEADER_TEXT);

  int16_t half = TRI_SIDE / 2;
  display_module::tft.fillTriangle(
    TRI_MARGIN_L, TRI_Y,
    TRI_MARGIN_L + TRI_SIDE, TRI_Y - half,
    TRI_MARGIN_L + TRI_SIDE, TRI_Y + half,
    COLOR_TRI
  );
  display_module::tft.fillTriangle(
    SCREEN_W - TRI_MARGIN_R, TRI_Y,
    SCREEN_W - TRI_MARGIN_R - TRI_SIDE, TRI_Y - half,
    SCREEN_W - TRI_MARGIN_R - TRI_SIDE, TRI_Y + half,
    COLOR_TRI
  );
}

void begin() {
  currentMenuIndex = 0;
  drawSetupHeaderAndTriangles();
  setup_battery::begin();
}

void update() {
  static bool longPressHandled = false;
  if (mirror_module::pressedLong()) {
    if (!longPressHandled) {
      Serial.println("Mirror long press");
      longPressHandled = true;
    }
  } else {
    longPressHandled = false;
  }

  switch (currentMenuIndex) {
    case 0:
      setup_battery::update();
      break;
    default:
      break;
  }
}

uint8_t readBatteryPercent() {
  return setup_battery::readBatteryPercent();
}

} // namespace setup_module
