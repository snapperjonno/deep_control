// =============================
// File: src/setup_battery.cpp
// =============================
#include "setup_battery.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "display_module.h"
#include "layout_constants.h"
#include "setup_module.h"
#include <Wire.h>
#include <Adafruit_MAX1704X.h>  // MAX17048 (0x36)

// --- Optional: USB VBUS sense wiring ---------------------------------------
#ifndef VBUS_SENSE_GPIO
  #define VBUS_SENSE_GPIO (11)   // using free GPIO11 (ADC2). Do NOT use 3/4 (I2C SDA/SCL)
#endif
#ifndef VBUS_USE_ANALOG
  #define VBUS_USE_ANALOG (1)    // 1=analogRead threshold, 0=digitalRead
#endif
#ifndef VBUS_ADC_THRESHOLD_COUNTS
  #define VBUS_ADC_THRESHOLD_COUNTS (1450) // ~1.2–1.3 V node => ~1500 counts @ 12-bit
#endif

#ifndef COLOR_ORANGE
  #define COLOR_ORANGE 0xFD20
#endif
#ifndef COLOR_BG
  #ifdef ST77XX_BLACK
    #define COLOR_BG ST77XX_BLACK
  #else
    #define COLOR_BG 0x0000
  #endif
#endif
#ifndef COLOR_TEXT
  #ifdef ST77XX_WHITE
    #define COLOR_TEXT ST77XX_WHITE
  #else
    #define COLOR_TEXT 0xFFFF
  #endif
#endif
#ifndef COLOR_LINE
  #ifdef ST77XX_WHITE
    #define COLOR_LINE ST77XX_WHITE
  #else
    #define COLOR_LINE 0xFFFF
  #endif
#endif
#ifndef COLOR_GREEN
  #ifdef ST77XX_GREEN
    #define COLOR_GREEN ST77XX_GREEN
  #else
    #define COLOR_GREEN 0x07E0
  #endif
#endif
#ifndef COLOR_RED
  #ifdef ST77XX_RED
    #define COLOR_RED ST77XX_RED
  #else
    #define COLOR_RED 0xF800
  #endif
#endif
#ifndef COLOR_BLACK
  #define COLOR_BLACK 0x0000
#endif

using display_module::tft;

// ---- Layout constants ------------------------------------------------------
#define NUM_X          SETUP_VALUE_X
#define NUM_Y          SETUP_VALUE_Y
#define BAR_X          SETUP_BAR_X
#define BAR_Y          SETUP_BAR_Y
#define BAR_WIDTH      SETUP_BAR_WIDTH
#define BAR_HEIGHT     SETUP_BAR_HEIGHT
#define SECTIONS       SETUP_BATTERY_SECTIONS
#define BOTTOM_Y       SETUP_BOTTOM_TEXT_Y

namespace {
  static bool     blink_on = false;
  static uint32_t blink_last_ms = 0;
  static int16_t  blink_prev_left = -1;
  static uint16_t blink_prev_w    = 0;

  Adafruit_MAX17048 max17;
  bool gauge_ok = false;
  bool i2c_init = false;
  uint8_t lastPct = 255;

  static constexpr int kStripeW = 10;
  static constexpr uint16_t kStripeColor = COLOR_BLACK;

  inline int interiorW() { return BAR_WIDTH - 2; }
  inline int baseX()     { return BAR_X + 1; }
  inline int maxX()      { return baseX() + interiorW() - 1; }

  static uint16_t colorForPixelIndex(int pixelIndexIntoBar) {
    const int sectW = (SECTIONS > 0) ? max(1, interiorW() / SECTIONS) : interiorW();
    const int idx   = pixelIndexIntoBar / sectW;
    return (idx == 0) ? COLOR_RED : (idx <= 1) ? COLOR_ORANGE : COLOR_GREEN;
  }

  void drawBottomLabel() {
    const char* BOTTOM_TEXT = "Battery %";
    tft.setFont(&OpenSans_SemiBold14pt7b);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(BOTTOM_TEXT, 0, BOTTOM_Y, &x1, &y1, &w, &h);
    setup_module::clearBetweenTriangles((int16_t)(y1 - 3), (int16_t)(y1 + h + 3));
    tft.setTextColor(COLOR_TEXT);
    tft.setCursor((int16_t)((int)tft.width() - (int)w) / 2, BOTTOM_Y);
    tft.print(BOTTOM_TEXT);
  }

  static void eraseOldNumber(uint8_t oldVal) {
    char oldBuf[4]; snprintf(oldBuf, sizeof(oldBuf), "%u", oldVal);
    int16_t obx, oby; uint16_t obw, obh;
    tft.getTextBounds(oldBuf, NUM_X, NUM_Y, &obx, &oby, &obw, &obh);
    tft.fillRect(obx - 1, oby - 1, obw + 2, obh + 2, COLOR_BG);
  }

  static void drawValue(uint8_t val) {
    tft.setCursor(NUM_X, NUM_Y);
    tft.print(val);
  }

