// File: settings_module.cpp — persistence lock (v2.3: Stomp CC defaults 80–83)
// Changes from v2.2:
//  • First‑run defaults for STOMP CC are now S1..S4 → 80,81,82,83 (was 24..27).
//  • If any s_cc_* key exists, we HONOUR stored values and load with a safe default (80) per slot.
//  • If no s_cc_* key exists, we seed 80..83 into NVS so the UI shows your requested defaults.

#include "settings_module.h"
#include <Preferences.h>

// Enable detailed persistence diagnostics (prints at boot and on saves)
#define SETTINGS_DEBUG 1

namespace settings_module {
  // -------- constants / helpers --------
  static Preferences prefs;
  static constexpr const char* NAMESPACE = "setup";
  static constexpr uint32_t CURRENT_SCHEMA_VERSION = 1; // bump when storage layout changes

  template <typename T>
  static inline T clampT(T v, T lo, T hi) { return (v < lo) ? lo : (v > hi ? hi : v); }

  // Keys (ALL <= 15 chars!)
  static constexpr const char* KEY_SCHEMA_VERSION      = "schema_version"; // 14
  static constexpr const char* KEY_BLE_CH              = "ble_ch";         // 6
  static constexpr const char* KEY_DIN_CH              = "din_ch";         // 6
  static constexpr const char* KEY_PRESET_MODE         = "preset_mode";    // 11
  static constexpr const char* KEY_LED_BRIGHT          = "led_bright";     // 10
  static constexpr const char* KEY_TFT_BRIGHT          = "tft_bright";     // 10
  static constexpr const char* KEY_MIRROR_DELAY_MS     = "mirror_ms";      // 9 (new short key)

  // Short, safe prefixes (<= 15 incl. index when appended)
  static constexpr const char* KEY_FADER_LBL_IDX       = "f_lbl_"; // +i
  static constexpr const char* KEY_FADER_CC            = "f_cc_";  // +i
  static constexpr const char* KEY_STOMP_LBL_IDX       = "s_lbl_"; // +i
  static constexpr const char* KEY_STOMP_CC            = "s_cc_";  // +i
  static constexpr const char* KEY_STOMP_TYPE          = "s_type_";// +i

  static constexpr const char* KEY_CUSTOM_FADER_COUNT  = "f_lbl_n"; // count
  static constexpr const char* KEY_CUSTOM_STOMP_COUNT  = "s_lbl_n"; // count

  // Legacy keys (read-only migration)
  static constexpr const char* LEGACY_BLE_CH_CAMEL     = "bleMidiChannel";  // 14 (u32)
  static constexpr const char* LEGACY_DIN_CH_CAMEL     = "dinMidiChannel";  // 14 (u32)
  static constexpr const char* LEGACY_MIRROR_DELAY_MS  = "mirror_delay_ms"; // 15 (float ms)

  // -------- storage‑backed state (RAM cache) --------
  static uint8_t schemaVersion = CURRENT_SCHEMA_VERSION;
  static uint8_t bleMidiChannel = 1;  // 1..16
  static uint8_t dinMidiChannel = 1;  // 1..16
  static uint8_t presetMode = 0;      // app‑defined
  static uint8_t ledBrightness = 10;  // app‑defined step index
  static uint8_t tftBrightness = 10;  // app‑defined step index
  static float   mirrorDelay   = 0.25f; // seconds

  static uint8_t faderLabelIndex[4]  = {0,0,0,0};
  static uint8_t faderCC[4]          = {20,21,22,23};
  static uint8_t stompLabelIndex[4]  = {0,0,0,0};
  static uint8_t stompCC[4]          = {80,81,82,83};  // NEW defaults
  static uint8_t stompType[4]        = {0,0,0,0}; // 0=momentary,1=toggle ... app‑defined

  // Custom labels live in RAM with a persisted count and individual keys
  static String  customFaderLabels[8];
  static uint8_t customFaderLabelCount = 0;
  static String  customStompLabels[8];
  static uint8_t customStompLabelCount = 0;

  // -------- internal helpers --------
  static inline uint8_t readU8(const char* key, uint8_t defv) { return prefs.getUChar(key, defv); }
  static inline void     writeU8(const char* key, uint8_t v)  { prefs.putUChar(key, v); }

  // Migrate channel from short U8 key or legacy camelCase U32 key
  static uint8_t readChannelMigrating(const char* shortKey, const char* legacyCamelKey) {
    // 1) Prefer short U8 key
    uint8_t u8 = prefs.getUChar(shortKey, 0);
    if (u8 != 0) return clampT<uint8_t>(u8, 1, 16);

    // 2) Legacy camelCase U32
    if (prefs.isKey(legacyCamelKey)) {
      uint32_t u32 = prefs.getUInt(legacyCamelKey, 0);
      if (u32 != 0) {
#ifdef SETTINGS_DEBUG
        Serial.print(F("[settings_module] migrate ")); Serial.print(legacyCamelKey);
        Serial.print(F(" u32=")); Serial.print(u32);
#endif
        uint8_t migrated = clampT<uint8_t>((uint8_t)u32, 1, 16);
        prefs.putUChar(shortKey, migrated); // rewrite under short key
#ifdef SETTINGS_DEBUG
        Serial.print(F(" -> key=")); Serial.print(shortKey);
        Serial.print(F(" u8=")); Serial.println(migrated);
#endif
        return migrated;
      }
    }
    // 3) Default
    return 1;
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
    for (uint8_t i = count; i < 8; ++i) {
      String k = String(itemPrefix) + i;
      prefs.remove(k.c_str());
    }
  }

