// ============================================================================
// File: src/setup_stomp_cc_v1.1.cpp — port of setup_fader_cc to Stomp CC
// Screens: VIEW (root) / SELECT (single Sx) / EDIT (CC digits)
// Notes:
//  • Mirrors the layout, fonts, and redraw strategy of setup_fader_cc_v1.14.
//  • Uses settings_module::getStompCC / setStompCC.
//  • Labels are S1..S4 and copy the spacing tricks (digit cells, cursor‑based X for label number).
// ============================================================================

#include "setup_stomp_cc.h"
#include "display_module.h"
#include <Adafruit_ST77XX.h>
#include <Arduino.h>
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "fonts/OpenSans_Regular24pt7b.h"
#include "layout_constants.h"
#include "setup_module.h"   // clearBetweenTriangles()

using display_module::tft;

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
  // ---------- types & helpers ----------
  struct Bounds { int16_t x1; int16_t y1; uint16_t w; uint16_t h; };

  static inline int16_t iMin16(int16_t a, int16_t b){ return (a<b)?a:b; }
  static inline int16_t iMax16(int16_t a, int16_t b){ return (a>b)?a:b; }

  static void textBounds(const GFXfont* font, const char* txt, int16_t x, int16_t y, Bounds& b){ tft.setFont(font); tft.getTextBounds(txt, x, y, &b.x1, &b.y1, &b.w, &b.h); }
  static int16_t centerYForText(const GFXfont* font, const char* sample, int16_t baselineY){ Bounds b; textBounds(font, sample, 0, baselineY, b); return b.y1 + (int16_t)b.h/2; }
  static void drawCentered(const GFXfont* font, const char* txt, int16_t y, uint16_t color){ tft.setFont(font); tft.setTextColor(color); Bounds b; textBounds(font, txt, 0, y, b); int16_t cx=(tft.width()-(int16_t)b.w)/2; tft.setCursor(cx, y); tft.print(txt); }

  enum class State : uint8_t { VIEW, SELECT_STOMP, EDIT_CC };
  State   s_state = State::VIEW;
  uint8_t s_selected = 0;        // 0..3 (S1..S4)
  uint8_t s_vals[4] = {0,0,0,0}; // CC cache
  uint8_t s_edit = 0;            // 0..127
  bool    s_selectUiPrimed = false; // draw triangles/bottom once per entry

  // ---- geometry constants ----
  inline int16_t labelY() { return 54; }
  inline int16_t valueY() { return 90; }
  inline int16_t bottomY() { return SETUP_BOTTOM_TEXT_Y; }
  inline int16_t valueBaseline_confirm() { return 80; }

  inline int16_t innerL() { return TRI_MARGIN_L + TRI_SIDE + 2; }
  inline int16_t innerR() { return tft.width() - (TRI_MARGIN_R + TRI_SIDE + 2); }

  static void drawSideTrianglesAtCenterY(int16_t centerY){
    const int16_t half=TRI_SIDE/2; const int16_t yTop=centerY-half-2; const int16_t h=TRI_SIDE+4;
    tft.fillRect(0, yTop, innerL(), h, ST77XX_BLACK);
    tft.fillRect(innerR(), yTop, tft.width()-innerR(), h, ST77XX_BLACK);
    tft.fillTriangle(TRI_MARGIN_L,centerY,TRI_MARGIN_L+TRI_SIDE,centerY-half,TRI_MARGIN_L+TRI_SIDE,centerY+half,ST77XX_WHITE);
    tft.fillTriangle(tft.width()-TRI_MARGIN_R,centerY,tft.width()-TRI_MARGIN_R-TRI_SIDE,centerY-half,tft.width()-TRI_MARGIN_R-TRI_SIDE,centerY+half,ST77XX_WHITE);
  }
  static void eraseDefaultBottomTrianglesStrip(){ const int16_t half=TRI_SIDE/2; tft.fillRect(0,TRI_Y-half-2,tft.width(),TRI_SIDE+4,ST77XX_BLACK); }
  inline uint8_t wrapCC(int v){ return (v<0)?127: (v>127?0:(uint8_t)v); }

  static void computeFourCenters(int16_t xL, int16_t xR, int16_t c[4]){ int16_t w=xR-xL; c[0]=xL+w*1/8; c[1]=xL+w*3/8; c[2]=xL+w*5/8; c[3]=xL+w*7/8; }
  static void computeRootCenters(int16_t c[4]){ computeFourCenters(0, tft.width(), c); }

  // bottom text helpers
  static void drawBottomAuto(const char* full, const char* shortTxt){ const GFXfont* f=&OpenSans_SemiBold14pt7b; tft.setFont(f); int16_t maxW=innerR()-innerL(); Bounds bF,bS; textBounds(f,full,0,bottomY(),bF); textBounds(f,shortTxt,0,bottomY(),bS); const char* use=full; Bounds* bu=&bF; if ((int16_t)bF.w+4>maxW){ use=shortTxt; bu=&bS; } int16_t yTop=bu->y1-3, yBot=(int16_t)(bu->y1+bu->h)+3; setup_module::clearBetweenTriangles(yTop,yBot); drawCentered(f,use,bottomY(),ST77XX_WHITE); }
  static void drawBottomPlain(const char* txt){ const GFXfont* f=&OpenSans_SemiBold14pt7b; tft.setFont(f); Bounds b; textBounds(f, txt, 0, bottomY(), b); int16_t yTop=b.y1-3, yBot=(int16_t)(b.y1+b.h)+3; setup_module::clearBetweenTriangles(yTop,yBot); drawCentered(f, txt, bottomY(), ST77XX_WHITE); }

  // ---------- ROOT screen (S1..S4) ----------
  static void drawRowFourRoot(const uint8_t vals[4], const int16_t centers[4]){
    const GFXfont* f=&OpenSans_SemiBold14pt7b; tft.setFont(f); const char* L[4]={("S1"),("S2"),("S3"),("S4")};
    for(int i=0;i<4;++i){ Bounds b; textBounds(f,L[i],0,labelY(),b); tft.fillRect(centers[i]-(int16_t)b.w/2-6,b.y1-6,(int16_t)b.w+12,(int16_t)b.h+12,ST77XX_BLACK); tft.setTextColor(ST77XX_WHITE); tft.setCursor(centers[i]-(int16_t)b.w/2,labelY()); tft.print(L[i]); }
    char buf[4]; for(int i=0;i<4;++i){ snprintf(buf,sizeof(buf),"%03u",(unsigned)vals[i]); Bounds b; textBounds(f,buf,0,valueY(),b); tft.fillRect(centers[i]-(int16_t)b.w/2-6,b.y1-6,(int16_t)b.w+12,(int16_t)b.h+12,ST77XX_BLACK); tft.setTextColor(ST77XX_GREEN); tft.setCursor(centers[i]-(int16_t)b.w/2,valueY()); tft.print(buf); }
  }

  // ---------- SELECT screen (single pair, centered, per-digit cells) ----------
  static constexpr int16_t kSelectGapPx = 20;      // gap between label and value
  static constexpr int16_t kDigitGapSmall = 2;     // extra spacing to prevent overlap (small digits)

  struct SelectLayout {
    int16_t baseY;            // label baseline
    int16_t valueBase;        // value baseline (center‑aligned to label)
    int16_t labelWMax;        // MAX width of any "S n" (n=1..4)
    int16_t labelWAct;        // actual width of current label (aesthetic only)
    int16_t cellW;            // max width of one digit (0..9) in small font + gap
    int16_t groupW;           // labelWMax + gap + (3*cellW)
    int16_t groupLeft;        // left X of whole pair (centered set)
    int16_t labelX;           // fixed left X for label ("S " stays put)
    int16_t digitX[3];        // fixed left X for each small digit cell

    // split the label into stable prefix and changing number
    int16_t labelPrefixW;     // width of "S " at baseY (bounds only; not used for cursor)
    int16_t labelNumWMax;     // max width among '1'..'4' at baseY
    int16_t labelNumX;        // fallback X for the changing number (bounds based)

    Bounds  bLabelMax;        // bounds of widest label at baseY
    Bounds  bDigitMax;        // bounds of "8" at valueBase (for clear height)
    int16_t labelClearTop;    // label clear rect top
    int16_t labelClearH;      // label clear rect height
  };

  static SelectLayout s_selectLayout; static bool s_selectLayoutBuilt=false; static int s_selectLastVal=-1; static uint8_t s_selectLastIndex=255; static int s_selectLastLabelNum=-1; static bool s_selectLabelPrefixDrawn=false; static int16_t s_selectLabelNumX_cursor = -1; // actual cursor X after printing "S "

  static void buildSelectLayout(SelectLayout& L, uint8_t selIndex){
    const GFXfont* fLab=&OpenSans_Regular24pt7b; const GFXfont* fVal=&OpenSans_SemiBold14pt7b; L.baseY=valueBaseline_confirm();

    // Measure label widths for S 1..S 4
    const char* samples[4] = {"S 1","S 2","S 3","S 4"};
    L.labelWMax = 0; Bounds bTmp; L.bLabelMax = {0,0,0,0};
    for(int i=0;i<4;++i){ textBounds(fLab, samples[i], 0, L.baseY, bTmp); if((int16_t)bTmp.w > L.labelWMax){ L.labelWMax=(int16_t)bTmp.w; L.bLabelMax=bTmp; } }

    // Actual label width for current selection (for clearing aesthetics only)
    char lab[4]; snprintf(lab,sizeof(lab),"S %u", (unsigned)(selIndex+1));
    Bounds bAct; textBounds(fLab, lab, 0, L.baseY, bAct); L.labelWAct=(int16_t)bAct.w;

    // value baseline align
    int16_t cY_lab=centerYForText(fLab, "S 1", L.baseY); int16_t cY_val=centerYForText(fVal, "8", L.baseY); L.valueBase = L.baseY + (cY_lab - cY_val);

    // digit cell width
    int16_t cw=0; for(char d='0'; d<='9'; ++d){ char s[2]={d,0}; Bounds b; textBounds(fVal, s, 0, L.valueBase, b); cw = iMax16(cw, (int16_t)b.w); }
    L.cellW = cw + kDigitGapSmall;

    // Split label into constant prefix and changing number
    Bounds bPref; textBounds(fLab, "S ", 0, L.baseY, bPref); L.labelPrefixW = (int16_t)bPref.w;
    int16_t numWMax = 0; for(char d='1'; d<='4'; ++d){ char s[2]={d,0}; Bounds b; textBounds(fLab, s, 0, L.baseY, b); numWMax = iMax16(numWMax, (int16_t)b.w); }
    L.labelNumWMax = numWMax;

    // Group geometry centered using MAX label width
    L.groupW = L.labelWMax + kSelectGapPx + 3*L.cellW;
    L.groupLeft = (tft.width() - L.groupW)/2;
    L.labelX = L.groupLeft;
    L.labelNumX = L.labelX + L.labelPrefixW;      // Fallback if cursor not available
    L.digitX[0] = L.labelX + L.labelWMax + kSelectGapPx;
    L.digitX[1] = L.digitX[0] + L.cellW;
    L.digitX[2] = L.digitX[1] + L.cellW;

    // Max digit bounds for clear height + labeled clear rect
    textBounds(fVal, "8", 0, L.valueBase, L.bDigitMax);
    L.labelClearTop = iMin16(L.bLabelMax.y1, L.bDigitMax.y1) - 8;
    int16_t labelClearBot = iMax16((int16_t)(L.bLabelMax.y1+L.bLabelMax.h), (int16_t)(L.bDigitMax.y1+L.bDigitMax.h)) + 8;
    L.labelClearH = labelClearBot - L.labelClearTop;
  }

  static void drawSelectPrefixOnce(const SelectLayout& L){
    if(s_selectLabelPrefixDrawn) return;
    // Clear just the fixed prefix+number rectangle (generous pad)
    const int16_t padL=4, padR=8, padT=4, padB=6;
    tft.fillRect(L.labelX - padL, L.labelClearTop - padT, L.labelWMax + padL + padR, L.labelClearH + padT + padB, ST77XX_BLACK);
    // Draw constant prefix "S " and capture live cursor X
    tft.setFont(&OpenSans_Regular24pt7b); tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(L.labelX, L.baseY); tft.print("S ");
    s_selectLabelNumX_cursor = tft.getCursorX();
    s_selectLabelPrefixDrawn = true;
  }

  static void drawSelectLabelNumber(const SelectLayout& L, uint8_t num, int prevNum){
    if(prevNum == (int)num) return;
    int16_t numX = (s_selectLabelNumX_cursor >= 0) ? s_selectLabelNumX_cursor : L.labelNumX;
    const int16_t padL=1, padR=3, padT=4, padB=6;
    tft.fillRect(numX - padL, L.labelClearTop - padT, L.labelNumWMax + padL + padR, L.labelClearH + padT + padB, ST77XX_BLACK);
    tft.setFont(&OpenSans_Regular24pt7b); tft.setTextColor(ST77XX_GREEN);
    char s[2] = {(char)('0' + num), 0};
    tft.setCursor(numX, L.baseY); tft.print(s);
  }

  static void drawSelectDigits(const SelectLayout& L, int value, int prev){
    const GFXfont* fVal=&OpenSans_SemiBold14pt7b; tft.setFont(fVal); tft.setTextColor(ST77XX_GREEN);
    int v=value%1000; int p=(prev<0?-1:prev%1000);
    int vd[3]={ v/100, (v/10)%10, v%10 };
    int pd[3]={ p<0?-1:p/100, p<0?-1:(p/10)%10, p<0?-1:p%10 };
    for(int i=0;i<3;++i){ if(vd[i]!=pd[i]){
        int16_t x=L.digitX[i];
        const int16_t padL=2, padR=4, padT=8, padB=10;
        tft.fillRect(x - padL, L.bDigitMax.y1 - padT, L.cellW + padL + padR, (int16_t)L.bDigitMax.h + padT + padB, ST77XX_BLACK);
        char s[2]={(char)('0'+vd[i]),0}; tft.setCursor(x, L.valueBase); tft.print(s);
      }
    }
  }

  // ---------- EDIT screen (label left, big value right; per-digit cells) ----------
  static constexpr int16_t kDigitGapBig = 3;      // extra spacing for big digits

  struct EditLayout { int16_t baseY; int16_t cellW; int16_t digitsLeftX; int16_t digitX[3]; int16_t labelBase; int16_t labelX; int16_t groupLeft; int16_t groupW; Bounds bMaxVal; Bounds bSmallLab; };
  static EditLayout s_editLayout; static bool s_editLayoutBuilt=false; static int s_editLast=-1;

  static void buildEditLayout(EditLayout& E, uint8_t selIndex){
    E.baseY = valueBaseline_confirm();
    const GFXfont* fBig=&OpenSans_Regular24pt7b; const GFXfont* fSmall=&OpenSans_SemiBold14pt7b;
    textBounds(fBig, "888", 0, E.baseY, E.bMaxVal);
    int16_t cellW=0; for(char d='0'; d<='9'; ++d){ char s[2]={d,0}; Bounds b; textBounds(fBig, s, 0, E.baseY, b); cellW = iMax16(cellW, (int16_t)b.w); } E.cellW = cellW + kDigitGapBig; // add gap
    char lab[4]; snprintf(lab,sizeof(lab), "S %u", (unsigned)(selIndex+1));
    int16_t cYbig=centerYForText(fBig, "888", E.baseY); int16_t cYs=centerYForText(fSmall, lab, E.baseY);
    E.labelBase = E.baseY + (cYbig - cYs);
    textBounds(fSmall, lab, 0, E.labelBase, E.bSmallLab);
    const int16_t gap=20; int16_t digitsW = E.cellW * 3;
    E.groupW = (int16_t)E.bSmallLab.w + gap + digitsW;
    E.groupLeft = (tft.width() - E.groupW)/2;
    E.labelX = E.groupLeft;
    E.digitsLeftX = E.labelX + (int16_t)E.bSmallLab.w + gap;
    E.digitX[0]=E.digitsLeftX; E.digitX[1]=E.digitsLeftX + E.cellW; E.digitX[2]=E.digitsLeftX + 2*E.cellW;
  }

  static void drawEditStatic(const EditLayout& E, uint8_t selIndex){
    int16_t top = iMin16(E.bSmallLab.y1, E.bMaxVal.y1) - 12; int16_t h = iMax16((int16_t)(E.bSmallLab.y1+E.bSmallLab.h), (int16_t)(E.bMaxVal.y1+E.bMaxVal.h)) - (top-12) + 4;
    tft.fillRect(innerL(), top, innerR()-innerL(), h, ST77XX_BLACK);
    tft.setFont(&OpenSans_SemiBold14pt7b); tft.setTextColor(ST77XX_RED);
    char lab[4]; snprintf(lab,sizeof(lab),"S %u", (unsigned)(selIndex+1)); tft.setCursor(E.labelX, E.labelBase); tft.print(lab);
  }

  static void drawEditDigits(const EditLayout& E, int value, int prev){
    const GFXfont* fBig=&OpenSans_Regular24pt7b; tft.setFont(fBig); tft.setTextColor(ST77XX_RED);
    int v=value%1000; int p=(prev<0?-1:prev%1000); int vd[3]={ v/100, (v/10)%10, v%10 }; int pd[3]={ p<0?-1:p/100, p<0?-1:(p/10)%10, p<0?-1:p%10 };
    for(int i=0;i<3;++i){ if(vd[i]!=pd[i]){
        const int16_t padL=2, padR=4, padT=10, padB=12;
        int16_t x=E.digitX[i]; int16_t top=E.bMaxVal.y1 - padT; int16_t h=(int16_t)E.bMaxVal.h + padT + padB; tft.fillRect(x - padL, top, E.cellW + padL + padR, h, ST77XX_BLACK);
        char s[2]={(char)('0'+vd[i]),0}; tft.setCursor(x, E.baseY); tft.print(s);
      }
    }
  }
}

