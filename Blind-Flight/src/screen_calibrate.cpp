#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "settings.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Pour Offset Calibration Screen
// ============================================================
// Lets the user fine-tune glass alignment under the pour spout.
//
// Flow:
//   1. Homes the motor, moves glass 1 to the pour position.
//   2. Encoder nudges the disc ±CAL_NUDGE_STEPS per click.
//      The display shows the current offset and direction hints.
//   3. Right button (CONFIRM): saves the offset to NVS, then
//      visits all 4 glass positions sequentially so the user
//      can verify alignment at each stop.
//   4. Left button (CANCEL): discards changes and returns.
//
// The offset is stored as a signed int16_t in NVS ("homeOff"),
// clamped to ±200 microsteps (~±45°). It is added to the
// effective pour offset in the motor module.
// ============================================================

// Nudge step size and speed come from config.h (CAL_NUDGE_STEPS, NUDGE_SPEED)
// so this screen, glass diag, and the pour-time nudge stay in sync.

// Numeric offset field — kept as constants so the partial redraw and the
// full redraw always agree on where the value lives. FONT_LARGE is 8px tall
// per unit of text size, drawn with MC_DATUM (vertically centred on VAL_Y).
#define CAL_VAL_Y    (CONTENT_Y + 55)
#define CAL_VAL_H    (8 * FONT_LARGE + 4)

enum CalState {
    CAL_HOMING,       // running homing sequence
    CAL_ADJUSTING,    // user nudging with encoder
    CAL_VERIFYING,    // visiting each glass position
    CAL_DONE          // verification complete
};

static CalState calState = CAL_HOMING;
static int16_t  calOffset = 0;      // working offset value
static int      verifyGlass = 0;    // 1–4 during verification

// ============================================================
// Drawing helpers
// ============================================================

// Redraw only the numeric offset field. A full drawAdjusting() is a
// fillScreen plus every string — 50–100 ms of SPI, which made the encoder
// feel laggy on every detent. This touches one 224×36 rect instead.
static void drawOffsetValue() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillRect(8, CAL_VAL_Y - CAL_VAL_H / 2, SCREEN_W - 16, CAL_VAL_H, COL_BG);

    char buf[16];
    snprintf(buf, sizeof(buf), "%+d", calOffset);
    uiDrawCenteredText(buf, CAL_VAL_Y, FONT_LARGE, COL_ACCENT);
}

static void drawAdjusting() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("CALIBRATE", COL_ACCENT);

    uiDrawCenteredText("Glass 1 at Spout", CONTENT_Y + 8, FONT_BODY, COL_TEXT);

    // Show current offset value large
    drawOffsetValue();

    uiDrawCenteredText("steps", CONTENT_Y + 95, FONT_SMALL, COL_DIM);

    // Direction hints
    uiDrawHint("Turn encoder to nudge", CONTENT_Y + 125);
    uiDrawHint("disc left / right", CONTENT_Y + 145);

    uiDrawSoftButtons("CANCEL", "CONFIRM");
}

static void drawVerifying() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("VERIFYING", COL_SELECTED);

    char buf[24];
    snprintf(buf, sizeof(buf), "Glass %d", verifyGlass);
    uiDrawCenteredText(buf, CONTENT_Y + 30, FONT_LARGE, COL_ACCENT);

    uiDrawCenteredText("Check Alignment", CONTENT_Y + 85, FONT_BODY, COL_TEXT);

    // Progress dots
    int dotY = CONTENT_Y + 125;
    int dotR = 6;
    int dotGap = 24;
    int dotsW = (NUM_GLASSES - 1) * dotGap;
    int dotX0 = (SCREEN_W - dotsW) / 2;
    for (int i = 0; i < NUM_GLASSES; i++) {
        int cx = dotX0 + i * dotGap;
        uint16_t col = (i < verifyGlass) ? COL_SELECTED : COL_DIM;
        tft->fillCircle(cx, dotY, dotR, col);
    }

    if (verifyGlass >= NUM_GLASSES) {
        uiDrawSoftButtons("", "DONE");
    } else {
        uiDrawSoftButtons("", "NEXT");
    }
}

static void drawDone() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("CALIBRATE", COL_SELECTED);

    uiDrawCenteredText("Saved!", CONTENT_Y + 40, FONT_LARGE, COL_SELECTED);

    char buf[24];
    snprintf(buf, sizeof(buf), "Offset: %+d", calOffset);
    uiDrawCenteredText(buf, CONTENT_Y + 90, FONT_BODY, COL_ACCENT);

    uiDrawSoftButtons("", "OK");
}

// ============================================================
// Screen callbacks
// ============================================================

