// =============================
// File: src/setup_tft.cpp
// =============================
#include "setup_tft.h"
#include "pinmap_module.h"
#include "display_module.h"
#include <Adafruit_ST77XX.h>
#include <Arduino.h>
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "layout_constants.h"
#include "setup_module.h"   // for clearBetweenTriangles()

using display_module::tft;

// Perceptual PWM table (1..20). No true OFF to avoid lockout.
// Index 0 is unused. Index 1 is a minimal but visible glow; 20 is full.
static const uint8_t TFT_PWM_TABLE[21] = {
  /*0 (unused)*/ 0,
  /*1*/ 2, 3, 6, 10, 15, 22, 30, 40, 52, 68,
  /*11*/ 86, 108, 134, 164, 190, 208, 224, 238, 248, 255
};

// Local state
static uint8_t s_current = 20;     // committed value 1..20
static uint8_t s_edit    = 20;     // edit cursor value 1..20
static uint8_t s_lastEditDraw = 255; // sentinel to force first delta draw
static bool    s_inEdit  = false;

static inline uint8_t clamp120(int v) { return v < 1 ? 1 : (v > 20 ? 20 : (uint8_t)v); }

uint8_t setup_tft::load_brightness() {
  uint8_t v = settings_module::getTftBrightness();
  if (v < 1) v = 1;  // migrate legacy 0 → 1
  if (v > 20) v = 20;
  return v;
}
void setup_tft::save_brightness(uint8_t value) {
  if (value < 1) value = 1;
  if (value > 20) value = 20;
  settings_module::setTftBrightness(value);
}

// --- Bar drawing helpers (consistent interior mapping, no fencepost artifacts) ---
static inline int interiorW() { return SETUP_BAR_WIDTH - 2; }
static inline int valueToCols(uint8_t v) { return (interiorW() * (int)v) / 20; }
static inline int colToX(int p) { return SETUP_BAR_X + 1 + (interiorW() - 1 - p); }

static void drawBarOutline(bool withDividers) {
  tft.drawRect(SETUP_BAR_X, SETUP_BAR_Y, SETUP_BAR_WIDTH, SETUP_BAR_HEIGHT, ST77XX_WHITE);
  if (withDividers) {
    for (int i = 1; i < SETUP_LED_SECTIONS; ++i) { // reuse LED divider count
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

// Delta draw: only touch changed pixel columns
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
  // Always show numeric 1..20; no OFF state on TFT
  tft.setFont(&OpenSans_SemiBold14pt7b);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  // Clear a region large enough for up to 3 digits
  int16_t nbx, nby; uint16_t nbw, nbh;
  tft.getTextBounds("999", SETUP_VALUE_X, SETUP_VALUE_Y, &nbx, &nby, &nbw, &nbh);
  tft.fillRect(nbx - 2, nby - 2, nbw + 6, nbh + 6, ST77XX_BLACK);

  char buf[8];
  snprintf(buf, sizeof(buf), "%u", (unsigned)value);
  tft.setCursor(SETUP_VALUE_X, SETUP_VALUE_Y);
  tft.print(buf);
}

static void drawBottomLabel(const char* txt) {
  // Use OpenSans to match other screens
  tft.setFont(&OpenSans_SemiBold14pt7b);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(txt, 0, SETUP_BOTTOM_TEXT_Y, &x1, &y1, &w, &h);

  // Clear a tight band behind the text without touching triangles
  int16_t top = y1 - 2;
  int16_t bottom = y1 + (int16_t)h + 2;
  setup_module::clearBetweenTriangles(top, bottom);

  const int cx = (SCREEN_W - (int)w) / 2;
  tft.setCursor(cx, SETUP_BOTTOM_TEXT_Y);
  tft.print(txt);
}

void setup_tft::begin() {
  pinMode(pinmap::TFT_BL_PWM, OUTPUT);
  s_current = clamp120(load_brightness());
  s_edit    = s_current;
  s_lastEditDraw = 255; // force first delta draw
  analogWrite(pinmap::TFT_BL_PWM, TFT_PWM_TABLE[s_current]);
}

void setup_tft::apply_pwm() { analogWrite(pinmap::TFT_BL_PWM, TFT_PWM_TABLE[s_current]); }
uint8_t setup_tft::get_current() { return s_current; }
uint8_t setup_tft::get_edit()    { return s_edit; }

void setup_tft::show_tft() {
  s_inEdit = false;
  drawValue(s_current);
  drawBarFull(s_current, ST77XX_GREEN, true);
  drawBottomLabel("display");
}

void setup_tft::show_tft_brightness() {
  s_inEdit = true;
  s_lastEditDraw = 255; // force full draw first time
  drawValue(s_edit);
  drawBarFull(s_edit, ST77XX_RED, true);
  drawBottomLabel("screen brightness");
}

void setup_tft::on_encoder_turn(int8_t dir) {
  if (!s_inEdit || dir == 0) return;
  uint8_t prev = s_edit;

  int step = (dir > 0 ? +1 : -1);
  uint8_t nextVal = clamp120((int)s_edit + step);

  if (nextVal != prev) {
    s_edit = nextVal;
    analogWrite(pinmap::TFT_BL_PWM, TFT_PWM_TABLE[s_edit]); // live preview
    drawValue(s_edit);
    if (s_lastEditDraw == 255) {
      drawBarFull(s_edit, ST77XX_RED, true);
    } else {
      drawBarDelta(s_lastEditDraw, s_edit, ST77XX_RED);
    }
    s_lastEditDraw = s_edit;
  }
}

void setup_tft::on_toggle(int8_t dir) { on_encoder_turn(dir); }

void setup_tft::on_encoder_press() {
  if (!s_inEdit) { s_edit = s_current; show_tft_brightness(); return; }
  s_current = s_edit;
  save_brightness(s_current);
  apply_pwm();
  show_tft();
}