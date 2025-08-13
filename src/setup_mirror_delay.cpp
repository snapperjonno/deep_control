// =============================
// File: src/setup_mirror_delay_v1.4.cpp — shift left digit + dot left 4px, protect dot
// =============================
// Changes from v1.3:
// • Move the WHOLE digit and the decimal '.' 4px to the LEFT to open a gap
//   between '.' and the right (tenths) digit.
// • Keep the dot fenced so digit clears never wipe it.
// • '.' and " sec" still draw once; only digit cells redraw on change.

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

  // Bounds helper
  struct Bounds { int16_t x1; int16_t y1; uint16_t w; uint16_t h; };
  static void textBounds(const GFXfont* font, const char* txt, int16_t x, int16_t y, Bounds& b) {
    tft.setFont(font); tft.getTextBounds(txt, x, y, &b.x1, &b.y1, &b.w, &b.h);
  }
  static int16_t centerYForText(const GFXfont* font, const char* sample, int16_t baselineY) {
    Bounds b; textBounds(font, sample, 0, baselineY, b); return b.y1 + (int16_t)b.h/2; }

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

  // Helpers to move triangles vertically for the edit screen
  static void drawSideTrianglesAtCenterY(int16_t centerY) {
    const int16_t half = TRI_SIDE / 2; const int16_t yTop = centerY - half - 2; const int16_t h = TRI_SIDE + 4;
    int16_t leftW = TRI_MARGIN_L + TRI_SIDE + 2; tft.fillRect(0, yTop, leftW, h, ST77XX_BLACK);
    int16_t rightX = tft.width() - (TRI_MARGIN_R + TRI_SIDE + 2); int16_t rightW = TRI_MARGIN_R + TRI_SIDE + 2; tft.fillRect(rightX, yTop, rightW, h, ST77XX_BLACK);
    tft.fillTriangle(TRI_MARGIN_L, centerY, TRI_MARGIN_L + TRI_SIDE, centerY - half, TRI_MARGIN_L + TRI_SIDE, centerY + half, ST77XX_WHITE);
    tft.fillTriangle(tft.width() - TRI_MARGIN_R, centerY, tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY - half, tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY + half, ST77XX_WHITE);
  }
  static void eraseDefaultBottomTrianglesStrip() { const int16_t half = TRI_SIDE / 2; tft.fillRect(0, TRI_Y - half - 2, tft.width(), TRI_SIDE + 4, ST77XX_BLACK); }

  // Layout baselines
  inline int16_t valueBaseline_view()  { return 78; }
  inline int16_t valueBaseline_edit()  { return 78; }

  // ---------- Flicker-free segmented value layout for "x.x sec" ----------
  static constexpr int16_t kLeftNudgePx = 4; // move WHOLE digit and '.' left by this amount

  struct SegLayout {
    int16_t baseY;        // value baseline
    int16_t cellW;        // fixed digit cell width (max of '0'..'9')
    int16_t dotW;         // width of "."
    Bounds  bDigitMax;    // max digit bounds at baseY
    Bounds  bDot;         // bounds of "." at baseY
    Bounds  bSec;         // bounds of " sec" at baseY
    int16_t groupW;       // width of [D][.][d] + bSec.w
    int16_t groupLeft;    // left X of whole group (centered)
    int16_t xWhole;       // left of WHOLE digit cell
    int16_t xDot;         // left of '.'
    int16_t xTenths;      // left of TENTHS digit cell
    int16_t xSec;         // left of " sec"
  };
  static SegLayout s_seg; static bool s_segBuilt = false;

  static void buildSegLayout(SegLayout& L, int16_t baseY) {
    L.baseY = baseY; const GFXfont* f = &OpenSans_Regular20pt7b; tft.setFont(f);
    // Max digit width
    int16_t maxW = 0; Bounds b; for(char d='0'; d<='9'; ++d){ char s[2]={d,0}; textBounds(f, s, 0, baseY, b); if((int16_t)b.w>maxW) maxW = (int16_t)b.w; }
    L.cellW = maxW + 2; // +2px breathing room
    textBounds(f, ".", 0, baseY, L.bDot); L.dotW = (int16_t)L.bDot.w;
    textBounds(f, "8", 0, baseY, L.bDigitMax);
    textBounds(f, " sec", 0, baseY, L.bSec);

    L.groupW = L.cellW + L.dotW + L.cellW + (int16_t)L.bSec.w; // D + . + d + " sec"
    L.groupLeft = (tft.width() - L.groupW)/2;

    // Base positions
    L.xWhole  = L.groupLeft;
    L.xDot    = L.xWhole + L.cellW;
    L.xTenths = L.xDot + L.dotW;
    L.xSec    = L.xTenths + L.cellW;

    // Nudge WHOLE and DOT left to open a gap before the right digit
    L.xWhole -= kLeftNudgePx;
    L.xDot   -= kLeftNudgePx;
  }

  static void drawStaticParts_Select(){
    const GFXfont* f = &OpenSans_Regular20pt7b; tft.setFont(f); tft.setTextColor(ST77XX_RED);
    // '.' (draw once)
    tft.fillRect(s_seg.xDot-2, s_seg.bDot.y1-6, s_seg.dotW+4, (int16_t)s_seg.bDot.h+12, ST77XX_BLACK);
    tft.setCursor(s_seg.xDot, s_seg.baseY); tft.print(".");
    // " sec" (draw once)
    tft.fillRect(s_seg.xSec-2, s_seg.bSec.y1-6, (int16_t)s_seg.bSec.w+4, (int16_t)s_seg.bSec.h+12, ST77XX_BLACK);
    tft.setCursor(s_seg.xSec, s_seg.baseY); tft.print(" sec");
  }

  // Clear only the digit cell, but never over the '.' area
  static void clearDigitCellSafely(bool isWhole){
    // Base clear box for a digit cell
    const int16_t padT=8, padB=10, padL=2, padR=4;
    int16_t x = isWhole ? s_seg.xWhole : s_seg.xTenths;
    int16_t left  = x - padL;
    int16_t right = x + s_seg.cellW + padR; // exclusive
    // Guard the dot area: [xDot, xDot + dotW)
    int16_t dotL = s_seg.xDot - 1;               // tiny safety margin
    int16_t dotR = s_seg.xDot + s_seg.dotW + 1;  // tiny safety margin
    if (isWhole && right > dotL)   right = dotL;     // don't cross into dot
    if (!isWhole && left  < dotR)  left  = dotR;     // don't cross into dot
    if (right > left) {
      tft.fillRect(left, s_seg.bDigitMax.y1 - padT, right - left, (int16_t)s_seg.bDigitMax.h + padT + padB, ST77XX_BLACK);
    }
  }

  static void drawDigit(bool isWhole, uint8_t digit){
    const GFXfont* f = &OpenSans_Regular20pt7b; tft.setFont(f); tft.setTextColor(ST77XX_RED);
    clearDigitCellSafely(isWhole);
    int16_t x = isWhole ? s_seg.xWhole : s_seg.xTenths; char s[2]={(char)('0'+digit),0}; tft.setCursor(x, s_seg.baseY); tft.print(s);
  }

  static void drawValueSegmented(uint8_t tenths, int prev){
    uint8_t whole = tenths/10; uint8_t frac = tenths%10; int p = (prev<0 ? -1 : prev);
    uint8_t pWhole = (p<0?255:(uint8_t)(p/10)); uint8_t pFrac = (p<0?255:(uint8_t)(p%10));
    if(whole != pWhole) drawDigit(true,  whole);
    if(frac  != pFrac ) drawDigit(false, frac );
  }

  // Bottom caption draw (centered, leaving triangles intact)
  static void drawBottomCentered(const char* txt) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f);
    int16_t y = SETUP_BOTTOM_TEXT_Y; Bounds b; textBounds(f, txt, 0, y, b);
    int16_t yTop = b.y1 - 3, yBot = (int16_t)(b.y1 + b.h) + 3; setup_module::clearBetweenTriangles(yTop, yBot);
    int16_t cx = (tft.width() - (int16_t)b.w) / 2; tft.setCursor(cx, y); tft.setTextColor(ST77XX_WHITE); tft.print(txt);
  }
}

