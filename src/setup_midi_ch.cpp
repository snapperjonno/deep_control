// =============================
// File: src/setup_midi_ch_v4.0.cpp — confirmation screen: fixed cells, no movement/flicker
// =============================
// What changed vs your v3.9.2:
// • Screen 3 (confirmation / EDIT_CH): big red value now uses TWO fixed-width digit
//   cells (max glyph width for 0..9) so the number never shifts as it changes.
//   Only the digit(s) that actually change are redrawn on encoder turns.
// • VIEW and SELECT screens unchanged in appearance; minor internal refactor to
//   call the new fixed-cell renderer on confirmation.

// Leave on while validating saves; comment out when done
#define MIDI_CH_DEBUG_SAVE 1

#include "setup_midi_ch.h"
#include "display_module.h"
#include <Adafruit_ST77XX.h>
#include <Arduino.h>
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "fonts/OpenSans_Regular24pt7b.h" // big red digits on confirmation
#include "layout_constants.h"
#include "setup_module.h"   // clearBetweenTriangles()

using display_module::tft;

// ---- FALLBACKS ----
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
  enum class State : uint8_t { VIEW, SELECT_OUTPUT, EDIT_CH };
  enum class OutputSel : uint8_t { BLE = 0, DIN = 1 };

  State      s_state = State::VIEW;
  OutputSel  s_selected = OutputSel::BLE;
  uint8_t    s_ble = 1;   // 1..16
  uint8_t    s_din = 1;   // 1..16
  uint8_t    s_edit = 1;  // current edit cursor (1..16)

  // ---- layout helpers ----
  inline int16_t colLX() { return 18; }
  inline int16_t colRX() { return tft.width() - 18; }
  inline int16_t labelY() { return 54; }
  inline int16_t valueY() { return 90; }
  inline int16_t bottomY() { return SETUP_BOTTOM_TEXT_Y; }

  // Confirmation screen baseline: align with mirror_delay_select
  inline int16_t valueBaseline_confirm() { return 80; }
  inline int16_t valueXNudge_confirm() { return -3; } // negative = left, positive = right

  static constexpr int LABEL_INSET = 28; // 

  struct Bounds { int16_t x1; int16_t y1; uint16_t w; uint16_t h; };

  inline int16_t iMin3(int16_t a, int16_t b, int16_t c) { return min(a, min(b, c)); }
  inline int16_t iMax3(int16_t a, int16_t b, int16_t c) { return max(a, max(b, c)); }

  void textBounds(const GFXfont* font, const char* txt, int16_t x, int16_t y, Bounds& b) {
    tft.setFont(font); tft.getTextBounds(txt, x, y, &b.x1, &b.y1, &b.w, &b.h);
  }

  void drawCentered(const GFXfont* font, const char* txt, int16_t y, uint16_t color) {
    tft.setFont(font); tft.setTextColor(color); Bounds b; textBounds(font, txt, 0, y, b);
    int16_t cx = (tft.width() - (int16_t)b.w) / 2; tft.setCursor(cx, y); tft.print(txt);
  }

  int16_t centerYForText(const GFXfont* font, const char* sample, int16_t baselineY) {
    Bounds b; textBounds(font, sample, 0, baselineY, b); return b.y1 + (int16_t)b.h/2; }

  void clearBoxForText(const GFXfont* font, const char* sample, int16_t xCenter, int16_t yBaseline, uint8_t pad = 6) {
    tft.setFont(font); Bounds b; textBounds(font, sample, 0, yBaseline, b);
    int16_t left = xCenter - (int16_t)b.w/2 - pad; int16_t top  = b.y1 - pad;
    int16_t w    = (int16_t)b.w + (pad * 2); int16_t h    = (int16_t)b.h + (pad * 2);
    if (w > 0 && h > 0) tft.fillRect(left, top, w, h, ST77XX_BLACK);
  }

  // Draw BLE/DIN labels and return their centre X coordinates
  void drawLabels(uint16_t colBle, uint16_t colDin, int16_t& cxBle, int16_t& cxDin) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f);
    const char* LBLE = "BT"; Bounds bbBle; textBounds(f, LBLE, 0, labelY(), bbBle);
    int16_t xBle = colLX() + LABEL_INSET; clearBoxForText(f, "BT", xBle + (int16_t)bbBle.w/2, labelY());
    tft.setTextColor(colBle); tft.setCursor(xBle, labelY()); tft.print(LBLE); cxBle = xBle + (int16_t)bbBle.w/2;

    const char* LDIN = "DIN"; Bounds bbDin; textBounds(f, LDIN, 0, labelY(), bbDin);
    int16_t xDinRight = colRX() - LABEL_INSET; int16_t xDin = xDinRight - (int16_t)bbDin.w;
    clearBoxForText(f, "DIN", xDin + (int16_t)bbDin.w/2, labelY());
    tft.setTextColor(colDin); tft.setCursor(xDin, labelY()); tft.print(LDIN); cxDin = xDin + (int16_t)bbDin.w/2;
  }

  void drawValues(uint8_t ble, uint8_t din, int16_t cxBle, int16_t cxDin, bool hiBle, bool hiDin) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f); char buf[4];
    snprintf(buf, sizeof(buf), "%02u", (unsigned)ble); clearBoxForText(f, "88", cxBle, valueY());
    tft.setTextColor(hiBle ? ST77XX_GREEN : ST77XX_WHITE); Bounds bBle; textBounds(f, buf, 0, valueY(), bBle);
    tft.setCursor(cxBle - (int16_t)bBle.w/2, valueY()); tft.print(buf);
    snprintf(buf, sizeof(buf), "%02u", (unsigned)din); clearBoxForText(f, "88", cxDin, valueY());
    tft.setTextColor(hiDin ? ST77XX_GREEN : ST77XX_WHITE); Bounds bDin; textBounds(f, buf, 0, valueY(), bDin);
    tft.setCursor(cxDin - (int16_t)bDin.w/2, valueY()); tft.print(buf);
  }

  void drawBottomAuto(const char* full, const char* shortTxt) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f);
    int16_t innerL = TRI_MARGIN_L + TRI_SIDE + 1; int16_t innerR = tft.width() - (TRI_MARGIN_R + TRI_SIDE + 1); int16_t maxW = innerR - innerL;
    Bounds bFull, bShort; textBounds(f, full, 0, bottomY(), bFull); textBounds(f, shortTxt, 0, bottomY(), bShort);
    const char* use = ((int16_t)bFull.w + 4 > maxW) ? shortTxt : full; Bounds* bUse = ((int16_t)bFull.w + 4 > maxW) ? &bShort : &bFull;
    int16_t yTop = bUse->y1 - 3; int16_t yBot = (int16_t)(bUse->y1 + bUse->h) + 3; setup_module::clearBetweenTriangles(yTop, yBot);
    drawCentered(f, use, bottomY(), ST77XX_WHITE);
  }

  void drawBottomSelectTagColored(bool isBle) {
    const GFXfont* f = &OpenSans_SemiBold14pt7b; const int16_t y = bottomY(); tft.setFont(f);
    const char* lead = "set "; const char* tag  = isBle ? " BT MIDI " : " DIN MIDI "; const char* tail = " ch";
    Bounds bLead, bTag, bTail; textBounds(f, lead, 0, y, bLead); textBounds(f, tag,  0, y, bTag); textBounds(f, tail, 0, y, bTail);
    int16_t totalW = (int16_t)bLead.w + (int16_t)bTag.w + (int16_t)bTail.w; int16_t startX = (tft.width() - totalW) / 2;
    int16_t yTop = iMin3(bLead.y1, bTag.y1, bTail.y1) - 3; int16_t yBot = iMax3((int16_t)(bLead.y1 + bLead.h), (int16_t)(bTag.y1 + bTag.h), (int16_t)(bTail.y1 + bTail.h)) + 3;
    setup_module::clearBetweenTriangles(yTop, yBot);
    tft.setCursor(startX, y);                               tft.setTextColor(ST77XX_WHITE); tft.print(lead);
    tft.setCursor(startX + (int16_t)bLead.w, y);            tft.setTextColor(ST77XX_GREEN); tft.print(tag);
    tft.setCursor(startX + (int16_t)bLead.w + (int16_t)bTag.w, y); tft.setTextColor(ST77XX_WHITE); tft.print(tail);
  }

  void drawSideTrianglesAtCenterY(int16_t centerY) {
    const int16_t half = TRI_SIDE / 2; const int16_t yTop = centerY - half - 2; const int16_t h = TRI_SIDE + 4;
    int16_t leftW = TRI_MARGIN_L + TRI_SIDE + 2; tft.fillRect(0, yTop, leftW, h, ST77XX_BLACK);
    int16_t rightX = tft.width() - (TRI_MARGIN_R + TRI_SIDE + 2); int16_t rightW = TRI_MARGIN_R + TRI_SIDE + 2; tft.fillRect(rightX, yTop, rightW, h, ST77XX_BLACK);
    tft.fillTriangle(TRI_MARGIN_L, centerY, TRI_MARGIN_L + TRI_SIDE, centerY - half, TRI_MARGIN_L + TRI_SIDE, centerY + half, ST77XX_WHITE);
    tft.fillTriangle(tft.width() - TRI_MARGIN_R, centerY, tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY - half, tft.width() - TRI_MARGIN_R - TRI_SIDE, centerY + half, ST77XX_WHITE);
  }

  void eraseDefaultBottomTrianglesStrip() { const int16_t half = TRI_SIDE / 2; tft.fillRect(0, TRI_Y - half - 2, tft.width(), TRI_SIDE + 4, ST77XX_BLACK); }

  // ---------- NEW: fixed two-digit cell layout for confirmation screen ----------
  static constexpr int16_t kDigitGapBig = 3; // extra spacing
  struct EditLayout { int16_t baseY; int16_t cellW; int16_t digitX[2]; Bounds bMax; };
  static EditLayout s_editLayout; static bool s_editLayoutBuilt=false; static int s_editLast=-1;

  static void buildEditLayout(EditLayout& E){
    E.baseY = valueBaseline_confirm(); const GFXfont* fBig = &OpenSans_Regular24pt7b; tft.setFont(fBig);
    // Max digit width across 0..9
    int16_t cellW=0; Bounds b; for(char d='0'; d<='9'; ++d){ char s[2]={d,0}; textBounds(fBig, s, 0, E.baseY, b); if((int16_t)b.w>cellW) cellW=(int16_t)b.w; }
    E.cellW = cellW + kDigitGapBig; textBounds(fBig, "888", 0, E.baseY, E.bMax);
    // Centre the two-digit group as a set
    int16_t groupW = 2*E.cellW; int16_t left = (tft.width() - groupW)/2 + valueXNudge_confirm();
    E.digitX[0] = left; E.digitX[1] = left + E.cellW;
  }

  static void drawEditStatic(){
    // Draw triangles once and clear a generous band for digits
    int16_t centerY = centerYForText(&OpenSans_Regular24pt7b, "88", s_editLayout.baseY); drawSideTrianglesAtCenterY(centerY);
    int16_t top = s_editLayout.bMax.y1 - 14; int16_t h = (int16_t)s_editLayout.bMax.h + 28; int16_t left = (tft.width() - (2*s_editLayout.cellW))/2 + valueXNudge_confirm() - 6; int16_t w = 2*s_editLayout.cellW + 12;
    tft.fillRect(left, top, w, h, ST77XX_BLACK);
  }

  static void drawEditDigits(int value, int prev){
    const GFXfont* fBig=&OpenSans_Regular24pt7b; tft.setFont(fBig); tft.setTextColor(ST77XX_RED);
    int v = (value<1?1:(value>16?16:value)); int p = (prev<0?-1:prev);
    int vd[2] = { (v/10)%10, v%10 }; int pd[2] = { p<0?-1:((p/10)%10), p<0?-1:(p%10) };
    for(int i=0;i<2;++i){ if(vd[i]!=pd[i]){
        const int16_t padL=2,padR=4,padT=10,padB=12; int16_t x=s_editLayout.digitX[i];
        tft.fillRect(x-padL, s_editLayout.bMax.y1-padT, s_editLayout.cellW+padL+padR, (int16_t)s_editLayout.bMax.h+padT+padB, ST77XX_BLACK);
        char s[2]={(char)('0'+vd[i]),0}; tft.setCursor(x, s_editLayout.baseY); tft.print(s);
      } }
  }

  inline uint8_t clampCh(int v) { return (v < 1) ? 1 : (v > 16 ? 16 : (uint8_t)v); }
  inline uint8_t wrapCh(int v)  { if (v < 1) return 16; if (v > 16) return 1; return (uint8_t)v; }
}

