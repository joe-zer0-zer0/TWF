#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "settings.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Per-Glass Alignment Diagnostic
// ============================================================
// Measures the positional error at each glass position to help
// diagnose cumulative step loss, mechanical misalignment, or
// friction-related drift.
//
// Flow:
//   1. Homes the motor, moves glass 1 to pour position.
//   2. User nudges encoder to align glass perfectly, confirms.
//   3. Disc is nudged back to undo the adjustment, then moves
//      to the next glass — repeat for all 4 glasses.
//   4. Summary screen shows per-glass offsets, inter-glass
//      deltas, and average drift. Data is also dumped to Serial.
//
// After confirming each glass, the nudge is undone so that
// motorMoveToGlass() starts from the correct tracked position.
// This means offsets reflect the SYSTEM's error at each glass,
// including any cumulative step loss from sequential moves.
//
// Run multiple times (CW empty, CW loaded, etc.) and compare
// the serial output to characterize the error pattern.
// ============================================================

#define DIAG_NUDGE_STEPS  10  // microsteps per encoder click (~2.25°), matches calibrate
#define NUDGE_KICKSTART    8  // extra fast steps to break static friction before measured steps
#define NUDGE_SPEED       600 // microsteps/sec for nudge moves (above loaded resonance zone)

enum DiagState {
    DIAG_HOMING,
    DIAG_ADJUSTING,
    DIAG_SUMMARY
};

static DiagState diagState   = DIAG_HOMING;
static int       diagGlass   = 1;
static int16_t   diagOffset[NUM_GLASSES];
static int16_t   diagNudge   = 0;      // accumulated nudge for current glass
static int       diagMagWidth = 0;
static int       diagRunNum  = 0;

// ============================================================
// Drawing helpers
// ============================================================

static void drawAdjusting() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("GLASS DIAG", COL_ACCENT);

    char buf[24];
    snprintf(buf, sizeof(buf), "Glass %d", diagGlass);
    uiDrawCenteredText(buf, CONTENT_Y + 8, FONT_BODY, COL_TEXT);

    snprintf(buf, sizeof(buf), "%+d", diagNudge);
    uiDrawCenteredText(buf, CONTENT_Y + 50, FONT_LARGE, COL_ACCENT);
    uiDrawCenteredText("steps", CONTENT_Y + 88, FONT_SMALL, COL_DIM);

    uiDrawHint("Nudge to align glass", CONTENT_Y + 115);
    uiDrawHint("under pour opening", CONTENT_Y + 135);

    // Progress dots
    int dotY = CONTENT_Y + 165;
    int dotGap = 24;
    int dotsW = (NUM_GLASSES - 1) * dotGap;
    int dotX0 = (SCREEN_W - dotsW) / 2;
    for (int i = 0; i < NUM_GLASSES; i++) {
        int cx = dotX0 + i * dotGap;
        uint16_t col;
        if (i < diagGlass - 1)     col = COL_SELECTED;
        else if (i == diagGlass - 1) col = COL_ACCENT;
        else                         col = COL_DIM;
        tft->fillCircle(cx, dotY, 5, col);
    }

    uiDrawSoftButtons("CANCEL", "NEXT");
}

static void drawSummary() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESULTS", COL_SELECTED);

    int labelX = 16;
    int valX   = 100;
    int deltaX = 180;
    int y      = CONTENT_Y + 4;
    int lineH  = 28;

    // Column headers
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString("Glass", labelX, y);
    tft->drawString("Offset", valX, y);
    tft->drawString("Delta", deltaX, y);
    y += 18;

    // Per-glass rows
    tft->setTextSize(FONT_BODY);
    char buf[16];
    for (int i = 0; i < NUM_GLASSES; i++) {
        // Label
        snprintf(buf, sizeof(buf), "G%d", i + 1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString(buf, labelX, y);

        // Offset — color by severity
        snprintf(buf, sizeof(buf), "%+d", diagOffset[i]);
        uint16_t valCol = (abs(diagOffset[i]) > 20) ? COL_ERROR :
                          (abs(diagOffset[i]) > 10) ? COL_MOVING : COL_SELECTED;
        tft->setTextColor(valCol, COL_BG);
        tft->drawString(buf, valX, y);

        // Delta from previous glass
        tft->setTextColor(COL_DIM, COL_BG);
        if (i > 0) {
            int delta = diagOffset[i] - diagOffset[i - 1];
            snprintf(buf, sizeof(buf), "%+d", delta);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        tft->drawString(buf, deltaX, y);

        y += lineH;
    }

    // Analysis: average inter-glass drift
    y += 4;
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);

    int spread = diagOffset[NUM_GLASSES - 1] - diagOffset[0];
    float avgDrift = (float)spread / (NUM_GLASSES - 1);
    char line[48];
    snprintf(line, sizeof(line), "G1 base: %+d  Avg drift: %+.1f/move",
             diagOffset[0], avgDrift);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString(line, labelX, y);

    y += 16;
    snprintf(line, sizeof(line), "Run #%d  Mag: %d  HomeOff: %+d",
             diagRunNum, diagMagWidth, settingsGetHomeOffset());
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString(line, labelX, y);

    uiDrawSoftButtons("BACK", "RE-RUN");
}

