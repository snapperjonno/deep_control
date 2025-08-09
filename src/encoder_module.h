// File: encoder_module.h
//
// encoder_module.h: Declares encoder module interface.
// IMPORTANT: Do NOT modify this file.
#pragma once

namespace encoder_module {
  void begin();  // Setup encoder pins
  void update(); // Poll encoder rotation
}