// ---- Public API ----
void setup_midi_ch::begin() {
  s_ble = settings_module::getBleMidiChannel(); s_din = settings_module::getDinMidiChannel();
  if (s_ble < 1 || s_ble > 16) s_ble = 1; if (s_din < 1 || s_din > 16) s_din = 1; s_state = State::VIEW; s_selected = OutputSel::BLE; s_edit = s_ble;
}

void setup_midi_ch::show_midi_ch() {
  s_state = State::VIEW; int16_t cxBle, cxDin; drawSideTrianglesAtCenterY(TRI_Y);
  drawLabels(ST77XX_WHITE, ST77XX_WHITE, cxBle, cxDin); drawValues(s_ble, s_din, cxBle, cxDin, /*hiBle=*/true, /*hiDin=*/true); drawBottomAuto("MIDI channel", "MIDI ch");
}

void setup_midi_ch::show_midi_ch_select() {
  s_state = State::SELECT_OUTPUT; bool selBle = (s_selected == OutputSel::BLE);
  eraseDefaultBottomTrianglesStrip(); int16_t baselineMirror = 80; int16_t centerY = centerYForText(&OpenSans_Regular24pt7b, "3.0 sec", baselineMirror); drawSideTrianglesAtCenterY(centerY);
  int16_t cxBle, cxDin; uint16_t nameBle = selBle ? ST77XX_GREEN : ST77XX_WHITE; uint16_t nameDin = selBle ? ST77XX_WHITE : ST77XX_GREEN; drawLabels(nameBle, nameDin, cxBle, cxDin);
  drawValues(s_ble, s_din, cxBle, cxDin, /*hiBle=*/false, /*hiDin=*/false);
  const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f); char buf[4]; int16_t cxSel = selBle ? cxBle : cxDin; uint8_t vSel  = selBle ? s_ble : s_din;
  snprintf(buf, sizeof(buf), "%02u", (unsigned)vSel); Bounds b; textBounds(f, buf, 0, valueY(), b); tft.setTextColor(ST77XX_GREEN); tft.setCursor(cxSel - (int16_t)b.w/2, valueY()); tft.print(buf);
  drawBottomAuto("select OUTPUT", "select MIDI OUT");
}

