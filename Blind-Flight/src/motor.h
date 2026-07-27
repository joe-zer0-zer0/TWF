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

// Homing failure phases, reported in the telemetry H record.
#define HOME_FAIL_NONE              0
#define HOME_FAIL_STUCK_ON_MAGNET   1   // Phase 1: never left the magnet
#define HOME_FAIL_MAGNET_NOT_FOUND  2   // Phase 2: no leading edge in 1.5 rev
#define HOME_FAIL_MAGNET_TOO_WIDE   3   // Phase 3: no trailing edge within 90°

void motorInit();

// Home the disc — scans for Hall sensor, finds both edges of the
// magnet, and centers on the midpoint. Self-calibrates for magnet
// width, eliminating per-unit HOME_OFFSET tuning.
// Returns true if home found, false on timeout.
//
// `attempt` is the 0-based retry index within one runHomingSequence()
// call. It is recorded in telemetry and has no effect on behaviour —
// retry policy lives in the caller, which differs between the screen
// and headless builds.
bool motorHome(int attempt = 0);

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

// Enable/disable motor driver (disable saves power, loses holding torque).
//
// motorEnable() is idempotent and blocks for ~5 ms on the disabled ->
// enabled transition only, giving the TMC2209's charge pump and current
// regulation time to come up before the first STEP pulse. Every code path
// that steps the motor must go through it — stepping into an unsettled
// driver is what makes the first nudge after entering a screen do nothing.
// Never write PIN_MOTOR_EN directly.
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

// Set per-unit home offset trim (microsteps, positive = CW nudge).
// Added to POUR_OFFSET + side adjustment.
void motorSetHomeOffset(int offset);

// Returns the effective pour offset (POUR_OFFSET + side + home trim).
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
// Reset to 0 at the start of every verified move, so a spin that never
// crossed the magnet reports 0 rather than the previous spin's value.
int motorGetLastDrift();

// True if the most recent verified move actually saw a Hall crossing.
// Distinguishes "measured, drift happened to be zero" from "measured
// nothing" — which motorGetLastDrift() alone cannot express, and which
// the closed-loop correction in Session 6 needs.
bool motorGetLastDriftValid();

// Returns the magnet width (in microsteps) measured during the last
// successful homing sequence. 0 if never homed.
int motorGetLastMagnetWidth();
