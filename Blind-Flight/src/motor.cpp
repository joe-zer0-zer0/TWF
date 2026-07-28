#include "motor.h"
#include "config.h"
#include "telemetry.h"

// ============================================================
// Blind Flight — Motor Module (Session 18)
// ============================================================
// Session 18 changes:
//   - Center-finding homing: scans CW to leading edge, continues
//     to trailing edge, reverses to midpoint. Self-calibrates for
//     magnet width — no per-unit HOME_OFFSET needed.
//   - DIR polarity fix: uses MOTOR_CW_DIR / MOTOR_CCW_DIR from
//     config.h so the software notion of CW matches physical
//     disc rotation viewed from the top.
//   - POUR_OFFSET: motorMoveToGlass and motorSpinToGlass add
//     POUR_OFFSET to each glass position so glasses align with
//     the pour spout (135° CCW from the Hall sensor).
//
// Session 19 changes:
//   - moveStepsVerified(): motorSpinToGlass now watches PIN_HALL
//     during the spin and compares the actual step count at the
//     home-magnet crossing against the expected count, so
//     motorGetLastDrift() reports real data instead of a stub 0.
// ============================================================

// ============================================================
// Internal state
// ============================================================
static int currentMotorPos = 0;
static bool homed = false;

// Runtime spin speed parameters (Session 17)
// Defaults match MOTOR_MAX_SPEED / Normal preset
static int runtimeMaxSpeed   = MOTOR_MAX_SPEED;  // microsteps/sec
static int runtimeExtraMin   = 1;                 // min extra full revolutions
static int runtimeExtraMax   = 3;                 // max extra full revolutions

// Pour side offset (Session 20) + home offset calibration
// Combines POUR_OFFSET with user-selected side and per-unit trim
static int effectivePourOffset = POUR_OFFSET;
static int pourSideValue = 0;    // cached side (0–3)
static int homeOffsetValue = 0;  // cached trim (microsteps)

// Diagnostics
static int  lastDrift      = 0;   // drift (microsteps) from last verified spin
static bool lastDriftValid = false; // did THIS spin actually measure a crossing?
static int lastMagnetWidth = 0;   // magnet width from last homing

// Driver enable state (Session 2 / v1.4.1)
// The TMC2209 needs a few ms after EN goes LOW for its charge pump and
// current regulation to come up. Stepping during that window produces
// reduced torque and the first steps get lost — the stiction symptom.
// Tracking the state means the 5 ms settle is paid exactly once per
// disabled -> enabled transition, not on every move.
static bool driverEnabled = false;

// ============================================================
// Low-level step pulse
// ============================================================
static void stepMotor() {
    digitalWrite(PIN_MOTOR_STEP, HIGH);
    delayMicroseconds(2);
    digitalWrite(PIN_MOTOR_STEP, LOW);
}

// ============================================================
// Move N microsteps with trapezoidal velocity profile
// ============================================================
static int moveSteps(int steps, bool clockwise, int maxSpeed) {
    if (steps <= 0) return 0;

    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(10);

    int accelSteps = (int)((maxSpeed * maxSpeed -
                            MOTOR_MIN_SPEED * MOTOR_MIN_SPEED) /
                           (2.0f * MOTOR_ACCEL));
    int decelSteps = accelSteps;

    if (accelSteps + decelSteps > steps) {
        accelSteps = steps / 2;
        decelSteps = steps - accelSteps;
        // Cap peak speed so actual acceleration stays at MOTOR_ACCEL
        float cappedMax = sqrtf((float)MOTOR_MIN_SPEED * MOTOR_MIN_SPEED +
                                2.0f * MOTOR_ACCEL * accelSteps);
        if (cappedMax < maxSpeed) maxSpeed = (int)cappedMax;
    }

    int cruiseSteps = steps - accelSteps - decelSteps;
    float currentSpeed = MOTOR_MIN_SPEED;

    for (int i = 0; i < steps; i++) {
        stepMotor();

        if (i < accelSteps) {
            float progress = (float)(i + 1) / accelSteps;
            currentSpeed = MOTOR_MIN_SPEED + progress * (maxSpeed - MOTOR_MIN_SPEED);
        } else if (i >= accelSteps + cruiseSteps) {
            int decelIndex = i - accelSteps - cruiseSteps;
            float progress = 1.0f - (float)(decelIndex + 1) / decelSteps;
            currentSpeed = MOTOR_MIN_SPEED + progress * (maxSpeed - MOTOR_MIN_SPEED);
        } else {
            currentSpeed = maxSpeed;
        }

        if (currentSpeed < MOTOR_MIN_SPEED) currentSpeed = MOTOR_MIN_SPEED;
        if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;

        unsigned long stepDelay = (unsigned long)(1000000.0f / currentSpeed);
        delayMicroseconds(stepDelay);

        if ((i & 0xFF) == 0) yield();
    }

    return steps;
}

