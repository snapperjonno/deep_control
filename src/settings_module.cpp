// File: settings_module.cpp
#include "settings_module.h"
#include <Preferences.h>

namespace settings_module {
  // -------- constants / helpers --------
  static Preferences prefs;
  static constexpr const char* NAMESPACE = "setup";
  static constexpr uint32_t CURRENT_SCHEMA_VERSION = 1; // bump when storage layout changes

  template <typename T>
  static inline T clampT(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

  // Keys (singletons). Indexed keys are constructed inline where needed.
  static constexpr const char* KEY_SCHEMA_VERSION      = "schema_version";
  static constexpr const char* KEY_BLE_CH              = "ble_midi_channel";
  static constexpr const char* KEY_DIN_CH              = "din_midi_channel";
  static constexpr const char* KEY_PRESET_MODE         = "preset_mode";
  static constexpr const char* KEY_LED_BRIGHT          = "led_brightness";
  static constexpr const char* KEY_TFT_BRIGHT          = "tft_brightness";
  static constexpr const char* KEY_MIRROR_DELAY_MS     = "mirror_delay_ms";

  static constexpr const char* KEY_FADER_LBL_IDX       = "fader_label_idx_"; // +i
  static constexpr const char* KEY_FADER_CC            = "fader_cc_";         // +i
  static constexpr const char* KEY_STOMP_LBL_IDX       = "stomp_label_idx_"; // +i
  static constexpr const char* KEY_STOMP_CC            = "stomp_cc_";         // +i
  static constexpr const char* KEY_STOMP_TYPE          = "stomp_type_";       // +i

  static constexpr const char* KEY_CUSTOM_FADER_COUNT  = "custom_fader_count";
  static constexpr const char* KEY_CUSTOM_STOMP_COUNT  = "custom_stomp_count";

  // -------- storage-backed state --------
  static uint8_t schemaVersion = CURRENT_SCHEMA_VERSION;
  static uint8_t bleMidiChannel = 1;  // 1..16
  static uint8_t dinMidiChannel = 1;  // 1..16
  static uint8_t presetMode = 0;      // app-defined
  static uint8_t ledBrightness = 10;  // app-defined step index
  static uint8_t tftBrightness = 10;  // app-defined step index
  static float   mirrorDelay   = 0.25f; // seconds

  static uint8_t faderLabelIndex[4]  = {0,0,0,0};
  static uint8_t faderCC[4]          = {20,21,22,23};
  static uint8_t stompLabelIndex[4]  = {0,0,0,0};
  static uint8_t stompCC[4]          = {24,25,26,27};
  static uint8_t stompType[4]        = {0,0,0,0}; // 0=momentary,1=toggle ... app-defined

  // Custom labels live in RAM with a persisted count and individual keys
  static String customFaderLabels[8];
  static uint8_t customFaderLabelCount = 0;
  static String customStompLabels[8];
  static uint8_t customStompLabelCount = 0;

  // -------- internal helpers --------
  static inline uint8_t readU8(const char* key, uint8_t defv) {
    return (uint8_t)prefs.getUInt(key, defv);
  }
  static inline void writeU8(const char* key, uint8_t v) {
    prefs.putUInt(key, v);
  }

  static void loadIndexedU8Array(const char* keyPrefix, uint8_t* arr, size_t n, uint8_t defv) {
    for (size_t i = 0; i < n; ++i) {
      String k = String(keyPrefix) + (uint32_t)i;
      arr[i] = readU8(k.c_str(), defv);
    }
  }
  static void saveIndexedU8Array(const char* keyPrefix, const uint8_t* arr, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      String k = String(keyPrefix) + (uint32_t)i;
      writeU8(k.c_str(), arr[i]);
    }
  }

  static void loadCustomLabels(const char* countKey, const char* itemPrefix, String* dst, uint8_t& count) {
    count = readU8(countKey, 0);
    if (count > 8) count = 8;
    for (uint8_t i = 0; i < count; ++i) {
      String k = String(itemPrefix) + i;
      dst[i] = prefs.getString(k.c_str(), "");
    }
  }
  static void saveCustomLabels(const char* countKey, const char* itemPrefix, const String* src, uint8_t count) {
    writeU8(countKey, count);
    for (uint8_t i = 0; i < count; ++i) {
      String k = String(itemPrefix) + i;
      prefs.putString(k.c_str(), src[i]);
    }
    // remove any extra persisted keys beyond count
    for (uint8_t i = count; i < 8; ++i) {
      String k = String(itemPrefix) + i;
      prefs.remove(k.c_str());
    }
  }

