#pragma once

// ============================================================
// Blind Flight — Motor Module
// ============================================================
// TMC2209 step/dir control with trapezoidal acceleration,
// position tracking, and Hall-sensor homing.
// All movement is blocking (tight step loop).
//
// Session 17 additions:
//   - motorSetSpinSpeed() — runtime speed/rev preset
//   - motorIsHomed() — query homing state for About screen
//
// Session 18 changes:
//   - Center-finding homing (scans both magnet edges)
//   - DIR polarity fix via MOTOR_CW_DIR / MOTOR_CCW_DIR
//   - POUR_OFFSET applied in motorMoveToGlass / motorSpinToGlass
//
// Session 19 changes:
//   - motorGetLastDrift() is now backed by real Hall-sensor
//     verification during motorSpinToGlass (see motor.cpp).
// ============================================================

#include <Arduino.h>

void motorInit();

// Home the disc — scans for Hall sensor, finds both edges of the
// magnet, and centers on the midpoint. Self-calibrates for magnet
// width, eliminating per-unit HOME_OFFSET tuning.
// Returns true if home found, false on timeout.
bool motorHome();

// Move to absolute position (0 = home center, in microsteps).
// Automatically picks shortest rotation direction.
void motorMoveToPosition(int targetPos);

// Move to a glass number (1–4) at the pour position.
// Applies POUR_OFFSET so the glass aligns with the pour spout.
void motorMoveToGlass(int glass);

// Spin clockwise with extra full revolutions before landing on target glass.
// Always goes clockwise. Adds extraRevolutions full turns to ensure
// unpredictable spin duration (minimum 1 full revolution even if extraRevolutions=0).
// Applies POUR_OFFSET to the target position.
void motorSpinToGlass(int glass, int extraRevolutions);

// Spin clockwise for exactly the given number of microsteps.
// Updates internal position tracking. Used for theatrical final spins.
void motorSpinSteps(int steps);

// Current motor position in microsteps (0 = home)
int motorGetPosition();

// Enable/disable motor driver (disable saves power, loses holding torque)
void motorEnable();
void motorDisable();

// --- Session 17 additions ---

// Set spin speed preset at runtime.
// 0=Fast (2400sps, 1 extra rev), 1=Normal (1600sps, 1-3 extra),
// 2=Theatrical (800sps, 2-4 extra).
void motorSetSpinSpeed(uint8_t level);

// Set pour side (0=Front, 1=Right, 2=Rear, 3=Left).
// Adds side * MICROSTEPS_PER_GLASS to POUR_OFFSET so the
// disc rotates an extra 0/90/180/270° to align with the
// user's chosen pour spout location.
void motorSetPourSide(uint8_t side);

// Returns the effective pour offset (POUR_OFFSET + side adjustment).
int motorGetPourOffset();

// Has the motor been successfully homed since power-on?
bool motorIsHomed();

// Returns a randomized extra-revolution count based on the current
// spin speed preset (set via motorSetSpinSpeed). Use this instead of
// hardcoding extra revs so presets actually take effect.
int motorGetExtraRevs();

// --- Position verification ---

// Returns the drift (in microsteps) detected on the last verified spin.
// Positive = motor was behind tracked position (took more steps to reach sensor).
// Negative = motor was ahead. 0 = no drift or no spin since boot.
// Only updates when a spin (motorSpinToGlass) actually crosses the home
// magnet — otherwise retains its last value.
int motorGetLastDrift();

// Returns the magnet width (in microsteps) measured during the last
// successful homing sequence. 0 if never homed.
int motorGetLastMagnetWidth();
