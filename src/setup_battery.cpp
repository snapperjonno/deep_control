// =============================
// File: src/setup_battery.cpp
// =============================
#include "setup_battery.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>                       // include before font
#include "fonts/OpenSans_SemiBold14pt7b.h"     // OpenSans font used in project
#include "display_module.h"                    // display_module::tft
#include "pinmap_module.h"                     // pinmap::VBAT_PIN
#include "layout_constants.h"                  // shared setup layout
#include "setup_module.h"                      // for clearBetweenTriangles()

// ---- Colour palette (fallbacks map to ST77XX_* when available) -------------
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

using display_module::tft;

// ---- Layout constants (driven by layout_constants.h) -----------------------
#define NUM_X          SETUP_VALUE_X
#define NUM_Y          SETUP_VALUE_Y
#define BAR_X          SETUP_BAR_X
#define BAR_Y          SETUP_BAR_Y
#define BAR_WIDTH      SETUP_BAR_WIDTH
#define BAR_HEIGHT     SETUP_BAR_HEIGHT
#define SECTIONS       SETUP_BATTERY_SECTIONS
#define BOTTOM_Y       SETUP_BOTTOM_TEXT_Y

// Battery thresholds
#define BATT_EMPTY_V    3.2f
#define BATT_FULL_V     4.05f

// --- Charging detection tunables --------------------------------------------
#ifndef CHG_STEP_RISE_V
#define CHG_STEP_RISE_V 0.1f
#endif
#ifndef CHG_CUM_RISE_V
#define CHG_CUM_RISE_V 0.03f
#endif
#ifndef CHG_REQUIRE_BOTH
#define CHG_REQUIRE_BOTH 1
#endif
#ifndef CHG_NEG_DROP_V
#define CHG_NEG_DROP_V (-0.01f)
#endif

namespace {
  enum BattState { DISCHARGING, CHARGING, FULL };
  BattState battState = DISCHARGING;
  int fullCount = 0;

  uint8_t  lastPct = 255;
  BattState lastBattState = DISCHARGING;
  uint32_t chargeAnimLastMs = 0;
  uint8_t  chargeAnimStep = 0; // 0..3
  int16_t  chargeY = NUM_Y;
  int16_t  chargeBoxTop = 0;
  uint16_t chargeBoxW = 0;
  uint16_t chargeBoxH = 0;
  int16_t  chargeWordX = 0;
  int16_t  leftDotsX = 0, rightDotsX = 0;
  uint16_t dotsW = 0, dotsH = 0;
  int16_t  dotsTop = 0;
  int16_t  chargeBoxLeft = 0;
  uint16_t chargeWordW = 0;

  // Clear only the band between triangles that covers the bottom label's glyph box
  void clearBottomBandFor(const char* txt) {
    tft.setFont(&OpenSans_SemiBold14pt7b);
    int16_t x1, y1; uint16_t w, h;
    tft.getTextBounds(txt, 0, BOTTOM_Y, &x1, &y1, &w, &h);
    setup_module::clearBetweenTriangles((int16_t)(y1 - 3), (int16_t)(y1 + h + 3));
  }

  void drawBottomLabel() {
    const char* BOTTOM_TEXT = "battery %";
    clearBottomBandFor(BOTTOM_TEXT);
    tft.setFont(&OpenSans_SemiBold14pt7b);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    int16_t tx, ty; uint16_t tw, th;
    tft.getTextBounds(BOTTOM_TEXT, 0, BOTTOM_Y, &tx, &ty, &tw, &th);
    tft.setCursor((int16_t)((int)tft.width() - (int)tw) / 2, BOTTOM_Y);
    tft.print(BOTTOM_TEXT);
  }
}

