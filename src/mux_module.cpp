// =============================
// File: src/mux_module.cpp
// =============================
#include "mux_module.h"
#include <Arduino.h>
#include "pinmap_module.h"

namespace {
  // Debounce & state (lightweight, edge-only prints)
  static bool prev_states_physical[8] = { false,false,false,false,false,false,false,false };
  static unsigned long last_toggle_down_ts = 0;
  static const unsigned long DEBOUNCE_MS = 200;

  inline void select_channel(uint8_t ch) {
    // Write select lines using configurable bit positions — fixes A/B/C swaps in software
    digitalWrite(pinmap::MUX_CONTROL_A, (ch >> pinmap::MUX_SEL_BIT_A_POS) & 0x01);
    digitalWrite(pinmap::MUX_CONTROL_B, (ch >> pinmap::MUX_SEL_BIT_B_POS) & 0x01);
    digitalWrite(pinmap::MUX_CONTROL_C, (ch >> pinmap::MUX_SEL_BIT_C_POS) & 0x01);
    delayMicroseconds(10);
  }

  inline uint8_t physicalFor(mux_module::MuxInput in) {
    using namespace pinmap;
    switch (in) {
      case mux_module::MuxInput::Stomp1:     return MUX_CH_STOMP1;
      case mux_module::MuxInput::Stomp2:     return MUX_CH_STOMP2;
      case mux_module::MuxInput::Stomp3:     return MUX_CH_STOMP3;
      case mux_module::MuxInput::Stomp4:     return MUX_CH_STOMP4;
      case mux_module::MuxInput::EncoderPush:return MUX_CH_ENC_SW;
      case mux_module::MuxInput::ToggleUp:   return MUX_CH_TOGGLE_UP;
      case mux_module::MuxInput::ToggleDown: return MUX_CH_TOGGLE_DOWN;
      case mux_module::MuxInput::Mirror:     return MUX_CH_MIRROR;
      default:                               return 0;
    }
  }

  inline const __FlashStringHelper* nameFor(uint8_t physicalCh) {
    using namespace pinmap;
    if (physicalCh == MUX_CH_ENC_SW)      return F("Encoder pressed");
    if (physicalCh == MUX_CH_STOMP1)      return F("Stomp1 pressed");
    if (physicalCh == MUX_CH_STOMP2)      return F("Stomp2 pressed");
    if (physicalCh == MUX_CH_STOMP3)      return F("Stomp3 pressed");
    if (physicalCh == MUX_CH_STOMP4)      return F("Stomp4 pressed");
    if (physicalCh == MUX_CH_TOGGLE_DOWN) return F("Toggle Down pressed");
    if (physicalCh == MUX_CH_TOGGLE_UP)   return F("Toggle Up pressed");
    if (physicalCh == MUX_CH_MIRROR)      return F("Mirror pressed");
    return F("Unknown");
  }
}

namespace mux_module {

void begin() {
  pinMode(pinmap::MUX_CONTROL_A, OUTPUT);
  pinMode(pinmap::MUX_CONTROL_B, OUTPUT);
  pinMode(pinmap::MUX_CONTROL_C, OUTPUT);
  pinMode(pinmap::MUX_COM_PIN,   INPUT_PULLUP);
  if (pinmap::MUX_INH >= 0) {
    pinMode(pinmap::MUX_INH, OUTPUT);
    digitalWrite(pinmap::MUX_INH, LOW); // enable the mux
  }

  // Prime state snapshot
  digitalWrite(pinmap::MUX_CONTROL_A, LOW);
  digitalWrite(pinmap::MUX_CONTROL_B, LOW);
  digitalWrite(pinmap::MUX_CONTROL_C, LOW);
  for (uint8_t ch = 0; ch < 8; ++ch) {
    prev_states_physical[ch] = read_channel(ch);
  }
}

bool read_channel(uint8_t ch) {
  select_channel(ch);
  // Active LOW (buttons to GND with COM pullup)
  return digitalRead(pinmap::MUX_COM_PIN) == LOW;
}

bool read_input(MuxInput input) {
  return read_channel(physicalFor(input));
}

void update() {
  const unsigned long now = millis();

  for (uint8_t ch = 0; ch < 8; ++ch) {
    const bool state = read_channel(ch);

    if (state != prev_states_physical[ch]) {
      prev_states_physical[ch] = state;

      if (state) { // only on press
        if (ch == pinmap::MUX_CH_TOGGLE_DOWN) {
          if (now - last_toggle_down_ts < DEBOUNCE_MS) continue;
          last_toggle_down_ts = now;
        }
        Serial.print(F("[MUX] "));
        Serial.println(nameFor(ch));
      }
    }
  }
}

void debug_scan_once() {
  Serial.println(F("[MUX] scan: ch=0..7 (LOW=pressed)"));
  for (uint8_t ch = 0; ch < 8; ++ch) {
    bool s = read_channel(ch);
    Serial.print(F("  ch "));
    Serial.print(ch);
    Serial.print(F(": "));
    Serial.println(s ? F("LOW") : F("HIGH"));
  }
}

} // namespace mux_module
