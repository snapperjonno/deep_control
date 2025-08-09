// File: settings_module.cpp (corrected)
#include "settings_module.h"
#include <Preferences.h>

namespace settings_module {
  static Preferences prefs;
  static constexpr const char* NAMESPACE = "setup";

  static uint8_t bleMidiChannel;
  static uint8_t dinMidiChannel;
  static uint8_t presetMode;
  static uint8_t ledBrightness;
  static uint8_t tftBrightness;
  static float   mirrorDelay;

  static uint8_t faderLabelIndex[4];
  static uint8_t stompLabelIndex[4];
  static uint8_t faderCC[4];
  static uint8_t stompCC[4];
  static uint8_t stompType[4];

  static uint8_t customFaderLabelCount;
  static String  customFaderLabels[MAX_CUSTOM_LABELS];
  static uint8_t customStompLabelCount;
  static String  customStompLabels[MAX_CUSTOM_LABELS];

  void begin() {
    prefs.begin(NAMESPACE, false);
    bleMidiChannel = prefs.getUInt("ble_midi_channel", 1);
    dinMidiChannel = prefs.getUInt("din_midi_channel", 1);
    presetMode     = prefs.getUInt("preset_mode", 0);
    ledBrightness  = prefs.getUInt("led_brightness", 20);
    tftBrightness  = prefs.getUInt("tft_brightness", 20);
    mirrorDelay    = prefs.getFloat("mirror_delay", 0.5f);

    for (uint8_t i = 0; i < 4; ++i) {
      String key;
      key = "fader_lbl_" + String(i);
      faderLabelIndex[i] = prefs.getUInt(key.c_str(), i);

      key = "stomp_lbl_" + String(i);
      stompLabelIndex[i] = prefs.getUInt(key.c_str(), i);

      key = "fader_cc_" + String(i);
      faderCC[i] = prefs.getUInt(key.c_str(), 20 + i);

      key = "stomp_cc_" + String(i);
      stompCC[i] = prefs.getUInt(key.c_str(), 80 + i);

      key = "stomp_type_" + String(i);
      stompType[i] = prefs.getUInt(key.c_str(), 1);
    }

    customFaderLabelCount = prefs.getUInt("custom_fader_label_count", 0);
    for (uint8_t i = 0; i < customFaderLabelCount && i < MAX_CUSTOM_LABELS; ++i) {
      String key = "custom_fader_label_" + String(i);
      customFaderLabels[i] = prefs.getString(key.c_str(), "");
    }

    customStompLabelCount = prefs.getUInt("custom_stomp_label_count", 0);
    for (uint8_t i = 0; i < customStompLabelCount && i < MAX_CUSTOM_LABELS; ++i) {
      String key = "custom_stomp_label_" + String(i);
      customStompLabels[i] = prefs.getString(key.c_str(), "");
    }
  }

  uint8_t getBleMidiChannel() { return bleMidiChannel; }
  void setBleMidiChannel(uint8_t ch) { bleMidiChannel = ch; prefs.putUInt("ble_midi_channel", ch); }

  uint8_t getDinMidiChannel() { return dinMidiChannel; }
  void setDinMidiChannel(uint8_t ch) { dinMidiChannel = ch; prefs.putUInt("din_midi_channel", ch); }

  uint8_t getPresetMode() { return presetMode; }
  void setPresetMode(uint8_t m) { presetMode = m; prefs.putUInt("preset_mode", m); }

  uint8_t getLedBrightness() { return ledBrightness; }
  void setLedBrightness(uint8_t l) { ledBrightness = l; prefs.putUInt("led_brightness", l); }

  uint8_t getTftBrightness() { return tftBrightness; }
  void setTftBrightness(uint8_t l) { tftBrightness = l; prefs.putUInt("tft_brightness", l); }

  float getMirrorDelay() { return mirrorDelay; }
  void setMirrorDelay(float d) { mirrorDelay = d; prefs.putFloat("mirror_delay", d); }

  uint8_t getFaderLabelIndex(uint8_t f) { return (f < 4) ? faderLabelIndex[f] : 0; }
  void setFaderLabelIndex(uint8_t f, uint8_t i) {
    if (f < 4) {
      String key = String("fader_lbl_") + f;
      prefs.putUInt(key.c_str(), i);
    }
  }

