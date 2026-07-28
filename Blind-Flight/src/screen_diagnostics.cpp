#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "diagnostics.h"
#include "settings.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Diagnostics
// ============================================================
// Hidden screen reached by long-pressing the encoder while "About"
// is selected in Settings.
//
// Session 9 restructure. The settings menu had accumulated five
// overlapping engineering entries (Re-Home, Motor Test, Glass Diag,
// HW Diag, and this screen). They now all live here, behind the one
// gesture, and Settings is back to being settings.
//
// Re-Home in particular is an engineering tool, not an operator
// control: the disc positions are physically numbered and the tasting
// runs off the glasses themselves, so nothing after the final pour
// depends on the firmware knowing where the disc is. Losing verified
// position mid-flight is handled by re-homing on the next pour cycle
// (item 7k), not by a button.
//
// No new encoder long-press bindings. INPUT_ENC_LONG is already
// spoken for twice — Settings/About opens this screen, and the stats
// page uses it to clear flight data.
//
// Layout: a scrolling menu of tools, plus three info pages (usage
// stats, flight log, continuous spin) reached from it.
// ============================================================

// --- Menu items ---
enum DiagItem {
    DG_REHOME = 0,
    DG_AUTODIAG,
    DG_MOTORTEST,
    DG_GLASSDIAG,
    DG_HWDIAG,
    DG_STATS,
    DG_LOG,
    DG_SPIN,
    DG_ITEM_COUNT
};

static const char* diagItemLabels[DG_ITEM_COUNT] = {
    "Re-Home",
    "Auto Diag",
    "Motor Test",
    "Glass Diag",
    "HW Diag",
    "Usage Stats",
    "Flight Log",
    "Spin Test"
};

// --- View ---
enum DiagView {
    VIEW_MENU = 0,
    VIEW_STATS,
    VIEW_LOG,
    VIEW_SPIN,
    VIEW_CLEAR_CONFIRM
};

static DiagView currentView = VIEW_MENU;

static const int VISIBLE_ITEMS = CONTENT_H / MENU_ITEM_H;   // 4
static int menuSelected  = 0;
static int menuScrollOff = 0;

// Deferred push. Homing and screen pushes must not happen inside
// handleInput's caller chain while a transition is mid-flight, and
// homing blocks for seconds — so the input handler records what to do
// and the next draw pass does it. Same idiom as pendingBrowse /
// awaitingBrowseReturn elsewhere in the codebase.
static int pendingAction = -1;

// --- Flight log scroll state ---
static int logScrollPos = 0;
static const int LOG_VISIBLE = 5;

// --- Continuous spin test state ---
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
// View: Tool menu
// ============================================================

static void drawMenuList() {
    TFT_eSPI* tft = uiGetTFT();

    for (int i = 0; i < VISIBLE_ITEMS; i++) {
        int idx = menuScrollOff + i;
        int y = CONTENT_Y + i * MENU_ITEM_H;

        if (idx >= DG_ITEM_COUNT) {
            tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H, COL_BG);
            continue;
        }

        bool sel = (idx == menuSelected);
        uint16_t bg = sel ? COL_HIGHLIGHT : COL_BG;
        tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H, bg);

        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, bg);
        tft->drawString(diagItemLabels[idx], MENU_ITEM_X + 4,
                        y + MENU_ITEM_H / 2);

        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_DIM, bg);
        tft->drawString(">", MENU_ITEM_X + MENU_ITEM_W - 4,
                        y + MENU_ITEM_H / 2);
    }

    // Scroll indicator
    if (DG_ITEM_COUNT > VISIBLE_ITEMS) {
        int trackY = CONTENT_Y + SCROLL_BAR_PAD;
        int trackH = CONTENT_H - 2 * SCROLL_BAR_PAD;
        tft->fillRect(SCROLL_BAR_X, trackY, SCROLL_BAR_W, trackH, COL_SCROLL_BG);

        int thumbH = max(8, trackH * VISIBLE_ITEMS / DG_ITEM_COUNT);
        int thumbY = trackY + (trackH - thumbH) * menuScrollOff /
                     (DG_ITEM_COUNT - VISIBLE_ITEMS);
        tft->fillRect(SCROLL_BAR_X, thumbY, SCROLL_BAR_W, thumbH, COL_SCROLL_FG);
    }
}

static void drawMenu() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("DIAGNOSTICS", COL_HOME);
    drawMenuList();
    uiDrawSoftButtons("BACK", "SELECT");
}

static void ensureVisible() {
    if (menuSelected < menuScrollOff) {
        menuScrollOff = menuSelected;
    } else if (menuSelected >= menuScrollOff + VISIBLE_ITEMS) {
        menuScrollOff = menuSelected - VISIBLE_ITEMS + 1;
    }
}

