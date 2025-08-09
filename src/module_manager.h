// File: module_manager.h (updated)
#pragma once
#include <stddef.h>
#include "mux_module.h"
#include "encoder_module.h"
#include "mirror_module.h"
#include "display_module.h"
#include "settings_module.h"
#include "mode_manager.h"
#include "setup_module.h"
#include "play_module.h"

using module_fn = void(*)();

namespace module_manager {
  extern const module_fn BEGIN_FNS[];
  extern const size_t    BEGIN_COUNT;
  extern const module_fn UPDATE_FNS[];
  extern const size_t    UPDATE_COUNT;

  inline void begin_all() {
    Serial.print("Running begin_all, count = ");
    Serial.println(BEGIN_COUNT);
    for (size_t i = 0; i < BEGIN_COUNT; ++i) BEGIN_FNS[i]();
  }
  inline void update_all() {
    for (size_t i = 0; i < UPDATE_COUNT; ++i) UPDATE_FNS[i]();
  }
}