// Overload for backward compatibility (uses compile-time default)
static int moveSteps(int steps, bool clockwise) {
    return moveSteps(steps, clockwise, MOTOR_MAX_SPEED);
}

// ============================================================
// Verified move — watches Hall sensor for drift (Session 19)
// ============================================================
// Same trapezoidal move as moveSteps(), but for clockwise moves it
// also watches PIN_HALL for the magnet's leading-edge trigger as the
// disc passes the home position. The expected step offset to that
// trigger is computed from startPosNorm and the magnet geometry
// recorded during the last homing (lastMagnetWidth). The leading
// edge sits ~lastMagnetWidth/2 microsteps CCW of center (position 0),
// i.e. at (MICROSTEPS_PER_REV - lastMagnetWidth/2) mod REV.
//
// lastDrift = actualStepsToTrigger - expectedStepsToTrigger.
// Positive = motor took more steps than expected (behind tracked
// position). Negative = fewer steps than expected (ahead).
//
// Verification only happens if: homed, a magnet width was recorded,
// the move is clockwise, and the move is long enough to reach the
// first crossing.
//
// Session 5 (v1.5.0) changes:
//   - lastDrift is reset to 0 at the top of every call, and
//     lastDriftValid says whether THIS move measured anything. The
//     old code left the previous spin's drift in place, so a spin
//     that never crossed the magnet silently re-applied a stale
//     correction in motorSpinToGlass().
//   - Every crossing is recorded, not just the first. With 1–3 extra
//     revolutions that is 2–4 samples per spin, which is what
//     distinguishes error that accumulates during the spin from
//     error that arrives all at once.
//   - lastDrift keeps its old meaning (first crossing). Moving it to
//     the last crossing changes where the disc ends up and belongs to
//     Session 6, not here.
// ============================================================

// Crossings recorded per move. A spin is at most ~4 revolutions, so
// anything beyond this is Hall noise; the overflow count is logged.
#define MAX_CROSSINGS_PER_MOVE  8