static void serialDumpResults() {
    Serial.println("=== GLASS DIAG RESULTS ===");
    Serial.printf("Run #%d  MagnetWidth=%d  HomeOffset=%+d  EffPourOffset=%d  PourSide=%d\n",
                  diagRunNum, diagMagWidth,
                  settingsGetHomeOffset(), motorGetPourOffset(),
                  settingsGetPourSide());
    for (int i = 0; i < NUM_GLASSES; i++) {
        if (i > 0) {
            int delta = diagOffset[i] - diagOffset[i - 1];
            Serial.printf("  Glass %d: offset=%+4d  delta=%+d\n",
                          i + 1, diagOffset[i], delta);
        } else {
            Serial.printf("  Glass %d: offset=%+4d\n", i + 1, diagOffset[i]);
        }
    }
    int spread = diagOffset[NUM_GLASSES - 1] - diagOffset[0];
    float avgDrift = (float)spread / (NUM_GLASSES - 1);
    Serial.printf("  Spread G1-G4: %+d  Avg drift/move: %+.1f\n", spread, avgDrift);
    Serial.println("==========================");
}

// ============================================================
// Motor nudge helpers (bypass position tracking, same as calibrate)
// ============================================================

static void nudgeDisc(bool clockwise, int steps) {
    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(50);

    int kickSteps = NUDGE_KICKSTART;
    unsigned long kickDelay = 1000000UL / (NUDGE_SPEED * 2);   // 2× nudge speed for kickstart
    unsigned long normalDelay = 1000000UL / NUDGE_SPEED;

    for (int i = 0; i < kickSteps + steps; i++) {
        digitalWrite(PIN_MOTOR_STEP, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_MOTOR_STEP, LOW);
        delayMicroseconds(i < kickSteps ? kickDelay : normalDelay);
    }
}

static void undoNudgeRaw() {
    if (diagNudge == 0) return;
    bool clockwise = (diagNudge < 0);
    int steps = abs(diagNudge);
    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(50);

    int kickSteps = NUDGE_KICKSTART;
    unsigned long kickDelay = 1000000UL / (NUDGE_SPEED * 2);
    unsigned long normalDelay = 1000000UL / NUDGE_SPEED;

    for (int i = 0; i < kickSteps + steps; i++) {
        digitalWrite(PIN_MOTOR_STEP, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_MOTOR_STEP, LOW);
        delayMicroseconds(i < kickSteps ? kickDelay : normalDelay);
    }
}

// ============================================================
// Start a run (home + move to glass 1)
// ============================================================

static bool startRun() {
    diagGlass = 1;
    diagNudge = 0;
    memset(diagOffset, 0, sizeof(diagOffset));
    diagState = DIAG_HOMING;

    bool ok = runHomingSequence();
    if (!ok) {
        audioPlayTone(TONE_ERROR);
        return false;
    }

    diagMagWidth = motorGetLastMagnetWidth();
    motorMoveToGlass(1);
    audioPlayTone(TONE_ARRIVE);
    diagState = DIAG_ADJUSTING;
    return true;
}

// ============================================================
// Screen callbacks
// ============================================================

static void diagDraw(bool fullRedraw) {
    if (!fullRedraw) return;
    switch (diagState) {
        case DIAG_HOMING:    break;
        case DIAG_ADJUSTING: drawAdjusting(); break;
        case DIAG_SUMMARY:   drawSummary();   break;
    }
}

static void diagInput(InputEvent evt) {
    switch (diagState) {

        case DIAG_ADJUSTING:
            if (evt == INPUT_ENC_CW) {
                int physicalSteps = NUDGE_KICKSTART + DIAG_NUDGE_STEPS;
                if (diagNudge + physicalSteps > 400) break;
                nudgeDisc(true, DIAG_NUDGE_STEPS);
                diagNudge += physicalSteps;
                audioPlayTone(TONE_CLICK);
                drawAdjusting();

            } else if (evt == INPUT_ENC_CCW) {
                int physicalSteps = NUDGE_KICKSTART + DIAG_NUDGE_STEPS;
                if (diagNudge - physicalSteps < -400) break;
                nudgeDisc(false, DIAG_NUDGE_STEPS);
                diagNudge -= physicalSteps;
                audioPlayTone(TONE_CLICK);
                drawAdjusting();

            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                diagOffset[diagGlass - 1] = diagNudge;

                Serial.printf("[GlassDiag] Glass %d: offset=%+d\n",
                              diagGlass, diagNudge);

                // Undo nudge so motor's tracked position stays correct
                undoNudgeRaw();

                if (diagGlass < NUM_GLASSES) {
                    diagGlass++;
                    diagNudge = 0;
                    motorMoveToGlass(diagGlass);
                    audioPlayTone(TONE_ARRIVE);
                    uiRequestRedraw();
                } else {
                    diagState = DIAG_SUMMARY;
                    motorDisable();
                    serialDumpResults();
                    uiRequestRedraw();
                }

            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                motorDisable();
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case DIAG_SUMMARY:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                // Re-run
                audioPlayTone(TONE_SELECT);
                diagRunNum++;
                if (!startRun()) {
                    uiPopScreenT(TRANS_WIPE_LEFT);
                    return;
                }
                uiRequestRedraw();

            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        default:
            break;
    }
}

static void diagOnEnter() {
    diagRunNum = 1;
    if (!startRun()) {
        uiPopScreenT(TRANS_WIPE_LEFT);
    }
}

const Screen screenGlassDiag = {
    "GlassDiag",
    diagDraw,
    diagInput,
    diagOnEnter
};