  // -------- API --------
  void begin() {
    prefs.begin(NAMESPACE, false);
#ifdef SETTINGS_DEBUG
    Serial.println(F("[settings_module] begin()"));
    Serial.print(F("  isKey(ble_ch)=")); Serial.println(prefs.isKey(KEY_BLE_CH));
    Serial.print(F("  ble u8(short)="));    Serial.println(prefs.getUChar(KEY_BLE_CH, 0));
    Serial.print(F("  ble u32(legacy)="));  Serial.println(prefs.getUInt(LEGACY_BLE_CH_CAMEL, 0));
    Serial.print(F("  isKey(din_ch)=")); Serial.println(prefs.isKey(KEY_DIN_CH));
    Serial.print(F("  din u8(short)="));    Serial.println(prefs.getUChar(KEY_DIN_CH, 0));
    Serial.print(F("  din u32(legacy)="));  Serial.println(prefs.getUInt(LEGACY_DIN_CH_CAMEL, 0));
#endif

    // Ensure schema_version exists; if missing, create it
    if (!prefs.isKey(KEY_SCHEMA_VERSION)) {
      schemaVersion = CURRENT_SCHEMA_VERSION;
      writeU8(KEY_SCHEMA_VERSION, (uint8_t)schemaVersion);
    } else {
      schemaVersion = readU8(KEY_SCHEMA_VERSION, CURRENT_SCHEMA_VERSION);
    }

    if (schemaVersion != CURRENT_SCHEMA_VERSION) {
      schemaVersion = CURRENT_SCHEMA_VERSION;
      writeU8(KEY_SCHEMA_VERSION, (uint8_t)schemaVersion);
    }

    // Channels (with migration)
    bleMidiChannel = readChannelMigrating(KEY_BLE_CH, LEGACY_BLE_CH_CAMEL);
    dinMidiChannel = readChannelMigrating(KEY_DIN_CH, LEGACY_DIN_CH_CAMEL);
    // Ensure stored under short keys
    prefs.putUChar(KEY_BLE_CH, bleMidiChannel);
    prefs.putUChar(KEY_DIN_CH, dinMidiChannel);

    // Others
    presetMode     = readU8(KEY_PRESET_MODE, 0);
    ledBrightness  = readU8(KEY_LED_BRIGHT, 10);
    tftBrightness  = readU8(KEY_TFT_BRIGHT, 10);
    {
      float ms = prefs.getFloat(KEY_MIRROR_DELAY_MS, -1.0f);
      if (ms < 0.0f) { ms = prefs.getFloat(LEGACY_MIRROR_DELAY_MS, 250.0f); prefs.putFloat(KEY_MIRROR_DELAY_MS, ms); }
      mirrorDelay = ms / 1000.0f;
    }

    loadIndexedU8Array(KEY_FADER_LBL_IDX, faderLabelIndex, 4, 0);
    loadIndexedU8Array(KEY_FADER_CC,      faderCC,         4, 20);
    loadIndexedU8Array(KEY_STOMP_LBL_IDX, stompLabelIndex, 4, 0);

    // ---- STOMP CC: seed 80..83 on first run; otherwise honour stored values ----
    bool haveAnyStomp = false;
    for (uint8_t i = 0; i < 4; ++i) { String k = String(KEY_STOMP_CC) + i; if (prefs.isKey(k.c_str())) { haveAnyStomp = true; break; } }
    if (haveAnyStomp) {
      loadIndexedU8Array(KEY_STOMP_CC, stompCC, 4, 80);
    } else {
      stompCC[0]=80; stompCC[1]=81; stompCC[2]=82; stompCC[3]=83;
      saveIndexedU8Array(KEY_STOMP_CC, stompCC, 4);
#ifdef SETTINGS_DEBUG
      Serial.println(F("[settings_module] seeded STOMP CC defaults 80,81,82,83"));
#endif
    }

    loadIndexedU8Array(KEY_STOMP_TYPE,    stompType,       4, 0);
  }