  static void drawBarFrame() {
    tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, COLOR_LINE);
    for (int i = 1; i < SECTIONS; ++i) {
      int mx = BAR_X + (BAR_WIDTH * i) / SECTIONS;
      tft.drawLine(mx, BAR_Y, mx, BAR_Y + BAR_HEIGHT - 1, COLOR_LINE);
    }
  }

  static void drawDeltaBar(uint8_t pct) {
    const int totalW   = BAR_WIDTH - 1;
    const int oldPx    = (lastPct > 100) ? 0 : (totalW * (int)lastPct) / 100;
    const int newPx    = (totalW * (int)pct) / 100;

    if (newPx > oldPx) {
      for (int p = oldPx; p < newPx; ++p) {
        int xInterior = BAR_X + totalW - p;
        int x = xInterior;
        uint16_t col = colorForPixelIndex(p);
        tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, col);
      }
    } else if (newPx < oldPx) {
      for (int p = newPx; p < oldPx; ++p) {
        int x = BAR_X + totalW - p;
        tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, COLOR_BG);
      }
    }
    drawBarFrame();
  }

  static void repaintFillUnderStripe(int left, int w, uint8_t pct) {
    if (w <= 0) return;
    const int iW    = interiorW();
    const int bX    = baseX();
    const int right = min(left + w - 1, maxX());
    const int filledCols = (iW * (int)pct) / 100;
    const int boundaryX  = bX + (iW - filledCols);

    for (int x = left; x <= right; ++x) {
      if (x >= boundaryX && x <= maxX()) {
        int pixelIndexFromRight = (maxX() - x);
        uint16_t col = colorForPixelIndex(pixelIndexFromRight);
        tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, col);
      } else {
        tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, COLOR_BG);
      }
    }
    drawBarFrame();
  }

  static void clearBlinkOverlay() {
    if (blink_prev_left >= 0 && blink_prev_w > 0) {
      repaintFillUnderStripe(blink_prev_left, blink_prev_w, lastPct);
      blink_prev_left = -1;
      blink_prev_w    = 0;
    }
  }

  static void drawBlinkOverlay(uint8_t pct) {
    if (pct > 100) pct = 100;

    const int iW       = interiorW();
    const int bX       = baseX();
    const int filled   = (iW * (int)pct) / 100;
    if (filled <= 0) { clearBlinkOverlay(); return; }

    const int boundaryX = bX + (iW - filled);

    int left  = boundaryX;
    int width = min(kStripeW, maxX() - left + 1);

    if (blink_prev_left != left || blink_prev_w != (uint16_t)width) {
      clearBlinkOverlay();
    }

    for (int i = 0; i < width; ++i) {
      int x = left + i;
      tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, kStripeColor);
    }

    blink_prev_left = left;
    blink_prev_w    = (uint16_t)width;
  }

  static bool vbus_present() {
  #if (VBUS_SENSE_GPIO >= 0)
    #if VBUS_USE_ANALOG
      analogReadResolution(12);
      analogSetPinAttenuation(VBUS_SENSE_GPIO, ADC_6db);
      int raw = analogRead(VBUS_SENSE_GPIO);
      return raw >= VBUS_ADC_THRESHOLD_COUNTS;
    #else
      pinMode(VBUS_SENSE_GPIO, INPUT);
      return digitalRead(VBUS_SENSE_GPIO) == HIGH;
    #endif
  #else
    return false;
  #endif
  }
}

namespace setup_battery {

void begin() {
  #ifdef PIN_I2C_POWER
    pinMode(PIN_I2C_POWER, OUTPUT);
    digitalWrite(PIN_I2C_POWER, HIGH);
  #endif

  if (!i2c_init) { Wire.begin(); i2c_init = true; }
  if (!gauge_ok) { gauge_ok = max17.begin(); }

  #if (VBUS_SENSE_GPIO >= 0)
    #if !VBUS_USE_ANALOG
      pinMode(VBUS_SENSE_GPIO, INPUT);
    #endif
  #endif

  drawBottomLabel();
  lastPct = 255;
  blink_on = false; blink_last_ms = millis();
  blink_prev_left = -1; blink_prev_w = 0;
}

void update() {
  uint8_t pct = readBatteryPercent();

  if (pct != lastPct) {
    drawDeltaBar(pct);
    if (lastPct != 255) eraseOldNumber(lastPct);
    tft.setTextColor(COLOR_TEXT);
    drawValue(pct);
    clearBlinkOverlay();
    lastPct = pct;
  }

  const bool charging = isCharging();
  const bool full = (pct >= 99);
  if (charging && !full) {
    uint32_t now = millis();
    if (now - blink_last_ms >= 1000) {
      blink_last_ms = now;
      blink_on = !blink_on;
      if (!blink_on) {
        clearBlinkOverlay();
      } else {
        drawBlinkOverlay(pct);
      }
    }
  } else {
    if (blink_prev_left >= 0) { clearBlinkOverlay(); blink_on = false; }
  }
}

uint8_t readBatteryPercent() {
  static uint32_t lastReadMs = 0;
  static uint8_t  cachedPct  = 0;

  const uint32_t now = millis();
  if (lastReadMs == 0 || now - lastReadMs >= 1000) {
    lastReadMs = now;

    float percent = (float)cachedPct;
    if (gauge_ok) {
      percent = max17.cellPercent();
      if (percent < 0.0f)   percent = 0.0f;
      if (percent > 100.0f) percent = 100.0f;
    }
    cachedPct = (uint8_t)(percent + 0.5f);
  }
  return cachedPct;
}

bool isCharging() {
  return vbus_present();
}

void oneShotQuickStart() {
  if (gauge_ok) { max17.quickStart(); lastPct = 255; }
}

float readVoltage() {
  if (!gauge_ok) return NAN;
  return max17.cellVoltage();
}

} // namespace setup_battery
