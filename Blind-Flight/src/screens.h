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

// Session 17
extern const Screen screenSettings;
extern const Screen screenAbout;

// Motor diagnostics screen (pending — see screen_motor_test.cpp)
extern const Screen screenMotorTest;

// Calibration screen (pour offset fine-tuning)
extern const Screen screenCalibrate;

// Favorites management screen (Settings sub-screen)
extern const Screen screenFavorites;

// Diagnostics screen (hidden — long-press encoder on About)
extern const Screen screenDiagnostics;

// Head-to-Head multiplayer (Session 23)
extern const Screen screenH2H;

// Palate Training sub-mode selector
extern const Screen screenPalateTraining;

// Wi-Fi setup sub-screen (STA mode credential management)
extern const Screen screenWifiSetup;

// OTA firmware update screen
extern const Screen screenOtaUpdate;

// Join Wi-Fi screen (QR code + password + PIN)
extern const Screen screenWifiQR;

// Wi-Fi connect confirmation screen
extern const Screen screenWifiConfirm;

// Blocking homing helper — draws its own UI, returns to caller.
// Call this from setup() or from a screen's input handler.
// Returns true if homing succeeded.
bool runHomingSequence();