namespace setup_battery {

void begin() {
  drawBottomLabel();
  lastPct = 255;
  lastBattState = DISCHARGING;
  chargeAnimLastMs = 0;
  chargeAnimStep = 0;
}

static void drawDeltaBar(uint8_t pct) {
  int totalW   = BAR_WIDTH - 1;
  int oldPx    = (lastPct > 100) ? 0 : (totalW * (int)lastPct) / 100;
  int newPx    = (totalW * (int)pct) / 100;
  int sectionW = totalW / SECTIONS;

  if (newPx > oldPx) {
    for (int p = oldPx; p < newPx; ++p) {
      int x   = BAR_X + totalW - p;
      int idx = p / sectionW; // 0 == rightmost section
      uint16_t col = (idx == 0) ? COLOR_RED
                       : (idx <= 1) ?  COLOR_ORANGE
                                    :  COLOR_GREEN;
      tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, col);
    }
  } else if (newPx < oldPx) {
    for (int p = newPx; p < oldPx; ++p) {
      int x = BAR_X + totalW - p;
      tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, COLOR_BG);
    }
  }

  tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, COLOR_LINE);
  for (int i = 1; i < SECTIONS; ++i) {
    int mx = BAR_X + (BAR_WIDTH * i) / SECTIONS;
    tft.drawLine(mx, BAR_Y, mx, BAR_Y + BAR_HEIGHT - 1, COLOR_LINE);
  }
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

