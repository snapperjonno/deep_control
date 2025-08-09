// File: mux_module.cpp
//
// mux_module.cpp: Handles multiplexer channel selection and input reading.
// IMPORTANT: Do NOT modify this file.

#include "mux_module.h"
#include <Arduino.h>
#include "pinmap_module.h"  // Use central pinmap

static bool prev_states[8] = { false };
static unsigned long last_toggle_down_ts = 0;
static const unsigned long DEBOUNCE_MS = 200;

// Selects a channel on the MUX
static void select_channel(uint8_t ch) {
  digitalWrite(pinmap::MUX_CONTROL_A, ch & 1);
  digitalWrite(pinmap::MUX_CONTROL_B, (ch >> 1) & 1);
  digitalWrite(pinmap::MUX_CONTROL_C, (ch >> 2) & 1);
  delayMicroseconds(10);
}

// Public: read one channel state (active LOW)
bool mux_module::read_channel(uint8_t ch) {
  select_channel(ch);
  return digitalRead(pinmap::MUX_COM_PIN) == LOW;
}

void mux_module::begin() {
  pinMode(pinmap::MUX_CONTROL_A, OUTPUT);
  pinMode(pinmap::MUX_CONTROL_B, OUTPUT);
  pinMode(pinmap::MUX_CONTROL_C, OUTPUT);
  pinMode(pinmap::MUX_COM_PIN, INPUT_PULLUP);
  digitalWrite(pinmap::MUX_CONTROL_A, LOW);
  digitalWrite(pinmap::MUX_CONTROL_B, LOW);
  digitalWrite(pinmap::MUX_CONTROL_C, LOW);
  for (uint8_t i = 0; i < 8; ++i) prev_states[i] = read_channel(i);
}

void mux_module::update() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < 8; ++i) {
    bool state = read_channel(i);
    if (state != prev_states[i]) {
      prev_states[i] = state;
      if (state) {
        // Debounce toggle down
        if (i == static_cast<uint8_t>(pinmap::MuxChannel::TOGGLE_DOWN)) {
          if (now - last_toggle_down_ts < DEBOUNCE_MS) continue;
          last_toggle_down_ts = now;
        }
        Serial.print("[MUX] ");
        switch (i) {
          case static_cast<uint8_t>(pinmap::MuxChannel::ENC_SW):      Serial.println("Encoder pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::MIRROR):      Serial.println("Mirror pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::STOMP1):      Serial.println("Stomp1 pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::STOMP2):      Serial.println("Stomp2 pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::STOMP3):      Serial.println("Stomp3 pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::STOMP4):      Serial.println("Stomp4 pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::TOGGLE_DOWN): Serial.println("Toggle Down pressed"); break;
          case static_cast<uint8_t>(pinmap::MuxChannel::TOGGLE_UP):   Serial.println("Toggle Up pressed"); break;
        }
      }
    }
  }
}