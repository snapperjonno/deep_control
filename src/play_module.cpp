// File: play_module.cpp
#include "play_module.h"
#include "display_module.h"
#include "encoder_module.h"
#include "mirror_module.h"
#include "settings_module.h"

namespace play_module {
  void begin() {
    // TODO: clear screen, e.g., display_module::tft.fillScreen(ST77XX_BLACK);
    // TODO: implement drawPlayScreen using display_module::tft
  }

  void update() {
    // TODO: integrate encoder_module rotation input for preset change.
    // encoder_module currently provides only begin() and update().
    // Track encoder_value changes to detect delta and send MIDI PC messages.

    // TODO: handle short-press, mirror_module::pressed() not implemented
    // if (mirror_module::pressed()) {
    //   // handle stomp presses
    // }
  }
}  // namespace play_module