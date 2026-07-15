#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "diagnostics.h"
#include "settings.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Diagnostics Screen
// ============================================================
// Hidden screen accessed via long-press on encoder while
// "About" is selected in Settings. Three pages:
//   0: Usage Stats (mode counters, total flights)
//   1: Flight Log (scrollable list of recent flights)
//   2: Continuous Motor Test (non-blocking spin loop)
//
// Navigation:
//   Encoder CW/CCW : scroll within page (flight log) or
//                     no-op (stats, motor test)
//   Right button   : next page / start-stop motor
//   Left button    : previous page / back to settings
//   Encoder click  : same as right button
// ============================================================

// --- Page enum ---
enum DiagPage {
    DIAG_STATS = 0,
    DIAG_LOG,
    DIAG_MOTOR,
    DIAG_CLEAR_CONFIRM,
    DIAG_PAGE_COUNT = 3   // clearConfirm is not a navigable page
};

static DiagPage currentPage = DIAG_STATS;

// --- Flight log scroll state ---
static int logScrollPos = 0;     // index of first visible record
static const int LOG_VISIBLE = 5; // records visible at once

// --- Continuous motor test state ---
static bool motorRunning = false;
static unsigned long motorStepTimer = 0;
static int motorTestSteps = 0;
static unsigned long motorStartTime = 0;

// --- Mode name lookup ---
static const char* modeShortName(uint8_t mode) {
    switch (mode) {
        case 0: return "BAS";
        case 1: return "NAM";
        case 2: return "GUS";
        case 3: return "RNK";
        default: return "???";
    }
}

static const char* modeFullName(uint8_t mode) {
    switch (mode) {
        case 0: return "Basic";
        case 1: return "Named";
        case 2: return "Best Guess";
        case 3: return "Ranked";
        default: return "Unknown";
    }
}

// ============================================================
// Page: Usage Stats
// ============================================================

static void drawStats() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("DIAGNOSTICS", COL_HOME);

    int y = CONTENT_Y + 8;
    int labelX = 16;
    int valX = SCREEN_W - 16;

    tft->setTextSize(FONT_BODY);

    // Total flights
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString("Total Flights", labelX, y);
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString(String(diagGetTotalFlights()), valX, y);

    y += 32;

    // Per-mode counts
    for (int m = 0; m < 4; m++) {
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString(modeFullName(m), labelX + 8, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(String(diagGetModeCount(m)), valX, y);
        y += 26;
    }

    // Motor diagnostics
    y += 6;
    tft->drawFastHLine(labelX, y, SCREEN_W - 2 * labelX, COL_DIM);
    y += 12;

    // Last drift
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString("Last Drift", labelX, y);
    tft->setTextDatum(MR_DATUM);
    int drift = motorGetLastDrift();
    uint16_t driftCol = (abs(drift) > 20) ? COL_ERROR : COL_ACCENT;
    tft->setTextColor(driftCol, COL_BG);
    char driftBuf[12];
    snprintf(driftBuf, sizeof(driftBuf), "%+d", drift);
    tft->drawString(driftBuf, valX, y);
    y += 26;

    // Home offset
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString("Home Offset", labelX, y);
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(COL_ACCENT, COL_BG);
    char offBuf[12];
    snprintf(offBuf, sizeof(offBuf), "%+d", settingsGetHomeOffset());
    tft->drawString(offBuf, valX, y);

    // Page indicator
    y = CONTENT_Y + CONTENT_H - 20;
    uiDrawCenteredText("1 / 3", y, FONT_SMALL, COL_DIM);

    uiDrawSoftButtons("BACK", "NEXT");
}

// ============================================================
// Page: Flight Log
// ============================================================