// ---- Public API ----
void setup_stomp_cc::begin(){ for(uint8_t i=0;i<4;++i) s_vals[i]=settings_module::getStompCC(i); s_state=State::VIEW; s_selected=0; s_edit=s_vals[0]; s_selectUiPrimed=false; s_editLayoutBuilt=false; s_editLast=-1; s_selectLayoutBuilt=false; s_selectLastVal=-1; s_selectLastIndex=255; s_selectLastLabelNum=-1; s_selectLabelPrefixDrawn=false; s_selectLabelNumX_cursor=-1; }

void setup_stomp_cc::show_stomp_cc(){ s_state=State::VIEW; s_selectUiPrimed=false; int16_t centers[4]; computeRootCenters(centers); drawRowFourRoot(s_vals, centers); drawBottomAuto("Stomp CC numbers","Stomp CC"); }

void setup_stomp_cc::show_stomp_cc_select(){
  if(!s_selectUiPrimed){ s_state=State::SELECT_STOMP; eraseDefaultBottomTrianglesStrip(); int16_t cY=centerYForText(&OpenSans_Regular24pt7b, "3.0 sec", valueBaseline_confirm()); drawSideTrianglesAtCenterY(cY); drawBottomAuto("select STOMP","select STP"); s_selectUiPrimed=true; }
  else { s_state=State::SELECT_STOMP; }

  buildSelectLayout(s_selectLayout, s_selected); s_selectLayoutBuilt=true; s_selectLastVal=-1; s_selectLastIndex=s_selected; s_selectLastLabelNum=-1; s_selectLabelPrefixDrawn=false; s_selectLabelNumX_cursor=-1;

  // Draw label prefix once, capture cursor X, then initial number; digits drawn fresh
  drawSelectPrefixOnce(s_selectLayout);
  drawSelectLabelNumber(s_selectLayout, (uint8_t)(s_selected+1), -1);
  s_selectLastLabelNum = (int)(s_selected+1);

  drawSelectDigits(s_selectLayout, s_vals[s_selected], -1);
}

