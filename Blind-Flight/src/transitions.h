#pragma once

// ============================================================
// Blind Flight — Transition Effects (Session 9)
// ============================================================
// Screen transition animations and reusable visual effects.
//
// Transition types are used with the T-suffixed screen
// navigation functions (uiPushScreenT, uiPopScreenT,
// uiReplaceScreenT). The transition runs synchronously
// around the screen swap: phase-out animates over the old
// content, the new screen draws, then phase-in reveals it.
//
// Standalone effects (slot roll, spin indicator) can be
// called directly by any screen — they're not tied to
// screen navigation.
//
// Backlight PWM is managed here since multiple effects
// (fade, wipe) need it. Splash screen uses the shared
// setBacklight() instead of its own copy.
// ============================================================

#include <Arduino.h>
#ifndef HEADLESS_BUILD
#include <TFT_eSPI.h>
#else
class TFT_eSPI;
#endif

// ============================================================
// Transition types for screen navigation
// ============================================================
enum TransitionType {
    TRANS_NONE = 0,     // Instant swap (default, same as non-T functions)
    TRANS_FADE,         // Backlight dim → draw → brighten
    TRANS_WIPE_DOWN,    // Black rows sweep top→bottom, brief fade in
    TRANS_WIPE_LEFT,    // Black cols sweep right→left, brief fade in
};

// ============================================================
// Transition timing constants
// ============================================================

// Fade transition
#define TRANS_FADE_STEPS        8       // Number of brightness steps per phase
#define TRANS_FADE_STEP_MS      25      // Delay per step (~200ms per phase)
#define TRANS_FADE_DARK_MS      40      // Hold at black between phases

// Wipe transition
#define TRANS_WIPE_STRIP_PX     8       // Pixel height/width per wipe step
#define TRANS_WIPE_STEP_MS      3       // Delay per strip (~105ms for 280/8 strips)
#define TRANS_WIPE_FADE_STEPS   5       // Quick fade-in after wipe
#define TRANS_WIPE_FADE_MS      20      // Delay per fade step (~100ms)

// Slot-machine roll
#define TRANS_SLOT_FRAMES       20      // Total animation frames
#define TRANS_SLOT_MAX_OFFSET   120     // Starting pixel offset (text distance to travel)


// ============================================================
// Initialization
// ============================================================

// Initialize backlight PWM. Call once in setup(), after tft.init().
void transInit();

// Provide the TFT pointer (called by uiInit — not needed externally).
void transSetTFT(TFT_eSPI* tftPtr);

// ============================================================
// Backlight control (shared — used by transitions and splash)
// ============================================================

// Set backlight brightness (0 = off, 255 = full).
void setBacklight(int brightness);

// Get current backlight brightness.
int getBacklight();

// ============================================================
// Transition phases (called by screen manager)
// ============================================================

// Phase-out: animate over the current screen content.
// After this returns the display may be black or dimmed.
void transRunOut(TransitionType type);

// Phase-in: reveal the newly-drawn screen content.
// After this returns the backlight is at full brightness.
void transRunIn(TransitionType type);

// ============================================================
// Standalone effects (callable from any screen)
// ============================================================

// Slot-machine text roll: animates text scrolling into place
// within a defined rectangle. The text decelerates from fast
// to stopped, like a slot machine reel settling.
//
// x, y, w, h — bounding rectangle on screen
// text       — final text to display
// fgColor    — text color
// bgColor    — background fill color
// fontSize   — TFT_eSPI text size multiplier
void transSlotRoll(int x, int y, int w, int h,
                   const char* text,
                   uint16_t fgColor, uint16_t bgColor,
                   int fontSize);