// Homing blocks for seconds and draws its own full-screen UI, and on
// failure the screen has to leave. Neither belongs in onEnter(), which
// runs mid-transition — a uiPopScreenT() nested inside a push corrupts
// the screen stack (see CLAUDE.md). Deferred to the first draw pass,
// matching the pattern in screen_motor_test.cpp.
static bool pendingStart = false;
static bool pendingExit  = false;

static void startCalibration() {
    if (!runHomingSequence()) {
        audioPlayTone(TONE_ERROR);
        pendingExit = true;
        return;
    }

    // Move glass 1 to the pour position (using current offset)
    motorMoveToGlass(1);
    audioPlayTone(TONE_ARRIVE);
    calState = CAL_ADJUSTING;
}

static void calibrateDraw(bool fullRedraw) {
    if (pendingStart) {
        pendingStart = false;
        startCalibration();
        fullRedraw = true;
    }

    if (pendingExit) {
        pendingExit = false;
        uiPopScreenT(TRANS_WIPE_LEFT);
        return;
    }

    if (!fullRedraw) return;

    switch (calState) {
        case CAL_HOMING:
            // Transient: the first draw pass runs the homing and moves on.
            break;
        case CAL_ADJUSTING:
            drawAdjusting();
            break;
        case CAL_VERIFYING:
            drawVerifying();
            break;
        case CAL_DONE:
            drawDone();
            break;
    }
}

// Raw nudge — bypasses the motor module's position tracking on purpose:
// the whole point of this screen is to move the disc relative to where the
// firmware thinks it is. motorEnable() guarantees the driver has settled
// before the first STEP pulse.
static void nudgeDisc(bool clockwise) {
    motorEnable();
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(50);

    unsigned long stepDly = 1000000UL / NUDGE_SPEED;
    for (int i = 0; i < CAL_NUDGE_STEPS; i++) {
        digitalWrite(PIN_MOTOR_STEP, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_MOTOR_STEP, LOW);
        delayMicroseconds(stepDly);
    }
}

static void calibrateInput(InputEvent evt) {
    switch (calState) {

        case CAL_ADJUSTING:
            if (evt == INPUT_ENC_CW) {
                if (calOffset + CAL_NUDGE_STEPS > 200) break;
                nudgeDisc(true);
                calOffset += CAL_NUDGE_STEPS;
                audioPlayTone(TONE_CLICK);
                drawOffsetValue();
            } else if (evt == INPUT_ENC_CCW) {
                if (calOffset - CAL_NUDGE_STEPS < -200) break;
                nudgeDisc(false);
                calOffset -= CAL_NUDGE_STEPS;
                audioPlayTone(TONE_CLICK);
                drawOffsetValue();
            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                // Confirm: save and start verification
                audioPlayTone(TONE_CONFIRM);
                settingsSetHomeOffset(calOffset);
                motorSetHomeOffset(calOffset);
                settingsSave();

                Serial.printf("[Cal] Saved offset=%d, effectivePour=%d\n",
                              calOffset, motorGetPourOffset());

                // Re-home to pick up the new offset cleanly
                runHomingSequence();

                Serial.printf("[Cal] Post-home pos=%d, starting verification\n",
                              motorGetPosition());

                // Start verification: visit each glass
                calState = CAL_VERIFYING;
                verifyGlass = 1;
                Serial.printf("[Cal] Verify glass 1\n");
                motorMoveToGlass(1);
                audioPlayTone(TONE_ARRIVE);
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                // Cancel: discard and return
                audioPlayTone(TONE_SELECT);
                motorDisable();
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case CAL_VERIFYING:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                if (verifyGlass < NUM_GLASSES) {
                    verifyGlass++;
                    Serial.printf("[Cal] Verify glass %d (pos before=%d)\n",
                                  verifyGlass, motorGetPosition());
                    motorMoveToGlass(verifyGlass);
                    audioPlayTone(TONE_ARRIVE);
                    uiRequestRedraw();
                } else {
                    // All glasses verified
                    audioPlayTone(TONE_CONFIRM);
                    calState = CAL_DONE;
                    motorDisable();
                    uiRequestRedraw();
                }
            }
            break;

        case CAL_DONE:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK ||
                evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        default:
            break;
    }
}

static void calibrateOnEnter() {
    // Load current saved offset as starting point
    calOffset = settingsGetHomeOffset();
    calState = CAL_HOMING;
    pendingStart = true;
    pendingExit  = false;
}

const Screen screenCalibrate = {
    "Calibrate",
    calibrateDraw,
    calibrateInput,
    calibrateOnEnter
};