static int moveStepsVerified(int steps, bool clockwise, int maxSpeed,
                             int startPosNorm, int contextGlass) {
    if (steps <= 0) return 0;

    lastDrift      = 0;
    lastDriftValid = false;

    bool canVerify = homed && lastMagnetWidth > 0 && clockwise;
    int expectedStepsToTrigger = -1;
    if (canVerify) {
        int leadingEdgePos = (MICROSTEPS_PER_REV - lastMagnetWidth / 2) % MICROSTEPS_PER_REV;
        expectedStepsToTrigger = (leadingEdgePos - startPosNorm + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
        if (expectedStepsToTrigger >= steps) canVerify = false;  // won't reach a crossing this move
    }

    bool prevHallLow = (digitalRead(PIN_HALL) == LOW);

    // Crossings are buffered here and flushed after the move. Logging
    // from inside the step loop would stretch the step interval and
    // cost steps.
    int crossSteps[MAX_CROSSINGS_PER_MOVE];
    int crossCount = 0;

    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(10);

    int accelSteps = (int)((maxSpeed * maxSpeed -
                            MOTOR_MIN_SPEED * MOTOR_MIN_SPEED) /
                           (2.0f * MOTOR_ACCEL));
    int decelSteps = accelSteps;

    if (accelSteps + decelSteps > steps) {
        accelSteps = steps / 2;
        decelSteps = steps - accelSteps;
        float cappedMax = sqrtf((float)MOTOR_MIN_SPEED * MOTOR_MIN_SPEED +
                                2.0f * MOTOR_ACCEL * accelSteps);
        if (cappedMax < maxSpeed) maxSpeed = (int)cappedMax;
    }

    int cruiseSteps = steps - accelSteps - decelSteps;
    float currentSpeed = MOTOR_MIN_SPEED;

    for (int i = 0; i < steps; i++) {
        stepMotor();

        if (canVerify) {
            bool hallLow = (digitalRead(PIN_HALL) == LOW);
            if (hallLow && !prevHallLow) {
                if (crossCount < MAX_CROSSINGS_PER_MOVE) {
                    crossSteps[crossCount] = i + 1;
                }
                crossCount++;
            }
            prevHallLow = hallLow;
        }

        if (i < accelSteps) {
            float progress = (float)(i + 1) / accelSteps;
            currentSpeed = MOTOR_MIN_SPEED + progress * (maxSpeed - MOTOR_MIN_SPEED);
        } else if (i >= accelSteps + cruiseSteps) {
            int decelIndex = i - accelSteps - cruiseSteps;
            float progress = 1.0f - (float)(decelIndex + 1) / decelSteps;
            currentSpeed = MOTOR_MIN_SPEED + progress * (maxSpeed - MOTOR_MIN_SPEED);
        } else {
            currentSpeed = maxSpeed;
        }

        if (currentSpeed < MOTOR_MIN_SPEED) currentSpeed = MOTOR_MIN_SPEED;
        if (currentSpeed > maxSpeed) currentSpeed = maxSpeed;

        unsigned long stepDelay = (unsigned long)(1000000.0f / currentSpeed);
        delayMicroseconds(stepDelay);

        if ((i & 0xFF) == 0) yield();
    }

    // --- Flush crossings to telemetry ---
    // Crossing k sits one full revolution further along than crossing
    // k-1, so its expected step count grows by MICROSTEPS_PER_REV.
    if (canVerify) {
        int logged = (crossCount < MAX_CROSSINGS_PER_MOVE)
                     ? crossCount : MAX_CROSSINGS_PER_MOVE;
        for (int k = 0; k < logged; k++) {
            int expected = expectedStepsToTrigger + k * MICROSTEPS_PER_REV;
            int drift    = crossSteps[k] - expected;
            telemetryLogCrossing(contextGlass, k, expected, crossSteps[k], drift);
            if (k == 0) {
                lastDrift      = drift;
                lastDriftValid = true;
            }
        }
        if (crossCount > logged) {
            telemetryPrintf("# WARN %d Hall crossings in one move, logged %d",
                            crossCount, logged);
        }
    }

    return steps;
}

// ============================================================
// API implementation
// ============================================================

void motorInit() {
    pinMode(PIN_MOTOR_STEP, OUTPUT);
    pinMode(PIN_MOTOR_DIR, OUTPUT);
    pinMode(PIN_MOTOR_EN, OUTPUT);
    digitalWrite(PIN_MOTOR_EN, HIGH);   // Disabled at start
    driverEnabled = false;
    digitalWrite(PIN_MOTOR_STEP, LOW);
    digitalWrite(PIN_MOTOR_DIR, LOW);

    // Hall sensor
    pinMode(PIN_HALL, INPUT);
}

// ============================================================
// Center-finding homing sequence
// ============================================================
// 1. If already on magnet, step off first.
// 2. Scan CW until Hall triggers (leading edge).
// 3. Continue CW slowly until Hall releases (trailing edge).
// 4. Reverse CCW to the midpoint of the magnet.
// Sets position 0 at the magnet center.
// ============================================================

// Read the Hall line and require several consecutive samples to agree
// before believing an edge. The line is single-ended and runs alongside
// the motor leads, and a switching stepper couples short spikes onto
// it; one digitalRead() was all it took to declare the magnet found,
// which is how a 1-microstep "magnet" got accepted. A real edge holds
// for tens of milliseconds at HOMING_SPEED, so the confirmation window
// (~120 us) is far too short to smear it — and the cost is paid only at
// an apparent edge, since a disagreeing sample returns immediately.
static bool hallStable(int level) {
    for (int i = 0; i < HOME_HALL_CONFIRM; i++) {
        if (digitalRead(PIN_HALL) != level) return false;
        if (i + 1 < HOME_HALL_CONFIRM) delayMicroseconds(HOME_HALL_CONFIRM_US);
    }
    return true;
}

bool motorHome(int attempt) {
    motorEnable();

    // --- Phase 1: If sitting on magnet, move off ---
    if (digitalRead(PIN_HALL) == LOW) {
        digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
        delayMicroseconds(10);
        for (int i = 0; i < MICROSTEPS_PER_GLASS; i++) {
            stepMotor();
            delayMicroseconds(1000000 / HOMING_SPEED);
            if (digitalRead(PIN_HALL) == HIGH) break;
            if ((i & 0xFF) == 0) yield();
        }
        // If still on magnet after a full glass worth of steps, bail
        if (digitalRead(PIN_HALL) == LOW) {
            Serial.println("[Motor] Homing FAIL: couldn't move off magnet");
            telemetryLogHoming(0, attempt, HOME_FAIL_STUCK_ON_MAGNET);
            return false;
        }
    }

    // --- Phases 2 and 3: find the magnet and measure it ---
    // These are one loop rather than two straight-line phases because a
    // candidate can now be rejected. A noise pulse costs the few steps
    // spent measuring it and the scan carries on from there, which is
    // much cheaper than failing the whole attempt and re-homing.
    digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
    delayMicroseconds(10);

    const int maxSteps = MICROSTEPS_PER_REV + MICROSTEPS_PER_REV / 2;
    int  stepsUsed   = 0;
    int  magnetWidth = 0;
    int  glitches    = 0;
    bool haveMagnet  = false;

    while (!haveMagnet && stepsUsed < maxSteps) {

        // --- Phase 2: Scan CW for leading edge ---
        bool found = false;
        while (stepsUsed < maxSteps) {
            if (hallStable(LOW)) { found = true; break; }
            stepMotor();
            delayMicroseconds(1000000 / HOMING_SPEED);
            stepsUsed++;
            if ((stepsUsed & 0xFF) == 0) yield();
        }
        if (!found) break;

        // --- Phase 3: Continue CW to trailing edge ---
        // Count how many steps the Hall stays active = magnet width.
        magnetWidth = 0;
        bool exitedMagnet = false;
        for (int i = 0; i < MICROSTEPS_PER_GLASS && stepsUsed < maxSteps; i++) {
            stepMotor();
            delayMicroseconds(1000000 / HOMING_SPEED);
            stepsUsed++;
            magnetWidth++;
            if (hallStable(HIGH)) {
                exitedMagnet = true;
                break;
            }
            if ((i & 0xFF) == 0) yield();
        }

        if (!exitedMagnet) {
            // Ran out of scan budget mid-pulse: report that as not
            // found, which is what it is — do not blame the magnet.
            if (stepsUsed >= maxSteps) break;

            // Magnet wider than 90° — something is wrong
            Serial.printf("[Motor] Homing FAIL: magnet wider than %d steps\n",
                          MICROSTEPS_PER_GLASS);
            telemetryLogHoming(magnetWidth, attempt, HOME_FAIL_MAGNET_TOO_WIDE);
            return false;
        }

        if (magnetWidth >= HOME_MAGNET_WIDTH_MIN) {
            haveMagnet = true;
        } else {
            // Too narrow to be the magnet. The Hall line has already
            // gone inactive, so the outer loop resumes scanning from
            // here. Recorded rather than silently swallowed: the noise
            // rate is data we want.
            glitches++;
            Serial.printf("[Motor] Homing: rejected %d-step pulse as noise (%d so far)\n",
                          magnetWidth, glitches);
            telemetryLogHoming(magnetWidth, attempt, HOME_NOTE_GLITCH_REJECTED);
        }
    }

    if (!haveMagnet) {
        Serial.println("[Motor] Homing FAIL: magnet not found in 1.5 revolutions");
        telemetryLogHoming(0, attempt, HOME_FAIL_MAGNET_NOT_FOUND);
        return false;
    }

    // --- Phase 4: Reverse CCW to magnet center ---
    int centerSteps = magnetWidth / 2;
    digitalWrite(PIN_MOTOR_DIR, MOTOR_CCW_DIR);
    delay(20);  // 20ms settling: disc has physical inertia from CW Phase 3
    for (int i = 0; i < centerSteps; i++) {
        stepMotor();
        delayMicroseconds(1000000 / HOMING_SPEED);
    }

    currentMotorPos = 0;
    homed = true;
    lastMagnetWidth = magnetWidth;

    Serial.printf("[Motor] Homed: magnet width=%d steps (%.1f°), centered (backed %d)\n",
                  magnetWidth, magnetWidth * 360.0f / MICROSTEPS_PER_REV, centerSteps);
    telemetryLogHoming(magnetWidth, attempt, HOME_FAIL_NONE);

    return true;
}

// ============================================================
// Position movement
// ============================================================

void motorMoveToPosition(int targetPos) {
    targetPos = targetPos % MICROSTEPS_PER_REV;
    if (targetPos < 0) targetPos += MICROSTEPS_PER_REV;

    int currentNorm = currentMotorPos % MICROSTEPS_PER_REV;
    if (currentNorm < 0) currentNorm += MICROSTEPS_PER_REV;

    int cwDist = (targetPos - currentNorm + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
    int ccwDist = (currentNorm - targetPos + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;

    bool clockwise;
    int stepsToMove;

    if (cwDist <= ccwDist) {
        clockwise = true;
        stepsToMove = cwDist;
    } else {
        clockwise = false;
        stepsToMove = ccwDist;
    }

    Serial.printf("[Motor] MoveToPos: cur=%d target=%d steps=%d dir=%s\n",
                  currentNorm, targetPos, stepsToMove, clockwise ? "CW" : "CCW");

    // Logged even for a zero-step move: the M records are what expose
    // direction reversals across a pour sequence, and a no-op move
    // still marks where in the sequence we are.
    telemetryLogMove(currentNorm, targetPos, clockwise, stepsToMove);

    if (stepsToMove == 0) return;

    moveSteps(stepsToMove, clockwise);
    currentMotorPos = targetPos;
}

// ============================================================
// Glass positioning (with pour offset)
// ============================================================
// Glass N occupies the angular position:
//   ((N-1) * 90° + POUR_OFFSET) mod 360°
// This aligns the glass with the pour spout, not the sensor.
// ============================================================

void motorMoveToGlass(int glass) {
    if (glass < 1) glass = 1;
    if (glass > NUM_GLASSES) glass = NUM_GLASSES;
    int targetPos = ((glass - 1) * MICROSTEPS_PER_GLASS + effectivePourOffset) % MICROSTEPS_PER_REV;
    Serial.printf("[Motor] MoveToGlass(%d): base=%d + offset=%d = target=%d\n",
                  glass, (glass - 1) * MICROSTEPS_PER_GLASS, effectivePourOffset, targetPos);
    motorMoveToPosition(targetPos);
}

void motorSpinToGlass(int glass, int extraRevolutions) {
    if (glass < 1) glass = 1;
    if (glass > NUM_GLASSES) glass = NUM_GLASSES;

    int targetPos = ((glass - 1) * MICROSTEPS_PER_GLASS + effectivePourOffset) % MICROSTEPS_PER_REV;

    int currentNorm = currentMotorPos % MICROSTEPS_PER_REV;
    if (currentNorm < 0) currentNorm += MICROSTEPS_PER_REV;

    // Clockwise distance to target
    int cwDist = (targetPos - currentNorm + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
    if (cwDist == 0) cwDist = MICROSTEPS_PER_REV;  // at least one full rev

    // Add extra full revolutions for theatrics / unpredictability
    int totalSteps = cwDist + extraRevolutions * MICROSTEPS_PER_REV;

    telemetryLogMove(currentNorm, targetPos, true, totalSteps);

    motorEnable();
    // Always clockwise, runtime speed. Crossing records are emitted
    // from inside, tagged with this glass number.
    moveStepsVerified(totalSteps, true, runtimeMaxSpeed, currentNorm, glass);
    currentMotorPos = targetPos;

    // Closed-loop drift correction: if the Hall sensor detected drift
    // during this spin, adjust our tracked position so the next move
    // starts from where the disc actually is, not where we assumed.
    // Positive lastDrift = disc is behind (lagging), so our tracked
    // position is ahead of reality — subtract to correct.
    if (lastDrift != 0) {
        int corrected = (currentMotorPos - lastDrift + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
        Serial.printf("[Motor] Drift correction: drift=%d, pos %d -> %d\n",
                      lastDrift, currentMotorPos, corrected);
        currentMotorPos = corrected;
    }
}

void motorSpinSteps(int steps) {
    if (steps <= 0) return;
    telemetryLogMove(currentMotorPos % MICROSTEPS_PER_REV,
                     (currentMotorPos + steps) % MICROSTEPS_PER_REV,
                     true, steps);
    motorEnable();
    moveSteps(steps, true, runtimeMaxSpeed);         // always clockwise, runtime speed
    currentMotorPos = (currentMotorPos + steps) % MICROSTEPS_PER_REV;
}

int motorGetPosition() {
    return currentMotorPos;
}

void motorEnable() {
    if (driverEnabled) return;          // already powered — no settle needed
    digitalWrite(PIN_MOTOR_EN, LOW);
    delay(5);                           // TMC2209 charge pump + current regulation
    driverEnabled = true;
}

void motorDisable() {
    digitalWrite(PIN_MOTOR_EN, HIGH);
    driverEnabled = false;
}

// ============================================================
// Session 17 additions
// ============================================================

void motorSetSpinSpeed(uint8_t level) {
    switch (level) {
        case 0:  // Fast
            runtimeMaxSpeed = 2400;
            runtimeExtraMin = 1;
            runtimeExtraMax = 1;
            break;
        case 2:  // Theatrical
            runtimeMaxSpeed = 800;
            runtimeExtraMin = 2;
            runtimeExtraMax = 4;
            break;
        default: // Normal (1)
            runtimeMaxSpeed = 1600;
            runtimeExtraMin = 1;
            runtimeExtraMax = 3;
            break;
    }
    Serial.printf("[Motor] Spin speed set: level=%d maxSpd=%d extraMin=%d extraMax=%d\n",
                  level, runtimeMaxSpeed, runtimeExtraMin, runtimeExtraMax);
}

static void recalcEffectiveOffset() {
    int raw = POUR_OFFSET + pourSideValue * MICROSTEPS_PER_GLASS + homeOffsetValue;
    effectivePourOffset = ((raw % MICROSTEPS_PER_REV) + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
}

void motorSetPourSide(uint8_t side) {
    if (side > 3) side = 3;
    pourSideValue = side;
    recalcEffectiveOffset();
    Serial.printf("[Motor] Pour side set: side=%d effectiveOffset=%d\n",
                  side, effectivePourOffset);
}

void motorSetHomeOffset(int offset) {
    homeOffsetValue = offset;
    recalcEffectiveOffset();
    Serial.printf("[Motor] Home offset set: offset=%d effectiveOffset=%d\n",
                  offset, effectivePourOffset);
}

int motorGetPourOffset() {
    return effectivePourOffset;
}

bool motorIsHomed() {
    return homed;
}

int motorGetExtraRevs() {
    if (runtimeExtraMax <= runtimeExtraMin) return runtimeExtraMin;
    return runtimeExtraMin + random(runtimeExtraMax - runtimeExtraMin + 1);
}

// ============================================================
// Diagnostics (Session 18)
// ============================================================

int motorGetLastDrift() {
    // Set by moveStepsVerified() during motorSpinToGlass (Session 19).
    // Session 5: reset to 0 at the start of every verified move, so a
    // spin that never crossed the magnet reports 0 rather than the
    // previous spin's number.
    return lastDrift;
}

bool motorGetLastDriftValid() {
    return lastDriftValid;
}

int motorGetLastMagnetWidth() {
    return lastMagnetWidth;
}

// ============================================================
// Characterisation traverse (Session 9)
// ============================================================
// See motor.h for the rationale. Structurally this is motorHome()
// Phases 1–3 with debounced edges and without the Phase 4 reversal,
// counting every step from the start so the centre crossing can be
// expressed as a distance rather than an absolute.
// ============================================================

bool motorMeasureHomeCW(int* stepsToCentre, int* magnetWidth) {
    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
    delayMicroseconds(10);

    const unsigned long stepDelay = 1000000UL / HOMING_SPEED;

    // Two revolutions. One is enough from any position off the magnet;
    // the second covers the case where the disc starts sitting on it and
    // the usable leading edge is a full turn away.
    const int budget = 2 * MICROSTEPS_PER_REV;

    // Starting on the magnet makes the edge in front of us a trailing
    // edge, which is not the one we can measure a width from. Walk off
    // it first — the steps still count, so the arithmetic is unaffected.
    bool waitingToLeave = (digitalRead(PIN_HALL) == LOW);

    int taken    = 0;
    int leading  = -1;
    int trailing = -1;
    int run      = 0;   // consecutive samples in the state we're waiting for

    while (taken < budget) {
        stepMotor();
        taken++;
        delayMicroseconds(stepDelay);
        if ((taken & 0xFF) == 0) yield();

        bool low = (digitalRead(PIN_HALL) == LOW);

        if (waitingToLeave) {
            run = low ? 0 : run + 1;
            if (run >= MEASURE_EDGE_DEBOUNCE) { waitingToLeave = false; run = 0; }
            continue;
        }

        if (leading < 0) {
            run = low ? run + 1 : 0;
            if (run >= MEASURE_EDGE_DEBOUNCE) {
                // The first LOW of the accepted run is the real edge.
                leading = taken - MEASURE_EDGE_DEBOUNCE + 1;
                run = 0;
            }
            continue;
        }

        run = low ? 0 : run + 1;
        if (run >= MEASURE_EDGE_DEBOUNCE) {
            trailing = taken - MEASURE_EDGE_DEBOUNCE + 1;
            break;
        }
    }

    if (leading < 0 || trailing < 0) {
        telemetryPrintf("# MEASURE fail: steps=%d leading=%d trailing=%d",
                        taken, leading, trailing);
        return false;
    }

    int width = trailing - leading;
    if (width < MEASURE_MIN_WIDTH) {
        telemetryPrintf("# MEASURE fail: width=%d below %d (noise edge?)",
                        width, MEASURE_MIN_WIDTH);
        return false;
    }

    // The disc was at position 0 after `centre` steps. It has since
    // travelled to `taken`, so it now sits (taken - centre) CW of home.
    int centre = leading + width / 2;
    currentMotorPos = ((taken - centre) % MICROSTEPS_PER_REV
                       + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
    homed = true;
    lastMagnetWidth = width;

    if (stepsToCentre) *stepsToCentre = centre;
    if (magnetWidth)   *magnetWidth   = width;
    return true;
}
