#pragma once

// ============================================================
// Blind Flight — Screen Definitions (updated Session 17)
// ============================================================
// Each screen is a const Screen struct with draw, handleInput,
// and optional onEnter callbacks. Screens are pushed/popped
// via the UI module's screen stack.
//
// Session 17 additions:
//   screenSettings — persistent settings with edit mode
//   screenAbout    — system info with live battery/Wi-Fi
// ============================================================

#include "ui.h"

// Screen instances
extern const Screen screenHome;
extern const Screen screenMenu;
extern const Screen screenDetail;
extern const Screen screenListDemo;
extern const Screen screenTextDemo;

// Session 17
extern const Screen screenSettings;
extern const Screen screenAbout;

// Motor diagnostics screen (pending — see screen_motor_test.cpp)
extern const Screen screenMotorTest;

// Blocking homing helper — draws its own UI, returns to caller.
// Call this from setup() or from a screen's input handler.
// Returns true if homing succeeded.
bool runHomingSequence();
