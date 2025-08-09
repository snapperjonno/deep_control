// File: mode_manager.cpp
#include "mode_manager.h"
#include "mirror_module.h"
#include "settings_module.h"
#include "setup_module.h"
#include "play_module.h"

bool setupMode = false;

void mode_manager::begin() {
  mirror_module::begin();
  settings_module::begin();

  setupMode = true;  // force SETUP screen on boot for now

  if (setupMode) {
    setup_module::begin();  // now runs!
  } else {
    play_module::begin();
  }
}

void mode_manager::update() {
  if (setupMode) {
    setup_module::update();
  } else {
    play_module::update();
  }
}
