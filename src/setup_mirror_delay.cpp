// =============================
// File: src/setup_mirror_delay.cpp
// =============================
#include "setup_mirror_delay.h"
#include <Arduino.h>
#include <math.h>
#include <Adafruit_ST77XX.h>
#include "display_module.h"
#include "settings_module.h"
#include "setup_module.h"        // clearBetweenTriangles()
#include "layout_constants.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "fonts/OpenSans_Regular20pt7b.h"  // value font per user spec

using display_module::tft;

// ---- FALLBACKS (compile-safe if not defined in layout_constants.h) ----
#ifndef TRI_Y
#define TRI_Y 117
#endif
#ifndef TRI_SIDE
#define TRI_SIDE 16
#endif
#ifndef TRI_MARGIN_L
#define TRI_MARGIN_L 10
#endif
#ifndef TRI_MARGIN_R
#define TRI_MARGIN_R 10
#endif
#ifndef SETUP_BOTTOM_TEXT_Y
#define SETUP_BOTTOM_TEXT_Y 120
#endif

namespace {
  // Values are stored as tenths of a second: 1..30  => 0.1s .. 3.0s
  static uint8_t s_current10 = 10; // default 1.0s
  static uint8_t s_edit10    = 10;
  static bool    s_inEdit    = false;

  inline uint8_t clamp10(int v) { return (v < 1) ? 1 : (v > 30 ? 30 : (uint8_t)v); }

  // Format "x.x sec"
  void format_secs10(uint8_t tenths, char* out, size_t sz) {
    uint8_t whole = tenths / 10;
    uint8_t tenths_only = tenths % 10;
    snprintf(out, sz, "%u.%u sec", (unsigned)whole, (unsigned)tenths_only);
  }

  // Bounds helper
  struct Bounds { int16_t x1; int16_t y1; uint16_t w; uint16_t h; };
  static void textBounds(const GFXfont* font, const char* txt, int16_t x, int16_t y, Bounds& b) {
    tft.setFont(font);
    tft.getTextBounds(txt, x, y, &b.x1, &b.y1, &b.w, &b.h);
  }
  static int16_t centerYForText(const GFXfont* font, const char* sample, int16_t baselineY) {
    Bounds b; textBounds(font, sample, 0, baselineY, b);
    int16_t top = b.y1;
    int16_t bottom = b.y1 + (int16_t)b.h;
    return top + (bottom - top)/2;
  }

  // Clear a box around centered text (manual clear)
  static void clearBoxForText(const GFXfont* font, const char* sample, int16_t xCenter, int16_t yBaseline, uint8_t pad = 6) {
    tft.setFont(font);
    Bounds b; textBounds(font, sample, 0, yBaseline, b);
    int16_t left = xCenter - (int16_t)b.w/2 - pad;
    int16_t top  = b.y1 - pad;
    int16_t w    = (int16_t)b.w + (pad * 2);
    int16_t h    = (int16_t)b.h + (pad * 2);
    if (w > 0 && h > 0) tft.fillRect(left, top, w, h, ST77XX_BLACK);
  }

  // Bottom caption draw (centered, leaving triangles intact)
  static void drawBottomCentered(const char* txt) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b;
    tft.setFont(f);
    int16_t y = SETUP_BOTTOM_TEXT_Y;
    Bounds b; textBounds(f, txt, 0, y, b);
    int16_t yTop = b.y1 - 3;
    int16_t yBot = (int16_t)(b.y1 + b.h) + 3;
    setup_module::clearBetweenTriangles(yTop, yBot);
    int16_t cx = (tft.width() - (int16_t)b.w) / 2;
    tft.setCursor(cx, y);
    tft.setTextColor(ST77XX_WHITE);
    tft.print(txt);
  }

  // Helpers to move triangles vertically for the edit screen
  static void drawSideTrianglesAtCenterY(int16_t centerY) {
    const int16_t half = TRI_SIDE / 2;
    const int16_t yTop = centerY - half - 2;
    const int16_t h    = TRI_SIDE + 4;
    // Clear left area only
    int16_t leftW = TRI_MARGIN_L + TRI_SIDE + 2;
    tft.fillRect(0, yTop, leftW, h, ST77XX_BLACK);
    // Clear right area only
    int16_t rightX = tft.width() - (TRI_MARGIN_R + TRI_SIDE + 2);
    int16_t rightW = TRI_MARGIN_R + TRI_SIDE + 2;
    tft.fillRect(rightX, yTop, rightW, h, ST77XX_BLACK);

    tft.fillTriangle(
      TRI_MARGIN_L, centerY,
      TRI_MARGIN_L + TRI_SIDE, centerY - half,
      TRI_MARGIN_L + TRI_SIDE, centerY + half,
      ST77XX_WHITE
    );
    tft.fillTriangle(
      tft.width() - TRI_MARGIN_R, centerY,
      tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY - half,
      tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY + half,
      ST77XX_WHITE
    );
  }
  static void eraseDefaultBottomTrianglesStrip() {
    const int16_t half = TRI_SIDE / 2;
    tft.fillRect(0, TRI_Y - half - 2, tft.width(), TRI_SIDE + 4, ST77XX_BLACK);
  }

  // Layout baselines
  inline int16_t valueBaseline_view()  { return 78; }  // fits under header lines
  inline int16_t valueBaseline_edit()  { return 78; }  // align EDIT screen value to same visual Y as VIEW (triangles will center on this)   // slightly lower to give room for raised triangles
}