void update() {
  uint8_t pct = readBatteryPercent();

  if (battState == CHARGING) {
    if (lastBattState != CHARGING) {
      tft.fillRect(NUM_X - 2, NUM_Y - 22, tft.width() - NUM_X + 4, 44, COLOR_BG);
      tft.fillRect(BAR_X - 2, BAR_Y - 2, BAR_WIDTH + 4, BAR_HEIGHT + 4, COLOR_BG);

      const char* word = "charging";
      const char* fullMsg = "...charging...";
      int16_t tx, ty; uint16_t tw, th;
      tft.getTextBounds(word, 0, NUM_Y, &tx, &ty, &tw, &th);
      chargeWordX = (int16_t)((tft.width() - (int16_t)tw) / 2);
      chargeWordW = tw;
      chargeY = NUM_Y;
      tft.setCursor(chargeWordX, chargeY);
      tft.print(word);

      int16_t dtx, dty; uint16_t dtw, dth;
      tft.getTextBounds("...", 0, chargeY, &dtx, &dty, &dtw, &dth);
      dotsW = dtw; dotsH = dth; dotsTop = dty;
      leftDotsX  = (int16_t)(chargeWordX - (int16_t)dotsW - 5);
      rightDotsX = (int16_t)(chargeWordX + (int16_t)chargeWordW + 5);

      tft.getTextBounds(fullMsg, leftDotsX, chargeY, &tx, &ty, &tw, &th);
      chargeBoxLeft = tx; chargeBoxTop = ty; chargeBoxW = tw; chargeBoxH = th;

      chargeAnimStep = 0; chargeAnimLastMs = 0;
    }

    uint32_t nowMs = millis();
    if (chargeAnimLastMs == 0 || nowMs - chargeAnimLastMs >= 1000) {
      chargeAnimLastMs = nowMs;
      if (dotsW > 0 && dotsH > 0) {
        tft.fillRect(leftDotsX - 2,  dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
        tft.fillRect(rightDotsX - 2, dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
      }
      uint8_t dots = (chargeAnimStep <= 3) ? (uint8_t)(3 - chargeAnimStep) : 0;
      char left[4]  = {' ', ' ', ' ', 0};
      char right[4] = {' ', ' ', ' ', 0};
      for (uint8_t i = 0; i < dots; ++i) { left[2 - i] = '.'; }
      for (uint8_t i = 0; i < dots; ++i) { right[i] = '.'; }
      tft.setCursor(leftDotsX, chargeY);
      tft.print(left);
      tft.setCursor(rightDotsX, chargeY);
      tft.print(right);
      chargeAnimStep = (uint8_t)((chargeAnimStep + 1) % 4);
    }

    lastBattState = CHARGING;
    lastPct = pct;
    return;
  }

  if (lastBattState == CHARGING) {
    if (chargeBoxW > 0 && chargeBoxH > 0) {
      tft.fillRect((int16_t)(chargeBoxLeft - 3), (int16_t)(chargeBoxTop - 4),
                   (uint16_t)(chargeBoxW + 6), (uint16_t)(chargeBoxH + 8), COLOR_BG);
    }
    lastPct = 255;
  }
  lastBattState = DISCHARGING;

  if (pct == lastPct) return;

  eraseOldNumber(lastPct);
  drawValue(pct);
  drawDeltaBar(pct);
  lastPct = pct;
}

uint8_t readBatteryPercent() {
  static uint32_t lastReadMs = 0;
  static uint8_t cachedPct = 0;
  static float lastRawV_forSlope = 0.0f;
  static bool slopeInit = false;
  static float riseWindow[3] = {0.0f, 0.0f, 0.0f};
  static uint8_t riseIdx = 0;
  static BattState prevBattStateLocal = DISCHARGING;

  uint32_t now = millis();
  if (lastReadMs == 0 || now - lastReadMs >= 1000) {
    lastReadMs = now;

    int mv = analogReadMilliVolts(pinmap::VBAT_PIN);
    float voltage = mv * 0.001f * 1.922f;

    static float filteredVoltage = voltage;
    filteredVoltage = filteredVoltage * 0.8f + voltage * 0.2f;
    float pctf = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    pctf = constrain(pctf, 0.0f, 100.0f);
    cachedPct = static_cast<uint8_t>(pctf + 0.5f);

    if (filteredVoltage >= BATT_FULL_V + 0.05f) {
      fullCount++;
      if (fullCount >= 3) battState = FULL;
    } else {
      fullCount = 0;
    }

    if (!slopeInit) {
      if (filteredVoltage >= BATT_FULL_V + 0.05f) {
        fullCount = 3; battState = FULL; cachedPct = 100;
      } else if (voltage >= 4.10f) {
        battState = CHARGING;
      }
      lastRawV_forSlope = voltage;
      slopeInit = true;
    }

    float dv_raw = voltage - lastRawV_forSlope;

    bool stepRise = (dv_raw >= CHG_STEP_RISE_V);

    float pos = (dv_raw > 0.0f) ? dv_raw : 0.0f;
    riseWindow[riseIdx] = pos;
    riseIdx = (uint8_t)((riseIdx + 1) % 3);
    float sumRise = riseWindow[0] + riseWindow[1] + riseWindow[2];
    bool cumRise = (sumRise >= CHG_CUM_RISE_V);

    bool riseOK = CHG_REQUIRE_BOTH ? (stepRise && cumRise) : (stepRise || cumRise);

    if (battState != FULL) {
      if (riseOK) {
        battState = CHARGING;
      } else if (dv_raw <= CHG_NEG_DROP_V) {
        battState = DISCHARGING;
      }
    }

    lastRawV_forSlope = voltage;

    if ((prevBattStateLocal == CHARGING || prevBattStateLocal == FULL) && battState == DISCHARGING) {
      filteredVoltage = voltage;
      float pctfSnap = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_FULL_V) * 100.0f; // preserved from original
      pctfSnap = constrain(pctfSnap, 0.0f, 100.0f);
      cachedPct = static_cast<uint8_t>(pctfSnap + 0.5f);
      riseWindow[0] = riseWindow[1] = riseWindow[2] = 0.0f;
      riseIdx = 0;
      lastRawV_forSlope = voltage;
    }

    if (battState == FULL) {
      cachedPct = 100;
    }

    prevBattStateLocal = battState;
  }
  return cachedPct;
}

bool isCharging() {
  return battState == CHARGING; // simple exposure for other modules
}

} // namespace setup_battery