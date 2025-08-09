// File: setup_module.cpp
#include "setup_module.h"
#include <Arduino.h>
#include <SPI.h>
#include "display_module.h"
#include "mirror_module.h"
#include "settings_module.h"
#include "fonts/OpenSans_SemiBold14pt7b.h"
#include "pinmap_module.h"
#include "layout_constants.h"

#define COLOR_ORANGE 0xFD20
#define COLOR_BG      ST77XX_BLACK
#define COLOR_TEXT    ST77XX_WHITE
#define COLOR_LINE    ST77XX_WHITE
#define COLOR_TRI     ST77XX_WHITE
#define COLOR_GREEN   ST77XX_GREEN
#define COLOR_RED     ST77XX_RED

#define HEADER_Y       20
#define LINE_Y         13
#define LINE_LENGTH    60
#define LINE_THICKNESS 2
#define LINE_MARGIN_X  10
#define NUM_X          10
#define NUM_Y          70
#define BAR_X          64
#define BAR_Y          50
#define BAR_WIDTH      164
#define BAR_HEIGHT     24
#define SECTIONS       5
#define BOTTOM_Y       126
#define BATT_EMPTY_V    3.45f
#define BATT_FULL_V     4.05f
#define TRI_Y          117

namespace setup_module {
  int currentMenuIndex = 0;

  // Battery charge state detection
  enum BattState { DISCHARGING, CHARGING, FULL };
  static BattState battState = DISCHARGING;
  static int fullCount = 0;
  static const float USB_DETECT_V = 4.10f;

  void begin() {
    currentMenuIndex = 0;
    display_module::tft.fillScreen(COLOR_BG);
    display_module::tft.setFont(&OpenSans_SemiBold14pt7b);
    display_module::tft.setTextColor(COLOR_TEXT, COLOR_BG);

    const char* HEADER_TEXT = "setup";
    int16_t tx, ty; uint16_t tw, th;
    display_module::tft.getTextBounds(HEADER_TEXT, 0, HEADER_Y, &tx, &ty, &tw, &th);
    int16_t hx = (SCREEN_W - tw) / 2;
    display_module::tft.fillRect(LINE_MARGIN_X, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);
    display_module::tft.fillRect(SCREEN_W - LINE_MARGIN_X - LINE_LENGTH, LINE_Y, LINE_LENGTH, LINE_THICKNESS, COLOR_LINE);
    display_module::tft.setCursor(hx, HEADER_Y);
    display_module::tft.print(HEADER_TEXT);

    display_module::tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, COLOR_LINE);
    for (int i = 1; i < SECTIONS; ++i) {
      int mx = BAR_X + (BAR_WIDTH * i) / SECTIONS;
      display_module::tft.drawLine(mx, BAR_Y, mx, BAR_Y + BAR_HEIGHT - 1, COLOR_LINE);
    }

    const char* BOTTOM_TEXT = "battery level";
    display_module::tft.getTextBounds(BOTTOM_TEXT, 0, BOTTOM_Y, &tx, &ty, &tw, &th);
    display_module::tft.setCursor((SCREEN_W - tw) / 2, BOTTOM_Y);
    display_module::tft.print(BOTTOM_TEXT);