void setup_stomp_cc::show_stomp_cc_edit(){ s_state=State::EDIT_CC; s_selectUiPrimed=false; s_edit=s_vals[s_selected]; eraseDefaultBottomTrianglesStrip(); int16_t cY=centerYForText(&OpenSans_Regular24pt7b, "888", valueBaseline_confirm()); drawSideTrianglesAtCenterY(cY); buildEditLayout(s_editLayout, s_selected); s_editLayoutBuilt=true; drawEditStatic(s_editLayout, s_selected); drawEditDigits(s_editLayout, s_edit, -1); s_editLast = s_edit; drawBottomPlain("set CC number"); }

void setup_stomp_cc::on_encoder_turn(int8_t dir){ if(dir==0) return; switch(s_state){
  case State::VIEW: return;
  case State::SELECT_STOMP: {
    int v=(int)s_selected + (dir>0?+1:-1); if(v<0) v=3; else if(v>3) v=0; s_selected=(uint8_t)v;

    if(!s_selectLayoutBuilt) { buildSelectLayout(s_selectLayout, s_selected); s_selectLayoutBuilt=true; }

    drawSelectPrefixOnce(s_selectLayout);
    drawSelectLabelNumber(s_selectLayout, (uint8_t)(s_selected+1), s_selectLastLabelNum);
    s_selectLastLabelNum = (int)(s_selected+1);

    drawSelectDigits(s_selectLayout, s_vals[s_selected], -1);

    s_selectLastVal=s_vals[s_selected]; s_selectLastIndex=s_selected; return; }
  case State::EDIT_CC: {
    uint8_t prev=s_edit; s_edit=wrapCC((int)s_edit + (dir>0?+1:-1)); if(s_edit!=prev){ drawEditDigits(s_editLayout, s_edit, s_editLast); s_editLast = s_edit; } return; }
  }
}

void setup_stomp_cc::on_toggle(int8_t dir){ on_encoder_turn(dir); }

void setup_stomp_cc::on_encoder_press(){ switch(s_state){
  case State::VIEW: s_selected=0; show_stomp_cc_select(); return;
  case State::SELECT_STOMP: show_stomp_cc_edit(); return;
  case State::EDIT_CC: s_vals[s_selected]=s_edit; settings_module::setStompCC(s_selected, s_edit); show_stomp_cc(); return; }
}
