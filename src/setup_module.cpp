// =============================
// File: src/setup_module.cpp (updated for TFT)
// =============================
#include "setup_module.h"
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST77XX.h>
#include "display_module.h"
#include "mirror_module.h"
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "pinmap_module.h"
#include "layout_constants.h"
#include "setup_battery.h"
#include "setup_led.h"
#include "setup_tft.h"   // NEW

// Fallback for triangle Y if not provided by layout_constants.h
#ifndef TRI_Y
#define TRI_Y 117
#endif

#define COLOR_BG    ST77XX_BLACK
#define COLOR_TEXT  ST77XX_WHITE
#define COLOR_LINE  ST77XX_WHITE
#define COLOR_TRI   ST77XX_WHITE
#define HEADER_Y       20
#define LINE_Y         13
#define LINE_THICKNESS 2
#define LINE_MARGIN_X   10
#define LINE_LENGTH     50

namespace setup_module {

int  currentMenuIndex = 0;     
static bool inSetupMode = false;
static bool headerDrawn  = false;
static int8_t triMode    = -1; 

// runtime content-top derived from actual header text bounds to avoid clipping
static int s_contentTop = LINE_Y + LINE_THICKNESS + 2;

// Detail screens now include both LED and TFT brightness
static inline bool isDetail(int idx) { return (idx == 2) || (idx == 4); }

// Root indices in order of appearance
static inline int nextRoot(int idx) {
  // sequence: 0 -> 1 -> 3 -> 0
  if (idx <= 0) return 1;
  if (idx == 1) return 3;
  return 0; // for idx >= 3 or any other root, wrap to 0
}
static inline int prevRoot(int idx) {
  // reverse sequence: 0 <- 1 <- 3 <- 0
  if (idx <= 0) return 3;
  if (idx == 1) return 0;
  return 1; // idx >= 3 -> 1
}

static void drawStaticHeader() {
  // Clear a safe band behind header area
  display_module::tft.fillRect(0, 0, SCREEN_W, HEADER_Y + 16, COLOR_BG);

  display_module::tft.setFont(&OpenSans_SemiBold14pt7b);
  display_module::tft.setTextColor(COLOR_TEXT, COLOR_BG);

  const char* HEADER_TEXT = "setup";
  int16_t tx, ty; uint16_t tw, th;
  display_module::tft.getTextBounds(HEADER_TEXT, 0, HEADER_Y, &tx, &ty, &tw, &th);
  int16_t hx = (SCREEN_W - (int16_t)tw) / 2;

  // Compute a safe content-top below the header text AND lines to avoid clipping
  int headerBottom = ty + (int)th; // bottom of glyph bounds
  int linesBottom  = LINE_Y + LINE_THICKNESS;
  s_contentTop = max(linesBottom + 3, headerBottom + 3);

  // Draw top lines
  display_module::tft.fillRect(LINE_MARGIN_X, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);
  display_module::tft.fillRect(SCREEN_W - LINE_MARGIN_X - LINE_LENGTH, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);

  // Draw header text last so it's on top
  display_module::tft.setCursor(hx, HEADER_Y);
  display_module::tft.print(HEADER_TEXT);

  headerDrawn = true;
}

static void setTrianglesVisible(bool show) {
  const int16_t half = TRI_SIDE / 2;
  const int16_t stripTop = TRI_Y - half - 2;
  const int16_t stripH   = TRI_SIDE + 4;
  if ((triMode == 1 && show) || (triMode == 0 && !show)) return;
  // Always reset the strip before (re)drawing state
  display_module::tft.fillRect(0, stripTop, SCREEN_W, stripH, COLOR_BG);
  if (show) {
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
    triMode = 1;
  } else {
    triMode = 0;
  }
}

static void clearContent(bool preserveTriangles) {
  int16_t topStart = s_contentTop;
  const int16_t half = TRI_SIDE / 2;
  if (preserveTriangles && triMode == 1) {
    // Area above triangles (stop just before the triangle strip)
    int16_t top1 = topStart;
    int16_t h1   = (TRI_Y - half - 2) - top1;
    if (h1 > 0) display_module::tft.fillRect(0, top1, SCREEN_W, h1, COLOR_BG);

    // Area below triangles (start just after the strip)
    int16_t top2 = TRI_Y + half + 2;
    int16_t h2   = SCREEN_H - top2;
    if (h2 > 0) display_module::tft.fillRect(0, top2, SCREEN_W, h2, COLOR_BG);
  } else {
    display_module::tft.fillRect(0, topStart, SCREEN_W, SCREEN_H - topStart, COLOR_BG);
  }
}

void clearBetweenTriangles(int16_t yTop, int16_t yBottom) {
  if (yBottom < yTop) return;
  // Inner edges of the triangles
  const int16_t innerL = TRI_MARGIN_L + TRI_SIDE;            // just right of left triangle
  const int16_t innerR = SCREEN_W - TRI_MARGIN_R - TRI_SIDE; // just left of right triangle
  int16_t x = innerL + 1;                  // keep a 1px buffer next to the triangle edge
  int16_t w = (innerR - 1) - x + 1;        // up to 1px before the right triangle edge
  if (w <= 0) return;
  if (yTop < 0) yTop = 0;
  if (yBottom >= SCREEN_H) yBottom = SCREEN_H - 1;
  int16_t h = yBottom - yTop + 1;
  display_module::tft.fillRect(x, yTop, w, h, COLOR_BG);
}

static void showMenuIndex(int idx) {
  const bool wantTriangles = (idx != 2) && (idx != 4); // hide on brightness detail screens

  if (!headerDrawn) drawStaticHeader();

  // 1) Clear content first (preserving triangle strip)
  clearContent(/*preserveTriangles=*/wantTriangles);

  // 2) Draw triangles BEFORE the screen body so body never gets wiped after
  setTrianglesVisible(wantTriangles);

  // 3) Draw the screen body (battery/LED/TFT screens draw their bottom label)
  switch (idx) {
    case 0: setup_battery::begin(); break;
    case 1: setup_led::show_led(); break;
    case 2: setup_led::show_led_brightness(); break;
    case 3: setup_tft::show_tft(); break;                 // NEW
    case 4: setup_tft::show_tft_brightness(); break;      // NEW
    default: break;
  }
}

void begin() {
  inSetupMode = true;
  currentMenuIndex = 0;
  headerDrawn = false;
  triMode = -1;
  s_contentTop = LINE_Y + LINE_THICKNESS + 2; // reset; will be raised by drawStaticHeader()
  setup_led::begin();
  setup_tft::begin(); // NEW: init TFT PWM and load saved value
  showMenuIndex(currentMenuIndex);
}

void update() {
  if (mirror_module::pressedLong()) {
    begin();
  }
  if (!inSetupMode) return;

  switch (currentMenuIndex) {
    case 0: setup_battery::update(); break;
    case 1:
    case 2:
    case 3:
    case 4:
      break; // no periodic work for LED/TFT screens
    default: break;
  }
}

void onEncoderTurn(int8_t dir) {
  if (!inSetupMode || dir == 0) return;

  if (isDetail(currentMenuIndex)) {
    if (currentMenuIndex == 2) { setup_led::on_encoder_turn(dir); return; }
    if (currentMenuIndex == 4) { setup_tft::on_encoder_turn(dir); return; }
  }

  currentMenuIndex = (dir > 0) ? nextRoot(currentMenuIndex) : prevRoot(currentMenuIndex);
  showMenuIndex(currentMenuIndex);
}

void onEncoderPress() {
  if (!inSetupMode) return;

  if (currentMenuIndex == 1) {
    currentMenuIndex = 2; // enter LED detail
    showMenuIndex(currentMenuIndex);
    return;
  }

  if (currentMenuIndex == 2) {
    setup_led::on_encoder_press();
    currentMenuIndex = 1; // return to LED root
    showMenuIndex(currentMenuIndex);
    return;
  }

  if (currentMenuIndex == 3) {
    currentMenuIndex = 4; // enter TFT detail
    showMenuIndex(currentMenuIndex);
    return;
  }

  if (currentMenuIndex == 4) {
    setup_tft::on_encoder_press();
    currentMenuIndex = 3; // return to TFT root
    showMenuIndex(currentMenuIndex);
    return;
  }
}

void onToggle(int8_t dir) {
  if (!inSetupMode || dir == 0) return;

  if (isDetail(currentMenuIndex)) {
    if (currentMenuIndex == 2) { setup_led::on_toggle(dir); return; }
    if (currentMenuIndex == 4) { setup_tft::on_toggle(dir); return; }
  }

  onEncoderTurn(dir);
}

} // namespace setup_module
