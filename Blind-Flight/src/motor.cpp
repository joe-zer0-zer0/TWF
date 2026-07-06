#include "motor.h"
#include "config.h"

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

// Pour side offset (Session 20)
// Combines POUR_OFFSET with user-selected side (0–3 × MICROSTEPS_PER_GLASS)
static int effectivePourOffset = POUR_OFFSET;

// Diagnostics
static int lastDrift       = 0;   // drift (microsteps) from last verified spin
static int lastMagnetWidth = 0;   // magnet width from last homing

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

    digitalWrite(PIN_MOTOR_EN, LOW);
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(10);

    int accelSteps = (int)((maxSpeed * maxSpeed -
                            MOTOR_MIN_SPEED * MOTOR_MIN_SPEED) /
                           (2.0f * MOTOR_ACCEL));
    int decelSteps = accelSteps;

    if (accelSteps + decelSteps > steps) {
        accelSteps = steps / 2;
        decelSteps = steps - accelSteps;
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

        // Yield to RTOS every 256 steps to prevent task watchdog
        // starvation on long moves (multi-revolution spins).
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
// first crossing. Otherwise this behaves exactly like moveSteps()
// and lastDrift is left unchanged from the previous verified spin.
// ============================================================
static int moveStepsVerified(int steps, bool clockwise, int maxSpeed, int startPosNorm) {
    if (steps <= 0) return 0;

    bool canVerify = homed && lastMagnetWidth > 0 && clockwise;
    int expectedStepsToTrigger = -1;
    if (canVerify) {
        int leadingEdgePos = (MICROSTEPS_PER_REV - lastMagnetWidth / 2) % MICROSTEPS_PER_REV;
        expectedStepsToTrigger = (leadingEdgePos - startPosNorm + MICROSTEPS_PER_REV) % MICROSTEPS_PER_REV;
        if (expectedStepsToTrigger >= steps) canVerify = false;  // won't reach a crossing this move
    }

    bool prevHallLow = (digitalRead(PIN_HALL) == LOW);
    bool triggered = false;

    digitalWrite(PIN_MOTOR_EN, LOW);
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(10);

    int accelSteps = (int)((maxSpeed * maxSpeed -
                            MOTOR_MIN_SPEED * MOTOR_MIN_SPEED) /
                           (2.0f * MOTOR_ACCEL));
    int decelSteps = accelSteps;

    if (accelSteps + decelSteps > steps) {
        accelSteps = steps / 2;
        decelSteps = steps - accelSteps;
    }

    int cruiseSteps = steps - accelSteps - decelSteps;
    float currentSpeed = MOTOR_MIN_SPEED;

    for (int i = 0; i < steps; i++) {
        stepMotor();

        if (canVerify && !triggered) {
            bool hallLow = (digitalRead(PIN_HALL) == LOW);
            if (hallLow && !prevHallLow) {
                int actualStepsToTrigger = i + 1;
                lastDrift = actualStepsToTrigger - expectedStepsToTrigger;
                triggered = true;
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

        // Yield to RTOS every 256 steps to prevent task watchdog
        // starvation on long moves (multi-revolution spins).
        if ((i & 0xFF) == 0) yield();
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

bool motorHome() {
    digitalWrite(PIN_MOTOR_EN, LOW);

    // --- Phase 1: If sitting on magnet, move off ---
    if (digitalRead(PIN_HALL) == LOW) {
        digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
        delayMicroseconds(10);
        for (int i = 0; i < MICROSTEPS_PER_GLASS; i++) {
            stepMotor();
            delayMicroseconds(1000000 / HOMING_SPEED);
            if ((i & 0xFF) == 0) yield();
        }
        // If still on magnet after a full glass worth of steps, bail
        if (digitalRead(PIN_HALL) == LOW) {
            Serial.println("[Motor] Homing FAIL: couldn't move off magnet");
            return false;
        }
    }

    // --- Phase 2: Scan CW for leading edge ---
    digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
    delayMicroseconds(10);

    int maxSteps = MICROSTEPS_PER_REV + MICROSTEPS_PER_REV / 2;
    bool found = false;
    for (int i = 0; i < maxSteps; i++) {
        if (digitalRead(PIN_HALL) == LOW) {
            found = true;
            break;
        }
        stepMotor();
        delayMicroseconds(1000000 / HOMING_SPEED);
        if ((i & 0xFF) == 0) yield();
    }

    if (!found) {
        Serial.println("[Motor] Homing FAIL: magnet not found in 1.5 revolutions");
        return false;
    }

    // --- Phase 3: Continue CW to trailing edge ---
    // Count how many steps the Hall stays active = magnet width.
    int magnetWidth = 0;
    bool exitedMagnet = false;
    for (int i = 0; i < MICROSTEPS_PER_GLASS; i++) {
        stepMotor();
        delayMicroseconds(1000000 / HOMING_SPEED);
        magnetWidth++;
        if (digitalRead(PIN_HALL) == HIGH) {
            exitedMagnet = true;
            break;
        }
        if ((i & 0xFF) == 0) yield();
    }

    if (!exitedMagnet) {
        // Magnet wider than 90° — something is wrong
        Serial.printf("[Motor] Homing FAIL: magnet wider than %d steps\n",
                      MICROSTEPS_PER_GLASS);
        return false;
    }

    // --- Phase 4: Reverse CCW to magnet center ---
    int centerSteps = magnetWidth / 2;
    digitalWrite(PIN_MOTOR_DIR, MOTOR_CCW_DIR);
    delayMicroseconds(10);
    for (int i = 0; i < centerSteps; i++) {
        stepMotor();
        delayMicroseconds(1000000 / HOMING_SPEED);
    }

    currentMotorPos = 0;
    homed = true;
    lastMagnetWidth = magnetWidth;

    Serial.printf("[Motor] Homed: magnet width=%d steps (%.1f°), centered (backed %d)\n",
                  magnetWidth, magnetWidth * 360.0f / MICROSTEPS_PER_REV, centerSteps);

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

    digitalWrite(PIN_MOTOR_EN, LOW);
    moveStepsVerified(totalSteps, true, runtimeMaxSpeed, currentNorm);  // always clockwise, runtime speed
    currentMotorPos = targetPos;
}

void motorSpinSteps(int steps) {
    if (steps <= 0) return;
    digitalWrite(PIN_MOTOR_EN, LOW);
    moveSteps(steps, true, runtimeMaxSpeed);         // always clockwise, runtime speed
    currentMotorPos = (currentMotorPos + steps) % MICROSTEPS_PER_REV;
}

int motorGetPosition() {
    return currentMotorPos;
}

void motorEnable() {
    digitalWrite(PIN_MOTOR_EN, LOW);
}

void motorDisable() {
    digitalWrite(PIN_MOTOR_EN, HIGH);
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

void motorSetPourSide(uint8_t side) {
    if (side > 3) side = 3;
    effectivePourOffset = (POUR_OFFSET + side * MICROSTEPS_PER_GLASS) % MICROSTEPS_PER_REV;
    Serial.printf("[Motor] Pour side set: side=%d effectiveOffset=%d\n",
                  side, effectivePourOffset);
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
    // Stays at its last value if the most recent spin didn't cross the
    // home magnet (e.g. not yet homed, or a very short move).
    return lastDrift;
}

int motorGetLastMagnetWidth() {
    return lastMagnetWidth;
}
