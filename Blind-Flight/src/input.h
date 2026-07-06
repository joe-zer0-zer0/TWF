#pragma once

// ============================================================
// Blind Flight — Input Module (updated Session 16)
// ============================================================
// Handles rotary encoder (interrupt-driven) and soft buttons
// (polled with debounce). Call inputInit() once, then
// inputUpdate() every loop iteration before reading state.
//
// Session 16 addition: inputInjectEvent() — push a synthetic
// event into the queue from outside the ISR (e.g., from the
// Wi-Fi phone interface). Safe to call from the main loop
// context only (not from ISRs).
// ============================================================

#include <Arduino.h>

// Input event types for the screen system
enum InputEvent {
    INPUT_NONE,
    INPUT_ENC_CW,       // Encoder rotated clockwise (one detent)
    INPUT_ENC_CCW,      // Encoder rotated counter-clockwise
    INPUT_ENC_CLICK,    // Encoder push-button pressed
    INPUT_BTN_LEFT,     // Left soft button pressed
    INPUT_BTN_RIGHT     // Right soft button pressed
};

// --- API ---
void inputInit();
void inputUpdate();     // Call every loop — samples hardware, updates internal state

// After calling inputUpdate(), call inputGetEvent() in a loop
// until it returns INPUT_NONE to drain all pending events.
InputEvent inputGetEvent();

// Push a synthetic event into the queue (Session 16).
// Used by the Wi-Fi phone interface to inject button actions.
// Call from main loop context only — not interrupt-safe.
void inputInjectEvent(InputEvent evt);

// Raw encoder position (useful for screens that want cumulative delta)
int inputGetEncoderPos();
void inputSetEncoderPos(int pos);
