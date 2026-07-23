#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Hardware Diagnostics Screen
// ============================================================
// Low-level diagnostic for verifying motor driver, encoder-to-
// motor coupling, and Hall sensor. Four pages:
//
//   1. JOG   — encoder CW/CCW directly steps the motor
//   2. HALL  — live Hall sensor state readout
//   3. HOLD  — toggle motor driver enable (coil hold test)
//   4. STEP  — send precise step counts to verify accuracy
//
// Access: Settings → HW Diag
//
// Controls per page:
//   JOG:  Enc CW/CCW = jog motor, Click = cycle step size,
//         Left = BACK, Right = NEXT
//   HALL: auto-refreshes, Left = PREV, Right = NEXT
//   HOLD: Click = toggle enable, Left = PREV, Right = NEXT
//   STEP: Right = 1/4 rev CW (400 steps), Click = reset counter,
//         Left = PREV
// ============================================================

// --- Page enum ---
enum HwDiagPage {
    HW_JOG = 0,
    HW_HALL,
    HW_ENABLE,
    HW_STEPTEST,
    HW_PAGE_COUNT
};

// --- Jog step size options (microsteps per encoder click) ---
static const int JOG_SIZES[] = { 1, 10, 50, 200 };
static const int JOG_SIZE_COUNT = 4;
#define HW_DIAG_SPEED 800   // microsteps/sec for jog and step test

// --- State ---
static HwDiagPage hwPage      = HW_JOG;
static int   jogSizeIdx       = 2;       // default 50 microsteps
static long  jogTotal         = 0;       // net displacement (signed)
static bool  jogEnabled       = false;   // motor enabled for jog mode

static bool  enableState      = false;   // motor enable page toggle

static long  stepTestTotal    = 0;       // total steps sent
static bool  stepTestEnabled  = false;   // motor enabled for step test

// ============================================================
// Helpers
// ============================================================

static void ensureMotorOn(bool* flag) {
    if (!*flag) {
        motorEnable();
        delay(5);   // TMC2209 charge pump + current regulation settling
        *flag = true;
    }
}

static void rawStep(int steps, bool clockwise) {
    digitalWrite(PIN_MOTOR_DIR, clockwise ? MOTOR_CW_DIR : MOTOR_CCW_DIR);
    delayMicroseconds(50);

    unsigned long dly = 1000000UL / HW_DIAG_SPEED;
    for (int i = 0; i < steps; i++) {
        digitalWrite(PIN_MOTOR_STEP, HIGH);
        delayMicroseconds(5);
        digitalWrite(PIN_MOTOR_STEP, LOW);
        delayMicroseconds(dly);
        if ((i & 0xFF) == 0) yield();
    }
}

static void hwCleanup() {
    motorDisable();
    jogEnabled = false;
    enableState = false;
    stepTestEnabled = false;
}

// ============================================================
// Page 1: Jog Mode
// ============================================================

static void drawJog() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("JOG MODE", COL_ACCENT);

    int y = CONTENT_Y + 8;

    char buf[28];
    snprintf(buf, sizeof(buf), "Step: %d", JOG_SIZES[jogSizeIdx]);
    uiDrawCenteredText(buf, y, FONT_BODY, COL_TEXT);
    y += 24;

    float deg = JOG_SIZES[jogSizeIdx] * 360.0f / MICROSTEPS_PER_REV;
    snprintf(buf, sizeof(buf), "(%.1f deg/click)", deg);
    uiDrawCenteredText(buf, y, FONT_SMALL, COL_DIM);
    y += 30;

    // Total displacement — large
    snprintf(buf, sizeof(buf), "%+ld", jogTotal);
    uiDrawCenteredText(buf, y, FONT_LARGE, COL_ACCENT);
    y += 38;

    uiDrawCenteredText("steps", y, FONT_SMALL, COL_DIM);
    y += 22;

    // Hall state indicator (handy while jogging)
    bool hallActive = (digitalRead(PIN_HALL) == LOW);
    uint16_t hallCol = hallActive ? COL_SELECTED : COL_DIM;
    snprintf(buf, sizeof(buf), "Hall: %s", hallActive ? "ON" : "off");
    uiDrawCenteredText(buf, y, FONT_SMALL, hallCol);
    y += 20;

    uiDrawHint("Turn to jog, click: size", y);

    uiDrawCenteredText("1 / 4", CONTENT_Y + CONTENT_H - 20, FONT_SMALL, COL_DIM);
    uiDrawSoftButtons("BACK", "NEXT");
}