  // -------- API --------
  void begin() {
    prefs.begin(NAMESPACE, false);

    schemaVersion = readU8(KEY_SCHEMA_VERSION, CURRENT_SCHEMA_VERSION);
    if (schemaVersion != CURRENT_SCHEMA_VERSION) {
      // Reset to defaults on schema mismatch
      schemaVersion = CURRENT_SCHEMA_VERSION;
      bleMidiChannel = 1;
      dinMidiChannel = 1;
      presetMode     = 0;
      ledBrightness  = 10;
      tftBrightness  = 10;
      mirrorDelay    = 0.25f;
      for (int i = 0; i < 4; ++i) {
        faderLabelIndex[i] = 0; faderCC[i] = 20 + i;
        stompLabelIndex[i] = 0; stompCC[i] = 24 + i; stompType[i] = 0;
      }
      customFaderLabelCount = 0; customStompLabelCount = 0;

      prefs.clear();
      writeU8(KEY_SCHEMA_VERSION, schemaVersion);
      writeU8(KEY_BLE_CH, bleMidiChannel);
      writeU8(KEY_DIN_CH, dinMidiChannel);
      writeU8(KEY_PRESET_MODE, presetMode);
      writeU8(KEY_LED_BRIGHT, ledBrightness);
      writeU8(KEY_TFT_BRIGHT, tftBrightness);
      prefs.putFloat(KEY_MIRROR_DELAY_MS, mirrorDelay * 1000.0f);
      saveIndexedU8Array(KEY_FADER_LBL_IDX, faderLabelIndex, 4);
      saveIndexedU8Array(KEY_FADER_CC, faderCC, 4);
      saveIndexedU8Array(KEY_STOMP_LBL_IDX, stompLabelIndex, 4);
      saveIndexedU8Array(KEY_STOMP_CC, stompCC, 4);
      saveIndexedU8Array(KEY_STOMP_TYPE, stompType, 4);
      saveCustomLabels(KEY_CUSTOM_FADER_COUNT, "custom_fader_label_", customFaderLabels, customFaderLabelCount);
      saveCustomLabels(KEY_CUSTOM_STOMP_COUNT, "custom_stomp_label_", customStompLabels, customStompLabelCount);
    } else {
      bleMidiChannel = clampT<uint8_t>(readU8(KEY_BLE_CH, 1), 1, 16);
      dinMidiChannel = clampT<uint8_t>(readU8(KEY_DIN_CH, 1), 1, 16);
      presetMode     = readU8(KEY_PRESET_MODE, 0);
      ledBrightness  = readU8(KEY_LED_BRIGHT, 10);
      tftBrightness  = readU8(KEY_TFT_BRIGHT, 10);
      mirrorDelay    = prefs.getFloat(KEY_MIRROR_DELAY_MS, 250.0f) / 1000.0f;

      loadIndexedU8Array(KEY_FADER_LBL_IDX, faderLabelIndex, 4, 0);
      loadIndexedU8Array(KEY_FADER_CC, faderCC, 4, 20);
      loadIndexedU8Array(KEY_STOMP_LBL_IDX, stompLabelIndex, 4, 0);
      loadIndexedU8Array(KEY_STOMP_CC, stompCC, 4, 24);
      loadIndexedU8Array(KEY_STOMP_TYPE, stompType, 4, 0);

      loadCustomLabels(KEY_CUSTOM_FADER_COUNT, "custom_fader_label_", customFaderLabels, customFaderLabelCount);
      loadCustomLabels(KEY_CUSTOM_STOMP_COUNT, "custom_stomp_label_", customStompLabels, customStompLabelCount);
    }
  }

  // --- ble midi channel ---
  uint8_t getBleMidiChannel() { return bleMidiChannel; }
  void setBleMidiChannel(uint8_t ch) {
    bleMidiChannel = clampT<uint8_t>(ch, 1, 16);
    writeU8(KEY_BLE_CH, bleMidiChannel);
  }

