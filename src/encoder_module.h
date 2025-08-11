// File: src/encoder_module.h
#pragma once

namespace encoder_module {
  void begin();   // Setup encoder pins + seed mux states
  void update();  // Poll rotation, encoder press (via mux), and toggle (via mux)
}
