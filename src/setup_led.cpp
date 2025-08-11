// =============================
// File: src/setup_led.cpp
// =============================
#include "setup_led.h"
#include "pinmap_module.h"
#include "display_module.h"
#include <Adafruit_ST77XX.h>
#include <Arduino.h>
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "layout_constants.h"
#include "setup_module.h"   // for clearBetweenTriangles()

using display_module::tft;

// Perceptual LED curve (gamma ~2.6) with better top-end spread
const uint8_t LED_PWM_TABLE[21] = {
  0, 1, 2, 3, 5, 7, 11, 17, 24, 32, 42,
  54, 68, 83, 101, 121, 143, 167, 194, 223, 255
};

static uint8_t s_current = 20;
static uint8_t s_edit    = 20;
static uint8_t s_lastEditDraw = 255; // force first delta draw
static bool    s_inEdit  = false;

static inline uint8_t clamp020(int v) { return v < 0 ? 0 : (v > 20 ? 20 : (uint8_t)v); }

uint8_t setup_led::load_brightness() {
  uint8_t v = settings_module::getLedBrightness();
  return v > 20 ? 20 : v;
}
void setup_led::save_brightness(uint8_t value) {
  if (value > 20) value = 20;
  settings_module::setLedBrightness(value);
}

// --- Bar drawing helpers (consistent interior mapping, no fencepost artifacts) ---
static inline int interiorW() { return SETUP_BAR_WIDTH - 2; }
static inline int valueToCols(uint8_t v) { return (interiorW() * (int)v) / 20; }
static inline int colToX(int p) { return SETUP_BAR_X + 1 + (interiorW() - 1 - p); }

static void drawBarOutline(bool withDividers) {
  tft.drawRect(SETUP_BAR_X, SETUP_BAR_Y, SETUP_BAR_WIDTH, SETUP_BAR_HEIGHT, ST77XX_WHITE);
  if (withDividers) {
    for (int i = 1; i < SETUP_LED_SECTIONS; ++i) {
      tft.drawFastVLine(SETUP_BAR_X + (SETUP_BAR_WIDTH * i) / SETUP_LED_SECTIONS,
                        SETUP_BAR_Y, SETUP_BAR_HEIGHT, ST77XX_WHITE);
    }
  }
}

static void drawBarFull(uint8_t value, uint16_t fill, bool withDividers) {
  tft.fillRect(SETUP_BAR_X + 1, SETUP_BAR_Y + 1, interiorW(), SETUP_BAR_HEIGHT - 2, ST77XX_BLACK);
  int fillCols = valueToCols(value);
  if (fillCols > 0) {
    int x = SETUP_BAR_X + 1 + (interiorW() - fillCols);
    tft.fillRect(x, SETUP_BAR_Y + 1, fillCols, SETUP_BAR_HEIGHT - 2, fill);
  }
  drawBarOutline(withDividers);
}

// Delta draw: only touch changed pixel columns (mirror of battery logic, no stray 1px)
static void drawBarDelta(uint8_t oldValue, uint8_t newValue, uint16_t fill) {
  int oldCols = valueToCols(oldValue);
  int newCols = valueToCols(newValue);

  if (newCols > oldCols) {
    for (int p = oldCols; p < newCols; ++p) {
      int x = colToX(p);
      tft.drawFastVLine(x, SETUP_BAR_Y + 1, SETUP_BAR_HEIGHT - 2, fill);
    }
  } else if (newCols < oldCols) {
    for (int p = newCols; p < oldCols; ++p) {
      int x = colToX(p);
      tft.drawFastVLine(x, SETUP_BAR_Y + 1, SETUP_BAR_HEIGHT - 2, ST77XX_BLACK);
    }
  }

  drawBarOutline(true);
}