    int16_t half = TRI_SIDE / 2;
    display_module::tft.fillTriangle(TRI_MARGIN_L, TRI_Y,
      TRI_MARGIN_L + TRI_SIDE, TRI_Y - half,
      TRI_MARGIN_L + TRI_SIDE, TRI_Y + half,
      COLOR_TRI);
    display_module::tft.fillTriangle(SCREEN_W - TRI_MARGIN_R, TRI_Y,
      SCREEN_W - TRI_MARGIN_R - TRI_SIDE, TRI_Y - half,
      SCREEN_W - TRI_MARGIN_R - TRI_SIDE, TRI_Y + half,
      COLOR_TRI);
  }

  void update() {
    static bool longPressHandled = false;
    if (mirror_module::pressedLong()) {
      if (!longPressHandled) {
        Serial.println("Mirror long press");
        longPressHandled = true;
      }
    } else {
      longPressHandled = false;
    }
    if (currentMenuIndex != 0) return;

    uint8_t pct = readBatteryPercent();
    static uint8_t lastPct = 255;
    static BattState lastBattState = DISCHARGING;
    // Charging UI animation state (no other code touched)
    static uint32_t chargeAnimLastMs = 0;
    static uint8_t  chargeAnimStep = 0; // 0..3 (3 dots down to 0)
    static int16_t  chargeY = NUM_Y;    // baseline Y for vertical centering
    static int16_t  chargeBoxTop = 0;   // bounding box top for clearing
    static uint16_t chargeBoxW = 0;     // bounding box width for clearing
    static uint16_t chargeBoxH = 0;     // bounding box height for clearing
    static int16_t  chargeX = 0;      // X anchor for centered charging text
    static int16_t  chargeWordX = 0; // X for centered "charging" word
    static int16_t  leftDotsX = 0, rightDotsX = 0; // X for left/right dot fields
    static uint16_t dotsW = 0, dotsH = 0; // width/height of "..." field
    static int16_t  dotsTop = 0;          // top of dot fields for clearing
    static int16_t  chargeBoxLeft = 0;    // left of overall charging clear box
    static uint16_t chargeWordW = 0;      // width of the word "charging"

    // Show charging text when charger is detected
    if (battState == CHARGING) {
      // Entering charging state: clear UI regions and initialize animation
      if (lastBattState != CHARGING) {
        // Clear any old number region more generously
        display_module::tft.fillRect(NUM_X - 2, NUM_Y - 22, SCREEN_W - NUM_X + 4, 44, COLOR_BG);
        // Clear the entire bar area INCLUDING outline/dividers
        display_module::tft.fillRect(BAR_X - 2, BAR_Y - 2, BAR_WIDTH + 4, BAR_HEIGHT + 4, COLOR_BG);

        // Prepare centered word at same baseline Y and fixed dot fields on both sides
        const char* word = "charging";
        const char* fullMsg = "...charging..."; // for overall clear box on exit
        int16_t tx, ty; uint16_t tw, th;
        // Center the word itself so it never moves
        display_module::tft.getTextBounds(word, 0, NUM_Y, &tx, &ty, &tw, &th);
        chargeWordX = (int16_t)((SCREEN_W - (int16_t)tw) / 2);
        chargeWordW = tw;
        chargeY = NUM_Y;
        display_module::tft.setCursor(chargeWordX, chargeY);
        display_module::tft.print(word);
        // Measure three-dot field and set fixed positions left and right of the word
        int16_t dtx, dty; uint16_t dtw, dth;
        display_module::tft.getTextBounds("...", 0, chargeY, &dtx, &dty, &dtw, &dth);
        dotsW = dtw; dotsH = dth; dotsTop = dty;
        // Add ~5px gap between word and dots on each side
        leftDotsX  = (int16_t)(chargeWordX - (int16_t)dotsW - 5);
        rightDotsX = (int16_t)(chargeWordX + (int16_t)chargeWordW + 5);
        // Compute bounding box for unplug clear using full message width
        display_module::tft.getTextBounds(fullMsg, (int16_t)(leftDotsX), chargeY, &tx, &ty, &tw, &th);
        chargeBoxLeft = tx; chargeBoxTop = ty; chargeBoxW = tw; chargeBoxH = th;
        chargeAnimStep = 0; chargeAnimLastMs = 0;
      }

      // Animate: two sets of three dots, turning one off each second
      uint32_t nowMs = millis();
      if (chargeAnimLastMs == 0 || nowMs - chargeAnimLastMs >= 1000) {
        chargeAnimLastMs = nowMs;
        // Clear only the dot fields (word remains to avoid flicker)
        if (dotsW > 0 && dotsH > 0) {
          display_module::tft.fillRect(leftDotsX - 2,  dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
          display_module::tft.fillRect(rightDotsX - 2, dotsTop - 2, dotsW + 4, dotsH + 4, COLOR_BG);
        }
        // Build mirrored dot strings: reverse so dots nearest the word stay on longest
        uint8_t dots = (chargeAnimStep <= 3) ? (uint8_t)(3 - chargeAnimStep) : 0;
        char left[4]  = {' ', ' ', ' ', 0};
        char right[4] = {' ', ' ', ' ', 0};
        // Left: keep nearest-to-word first (positions 2, then 1, then 0)
        for (uint8_t i = 0; i < dots; ++i) { left[2 - i] = '.'; }
        // Right: keep nearest-to-word first (positions 0, then 1, then 2)
        for (uint8_t i = 0; i < dots; ++i) { right[i] = '.'; }
        // Draw dots around the fixed word position
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
    // returning from charging: force full redraw next
    if (lastBattState == CHARGING) {
      // Clear any residual charging text area (use stored max bounds)
      if (chargeBoxW > 0 && chargeBoxH > 0) {
        display_module::tft.fillRect((int16_t)(chargeBoxLeft - 3), (int16_t)(chargeBoxTop - 4), (uint16_t)(chargeBoxW + 6), (uint16_t)(chargeBoxH + 8), COLOR_BG);
      }
      lastPct = 255;
    }
    lastBattState = DISCHARGING;

    if (pct == lastPct) return;

    char oldBuf[4]; snprintf(oldBuf, sizeof(oldBuf), "%u", lastPct);
    int16_t obx, oby; uint16_t obw, obh;
    display_module::tft.getTextBounds(oldBuf, NUM_X, NUM_Y, &obx, &oby, &obw, &obh);
    display_module::tft.fillRect(obx - 1, oby - 1, obw + 2, obh + 2, COLOR_BG);

    char buf[4]; snprintf(buf, sizeof(buf), "%u", pct);
    display_module::tft.setCursor(NUM_X, NUM_Y);
    display_module::tft.print(buf);

    // Delta-based update: only redraw the changed edge of the bar
    int totalW = BAR_WIDTH - 1;
    int oldPx = (lastPct > 100) ? 0 : (totalW * lastPct) / 100;
    int newPx = (totalW * pct)     / 100;
    int sectionW = totalW / SECTIONS;
    // Growing: draw only the new pixels
    if (newPx > oldPx) {
      for (int p = oldPx; p < newPx; ++p) {
        int x = BAR_X + totalW - p;
        int idx = p / sectionW;
        uint16_t col = (idx == 0) ? COLOR_RED
                         : (idx <= 1) ? COLOR_ORANGE
                                      : COLOR_GREEN;
        display_module::tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, col);
      }
    }
    // Shrinking: clear only the trailing pixels
    else if (newPx < oldPx) {
      for (int p = newPx; p < oldPx; ++p) {
        int x = BAR_X + totalW - p;
        display_module::tft.fillRect(x, BAR_Y + 1, 1, BAR_HEIGHT - 2, COLOR_BG);
      }
    }

    display_module::tft.drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, COLOR_LINE);
    for (int i = 1; i < SECTIONS; ++i) {
      int mx = BAR_X + (BAR_WIDTH * i) / SECTIONS;
      display_module::tft.drawLine(mx, BAR_Y, mx, BAR_Y + BAR_HEIGHT - 1, COLOR_LINE);
    }
    lastPct = pct;
  }

  uint8_t readBatteryPercent() {
    // Throttle reading to once per second
    static uint32_t lastReadMs = 0;
    static uint8_t cachedPct = 0;
    // Step + short-window cumulative slope on RAW voltage (no extra wiring)
    static float lastRawV_forSlope = 0.0f;
    static bool slopeInit = false;
    static float riseWindow[3] = {0.0f, 0.0f, 0.0f};
    static uint8_t riseIdx = 0;
    // Track previous state locally to detect unplug events and snap filter
    static BattState prevBattStateLocal = DISCHARGING;

    uint32_t now = millis();
    if (lastReadMs == 0 || now - lastReadMs >= 1000) {
        lastReadMs = now;
        // Use ESP32's built-in mV reader for accuracy; include divider ratio
        int mv = analogReadMilliVolts(pinmap::VBAT_PIN);
        float voltage = mv * 0.001f * 1.922f; // RAW voltage for slope

        static float filteredVoltage = voltage;
        filteredVoltage = filteredVoltage * 0.8f + voltage * 0.2f;
        float pctf = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
        pctf = constrain(pctf, 0.0f, 100.0f);
        cachedPct = static_cast<uint8_t>(pctf + 0.5f);
        
        // FULL detection with small hysteresis (unchanged)
        if (filteredVoltage >= BATT_FULL_V + 0.05f) {
            fullCount++;
            if (fullCount >= 3) battState = FULL;
        } else {
            fullCount = 0;
        }

        // Boot-time classification (first sample): infer CHARGING/FULL immediately
        if (!slopeInit) {
            if (filteredVoltage >= BATT_FULL_V + 0.05f) {
                fullCount = 3; // satisfy hysteresis
                battState = FULL;
                cachedPct = 100;
            } else if (voltage >= USB_DETECT_V) {
                battState = CHARGING;
            }
            lastRawV_forSlope = voltage;
            slopeInit = true;
        }
        // Slope-based CHARGING inference using RAW voltage
        float dv_raw = voltage - lastRawV_forSlope;

        // Immediate step trigger: ~25–50 mV jump in 1 s
        bool stepRise = (dv_raw >= 0.04f);

        // 3-sample cumulative fallback: sum of positive rises over ~3 s
        float pos = (dv_raw > 0.0f) ? dv_raw : 0.0f;
        riseWindow[riseIdx] = pos;
        riseIdx = (uint8_t)((riseIdx + 1) % 3);
        float sumRise = riseWindow[0] + riseWindow[1] + riseWindow[2];
        bool cumRise = (sumRise >= 0.015f);

        if (battState != FULL) {
            if (stepRise || cumRise) {
                battState = CHARGING;
            } else if (dv_raw <= -0.01f) {
                battState = DISCHARGING;
            }
            // else keep previous battState
        }

        lastRawV_forSlope = voltage;

        // If we just transitioned from charging/full to discharging (USB unplug),
        // snap the filter to the raw voltage so the displayed percent updates immediately.
        if ((prevBattStateLocal == CHARGING || prevBattStateLocal == FULL) && battState == DISCHARGING) {
            filteredVoltage = voltage;
            float pctfSnap = (filteredVoltage - BATT_EMPTY_V) / (BATT_FULL_V - BATT_EMPTY_V) * 100.0f;
            pctfSnap = constrain(pctfSnap, 0.0f, 100.0f);
            cachedPct = static_cast<uint8_t>(pctfSnap + 0.5f);
            // reset slope window to avoid stale rises
            riseWindow[0] = riseWindow[1] = riseWindow[2] = 0.0f;
            riseIdx = 0;
            lastRawV_forSlope = voltage;
        }

        if (battState == FULL) {
            cachedPct = 100;
        }

        // Update local previous state after all decisions
        prevBattStateLocal = battState;
    }
    return cachedPct;
}
  }
