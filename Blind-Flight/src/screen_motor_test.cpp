#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "transitions.h"

// ============================================================
// Blind Flight — Motor Test Screen (Session 18 update)
// ============================================================
// Diagnostic screen for verifying glass positions and random
// selection. Accessed from Settings menu.
//
// Session 18: Removed local HOME_OFFSET — now uses centralized
// POUR_OFFSET via motorMoveToGlass(). Display shows pour offset,
// magnet width from last homing, and drift diagnostics.
//
// Controls:
//   Encoder CW/CCW : cycle through glasses 1–4
//   Right button   : GO — move motor to selected glass
//   Encoder click  : RANDOM — pick random unvisited glass
//   Left button    : BACK to settings
//
// Display:
//   - Large glass number (selected target)
//   - Four indicator boxes: green = visited, dim = available
//   - Status text showing last action result
//   - Motor position and pour offset (diagnostic)
//   - Magnet width from last homing
// ============================================================

// --- State ---
static int  testGlass    = 1;       // Currently selected glass (1–4)
static bool testVisited[NUM_GLASSES]; // Which glasses have been visited
static int  testVisitCount = 0;

// Status message
static char testStatus[40] = "Select or Random";

// ============================================================
// Helpers
// ============================================================

static void delayWithAudioTest(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        audioUpdate();
        delay(1);
    }
}

static void flushInputTest() {
    inputUpdate();
    while (inputGetEvent() != INPUT_NONE) {}
}

static void resetVisited() {
    for (int i = 0; i < NUM_GLASSES; i++) {
        testVisited[i] = false;
    }
    testVisitCount = 0;
}

static int selectRandomUnvisited() {
    int available[NUM_GLASSES];
    int count = 0;
    for (int i = 0; i < NUM_GLASSES; i++) {
        if (!testVisited[i]) {
            available[count++] = i + 1;
        }
    }
    if (count == 0) return 0;
    return available[random(count)];
}

// ============================================================
// Draw
// ============================================================

static void testDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("MOTOR TEST", COL_HOME);

    // --- Big glass number ---
    char numBuf[4];
    snprintf(numBuf, sizeof(numBuf), "%d", testGlass);
    uiDrawCenteredText(numBuf, CONTENT_Y + 30, FONT_XLARGE, COL_ACCENT);

    // Label
    uiDrawCenteredText("Glass", CONTENT_Y + 70, FONT_BODY, COL_TEXT);

    // --- Four indicator boxes ---
    int boxW = 44;
    int boxH = 34;
    int gap  = 10;
    int totalW = NUM_GLASSES * boxW + (NUM_GLASSES - 1) * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int boxY   = CONTENT_Y + 95;

    for (int i = 0; i < NUM_GLASSES; i++) {
        int x = startX + i * (boxW + gap);
        bool visited  = testVisited[i];
        bool selected = (i + 1 == testGlass);

        // Box color: green if visited, highlighted if selected, dim otherwise
        uint16_t bgCol;
        uint16_t txtCol;
        if (visited) {
            bgCol  = COL_SELECTED;  // green
            txtCol = COL_BG;
        } else if (selected) {
            bgCol  = COL_ACCENT;    // amber
            txtCol = COL_BG;
        } else {
            bgCol  = COL_DIM;
            txtCol = COL_TEXT;
        }

        tft->fillRoundRect(x, boxY, boxW, boxH, 4, bgCol);

        // Draw outline for selected glass
        if (selected && !visited) {
            tft->drawRoundRect(x, boxY, boxW, boxH, 4, COL_TEXT);
        }

        char label[4];
        snprintf(label, sizeof(label), "%d", i + 1);
        tft->setTextColor(txtCol, bgCol);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(label, x + boxW / 2, boxY + boxH / 2);
    }

    // --- Visit counter ---
    char countBuf[20];
    snprintf(countBuf, sizeof(countBuf), "%d of %d visited", testVisitCount, NUM_GLASSES);
    uiDrawHint(countBuf, boxY + boxH + 12);

    // --- Status message ---
    uiDrawCenteredText(testStatus, boxY + boxH + 38, FONT_SMALL, COL_DIM);

    // --- Motor diagnostics ---
    char posBuf[32];
    snprintf(posBuf, sizeof(posBuf), "Pos: %d  Pour: %d",
             motorGetPosition(), motorGetPourOffset());
    uiDrawHint(posBuf, boxY + boxH + 55);

    // Magnet width and drift
    int magW  = motorGetLastMagnetWidth();
    int drift = motorGetLastDrift();
    char diagBuf[40];
    if (magW > 0) {
        snprintf(diagBuf, sizeof(diagBuf), "Magnet: %d  Drift: %+d", magW, drift);
    } else {
        snprintf(diagBuf, sizeof(diagBuf), "Magnet: --  Drift: --");
    }
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(abs(drift) > 2 ? COL_ERROR : COL_DIM, COL_BG);
    tft->drawString(diagBuf, SCREEN_W / 2, boxY + boxH + 72);

    // --- Soft buttons ---
    if (testVisitCount >= NUM_GLASSES) {
        uiDrawSoftButtons("BACK", "RESET");
    } else {
        uiDrawSoftButtons("BACK", "GO");
    }
}