  // --- din midi channel ---
  uint8_t getDinMidiChannel() { return dinMidiChannel; }
  void setDinMidiChannel(uint8_t ch) {
    dinMidiChannel = clampT<uint8_t>(ch, 1, 16);
    writeU8(KEY_DIN_CH, dinMidiChannel);
  }

  // --- preset mode ---
  uint8_t getPresetMode() { return presetMode; }
  void setPresetMode(uint8_t m) {
    presetMode = m;
    prefs.putUInt(KEY_PRESET_MODE, m);
  }

  uint8_t getLedBrightness() { return ledBrightness; }
  void setLedBrightness(uint8_t l) {
    // No clamping here to avoid assumptions about your step table.
    ledBrightness = l;
    prefs.putUInt(KEY_LED_BRIGHT, l);
  }

  uint8_t getTftBrightness() { return tftBrightness; }
  void setTftBrightness(uint8_t l) {
    // No clamping here to avoid assumptions about your step table.
    tftBrightness = l;
    prefs.putUInt(KEY_TFT_BRIGHT, l);
  }

  float getMirrorDelaySeconds() { return mirrorDelay; }
  void setMirrorDelaySeconds(float s) {
    mirrorDelay = (s < 0.0f) ? 0.0f : s;
    prefs.putFloat(KEY_MIRROR_DELAY_MS, mirrorDelay * 1000.0f);
  }

  // --- label index getters/setters ---
  uint8_t getFaderLabelIndex(uint8_t i) { return (i < 4) ? faderLabelIndex[i] : 0; }
  void setFaderLabelIndex(uint8_t i, uint8_t idx) {
    if (i < 4) { faderLabelIndex[i] = idx; String k = String(KEY_FADER_LBL_IDX) + i; writeU8(k.c_str(), idx); }
  }
  uint8_t getStompLabelIndex(uint8_t i) { return (i < 4) ? stompLabelIndex[i] : 0; }
  void setStompLabelIndex(uint8_t i, uint8_t idx) {
    if (i < 4) { stompLabelIndex[i] = idx; String k = String(KEY_STOMP_LBL_IDX) + i; writeU8(k.c_str(), idx); }
  }

  // --- cc getters/setters ---
  uint8_t getFaderCC(uint8_t i) { return (i < 4) ? faderCC[i] : 0; }
  void setFaderCC(uint8_t i, uint8_t cc) {
    if (i < 4) { faderCC[i] = cc; String k = String(KEY_FADER_CC) + i; writeU8(k.c_str(), cc); }
  }
  uint8_t getStompCC(uint8_t i) { return (i < 4) ? stompCC[i] : 0; }
  void setStompCC(uint8_t i, uint8_t cc) {
    if (i < 4) { stompCC[i] = cc; String k = String(KEY_STOMP_CC) + i; writeU8(k.c_str(), cc); }
  }

  // --- stomp type getters/setters ---
  uint8_t getStompType(uint8_t i) { return (i < 4) ? stompType[i] : 0; }
  void setStompType(uint8_t i, uint8_t t) {
    if (i < 4) { stompType[i] = t; String k = String(KEY_STOMP_TYPE) + i; writeU8(k.c_str(), t); }
  }

  // --- custom fader labels ---
  uint8_t getCustomFaderLabelCount() { return customFaderLabelCount; }
  String  getCustomFaderLabel(uint8_t i) { return (i < customFaderLabelCount) ? customFaderLabels[i] : String(""); }
  void addCustomFaderLabel(const String& l) {
    if (customFaderLabelCount < 8) {
      customFaderLabels[customFaderLabelCount] = l;
      String k = String("custom_fader_label_") + customFaderLabelCount;
      prefs.putString(k.c_str(), l);
      ++customFaderLabelCount;
      prefs.putUInt(KEY_CUSTOM_FADER_COUNT, customFaderLabelCount);
    }
  }
  void updateCustomFaderLabel(uint8_t i, const String& l) {
    if (i < customFaderLabelCount) {
      String k = String("custom_fader_label_") + i;
      prefs.putString(k.c_str(), l);
      customFaderLabels[i] = l;
    }
  }
  void deleteCustomFaderLabel(uint8_t i) {
    if (i < customFaderLabelCount) {
      for (uint8_t j = i; j + 1 < customFaderLabelCount; ++j) {
        customFaderLabels[j] = customFaderLabels[j + 1];
        String k = String("custom_fader_label_") + j;
        prefs.putString(k.c_str(), customFaderLabels[j]);
      }
      --customFaderLabelCount;
      prefs.putUInt(KEY_CUSTOM_FADER_COUNT, customFaderLabelCount);
      String remKey = String("custom_fader_label_") + customFaderLabelCount;
      prefs.remove(remKey.c_str());
    }
  }