// ---- Public API ----
void setup_mirror_delay::begin() {
  float s = settings_module::getMirrorDelay(); if (!(s > 0.0f)) s = 1.0f; if (s < 0.1f) s = 0.1f; else if (s > 3.0f) s = 3.0f;
  uint8_t t10 = (uint8_t)roundf(s * 10.0f); if (t10 < 1) t10 = 1; if (t10 > 30) t10 = 30; s_current10 = s_edit10 = t10; s_inEdit = false;
}

void setup_mirror_delay::show_mirror() {
  s_inEdit = false; const GFXfont* fVal = &OpenSans_Regular20pt7b; tft.setFont(fVal);
  // Draw centered full string once (VIEW is static anyway)
  char buf[16]; uint8_t whole=s_current10/10, frac=s_current10%10; snprintf(buf,sizeof(buf),"%u.%u sec",(unsigned)whole,(unsigned)frac);
  int16_t y = valueBaseline_view(); Bounds b; textBounds(fVal, buf, 0, y, b); tft.fillRect((tft.width()-(int16_t)b.w)/2-6, b.y1-8, (int16_t)b.w+12, (int16_t)b.h+16, ST77XX_BLACK);
  tft.setTextColor(ST77XX_GREEN); tft.setCursor((tft.width()-(int16_t)b.w)/2, y); tft.print(buf);
  drawBottomCentered("Mirror delay");
}

void setup_mirror_delay::show_mirror_select() {
  s_inEdit = true; s_edit10 = s_current10; eraseDefaultBottomTrianglesStrip();
  int16_t y = valueBaseline_edit(); int16_t centerY = centerYForText(&OpenSans_Regular20pt7b, "3.0 sec", y); drawSideTrianglesAtCenterY(centerY);
  // Build segmented layout and draw static pieces once
  buildSegLayout(s_seg, y); s_segBuilt = true; drawStaticParts_Select();
  // Draw initial digits using segmented method
  drawValueSegmented(s_edit10, -1);
  drawBottomCentered("set Mirror delay");
}

void setup_mirror_delay::on_encoder_turn(int8_t dir) {
  if (!s_inEdit || dir == 0) return; uint8_t prev = s_edit10; int step = (dir > 0 ? +1 : -1); s_edit10 = clamp10((int)s_edit10 + step); if (s_edit10 == prev) return;
  // Redraw only the digit(s) that changed; dot/tail remain
  drawValueSegmented(s_edit10, prev);
}

void setup_mirror_delay::on_toggle(int8_t dir) { on_encoder_turn(dir); }

void setup_mirror_delay::on_encoder_press() {
  if (!s_inEdit) { show_mirror_select(); return; }
  s_current10 = s_edit10; settings_module::setMirrorDelay((float)s_current10 / 10.0f); show_mirror();
}

uint8_t setup_mirror_delay::get_current_tenths() { return s_current10; }
uint8_t setup_mirror_delay::get_edit_tenths()    { return s_edit10; }
