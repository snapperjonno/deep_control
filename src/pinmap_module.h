// File: pinmap_module.h
//
// IMPORTANT: Do NOT modify or delete this file without express permission from the project lead.
// pinmap_module.h: Centralized pin and multiplexer channel mappings.
#pragma once
#include <Arduino.h>

namespace pinmap {
  // SPI pins for TFT display
  constexpr uint8_t TFT_SCK     = 5;
  constexpr uint8_t TFT_MOSI    = 19;
  constexpr uint8_t TFT_CS      = 14;
  constexpr uint8_t TFT_DC      = 15;
  constexpr uint8_t TFT_BL_PWM  = 13;
  constexpr int8_t TFT_RST = -1; // TFT reset pin (set to actual pin number when wired)

  // Multiplexer control pins
  constexpr uint8_t MUX_CONTROL_A = 27;
  constexpr uint8_t MUX_CONTROL_B = 22;
  constexpr uint8_t MUX_CONTROL_C = 21;
  constexpr uint8_t MUX_COM_PIN   = 4;

  // Multiplexer channels
  enum MuxChannel : uint8_t {
    ENC_SW = 0, MIRROR, STOMP1, STOMP2, STOMP3, STOMP4, TOGGLE_DOWN, TOGGLE_UP
  };

  // Encoder pins
  constexpr uint8_t ENC_PIN_A = 7;
  constexpr uint8_t ENC_PIN_B = 33;

  // MIDI TX
  constexpr uint8_t MIDI_TX = 8;

  // Faders
  constexpr uint8_t FADER1_PIN = 34;
  constexpr uint8_t FADER2_PIN = 39;
  constexpr uint8_t FADER3_PIN = 36;
  constexpr uint8_t FADER4_PIN = 32;

  // LED PWM
  constexpr uint8_t LED_PWM = 12;

  // Battery sense
  constexpr uint8_t VBAT_PIN = 35;

}