// ============================================================
// Input
// ============================================================

static void testInput(InputEvent evt) {
    switch (evt) {

        case INPUT_ENC_CW:
            testGlass++;
            if (testGlass > NUM_GLASSES) testGlass = 1;
            snprintf(testStatus, sizeof(testStatus), "Manual: Glass %d", testGlass);
            audioPlayTone(TONE_CLICK);
            uiRequestRedraw();
            break;

        case INPUT_ENC_CCW:
            testGlass--;
            if (testGlass < 1) testGlass = NUM_GLASSES;
            snprintf(testStatus, sizeof(testStatus), "Manual: Glass %d", testGlass);
            audioPlayTone(TONE_CLICK);
            uiRequestRedraw();
            break;

        case INPUT_ENC_CLICK: {
            // RANDOM — pick a random unvisited glass
            if (testVisitCount >= NUM_GLASSES) {
                // All visited — reset
                resetVisited();
                snprintf(testStatus, sizeof(testStatus), "Reset — ready");
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
                break;
            }

            int pick = selectRandomUnvisited();
            if (pick == 0) break;  // shouldn't happen

            testGlass = pick;
            testVisited[pick - 1] = true;
            testVisitCount++;
            snprintf(testStatus, sizeof(testStatus), "Random -> Glass %d", pick);

            audioPlayTone(TONE_CONFIRM);
            uiRequestRedraw();

            // Brief pause to show the selection on screen
            delayWithAudioTest(600);

            // Move motor — uses centralized POUR_OFFSET
            motorMoveToGlass(pick);
            audioPlayTone(TONE_ARRIVE);
            delayWithAudioTest(300);

            uiRequestRedraw();
            flushInputTest();
            break;
        }

        case INPUT_BTN_RIGHT:
            if (testVisitCount >= NUM_GLASSES) {
                // RESET
                resetVisited();
                testGlass = 1;
                snprintf(testStatus, sizeof(testStatus), "Reset — ready");
                audioPlayTone(TONE_SELECT);
                uiRequestRedraw();
            } else {
                // GO — move to manually selected glass
                testVisited[testGlass - 1] = true;
                testVisitCount++;
                snprintf(testStatus, sizeof(testStatus), "Go -> Glass %d", testGlass);

                audioPlayTone(TONE_CONFIRM);
                uiRequestRedraw();

                delayWithAudioTest(400);

                motorMoveToGlass(testGlass);
                audioPlayTone(TONE_ARRIVE);
                delayWithAudioTest(300);

                uiRequestRedraw();
                flushInputTest();
            }
            break;

        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

// ============================================================
// onEnter
// ============================================================

static void testOnEnter() {
    testGlass = 1;
    resetVisited();
    snprintf(testStatus, sizeof(testStatus), "Select or Random");
}

// ============================================================
// Public screen instance
// ============================================================

const Screen screenMotorTest = {
    "MotorTest",
    testDraw,
    testInput,
    testOnEnter
};
