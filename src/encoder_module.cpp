// =============================
// File: src/encoder_module.cpp
// Encoder module with direction-change remainder reset + toggle hold auto-repeat
// =============================
#include "encoder_module.h"
#include <Arduino.h>
#include "pinmap_module.h"
#include "mux_module.h"
#include "setup_module.h"

#ifndef ENCODER_DEBUG
#define ENCODER_DEBUG 0 // default off for normal use
#endif

// --- Timing ---------------------------------------------------------------
static const unsigned long DEBOUNCE_MS           = 20;   // for button edges via mux
static const unsigned long TOG_REPEAT_DELAY_MS   = 800; // hold for 0.8s, then auto-repeat
static const unsigned long TOG_REPEAT_PERIOD_MS  = 60;   // repeat every 60ms while held

// --- Muxed inputs state ---------------------------------------------------
static bool enc_sw_prev = false; static unsigned long enc_sw_ts = 0;
static bool tog_up_prev = false; static unsigned long tog_up_ts = 0;
static bool tog_dn_prev = false; static unsigned long tog_dn_ts = 0;

// Auto-repeat trackers (timestamps are 0 when inactive)
static unsigned long up_press_start = 0, up_last_repeat = 0;
static unsigned long dn_press_start = 0, dn_last_repeat = 0;

// --- Quadrature decode ----------------------------------------------------
static const int8_t TRANS[16] = {
  0,-1,+1, 0,
 +1, 0, 0,-1,
 -1, 0, 0,+1,
  0,+1,-1, 0
};

static uint8_t ab_prev = 0;
static int8_t  mv_sum  = 0;
static int8_t  last_dir = 0;

static const int STEP_THRESH = 4; // one emit per full quadrature cycle

static inline bool fell(bool now, bool &prev, unsigned long &ts, unsigned long nowms) {
  bool edge = (prev && !now) && (nowms - ts >= DEBOUNCE_MS);
  if (prev != now) ts = nowms; prev = now; return edge;
}
static inline bool rose(bool now, bool &prev, unsigned long &ts, unsigned long nowms) {
  bool edge = (!prev && now) && (nowms - ts >= DEBOUNCE_MS);
  if (prev != now) ts = nowms; prev = now; return edge;
}

void encoder_module::begin() {
  pinMode(pinmap::ENC_PIN_A, INPUT_PULLUP);
  pinMode(pinmap::ENC_PIN_B, INPUT_PULLUP);
  uint8_t a = (uint8_t)digitalRead(pinmap::ENC_PIN_A);
  uint8_t b = (uint8_t)digitalRead(pinmap::ENC_PIN_B);
  ab_prev = (uint8_t)((a << 1) | b) & 0x03;

  enc_sw_prev = mux_module::read_channel(pinmap::ENC_SW);
  tog_up_prev = mux_module::read_channel(pinmap::TOGGLE_UP);
  tog_dn_prev = mux_module::read_channel(pinmap::TOGGLE_DOWN);
  unsigned long now = millis();
  enc_sw_ts = tog_up_ts = tog_dn_ts = now;

#if ENCODER_DEBUG
  Serial.println(F("[ENC] Debug ON — transitions, emits, and toggle autorepeat"));
  Serial.print(F("[ENC] Initial AB state: ")); Serial.println(ab_prev, BIN);
#endif
}

void encoder_module::update() {
  // --- Quadrature decode ---
  uint8_t a = (uint8_t)digitalRead(pinmap::ENC_PIN_A);
  uint8_t b = (uint8_t)digitalRead(pinmap::ENC_PIN_B);
  uint8_t ab_now = (uint8_t)(((a << 1) | b) & 0x03);

  uint8_t idx = (uint8_t)((ab_prev << 2) | ab_now);
  int8_t delta = TRANS[idx];

  if (delta != 0) {
    int8_t dir = (delta > 0) ? 1 : -1;
    if (last_dir != 0 && dir != last_dir) {
      mv_sum = 0; // clear remainder on direction change
#if ENCODER_DEBUG
      Serial.println(F("[ENC] Direction change — mv_sum reset"));
#endif
    }
    last_dir = dir;
    mv_sum += delta;
#if ENCODER_DEBUG
    Serial.print(F("[ENC] prev:")); Serial.print(ab_prev, BIN);
    Serial.print(F(" now:"));      Serial.print(ab_now,  BIN);
    Serial.print(F(" idx:"));      Serial.print(idx);
    Serial.print(F(" d:"));        Serial.print(delta);
    Serial.print(F(" mv:"));       Serial.println(mv_sum);
#endif
  }

  ab_prev = ab_now;

  if (mv_sum >= STEP_THRESH) {
#if ENCODER_DEBUG
    Serial.print(F("[ENC] EMIT +1, mv_sum before adjust:")); Serial.println(mv_sum);
#endif
    setup_module::onEncoderTurn(+1);
    mv_sum -= STEP_THRESH;
  } else if (mv_sum <= -STEP_THRESH) {
#if ENCODER_DEBUG
    Serial.print(F("[ENC] EMIT -1, mv_sum before adjust:")); Serial.println(mv_sum);
#endif
    setup_module::onEncoderTurn(-1);
    mv_sum += STEP_THRESH;
  }

  // --- Buttons via mux ---
  unsigned long now = millis();

  // Encoder push button
  bool enc_sw_now = mux_module::read_channel(pinmap::ENC_SW);
  if (fell(enc_sw_now, enc_sw_prev, enc_sw_ts, now)) {
    setup_module::onEncoderPress();
  }

  // Toggle UP — edge & hold-to-repeat
  bool up_now = mux_module::read_channel(pinmap::TOGGLE_UP);
  if (rose(up_now, tog_up_prev, tog_up_ts, now)) {
    setup_module::onToggle(+1);            // immediate action on press
    up_press_start = now; up_last_repeat = now; // arm repeat
#if ENCODER_DEBUG
    Serial.println(F("[ENC] TOGGLE +1 (edge)"));
#endif
  }
  // If still held after delay, start repeating
  if (up_now && up_press_start) {
    if (now - up_press_start >= TOG_REPEAT_DELAY_MS) {
      if (now - up_last_repeat >= TOG_REPEAT_PERIOD_MS) {
        setup_module::onToggle(+1);
        up_last_repeat = now;
#if ENCODER_DEBUG
        Serial.println(F("[ENC] TOGGLE +1 (repeat)"));
#endif
      }
    }
  }
  // Release clears repeat arming
  if (!up_now) { up_press_start = 0; up_last_repeat = 0; }

  // Toggle DOWN — edge & hold-to-repeat
  bool dn_now = mux_module::read_channel(pinmap::TOGGLE_DOWN);
  if (rose(dn_now, tog_dn_prev, tog_dn_ts, now)) {
    setup_module::onToggle(-1);            // immediate action on press
    dn_press_start = now; dn_last_repeat = now; // arm repeat
#if ENCODER_DEBUG
    Serial.println(F("[ENC] TOGGLE -1 (edge)"));
#endif
  }
  if (dn_now && dn_press_start) {
    if (now - dn_press_start >= TOG_REPEAT_DELAY_MS) {
      if (now - dn_last_repeat >= TOG_REPEAT_PERIOD_MS) {
        setup_module::onToggle(-1);
        dn_last_repeat = now;
#if ENCODER_DEBUG
        Serial.println(F("[ENC] TOGGLE -1 (repeat)"));
#endif
      }
    }
  }
  if (!dn_now) { dn_press_start = 0; dn_last_repeat = 0; }
}
