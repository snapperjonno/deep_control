// =============================
// File: src/encoder_module.cpp
// =============================
#include "encoder_module.h"
#include <Arduino.h>
#include "pinmap_module.h"
#include "mux_module.h"
#include "setup_module.h"

// Debounce for the muxed buttons
static const unsigned long DEBOUNCE_MS = 20;

static bool enc_sw_prev = false; static unsigned long enc_sw_ts = 0;
static bool tog_up_prev = false; static unsigned long tog_up_ts = 0;
static bool tog_dn_prev = false; static unsigned long tog_dn_ts = 0;

// Quadrature transition table (Gray code), signed movement per A/B edge
static const int8_t TRANS[16] = {
  0,-1,+1, 0,
 +1, 0, 0,-1,
 -1, 0, 0,+1,
  0,+1,-1, 0
};

static uint8_t ab_prev = 0;
static int8_t  mv_sum  = 0;

// Robust step emission: one logical step per full quadrature cycle (4 transitions)
static const int STEP_THRESH = 4;

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
}

void encoder_module::update() {
  // --- Quadrature decode ---
  uint8_t a = (uint8_t)digitalRead(pinmap::ENC_PIN_A);
  uint8_t b = (uint8_t)digitalRead(pinmap::ENC_PIN_B);
  uint8_t ab_now = (uint8_t)(((a << 1) | b) & 0x03);

  uint8_t idx = (uint8_t)((ab_prev << 2) | ab_now);
  mv_sum += TRANS[idx];
  ab_prev = ab_now;

  // Emit when a full cycle worth of movement accumulates (direction-agnostic)
if (mv_sum >= STEP_THRESH) {
    setup_module::onEncoderTurn(+1);
    mv_sum -= STEP_THRESH;
} else if (mv_sum <= -STEP_THRESH) {
    setup_module::onEncoderTurn(-1);
    mv_sum += STEP_THRESH;
}

  // --- Buttons via mux ---
  unsigned long now = millis();

  bool enc_sw_now = mux_module::read_channel(pinmap::ENC_SW);
  if (fell(enc_sw_now, enc_sw_prev, enc_sw_ts, now)) {
    setup_module::onEncoderPress();
  }

  bool up_now = mux_module::read_channel(pinmap::TOGGLE_UP);
  bool dn_now = mux_module::read_channel(pinmap::TOGGLE_DOWN);
  if (rose(up_now, tog_up_prev, tog_up_ts, now)) setup_module::onToggle(+1);
  if (rose(dn_now, tog_dn_prev, tog_dn_ts, now)) setup_module::onToggle(-1);
}