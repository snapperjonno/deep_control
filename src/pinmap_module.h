// =============================
// File: src/pinmap_module.h (update)
// =============================
#pragma once
#include <Arduino.h>

// FEATHER ESP32‑S3 pin & channel map — single source of truth.
// Edit *only this file* when wiring changes.
namespace pinmap {
  // --- TFT SPI (Adafruit 1.14" ST7789 240x135) ---
  constexpr uint8_t TFT_SCK     = 36;   // GPIO for SCK
  constexpr uint8_t TFT_MOSI    = 35;   // GPIO for MOSI
  constexpr uint8_t TFT_CS      = 14;
  constexpr uint8_t TFT_DC      = 8;
  constexpr uint8_t TFT_BL_PWM  = 13;   // LED backlight (LEDC capable)
  constexpr int8_t  TFT_RST     = -1;   // -1 if tied to board reset

  // --- Multiplexer (TC4051B/CD4051B) control pins ---
  constexpr uint8_t MUX_CONTROL_A = 10; // to 4051 A
  constexpr uint8_t MUX_CONTROL_B = 9;  // to 4051 B
  constexpr uint8_t MUX_CONTROL_C = 6;  // to 4051 C
  constexpr uint8_t MUX_COM_PIN   = 5;  // COM -> ESP32 input (INPUT_PULLUP, active LOW)

  // OPTIONAL: /INH (enable). Leave -1 if hard-tied LOW on the PCB.
  constexpr int8_t  MUX_INH       = -1;

  // Select-line bit positions (A=LSB, B=mid, C=MSB)
  constexpr uint8_t MUX_SEL_BIT_A_POS = 0; // A = LSB
  constexpr uint8_t MUX_SEL_BIT_B_POS = 1; // B = middle
  constexpr uint8_t MUX_SEL_BIT_C_POS = 2; // C = MSB

  // --- Logical → Physical MUX channel mapping ---
  constexpr uint8_t MUX_CH_MIRROR      = 0;
  constexpr uint8_t MUX_CH_TOGGLE_DOWN = 1;
  constexpr uint8_t MUX_CH_TOGGLE_UP   = 2;
  constexpr uint8_t MUX_CH_ENC_SW      = 3;
  constexpr uint8_t MUX_CH_STOMP4      = 4;
  constexpr uint8_t MUX_CH_STOMP1      = 5;
  constexpr uint8_t MUX_CH_STOMP3      = 6;
  constexpr uint8_t MUX_CH_STOMP2      = 7;

  // Back-compat aliases
  constexpr uint8_t ENC_SW      = MUX_CH_ENC_SW;
  constexpr uint8_t TOGGLE_UP   = MUX_CH_TOGGLE_UP;
  constexpr uint8_t TOGGLE_DOWN = MUX_CH_TOGGLE_DOWN;
  constexpr uint8_t STOMP1      = MUX_CH_STOMP1;
  constexpr uint8_t STOMP2      = MUX_CH_STOMP2;
  constexpr uint8_t STOMP3      = MUX_CH_STOMP3;
  constexpr uint8_t STOMP4      = MUX_CH_STOMP4;
  constexpr uint8_t MIRROR      = MUX_CH_MIRROR;

  // --- Encoder A/B (direct GPIOs) ---
  constexpr uint8_t ENC_PIN_A = 37;
  constexpr uint8_t ENC_PIN_B = 38;

  // --- DIN MIDI TX ---
  constexpr uint8_t MIDI_TX = 39;   // use Serial1.begin(..., /*rx*/-1, pinmap::MIDI_TX)

  // --- Faders (ADC capable) ---
  constexpr uint8_t FADER1_PIN = 18;
  constexpr uint8_t FADER2_PIN = 17;
  constexpr uint8_t FADER3_PIN = 16;
  constexpr uint8_t FADER4_PIN = 15;

  // --- LED PWM ---
  constexpr uint8_t LED_PWM = 12;

  // --- Power/VBUS sense (divider -> ADC pin). Set -1 to disable.
  constexpr int8_t  VBUS_SENSE_PIN = 4; // your wiring: USB 5V divider -> GPIO4
}