  // --- custom stomp labels ---
  uint8_t getCustomStompLabelCount() { return customStompLabelCount; }
  String  getCustomStompLabel(uint8_t i) { return (i < customStompLabelCount) ? customStompLabels[i] : String(""); }
  void addCustomStompLabel(const String& l) {
    if (customStompLabelCount < 8) {
      customStompLabels[customStompLabelCount] = l;
      String k = String("custom_stomp_label_") + customStompLabelCount;
      prefs.putString(k.c_str(), l);
      ++customStompLabelCount;
      prefs.putUInt(KEY_CUSTOM_STOMP_COUNT, customStompLabelCount);
    }
  }
  void updateCustomStompLabel(uint8_t i, const String& l) {
    if (i < customStompLabelCount) {
      String k = String("custom_stomp_label_") + i;
      prefs.putString(k.c_str(), l);
      customStompLabels[i] = l;
    }
  }
  void deleteCustomStompLabel(uint8_t i) {
    if (i < customStompLabelCount) {
      for (uint8_t j = i; j + 1 < customStompLabelCount; ++j) {
        customStompLabels[j] = customStompLabels[j + 1];
        String k = String("custom_stomp_label_") + j;
        prefs.putString(k.c_str(), customStompLabels[j]);
      }
      --customStompLabelCount;
      prefs.putUInt(KEY_CUSTOM_STOMP_COUNT, customStompLabelCount);
      String remKey = String("custom_stomp_label_") + customStompLabelCount;
      prefs.remove(remKey.c_str());
    }
  }

  // --- new helpers ---
  bool applyPresetDefaults(uint8_t /*mode*/) {
    // Stub: no changes applied yet. Intentionally returns false to signal no‑op.
    // When implemented, ensure this writes dependent values first (CCs, labels, types),
    // and avoids writing preset_mode here — that happens in setPresetModeAndApply().
    return false;
  }

  bool setPresetModeAndApply(uint8_t mode) {
    // 1) Apply defaults for dependent fields first.
    bool applied = applyPresetDefaults(mode);

    // 2) Only after dependent fields are consistent, persist the mode itself.
    setPresetMode(mode);
    return applied;
  }

  void dumpToSerial(Stream& out) {
    out.println(F("[settings_module] dump:"));
    out.print(F(" schema_version: ")); out.println(prefs.getUInt(KEY_SCHEMA_VERSION, CURRENT_SCHEMA_VERSION));
    out.print(F(" bleMidiChannel: ")); out.println(bleMidiChannel);
    out.print(F(" dinMidiChannel: ")); out.println(dinMidiChannel);
    out.print(F(" presetMode:     ")); out.println(presetMode);
    out.print(F(" ledBrightness:  ")); out.println(ledBrightness);
    out.print(F(" tftBrightness:  ")); out.println(tftBrightness);
    out.print(F(" mirrorDelay:    ")); out.println(mirrorDelay, 3);

    for (uint8_t i = 0; i < 4; ++i) {
      out.print(F(" fader[")); out.print(i); out.print(F("] lblIdx=")); out.print(faderLabelIndex[i]);
      out.print(F(" cc=")); out.print(faderCC[i]);
      out.print(F(" | stomp[")); out.print(i); out.print(F("] lblIdx=")); out.print(stompLabelIndex[i]);
      out.print(F(" cc=")); out.print(stompCC[i]);
      out.print(F(" type=")); out.println(stompType[i]);
    }

    out.print(F(" customFaderLabels(")); out.print(customFaderLabelCount); out.println(F(")"));
    for (uint8_t i = 0; i < customFaderLabelCount; ++i) {
      out.print(F("  [")); out.print(i); out.print(F("] ")); out.println(customFaderLabels[i]);
    }

    out.print(F(" customStompLabels(")); out.print(customStompLabelCount); out.println(F(")"));
    for (uint8_t i = 0; i < customStompLabelCount; ++i) {
      out.print(F("  [")); out.print(i); out.print(F("] ")); out.println(customStompLabels[i]);
    }
  }
}