  // --- ble midi channel ---
  uint8_t getBleMidiChannel() { return bleMidiChannel; }
  void setBleMidiChannel(uint8_t ch) {
    bleMidiChannel = clampT<uint8_t>(ch, 1, 16);
    prefs.putUChar(KEY_BLE_CH, bleMidiChannel); // short key
#ifdef SETTINGS_DEBUG
    Serial.print(F("[settings_module] save BLE cache=")); Serial.print(bleMidiChannel);
    Serial.print(F(" nvs u8(short)=")); Serial.print(prefs.getUChar(KEY_BLE_CH, 0));
    Serial.print(F(" u32(legacy)="));    Serial.println(prefs.getUInt(LEGACY_BLE_CH_CAMEL, 0));
#endif
  }

  // --- din midi channel ---
  uint8_t getDinMidiChannel() { return dinMidiChannel; }
  void setDinMidiChannel(uint8_t ch) {
    dinMidiChannel = clampT<uint8_t>(ch, 1, 16);
    prefs.putUChar(KEY_DIN_CH, dinMidiChannel); // short key
#ifdef SETTINGS_DEBUG
    Serial.print(F("[settings_module] save DIN cache=")); Serial.print(dinMidiChannel);
    Serial.print(F(" nvs u8(short)=")); Serial.print(prefs.getUChar(KEY_DIN_CH, 0));
    Serial.print(F(" u32(legacy)="));    Serial.println(prefs.getUInt(LEGACY_DIN_CH_CAMEL, 0));
#endif
  }

  // --- preset mode ---
  uint8_t getPresetMode() { return presetMode; }
  void setPresetMode(uint8_t m) {
    presetMode = m;
    writeU8(KEY_PRESET_MODE, m);
  }

  uint8_t getLedBrightness() { return ledBrightness; }
  void setLedBrightness(uint8_t l) {
    ledBrightness = l;
    writeU8(KEY_LED_BRIGHT, l);
  }

  uint8_t getTftBrightness() { return tftBrightness; }
  void setTftBrightness(uint8_t l) {
    tftBrightness = l;
    writeU8(KEY_TFT_BRIGHT, l);
  }

  float getMirrorDelay() { return mirrorDelay; }
  void  setMirrorDelay(float s) {
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
      String k = String("f_lbl_") + customFaderLabelCount;
      prefs.putString(k.c_str(), l);
      ++customFaderLabelCount;
      writeU8(KEY_CUSTOM_FADER_COUNT, customFaderLabelCount);
    }
  }
  void updateCustomFaderLabel(uint8_t i, const String& l) {
    if (i < customFaderLabelCount) {
      String k = String("f_lbl_") + i;
      prefs.putString(k.c_str(), l);
      customFaderLabels[i] = l;
    }
  }
  void deleteCustomFaderLabel(uint8_t i) {
    if (i < customFaderLabelCount) {
      for (uint8_t j = i; j + 1 < customFaderLabelCount; ++j) {
        customFaderLabels[j] = customFaderLabels[j + 1];
        String k = String("f_lbl_") + j;
        prefs.putString(k.c_str(), customFaderLabels[j]);
      }
      --customFaderLabelCount;
      writeU8(KEY_CUSTOM_FADER_COUNT, customFaderLabelCount);
      String remKey = String("f_lbl_") + customFaderLabelCount;
      prefs.remove(remKey.c_str());
    }
  }

  // --- custom stomp labels ---
  uint8_t getCustomStompLabelCount() { return customStompLabelCount; }
  String  getCustomStompLabel(uint8_t i) { return (i < customStompLabelCount) ? customStompLabels[i] : String(""); }
  void addCustomStompLabel(const String& l) {
    if (customStompLabelCount < 8) {
      customStompLabels[customStompLabelCount] = l;
      String k = String("s_lbl_") + customStompLabelCount;
      prefs.putString(k.c_str(), l);
      ++customStompLabelCount;
      writeU8(KEY_CUSTOM_STOMP_COUNT, customStompLabelCount);
    }
  }
  void updateCustomStompLabel(uint8_t i, const String& l) {
    if (i < customStompLabelCount) {
      String k = String("s_lbl_") + i;
      prefs.putString(k.c_str(), l);
      customStompLabels[i] = l;
    }
  }
  void deleteCustomStompLabel(uint8_t i) {
    if (i < customStompLabelCount) {
      for (uint8_t j = i; j + 1 < customStompLabelCount; ++j) {
        customStompLabels[j] = customStompLabels[j + 1];
        String k = String("s_lbl_") + j;
        prefs.putString(k.c_str(), customStompLabels[j]);
      }
      --customStompLabelCount;
      writeU8(KEY_CUSTOM_STOMP_COUNT, customStompLabelCount);
      String remKey = String("s_lbl_") + customStompLabelCount;
      prefs.remove(remKey.c_str());
    }
  }

  bool applyPresetDefaults(uint8_t /*mode*/) { return false; }
  bool setPresetModeAndApply(uint8_t mode) { bool applied = applyPresetDefaults(mode); setPresetMode(mode); return applied; }

  void dumpToSerial(Stream& out) {
    out.println(F("[settings_module] dump:"));
    out.print(F(" schema_version: ")); out.println(readU8(KEY_SCHEMA_VERSION, CURRENT_SCHEMA_VERSION));
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