// ============================================================
// View: Usage Stats
// ============================================================

static void drawStats() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("USAGE STATS", COL_HOME);

    int y = CONTENT_Y + 8;
    int labelX = 16;
    int valX = SCREEN_W - 16;

    tft->setTextSize(FONT_BODY);

    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString("Total Flights", labelX, y);
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString(String(diagGetTotalFlights()), valX, y);

    y += 32;

    for (int m = 0; m < 4; m++) {
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString(modeFullName(m), labelX + 8, y);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(String(diagGetModeCount(m)), valX, y);
        y += 26;
    }

    y += 6;
    tft->drawFastHLine(labelX, y, SCREEN_W - 2 * labelX, COL_DIM);
    y += 12;

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

    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString("Home Offset", labelX, y);
    tft->setTextDatum(MR_DATUM);
    tft->setTextColor(COL_ACCENT, COL_BG);
    char offBuf[12];
    snprintf(offBuf, sizeof(offBuf), "%+d", settingsGetHomeOffset());
    tft->drawString(offBuf, valX, y);

    uiDrawSoftButtons("BACK", "");
}

// ============================================================
// View: Flight Log
// ============================================================

static void drawLog() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("FLIGHT LOG", COL_HOME);

    int totalRecords = diagGetLogCount();

    if (totalRecords == 0) {
        uiDrawCenteredText("No flights", CONTENT_Y + 60, FONT_BODY, COL_DIM);
        uiDrawCenteredText("recorded", CONTENT_Y + 85, FONT_BODY, COL_DIM);
        uiDrawSoftButtons("BACK", "");
        return;
    }

    int y = CONTENT_Y + 4;
    int endIdx = logScrollPos + LOG_VISIBLE;
    if (endIdx > totalRecords) endIdx = totalRecords;

    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString("#", 10, y);
    tft->drawString("Mode", 36, y);
    tft->drawString("Order", 100, y);
    y += 16;

    tft->drawFastHLine(8, y, SCREEN_W - 16, COL_DIM);
    y += 4;

    for (int i = logScrollPos; i < endIdx; i++) {
        FlightRecord rec;
        if (!diagGetRecord(i, &rec)) continue;

        int flightNum = diagGetTotalFlights() - i;

        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);

        tft->setTextColor(COL_DIM, COL_BG);
        char numBuf[6];
        snprintf(numBuf, sizeof(numBuf), "%d", flightNum);
        tft->drawString(numBuf, 10, y + 10);

        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(modeShortName(rec.mode), 48, y + 10);

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

    if (totalRecords > LOG_VISIBLE) {
        char scrollBuf[16];
        snprintf(scrollBuf, sizeof(scrollBuf), "%d-%d of %d",
                 logScrollPos + 1, endIdx, totalRecords);
        uiDrawHint(scrollBuf, y + 4);
    }

    uiDrawSoftButtons("BACK", "");
}

// ============================================================
// View: Continuous Spin Test
// ============================================================

static void stopMotorTest() {
    if (motorRunning) {
        motorRunning = false;
        motorDisable();
    }
}

static void drawSpin(bool running) {
    uiGetTFT()->fillScreen(COL_BG);
    uiDrawTitleBar("SPIN TEST", COL_HOME);

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
        uiDrawSoftButtons("STOP", "");
    } else {
        uiDrawCenteredText("Continuous", CONTENT_Y + 50, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("Rotation", CONTENT_Y + 75, FONT_BODY, COL_TEXT);

        uiDrawHint("Runs motor at", CONTENT_Y + 115);
        uiDrawHint("homing speed", CONTENT_Y + 135);
        uiDrawSoftButtons("BACK", "START");
    }
}

// ============================================================
// View: Clear Confirmation
// ============================================================

