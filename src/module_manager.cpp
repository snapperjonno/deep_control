// File: module_manager.cpp (updated)
// IMPORTANT: Append new modules only.
#include "module_manager.h"
#include "display_module.h"

namespace module_manager {
  const module_fn BEGIN_FNS[] = {
    display_module::begin,
    settings_module::begin,
    mux_module::begin,
    encoder_module::begin,
    mirror_module::begin,
    mode_manager::begin
  };
  const size_t BEGIN_COUNT = sizeof(BEGIN_FNS) / sizeof(BEGIN_FNS[0]);

  const module_fn UPDATE_FNS[] = {
    mux_module::update,
    encoder_module::update,
    mirror_module::update,
    setup_module::update,
    mode_manager::update,
    display_module::update
  };
  const size_t UPDATE_COUNT = sizeof(UPDATE_FNS) / sizeof(UPDATE_FNS[0]);
}