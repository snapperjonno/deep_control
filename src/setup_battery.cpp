// =============================
// File: src/setup_battery.cpp
// =============================
#include "setup_battery.h"
#include <Arduino.h>
#include <Adafruit_GFX.h>                       // include before font
#include "fonts/OpenSans_SemiBold14pt7b.h"     // OpenSans font used in project
#include "display_module.h"                    // display_module::tft
#include "pinmap_module.h"                     // pinmap::VBAT_PIN

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
#ifndef COLOR_TRI
  #ifdef ST77XX_WHITE
    #define COLOR_TRI ST77XX_WHITE
  #else
    #define COLOR_TRI 0xFFFF
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

// ---- Layout constants (MATCH ORIGINAL) -------------------------------------
#define NUM_X          10
#define NUM_Y          70
#define BAR_X          64
#define BAR_Y          50
#define BAR_WIDTH      164
#define BAR_HEIGHT     24
#define SECTIONS       5
#define BOTTOM_Y       126

// Battery thresholds Original was 3.45f.
#define BATT_EMPTY_V    3.2f
#define BATT_FULL_V     4.05f

// --- Charging detection tunables --------------------------------------------
// Step rise threshold per 1s sample (Volts). Original was 0.04f.
#ifndef CHG_STEP_RISE_V
#define CHG_STEP_RISE_V 0.1f
#endif

// 3-sample cumulative positive rise (sum over ~3s). Original was 0.015f.
#ifndef CHG_CUM_RISE_V
#define CHG_CUM_RISE_V 0.03f
#endif

// Require BOTH step and cumulative rise to assert CHARGING (1=yes, 0=either).
#ifndef CHG_REQUIRE_BOTH
#define CHG_REQUIRE_BOTH 1
#endif

// Negative drop to assert DISCHARGING (kept same value as before, but tweakable)
#ifndef CHG_NEG_DROP_V
#define CHG_NEG_DROP_V (-0.01f)
#endif


namespace {
  // Charging state (MATCH ORIGINAL)
  enum BattState { DISCHARGING, CHARGING, FULL };
  BattState battState = DISCHARGING;
  int fullCount = 0;
  const float USB_DETECT_V = 4.10f;

  // UI/animation state (MATCH ORIGINAL)
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

  // Bottom label once-per-entry
  void drawBottomLabel() {
    display_module::tft.setFont(&OpenSans_SemiBold14pt7b);
    display_module::tft.setTextColor(COLOR_TEXT, COLOR_BG);
    const char* BOTTOM_TEXT = "battery level";
    int16_t tx, ty; uint16_t tw, th;
    display_module::tft.getTextBounds(BOTTOM_TEXT, 0, BOTTOM_Y, &tx, &ty, &tw, &th);
    display_module::tft.setCursor((int16_t)((int)240 - (int)tw) / 2, BOTTOM_Y); // 240 is SCREEN_W in this project
    display_module::tft.print(BOTTOM_TEXT);
  }
}

namespace setup_battery {

void begin() {
  // Only draw the bottom label; header/lines/triangles are handled in setup_module
  drawBottomLabel();
  // Force first draw when entering screen
  lastPct = 255;
  lastBattState = DISCHARGING;
  chargeAnimLastMs = 0;
  chargeAnimStep = 0;
}

// Exact copy of the original delta bar drawing semantics
static void drawDeltaBar(uint8_t pct) {
  int totalW   = BAR_WIDTH - 1;
  int oldPx    = (lastPct > 100) ? 0 : (totalW * (int)lastPct) / 100;
  int newPx    = (totalW * (int)pct) / 100;
  int sectionW = totalW / SECTIONS;

  if (newPx > oldPx) {
    // Growing: draw only new pixels (right -> left colour indexing)
    for (int p = oldPx; p < newPx; ++p) {
      int x   = BAR_X + totalW - p;
      int idx = p / sectionW; // 0 == rightmost section
      uint16_t col = (idx == 0) ? COLOR_RED
                       : (idx <= 1) ? COLOR_ORANGE
                                    : COLOR_GREEN;
      display_module::tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, col);
    }
  } else if (newPx < oldPx) {
    // Shrinking: clear trailing pixels
    for (int p = newPx; p < oldPx; ++p) {
      int x = BAR_X + totalW - p;
      display_module::tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, COLOR_BG);
    }
  }

  // Keep outline and internal dividers on top (MATCH ORIGINAL)
  display_module::tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, COLOR_LINE);
  for (int i = 1; i < SECTIONS; ++i) {
    int mx = BAR_X + (BAR_WIDTH * i) / SECTIONS;
    display_module::tft.drawLine(mx, BAR_Y, mx, BAR_Y + BAR_HEIGHT - 1, COLOR_LINE);
  }
}

static void eraseOldNumber(uint8_t oldVal) {
  char oldBuf[4]; snprintf(oldBuf, sizeof(oldBuf), "%u", oldVal);
  int16_t obx, oby; uint16_t obw, obh;
  display_module::tft.getTextBounds(oldBuf, NUM_X, NUM_Y, &obx, &oby, &obw, &obh);
  display_module::tft.fillRect(obx - 1, oby - 1, obw + 2, obh + 2, COLOR_BG);
}

static void drawValue(uint8_t val) {
  display_module::tft.setCursor(NUM_X, NUM_Y);
  display_module::tft.print(val);
}