static void drawClearConfirm() {
    uiGetTFT()->fillScreen(COL_BG);
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
// Deferred actions
// ============================================================
// Every tool here homes on entry. That is the bug hiding inside the
// consolidation: none of them used to, so each inherited whatever
// currentMotorPos claimed — and after the disc has been turned by
// hand, or after an idle timeout dropped the driver, that value is
// fiction and the tool silently measures nonsense.
//
// HW Diag is the deliberate exception. Its Hall page is the tool you
// reach for when the Hall sensor is the suspect, and on the screen
// build runHomingSequence() retries forever with no way out, so
// homing first would make a dead sensor unrecoverable from the UI.

static void runPendingAction() {
    int action = pendingAction;
    pendingAction = -1;

    switch (action) {
        case DG_REHOME:
            if (runHomingSequence()) audioPlayTone(TONE_HOME_FOUND);
            uiRequestRedraw();
            break;

        case DG_AUTODIAG:
            uiPushScreenT(&screenSelfTest, TRANS_WIPE_LEFT);
            break;

        case DG_MOTORTEST:
            // Homes itself on entry — see screen_motor_test.cpp.
            uiPushScreenT(&screenMotorTest, TRANS_WIPE_LEFT);
            break;

        case DG_GLASSDIAG:
            // Homes itself as the first step of every run.
            uiPushScreenT(&screenGlassDiag, TRANS_WIPE_LEFT);
            break;

        case DG_HWDIAG:
            uiPushScreenT(&screenHwDiag, TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

// ============================================================
// Screen callbacks
// ============================================================

static void diagDraw(bool fullRedraw) {
    if (pendingAction >= 0) {
        runPendingAction();
        return;
    }

    if (currentView == VIEW_SPIN && motorRunning) {
        motorTestUpdate();

        static unsigned long lastRefresh = 0;
        if (fullRedraw || millis() - lastRefresh >= 1000) {
            lastRefresh = millis();
            drawSpin(true);
        }
        return;
    }

    if (!fullRedraw) return;

    switch (currentView) {
        case VIEW_MENU:          drawMenu();          break;
        case VIEW_STATS:         drawStats();         break;
        case VIEW_LOG:           drawLog();           break;
        case VIEW_SPIN:          drawSpin(false);     break;
        case VIEW_CLEAR_CONFIRM: drawClearConfirm();  break;
    }
}

static void diagInput(InputEvent evt) {

    // --- Clear confirmation ---
    if (currentView == VIEW_CLEAR_CONFIRM) {
        if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
            diagClearAll();
            audioPlayTone(TONE_CONFIRM);
            currentView = VIEW_STATS;
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            currentView = VIEW_STATS;
            uiRequestRedraw();
        }
        return;
    }

    // --- Spin test running: any button stops ---
    if (currentView == VIEW_SPIN && motorRunning) {
        if (evt == INPUT_BTN_LEFT || evt == INPUT_BTN_RIGHT ||
            evt == INPUT_ENC_CLICK) {
            stopMotorTest();
            audioPlayTone(TONE_SELECT);
            uiRequestRedraw();
        }
        return;
    }

    switch (currentView) {

        case VIEW_MENU:
            if (evt == INPUT_ENC_CW) {
                if (menuSelected < DG_ITEM_COUNT - 1) {
                    menuSelected++;
                    ensureVisible();
                    audioPlayTone(TONE_CLICK);
                    drawMenuList();
                }
            } else if (evt == INPUT_ENC_CCW) {
                if (menuSelected > 0) {
                    menuSelected--;
                    ensureVisible();
                    audioPlayTone(TONE_CLICK);
                    drawMenuList();
                }
            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                switch (menuSelected) {
                    case DG_STATS:
                        currentView = VIEW_STATS;
                        uiRequestRedraw();
                        break;
                    case DG_LOG:
                        currentView = VIEW_LOG;
                        logScrollPos = 0;
                        uiRequestRedraw();
                        break;
                    case DG_SPIN:
                        currentView = VIEW_SPIN;
                        uiRequestRedraw();
                        break;
                    default:
                        // Tools home and/or push a screen — both are
                        // deferred out of the input handler.
                        pendingAction = menuSelected;
                        uiRequestRedraw();
                        break;
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case VIEW_STATS:
            if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                currentView = VIEW_MENU;
                uiRequestRedraw();
            } else if (evt == INPUT_ENC_LONG) {
                // Hidden: long-press on the stats page clears flight data
                audioPlayTone(TONE_SELECT);
                currentView = VIEW_CLEAR_CONFIRM;
                uiRequestRedraw();
            }
            break;

        case VIEW_LOG:
            if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                currentView = VIEW_MENU;
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
            }
            break;

        case VIEW_SPIN:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                motorEnable();
                motorRunning = true;
                motorTestSteps = 0;
                motorStartTime = millis();
                motorStepTimer = micros();
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                currentView = VIEW_MENU;
                uiRequestRedraw();
            }
            break;

        default:
            break;
    }
}

static void diagOnEnter() {
    // Returning here from a tool lands back on the menu with the same
    // item selected, which is what you want when running the same
    // measurement repeatedly.
    currentView = VIEW_MENU;
    logScrollPos = 0;
    pendingAction = -1;
    stopMotorTest();
}

const Screen screenDiagnostics = {
    "Diagnostics",
    diagDraw,
    diagInput,
    diagOnEnter
};
