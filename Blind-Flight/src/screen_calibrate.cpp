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
//   2. Encoder nudges the disc ±NUDGE_STEPS per click.
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

#define NUDGE_STEPS      10  // microsteps per encoder click (~2.25°)
#define CAL_NUDGE_SPEED 800  // microsteps/sec constant for all nudge steps

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

static void drawAdjusting() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("CALIBRATE", COL_ACCENT);

    uiDrawCenteredText("Glass 1 at Spout", CONTENT_Y + 8, FONT_BODY, COL_TEXT);

    // Show current offset value large
    char buf[16];
    snprintf(buf, sizeof(buf), "%+d", calOffset);
    uiDrawCenteredText(buf, CONTENT_Y + 55, FONT_LARGE, COL_ACCENT);

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

static void calibrateDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    switch (calState) {
        case CAL_HOMING:
            // Homing is done in onEnter; shouldn't linger here
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

static void calibrateInput(InputEvent evt) {
    switch (calState) {

        case CAL_ADJUSTING:
            if (evt == INPUT_ENC_CW) {
                calOffset += NUDGE_STEPS;
                if (calOffset > 200) calOffset = 200;
                motorEnable();
                digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
                delayMicroseconds(50);
                unsigned long stepDly = 1000000UL / CAL_NUDGE_SPEED;
                for (int i = 0; i < NUDGE_STEPS; i++) {
                    digitalWrite(PIN_MOTOR_STEP, HIGH);
                    delayMicroseconds(5);
                    digitalWrite(PIN_MOTOR_STEP, LOW);
                    delayMicroseconds(stepDly);
                }
                audioPlayTone(TONE_CLICK);
                drawAdjusting();
            } else if (evt == INPUT_ENC_CCW) {
                calOffset -= NUDGE_STEPS;
                if (calOffset < -200) calOffset = -200;
                motorEnable();
                digitalWrite(PIN_MOTOR_DIR, MOTOR_CCW_DIR);
                delayMicroseconds(50);
                unsigned long stepDly = 1000000UL / CAL_NUDGE_SPEED;
                for (int i = 0; i < NUDGE_STEPS; i++) {
                    digitalWrite(PIN_MOTOR_STEP, HIGH);
                    delayMicroseconds(5);
                    digitalWrite(PIN_MOTOR_STEP, LOW);
                    delayMicroseconds(stepDly);
                }
                audioPlayTone(TONE_CLICK);
                drawAdjusting();
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

    // Home the motor first
    bool ok = runHomingSequence();
    if (!ok) {
        audioPlayTone(TONE_ERROR);
        uiPopScreenT(TRANS_WIPE_LEFT);
        return;
    }

    // Move glass 1 to the pour position (using current offset)
    motorMoveToGlass(1);
    audioPlayTone(TONE_ARRIVE);

    calState = CAL_ADJUSTING;
}

const Screen screenCalibrate = {
    "Calibrate",
    calibrateDraw,
    calibrateInput,
    calibrateOnEnter
};
