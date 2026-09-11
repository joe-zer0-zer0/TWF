#pragma once

#include <Arduino.h>

// ============================================================
// Blind Flight — Status LED Module
// ============================================================
// Drives a common-cathode bi-color (red/green) LED via PWM.
// Red + green = amber. Patterns encode device state.
//
// Priority (highest wins):
//   OTA > error > lockout > low battery > spinning > pouring
//   > game active > charging > ready
// ============================================================

enum LedState {
    LED_OFF,
    LED_BOOT,           // solid amber — startup
    LED_HOMING,         // pulsing amber — motor homing
    LED_READY,          // solid green — idle, ready to play
    LED_GAME_ACTIVE,    // slow blink green — game in progress
    LED_SPINNING,       // fast blink amber — motor moving
    LED_POURING,        // pulse green — pour in progress
    LED_TASTING,        // double blink green — tasting phase
    LED_LOW_BATTERY,    // slow blink red — battery ≤25%
    LED_LOCKOUT,        // solid red — battery ≤15%
    LED_CHARGING,       // slow pulse amber — charging
    LED_OTA,            // fast alternating red/green — firmware update
    LED_ERROR,          // rapid blink red — error condition
};

void ledInit();

// Call from loop(). Determines effective state (applies battery/OTA
// overrides on top of the caller-set state) and drives animation.
void ledUpdate();

// Set the base state. Battery and OTA overrides are applied
// automatically in ledUpdate(), so callers only need to set
// game/ready/boot/homing states here.
void ledSetState(LedState state);

LedState ledGetState();

// Drive one animation frame without state logic. Call from inside
// blocking loops (homing, delayWithAudio, OTA download) to keep
// the LED animated while the main loop is blocked.
void ledTick();

// OTA override — call before/after a firmware update to force
// the LED into OTA mode regardless of other state.
void ledSetOtaOverride(bool active);

// Charging detection — reads the TP4056 CHRG pin if wired.
// Returns true when the charger IC reports active charging.
bool ledIsCharging();
