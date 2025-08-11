// File: src/layout_constants.h

#pragma once

// ==============================
// Shared across all screens
// ==============================
#define SCREEN_W      240
#define SCREEN_H      135

#define TRI_SIDE       20
#define TRI_MARGIN_L    7
#define TRI_MARGIN_R    7

// ==============================
// Layout: Setup screens (battery, LED level, LED brightness)
// These values match your current battery layout exactly
// ==============================

// Header triangles Y for setup screens (use if/when you switch setup_module to this)
#define SETUP_TRI_Y           22   // tweak to taste; currently header code uses TRI_Y

// Progress/value bar geometry (matches battery)
#define SETUP_BAR_X           64
#define SETUP_BAR_Y           50
#define SETUP_BAR_WIDTH       164
#define SETUP_BAR_HEIGHT      24

// Number value position (matches battery)
#define SETUP_VALUE_X         10
#define SETUP_VALUE_Y         70

// Bottom label Y (matches battery)
#define SETUP_BOTTOM_TEXT_Y   126

// LED/bar segment counts
#define SETUP_BATTERY_SECTIONS    5   // battery screen
#define SETUP_LED_SECTIONS        4   // LED screens (level + brightness)