// ============================================================
// Page 2: Hall Sensor Live
// ============================================================

static bool lastHallState = false;

static void drawHall() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("HALL SENSOR", COL_HOME);

    bool active = (digitalRead(PIN_HALL) == LOW);
    lastHallState = active;

    int cx = SCREEN_W / 2;
    int cy = CONTENT_Y + 60;

    uint16_t col = active ? COL_SELECTED : COL_ERROR;
    tft->fillCircle(cx, cy, 30, col);

    const char* status = active ? "TRIGGERED" : "CLEAR";
    uiDrawCenteredText(status, cy + 50, FONT_BODY, col);

    char buf[24];
    snprintf(buf, sizeof(buf), "GPIO %d: %s", PIN_HALL, active ? "LOW" : "HIGH");
    uiDrawCenteredText(buf, cy + 78, FONT_SMALL, COL_DIM);

    uiDrawHint("Rotate disc by hand", cy + 106);

    uiDrawCenteredText("2 / 4", CONTENT_Y + CONTENT_H - 20, FONT_SMALL, COL_DIM);
    uiDrawSoftButtons("PREV", "NEXT");
}

// Partial update — only redraws the indicator when state changes
static void updateHallIndicator() {
    bool active = (digitalRead(PIN_HALL) == LOW);
    if (active == lastHallState) return;
    lastHallState = active;

    TFT_eSPI* tft = uiGetTFT();
    int cx = SCREEN_W / 2;
    int cy = CONTENT_Y + 60;

    uint16_t col = active ? COL_SELECTED : COL_ERROR;
    tft->fillCircle(cx, cy, 30, col);

    // Overwrite previous text with padded strings
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(col, COL_BG);
    tft->drawString(active ? "TRIGGERED" : "  CLEAR  ", cx, cy + 50);

    tft->setTextSize(FONT_SMALL);
    tft->setTextColor(COL_DIM, COL_BG);
    char buf[24];
    snprintf(buf, sizeof(buf), "GPIO %d: %s ", PIN_HALL, active ? "LOW " : "HIGH");
    tft->drawString(buf, cx, cy + 78);
}

// ============================================================
// Page 3: Motor Enable (Hold) Test
// ============================================================

static void drawEnable() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("MOTOR HOLD", COL_ACCENT);

    int y = CONTENT_Y + 30;

    if (enableState) {
        uiDrawCenteredText("LOCKED", y, FONT_LARGE, COL_SELECTED);
        y += 50;
        uiDrawHint("Coils energized", y);     y += 18;
        uiDrawHint("Disc should resist", y);  y += 18;
        uiDrawHint("manual rotation", y);
    } else {
        uiDrawCenteredText("FREE", y, FONT_LARGE, COL_DIM);
        y += 50;
        uiDrawHint("Driver disabled", y);      y += 18;
        uiDrawHint("Disc should spin", y);     y += 18;
        uiDrawHint("freely by hand", y);
    }

    uiDrawHint("Click to toggle", y + 30);

    uiDrawCenteredText("3 / 4", CONTENT_Y + CONTENT_H - 20, FONT_SMALL, COL_DIM);
    uiDrawSoftButtons("PREV", "NEXT");
}

// ============================================================
// Page 4: Step Test
// ============================================================

static void drawStepTest() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("STEP TEST", COL_HOME);

    int y = CONTENT_Y + 10;

    uiDrawCenteredText("Quarter Rev = 400", y, FONT_BODY, COL_TEXT);
    y += 22;
    uiDrawCenteredText("Full Rev = 1600", y, FONT_SMALL, COL_DIM);
    y += 32;

    char buf[32];
    snprintf(buf, sizeof(buf), "%ld", stepTestTotal);
    uiDrawCenteredText(buf, y, FONT_LARGE, COL_ACCENT);
    y += 38;

    uiDrawCenteredText("steps sent", y, FONT_SMALL, COL_DIM);
    y += 22;

    float revs = (float)stepTestTotal / MICROSTEPS_PER_REV;
    snprintf(buf, sizeof(buf), "= %.2f revs", revs);
    uiDrawCenteredText(buf, y, FONT_BODY, COL_TEXT);
    y += 28;

    uiDrawHint("Right: GO  Click: Reset", y);

    uiDrawCenteredText("4 / 4", CONTENT_Y + CONTENT_H - 20, FONT_SMALL, COL_DIM);
    uiDrawSoftButtons("PREV", "1/4 REV");
}