static void drawLog() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("FLIGHT LOG", COL_HOME);

    int totalRecords = diagGetLogCount();

    if (totalRecords == 0) {
        uiDrawCenteredText("No flights", CONTENT_Y + 60, FONT_BODY, COL_DIM);
        uiDrawCenteredText("recorded", CONTENT_Y + 85, FONT_BODY, COL_DIM);

        int y = CONTENT_Y + CONTENT_H - 20;
        uiDrawCenteredText("2 / 3", y, FONT_SMALL, COL_DIM);

        uiDrawSoftButtons("PREV", "NEXT");
        return;
    }

    int y = CONTENT_Y + 4;
    int endIdx = logScrollPos + LOG_VISIBLE;
    if (endIdx > totalRecords) endIdx = totalRecords;

    // Column headers
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString("#", 10, y);
    tft->drawString("Mode", 36, y);
    tft->drawString("Order", 100, y);
    y += 16;

    // Draw divider line
    tft->drawFastHLine(8, y, SCREEN_W - 16, COL_DIM);
    y += 4;

    for (int i = logScrollPos; i < endIdx; i++) {
        FlightRecord rec;
        if (!diagGetRecord(i, &rec)) continue;

        // Flight number (most recent = total, working backward)
        int flightNum = diagGetTotalFlights() - i;

        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);

        // Flight number
        tft->setTextColor(COL_DIM, COL_BG);
        char numBuf[6];
        snprintf(numBuf, sizeof(numBuf), "%d", flightNum);
        tft->drawString(numBuf, 10, y + 10);

        // Mode abbreviation
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(modeShortName(rec.mode), 48, y + 10);

        // Glass order
        char orderBuf[16];
        int pos = 0;
        for (int g = 0; g < 4; g++) {
            if (rec.glassOrder[g] == 0) break;
            if (g > 0) orderBuf[pos++] = ',';
            orderBuf[pos++] = '0' + rec.glassOrder[g];
        }
        orderBuf[pos] = '\0';
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString(orderBuf, 100, y + 10);

        y += 30;
    }

    // Scroll indicator
    if (totalRecords > LOG_VISIBLE) {
        char scrollBuf[16];
        snprintf(scrollBuf, sizeof(scrollBuf), "%d-%d of %d",
                 logScrollPos + 1, endIdx, totalRecords);
        uiDrawHint(scrollBuf, y + 4);
    }

    // Page indicator
    int pageY = CONTENT_Y + CONTENT_H - 20;
    uiDrawCenteredText("2 / 3", pageY, FONT_SMALL, COL_DIM);

    uiDrawSoftButtons("PREV", "NEXT");
}

// ============================================================
// Page: Continuous Motor Test
// ============================================================

static void stopMotorTest() {
    if (motorRunning) {
        motorRunning = false;
        motorDisable();
    }
}

static void drawMotorTest(bool running) {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("MOTOR TEST", COL_HOME);

    if (running) {
        uiDrawCenteredText("RUNNING", CONTENT_Y + 40, FONT_BODY, COL_SELECTED);

        unsigned long elapsed = (millis() - motorStartTime) / 1000;
        char timeBuf[16];
        snprintf(timeBuf, sizeof(timeBuf), "%lum %02lus", elapsed / 60, elapsed % 60);
        uiDrawCenteredText(timeBuf, CONTENT_Y + 70, FONT_BODY, COL_ACCENT);

        int revs = motorTestSteps / MICROSTEPS_PER_REV;
        char revBuf[16];
        snprintf(revBuf, sizeof(revBuf), "%d revs", revs);
        uiDrawCenteredText(revBuf, CONTENT_Y + 100, FONT_BODY, COL_TEXT);

        uiDrawHint("Press to stop", CONTENT_Y + 140);
    } else {
        uiDrawCenteredText("Continuous", CONTENT_Y + 50, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("Rotation", CONTENT_Y + 75, FONT_BODY, COL_TEXT);

        uiDrawHint("Runs motor at", CONTENT_Y + 115);
        uiDrawHint("homing speed", CONTENT_Y + 135);
    }

    // Page indicator
    int pageY = CONTENT_Y + CONTENT_H - 20;
    uiDrawCenteredText("3 / 3", pageY, FONT_SMALL, COL_DIM);

    if (running) {
        uiDrawSoftButtons("STOP", "");
    } else {
        uiDrawSoftButtons("PREV", "START");
    }
}

// ============================================================
// Page: Clear Confirmation
// ============================================================

static void drawClearConfirm() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("CLEAR DATA?", COL_ERROR);

    uiDrawCenteredText("Erase all", CONTENT_Y + 50, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("flight data?", CONTENT_Y + 75, FONT_BODY, COL_TEXT);

    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "%d flights", diagGetTotalFlights());
    uiDrawCenteredText(countBuf, CONTENT_Y + 110, FONT_BODY, COL_ERROR);

    uiDrawSoftButtons("CANCEL", "CLEAR");
}