void update() {
  // Read battery percent (original throttling/smoothing lives inside readBatteryPercent)
  uint8_t pct = readBatteryPercent();

  // Charging state UI
  if (battState == CHARGING) {
    if (lastBattState != CHARGING) {
      // Clear old number region generously
      display_module::tft.fillRect(NUM_X - 2, NUM_Y - 22, 240 - NUM_X + 4, 44, COLOR_BG);
      // Clear bar area including outline/dividers
      display_module::tft.fillRect(BAR_X - 2, BAR_Y - 2, BAR_WIDTH + 4, BAR_HEIGHT + 4, COLOR_BG);

      const char* word = "charging";
      const char* fullMsg = "...charging...";
      int16_t tx, ty; uint16_t tw, th;
      display_module::tft.getTextBounds(word, 0, NUM_Y, &tx, &ty, &tw, &th);
      chargeWordX = (int16_t)((240 - (int16_t)tw) / 2);
      chargeWordW = tw;
      chargeY = NUM_Y;
      display_module::tft.setCursor(chargeWordX, chargeY);
      display_module::tft.print(word);

      int16_t dtx, dty; uint16_t dtw, dth;
      display_module::tft.getTextBounds("...", 0, chargeY, &dtx, &dty, &dtw, &dth);
      dotsW = dtw; dotsH = dth; dotsTop = dty;
      leftDotsX  = (int16_t)(chargeWordX - (int16_t)dotsW - 5);
      rightDotsX = (int16_t)(chargeWordX + (int16_t)chargeWordW + 5);

      display_module::tft.getTextBounds(fullMsg, leftDotsX, chargeY, &tx, &ty, &tw, &th);
      chargeBoxLeft = tx; chargeBoxTop = ty; chargeBoxW = tw; chargeBoxH = th;

      chargeAnimStep = 0; chargeAnimLastMs = 0;
    }

    uint32_t nowMs = millis();
    if (chargeAnimLastMs == 0 || nowMs - chargeAnimLastMs >= 1000) {
      chargeAnimLastMs = nowMs;
      if (dotsW > 0 && dotsH > 0) {
        display_module::tft.fillRect(leftDotsX - 2,  dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
        display_module::tft.fillRect(rightDotsX - 2, dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
      }
      uint8_t dots = (chargeAnimStep <= 3) ? (uint8_t)(3 - chargeAnimStep) : 0;
      char left[4]  = {' ', ' ', ' ', 0};
      char right[4] = {' ', ' ', ' ', 0};
      for (uint8_t i = 0; i < dots; ++i) { left[2 - i] = '.'; }
      for (uint8_t i = 0; i < dots; ++i) { right[i] = '.'; }
      display_module::tft.setCursor(leftDotsX, chargeY);
      display_module::tft.print(left);
      display_module::tft.setCursor(rightDotsX, chargeY);
      display_module::tft.print(right);
      chargeAnimStep = (uint8_t)((chargeAnimStep + 1) % 4);
    }

    lastBattState = CHARGING;
    lastPct = pct;
    return;
  }

  // returning from charging -> clear charging box and force full redraw
  if (lastBattState == CHARGING) {
    if (chargeBoxW > 0 && chargeBoxH > 0) {
      display_module::tft.fillRect((int16_t)(chargeBoxLeft - 3), (int16_t)(chargeBoxTop - 4), (uint16_t)(chargeBoxW + 6), (uint16_t)(chargeBoxH + 8), COLOR_BG);
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
  // Throttled to once per second, with slope-based charging detection and
  // filtered voltage mapping — copied from original implementation.
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
    float voltage = mv * 0.001f * 1.922f; // RAW voltage for slope (MATCH ORIGINAL)

    static float filteredVoltage = voltage;
    filteredVoltage = filteredVoltage * 0.8f + voltage * 0.2f;
    float pctf = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
    pctf = constrain(pctf, 0.0f, 100.0f);
    cachedPct = static_cast<uint8_t>(pctf + 0.5f);

    // FULL detection with small hysteresis
    if (filteredVoltage >= BATT_FULL_V + 0.05f) {
      fullCount++;
      if (fullCount >= 3) battState = FULL;
    } else {
      fullCount = 0;
    }

    // Boot-time classification
    if (!slopeInit) {
      if (filteredVoltage >= BATT_FULL_V + 0.05f) {
        fullCount = 3; battState = FULL; cachedPct = 100;
      } else if (voltage >= USB_DETECT_V) {
        battState = CHARGING;
      }
      lastRawV_forSlope = voltage;
      slopeInit = true;
    }

    // Slope-based CHARGING inference using RAW voltage
    float dv_raw = voltage - lastRawV_forSlope;

    // Immediate step trigger (per 1s)
    bool stepRise = (dv_raw >= CHG_STEP_RISE_V);

    // 3-sample cumulative positive rise (~3s window)
    float pos = (dv_raw > 0.0f) ? dv_raw : 0.0f;
    riseWindow[riseIdx] = pos;
    riseIdx = (uint8_t)((riseIdx + 1) % 3);
    float sumRise = riseWindow[0] + riseWindow[1] + riseWindow[2];
    bool cumRise = (sumRise >= CHG_CUM_RISE_V);

    // Combine according to policy (require BOTH by default)
    bool riseOK = CHG_REQUIRE_BOTH ? (stepRise && cumRise) : (stepRise || cumRise);

    if (battState != FULL) {
        if (riseOK) {
            battState = CHARGING;
        } else if (dv_raw <= CHG_NEG_DROP_V) {
            battState = DISCHARGING;
        }
        // else keep previous battState
    }


    lastRawV_forSlope = voltage;

    // Unplug snap
    if ((prevBattStateLocal == CHARGING || prevBattStateLocal == FULL) && battState == DISCHARGING) {
      filteredVoltage = voltage;
      float pctfSnap = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
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

} // namespace setup_battery