// ---- Public API ----
void setup_mirror_delay::begin() {
  // Load from settings (seconds) and quantize to 0.1s steps within 0.1..3.0
  float s = settings_module::getMirrorDelay();
  if (!(s > 0.0f)) s = 1.0f;
  if (s < 0.1f) s = 0.1f; else if (s > 3.0f) s = 3.0f;
  uint8_t t10 = (uint8_t)roundf(s * 10.0f);
  if (t10 < 1) t10 = 1; if (t10 > 30) t10 = 30;
  s_current10 = s_edit10 = t10;
  s_inEdit = false;
}

void setup_mirror_delay::show_mirror() {
  s_inEdit = false;
  // Draw current value in GREEN at centre
  const GFXfont* fVal = &OpenSans_Regular20pt7b;
  tft.setFont(fVal);
  char buf[16]; format_secs10(s_current10, buf, sizeof(buf));
  // Clear and draw centred
  int16_t y = valueBaseline_view();
  clearBoxForText(fVal, "3.0 sec", tft.width()/2, y, /*pad=*/8);
  tft.setTextColor(ST77XX_GREEN);
  Bounds b; textBounds(fVal, buf, 0, y, b);
  tft.setCursor((tft.width() - (int16_t)b.w)/2, y);
  tft.print(buf);

  // Bottom caption with default triangles intact
  drawBottomCentered("Mirror delay");
}

void setup_mirror_delay::show_mirror_select() {
  s_inEdit = true;
  s_edit10 = s_current10; // start from current

  // Remove default bottom triangles and draw raised triangles centred on value text
  eraseDefaultBottomTrianglesStrip();
  int16_t y = valueBaseline_edit();
  int16_t centerY = centerYForText(&OpenSans_Regular20pt7b, "3.0 sec", y);
  drawSideTrianglesAtCenterY(centerY);

  // Draw editable value in RED
  const GFXfont* fVal = &OpenSans_Regular20pt7b;
  tft.setFont(fVal);
  char buf[16]; format_secs10(s_edit10, buf, sizeof(buf));
  clearBoxForText(fVal, "3.0 sec", tft.width()/2, y, /*pad=*/8);
  tft.setTextColor(ST77XX_RED);
  Bounds b; textBounds(fVal, buf, 0, y, b);
  tft.setCursor((tft.width() - (int16_t)b.w)/2, y);
  tft.print(buf);

  // Bottom caption (single draw; do not redraw during turns)
  drawBottomCentered("set mirror delay");
}

void setup_mirror_delay::on_encoder_turn(int8_t dir) {
  if (!s_inEdit || dir == 0) return;
  uint8_t prev = s_edit10;
  int step = (dir > 0 ? +1 : -1);
  s_edit10 = clamp10((int)s_edit10 + step);
  if (s_edit10 == prev) return;

  // Redraw only the value (triangles and bottom row unchanged)
  const GFXfont* fVal = &OpenSans_Regular20pt7b;
  int16_t y = valueBaseline_edit();
  char buf[16]; format_secs10(s_edit10, buf, sizeof(buf));
  clearBoxForText(fVal, "3.0 sec", tft.width()/2, y, /*pad=*/8);
  tft.setTextColor(ST77XX_RED);
  Bounds b; textBounds(fVal, buf, 0, y, b);
  tft.setCursor((tft.width() - (int16_t)b.w)/2, y);
  tft.print(buf);
}

void setup_mirror_delay::on_toggle(int8_t dir) { on_encoder_turn(dir); }

void setup_mirror_delay::on_encoder_press() {
  if (!s_inEdit) { show_mirror_select(); return; }
  // Commit
  s_current10 = s_edit10;
  settings_module::setMirrorDelay((float)s_current10 / 10.0f);
  // Return to view screen
  show_mirror();
}

uint8_t setup_mirror_delay::get_current_tenths() { return s_current10; }
uint8_t setup_mirror_delay::get_edit_tenths()    { return s_edit10; }