static void drawValue(uint8_t value) {
  int16_t bx, by; uint16_t bw, bh;
  // Clear a generous box that fits either numeric value or the word "OFF"
  tft.getTextBounds("OFF", SETUP_VALUE_X, SETUP_VALUE_Y, &bx, &by, &bw, &bh);
  int16_t nbx, nby; uint16_t nbw, nbh;
  tft.getTextBounds("999", SETUP_VALUE_X, SETUP_VALUE_Y, &nbx, &nby, &nbw, &nbh);
  int16_t cx = min(bx, nbx) - 2;
  int16_t cy = min(by, nby) - 2;
  uint16_t cw = max((int)bw, (int)nbw) + 4;
  uint16_t ch = max((int)bh, (int)nbh) + 4;
  tft.fillRect(cx, cy, cw, ch, ST77XX_BLACK);

  tft.setFont(&OpenSans_SemiBold14pt7b);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(SETUP_VALUE_X, SETUP_VALUE_Y);
  if (value == 0) {
    tft.print("OFF");
  } else {
    tft.print((int)value);
  }
}

// Clear only the band between triangles that covers the bottom label's glyph box
static void clearBottomBandFor(const char* txt) {
  tft.setFont(&OpenSans_SemiBold14pt7b);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, SETUP_BOTTOM_TEXT_Y, &x1, &y1, &w, &h);
  // Expand a touch for anti-aliased edges
  setup_module::clearBetweenTriangles(y1 - 3, (int16_t)(y1 + h + 3));
}

static void drawBottomLabel(const char* txt) {
  clearBottomBandFor(txt);
  tft.setFont(&OpenSans_SemiBold14pt7b);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, SETUP_BOTTOM_TEXT_Y, &x1, &y1, &w, &h);
  int16_t cx = (tft.width() - (int)w) / 2;
  tft.setCursor(cx, SETUP_BOTTOM_TEXT_Y);
  tft.print(txt);
}

void setup_led::begin() {
  pinMode(pinmap::LED_PWM, OUTPUT);
  s_current = clamp020(load_brightness());
  s_edit    = s_current;
  s_lastEditDraw = 255; // force first delta draw
  analogWrite(pinmap::LED_PWM, LED_PWM_TABLE[s_current]);
}

void setup_led::apply_pwm() { analogWrite(pinmap::LED_PWM, LED_PWM_TABLE[s_current]); }
uint8_t setup_led::get_current() { return s_current; }
uint8_t setup_led::get_edit()    { return s_edit; }

void setup_led::show_led() {
  s_inEdit = false;
  drawValue(s_current);
  drawBarFull(s_current, ST77XX_GREEN, true);
  drawBottomLabel("LEDs");
}

void setup_led::show_led_brightness() {
  s_inEdit = true;
  s_lastEditDraw = 255; // force full draw first time
  drawValue(s_edit);
  drawBarFull(s_edit, ST77XX_RED, true);
  drawBottomLabel("led brightness"); // draw once when entering edit
}

void setup_led::on_encoder_turn(int8_t dir) {
  if (!s_inEdit || dir == 0) return;
  uint8_t prev = s_edit;

  int step = (dir > 0 ? +1 : -1);
  uint8_t nextVal = clamp020((int)s_edit + step);

  if (nextVal != prev) {
    s_edit = nextVal;
    analogWrite(pinmap::LED_PWM, LED_PWM_TABLE[s_edit]);
    drawValue(s_edit);
    if (s_lastEditDraw == 255) {
      drawBarFull(s_edit, ST77XX_RED, true);
    } else {
      drawBarDelta(s_lastEditDraw, s_edit, ST77XX_RED);
    }
    s_lastEditDraw = s_edit;
  }
}

void setup_led::on_toggle(int8_t dir) { on_encoder_turn(dir); }

void setup_led::on_encoder_press() {
  if (!s_inEdit) { s_edit = s_current; show_led_brightness(); return; }
  s_current = s_edit;
  save_brightness(s_current);
  apply_pwm();
  show_led();
}