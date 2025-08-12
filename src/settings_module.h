// File: settings_module.h
#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace settings_module {
  // Initialize storage and load cached values. Safe to call once at boot.
  void begin();

  // --- Existing getters/setters (unchanged signatures) ---
  uint8_t getBleMidiChannel();
  void    setBleMidiChannel(uint8_t channel);

  uint8_t getDinMidiChannel();
  void    setDinMidiChannel(uint8_t channel);

  uint8_t getPresetMode();
  void    setPresetMode(uint8_t mode);

  uint8_t getLedBrightness();
  void    setLedBrightness(uint8_t level);

  uint8_t getTftBrightness();
  void    setTftBrightness(uint8_t level);

  float   getMirrorDelay();
  void    setMirrorDelay(float delay);

  // Built‑in Fader labels (index into your global fader labels list)
  uint8_t getFaderLabelIndex(uint8_t fader);
  void    setFaderLabelIndex(uint8_t fader, uint8_t labelIndex);

  // Built‑in Stomp labels (index into your global stomp labels list)
  uint8_t getStompLabelIndex(uint8_t stomp);
  void    setStompLabelIndex(uint8_t stomp, uint8_t labelIndex);

  // Fader CC assignments
  uint8_t getFaderCC(uint8_t fader);
  void    setFaderCC(uint8_t fader, uint8_t cc);

  // Stomp CC assignments
  uint8_t getStompCC(uint8_t stomp);
  void    setStompCC(uint8_t stomp, uint8_t cc);

  // Stomp type: 0 = momentary (MO), 1 = toggle (TG)
  uint8_t getStompType(uint8_t stomp);
  void    setStompType(uint8_t stomp, uint8_t type);

  // --- Custom label management ---
  static const uint8_t MAX_CUSTOM_LABELS = 16;

  // Fader custom labels
  uint8_t getCustomFaderLabelCount();
  String  getCustomFaderLabel(uint8_t index);
  void    addCustomFaderLabel(const String& label);
  void    updateCustomFaderLabel(uint8_t index, const String& label);
  void    deleteCustomFaderLabel(uint8_t index);

  // Stomp custom labels
  uint8_t getCustomStompLabelCount();
  String  getCustomStompLabel(uint8_t index);
  void    addCustomStompLabel(const String& label);
  void    updateCustomStompLabel(uint8_t index, const String& label);
  void    deleteCustomStompLabel(uint8_t index);

  // --- New optional helpers (non‑breaking) ---
  bool    applyPresetDefaults(uint8_t mode);
  bool    setPresetModeAndApply(uint8_t mode);
  void    dumpToSerial(Stream& out = Serial);
}