void setup_midi_ch::show_midi_ch_confirmation() {
  s_state = State::EDIT_CH; s_edit = (s_selected == OutputSel::BLE) ? s_ble : s_din; eraseDefaultBottomTrianglesStrip();
  // Build fixed-cell layout and draw static band
  buildEditLayout(s_editLayout); s_editLayoutBuilt=true; s_editLast=-1; drawEditStatic();
  // Draw initial digits in fixed cells
  drawEditDigits(s_edit, -1);
  drawBottomSelectTagColored(s_selected == OutputSel::BLE);
}

void setup_midi_ch::on_encoder_turn(int8_t dir) {
  if (dir == 0) return;
  switch (s_state) {
    case State::VIEW: return;
    case State::SELECT_OUTPUT: {
      s_selected = (s_selected == OutputSel::BLE) ? OutputSel::DIN : OutputSel::BLE; bool selBle = (s_selected == OutputSel::BLE);
      int16_t cxBle, cxDin; uint16_t nameBle = selBle ? ST77XX_GREEN : ST77XX_WHITE; uint16_t nameDin = selBle ? ST77XX_WHITE : ST77XX_GREEN; drawLabels(nameBle, nameDin, cxBle, cxDin);
      drawValues(s_ble, s_din, cxBle, cxDin, /*hiBle=*/false, /*hiDin=*/false);
      const GFXfont* f = &OpenSans_SemiBold14pt7b; tft.setFont(f); char buf[4]; int16_t cxSel = selBle ? cxBle : cxDin; uint8_t vSel  = selBle ? s_ble : s_din;
      snprintf(buf, sizeof(buf), "%02u", (unsigned)vSel); Bounds b; textBounds(f, buf, 0, valueY(), b); tft.setTextColor(ST77XX_GREEN); tft.setCursor(cxSel - (int16_t)b.w/2, valueY()); tft.print(buf);
      return; }
    case State::EDIT_CH: {
      uint8_t prev = s_edit; s_edit = wrapCh((int)s_edit + (dir > 0 ? +1 : -1)); if (s_edit != prev) { drawEditDigits(s_edit, s_editLast); s_editLast = s_edit; } return; }
  }
}

void setup_midi_ch::on_toggle(int8_t dir) { on_encoder_turn(dir); }

void setup_midi_ch::on_encoder_press() {
  switch (s_state) {
    case State::VIEW: s_selected = OutputSel::BLE; show_midi_ch_select(); return;
    case State::SELECT_OUTPUT: show_midi_ch_confirmation(); return;
    case State::EDIT_CH:
      if (s_selected == OutputSel::BLE) { s_ble = clampCh(s_edit); settings_module::setBleMidiChannel(s_ble); }
      else { s_din = clampCh(s_edit); settings_module::setDinMidiChannel(s_din); }
#ifdef MIDI_CH_DEBUG_SAVE
      settings_module::dumpToSerial();
#endif
      show_midi_ch(); return;
  }
}