// ============================================================
// Screen callbacks
// ============================================================

static void hwDiagDraw(bool fullRedraw) {
    // Hall page polls for state changes on every draw pass
    if (hwPage == HW_HALL && !fullRedraw) {
        updateHallIndicator();
        return;
    }

    if (!fullRedraw) return;

    switch (hwPage) {
        case HW_JOG:      drawJog();      break;
        case HW_HALL:     drawHall();     break;
        case HW_ENABLE:   drawEnable();   break;
        case HW_STEPTEST: drawStepTest(); break;
        default: break;
    }
}

static void hwDiagInput(InputEvent evt) {
    switch (hwPage) {

    // ---- JOG ----
    case HW_JOG:
        if (evt == INPUT_ENC_CW) {
            ensureMotorOn(&jogEnabled);
            rawStep(JOG_SIZES[jogSizeIdx], true);
            jogTotal += JOG_SIZES[jogSizeIdx];
            audioPlayTone(TONE_CLICK);
            uiRequestRedraw();
        } else if (evt == INPUT_ENC_CCW) {
            ensureMotorOn(&jogEnabled);
            rawStep(JOG_SIZES[jogSizeIdx], false);
            jogTotal -= JOG_SIZES[jogSizeIdx];
            audioPlayTone(TONE_CLICK);
            uiRequestRedraw();
        } else if (evt == INPUT_ENC_CLICK) {
            jogSizeIdx = (jogSizeIdx + 1) % JOG_SIZE_COUNT;
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_RIGHT) {
            if (jogEnabled) { motorDisable(); jogEnabled = false; }
            audioPlayTone(TONE_SELECT);
            hwPage = HW_HALL;
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            hwCleanup();
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
        }
        break;

    // ---- HALL ----
    case HW_HALL:
        if (evt == INPUT_BTN_RIGHT) {
            audioPlayTone(TONE_SELECT);
            hwPage = HW_ENABLE;
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            hwPage = HW_JOG;
            uiRequestRedraw();
        }
        break;

    // ---- ENABLE ----
    case HW_ENABLE:
        if (evt == INPUT_ENC_CLICK) {
            enableState = !enableState;
            if (enableState) {
                motorEnable();
                delay(5);
            } else {
                motorDisable();
            }
            audioPlayTone(enableState ? TONE_CONFIRM : TONE_SELECT);
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_RIGHT) {
            audioPlayTone(TONE_SELECT);
            hwPage = HW_STEPTEST;
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            hwPage = HW_HALL;
            uiRequestRedraw();
        }
        break;

    // ---- STEP TEST ----
    case HW_STEPTEST:
        if (evt == INPUT_BTN_RIGHT) {
            ensureMotorOn(&stepTestEnabled);
            audioPlayTone(TONE_CONFIRM);
            rawStep(MICROSTEPS_PER_GLASS, true);   // 400 steps CW
            stepTestTotal += MICROSTEPS_PER_GLASS;
            audioPlayTone(TONE_ARRIVE);
            uiRequestRedraw();
        } else if (evt == INPUT_ENC_CLICK) {
            stepTestTotal = 0;
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            hwPage = HW_ENABLE;
            uiRequestRedraw();
        }
        break;

    default:
        break;
    }
}

static void hwDiagOnEnter() {
    hwPage = HW_JOG;
    jogSizeIdx = 2;         // default 50 microsteps
    jogTotal = 0;
    jogEnabled = false;
    enableState = false;
    stepTestTotal = 0;
    stepTestEnabled = false;
    lastHallState = !digitalRead(PIN_HALL);  // force first-draw update
}

// ============================================================
// Public screen instance
// ============================================================

const Screen screenHwDiag = {
    "HwDiag",
    hwDiagDraw,
    hwDiagInput,
    hwDiagOnEnter
};