// ============================================================
// Non-blocking motor step (called from draw loop)
// ============================================================

static void motorTestUpdate() {
    if (!motorRunning) return;

    unsigned long now = micros();
    // Step interval for HOMING_SPEED (400 steps/sec) = 2500 us
    unsigned long stepInterval = 1000000UL / HOMING_SPEED;

    if (now - motorStepTimer >= stepInterval) {
        motorStepTimer = now;

        digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
        digitalWrite(PIN_MOTOR_STEP, HIGH);
        delayMicroseconds(2);
        digitalWrite(PIN_MOTOR_STEP, LOW);
        motorTestSteps++;
    }
}

// ============================================================
// Screen callbacks
// ============================================================

static void diagDraw(bool fullRedraw) {
    // Run non-blocking motor step updates on every draw pass
    if (motorRunning) {
        motorTestUpdate();

        // Periodic display refresh (every second)
        static unsigned long lastRefresh = 0;
        if (fullRedraw || millis() - lastRefresh >= 1000) {
            lastRefresh = millis();
            drawMotorTest(true);
        }
        return;
    }

    if (!fullRedraw) return;

    switch (currentPage) {
        case DIAG_STATS:         drawStats();          break;
        case DIAG_LOG:           drawLog();             break;
        case DIAG_MOTOR:         drawMotorTest(false);  break;
        case DIAG_CLEAR_CONFIRM: drawClearConfirm();    break;
        default: break;
    }
}

static void diagInput(InputEvent evt) {

    // --- Clear confirmation ---
    if (currentPage == DIAG_CLEAR_CONFIRM) {
        if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
            diagClearAll();
            audioPlayTone(TONE_CONFIRM);
            currentPage = DIAG_STATS;
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            currentPage = DIAG_STATS;
            uiRequestRedraw();
        }
        return;
    }

    // --- Motor running: any button stops ---
    if (motorRunning) {
        if (evt == INPUT_BTN_LEFT || evt == INPUT_BTN_RIGHT ||
            evt == INPUT_ENC_CLICK) {
            stopMotorTest();
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
        }
        return;
    }

    switch (currentPage) {

        case DIAG_STATS:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_LOG;
                logScrollPos = 0;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            } else if (evt == INPUT_ENC_LONG) {
                // Hidden: long-press encoder on stats page to clear data
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_CLEAR_CONFIRM;
                uiRequestRedraw();
            }
            break;

        case DIAG_LOG:
            if (evt == INPUT_BTN_RIGHT) {
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_MOTOR;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_STATS;
                uiRequestRedraw();
            } else if (evt == INPUT_ENC_CW) {
                int total = diagGetLogCount();
                if (logScrollPos + LOG_VISIBLE < total) {
                    logScrollPos++;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CCW) {
                if (logScrollPos > 0) {
                    logScrollPos--;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_MOTOR;
                uiRequestRedraw();
            }
            break;

        case DIAG_MOTOR:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                // Start continuous motor test
                audioPlayTone(TONE_CONFIRM);
                motorEnable();
                motorRunning = true;
                motorTestSteps = 0;
                motorStartTime = millis();
                motorStepTimer = micros();
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                currentPage = DIAG_LOG;
                logScrollPos = 0;
                uiRequestRedraw();
            }
            break;

        default:
            break;
    }
}

static void diagOnEnter() {
    currentPage = DIAG_STATS;
    logScrollPos = 0;
    motorRunning = false;
}

static void diagOnExit() {
    stopMotorTest();
}

const Screen screenDiagnostics = {
    "Diagnostics",
    diagDraw,
    diagInput,
    diagOnEnter
};