  uint8_t getStompLabelIndex(uint8_t s) { return (s < 4) ? stompLabelIndex[s] : 0; }
  void setStompLabelIndex(uint8_t s, uint8_t i) {
    if (s < 4) {
      String key = String("stomp_lbl_") + s;
      prefs.putUInt(key.c_str(), i);
    }
  }

  uint8_t getFaderCC(uint8_t f) { return (f < 4) ? faderCC[f] : 0; }
  void setFaderCC(uint8_t f, uint8_t c) {
    if (f < 4) {
      String key = String("fader_cc_") + f;
      prefs.putUInt(key.c_str(), c);
    }
  }

  uint8_t getStompCC(uint8_t s) { return (s < 4) ? stompCC[s] : 0; }
  void setStompCC(uint8_t s, uint8_t c) {
    if (s < 4) {
      String key = String("stomp_cc_") + s;
      prefs.putUInt(key.c_str(), c);
    }
  }

  uint8_t getStompType(uint8_t s) { return (s < 4) ? stompType[s] : 1; }
  void setStompType(uint8_t s, uint8_t t) {
    if (s < 4) {
      String key = String("stomp_type_") + s;
      prefs.putUInt(key.c_str(), t);
    }
  }

  uint8_t getCustomFaderLabelCount() { return customFaderLabelCount; }
  String getCustomFaderLabel(uint8_t i) { return (i < customFaderLabelCount) ? customFaderLabels[i] : String(""); }
  void addCustomFaderLabel(const String& l) {
    if (customFaderLabelCount < MAX_CUSTOM_LABELS) {
      String key = String("custom_fader_label_") + customFaderLabelCount;
      prefs.putString(key.c_str(), l);
      customFaderLabels[customFaderLabelCount++] = l;
      prefs.putUInt("custom_fader_label_count", customFaderLabelCount);
    }
  }
  void updateCustomFaderLabel(uint8_t i, const String& l) {
    if (i < customFaderLabelCount) {
      String key = String("custom_fader_label_") + i;
      prefs.putString(key.c_str(), l);
      customFaderLabels[i] = l;
    }
  }
  void deleteCustomFaderLabel(uint8_t i) {
    if (i < customFaderLabelCount) {
      for (uint8_t j = i; j + 1 < customFaderLabelCount; ++j) {
        customFaderLabels[j] = customFaderLabels[j + 1];
        String key = String("custom_fader_label_") + j;
        prefs.putString(key.c_str(), customFaderLabels[j]);
      }
      --customFaderLabelCount;
      prefs.putUInt("custom_fader_label_count", customFaderLabelCount);
      String remKey = String("custom_fader_label_") + customFaderLabelCount;
      prefs.remove(remKey.c_str());
    }
  }

  uint8_t getCustomStompLabelCount() { return customStompLabelCount; }
  String getCustomStompLabel(uint8_t i) { return (i < customStompLabelCount) ? customStompLabels[i] : String(""); }
  void addCustomStompLabel(const String& l) {
    if (customStompLabelCount < MAX_CUSTOM_LABELS) {
      String key = String("custom_stomp_label_") + customStompLabelCount;
      prefs.putString(key.c_str(), l);
      customStompLabels[customStompLabelCount++] = l;
      prefs.putUInt("custom_stomp_label_count", customStompLabelCount);
    }
  }
  void updateCustomStompLabel(uint8_t i, const String& l) {
    if (i < customStompLabelCount) {
      String key = String("custom_stomp_label_") + i;
      prefs.putString(key.c_str(), l);
      customStompLabels[i] = l;
    }
  }
  void deleteCustomStompLabel(uint8_t i) {
    if (i < customStompLabelCount) {
      for (uint8_t j = i; j + 1 < customStompLabelCount; ++j) {
        customStompLabels[j] = customStompLabels[j + 1];
        String key = String("custom_stomp_label_") + j;
        prefs.putString(key.c_str(), customStompLabels[j]);
      }
      --customStompLabelCount;
      prefs.putUInt("custom_stomp_label_count", customStompLabelCount);
      String remKey = String("custom_stomp_label_") + customStompLabelCount;
      prefs.remove(remKey.c_str());
    }
  }
}