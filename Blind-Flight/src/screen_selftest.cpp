#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "selftest.h"
#include "transitions.h"
#include "wifi_portal.h"

// ============================================================
// Blind Flight — Alignment Self-Test Screen (Session 9)
// ============================================================
// Thin front-end over selftest.cpp, which holds the measurement
// itself and is compiled into both build environments. Everything
// this screen displays is also written to telemetry and readable at
// GET /log — nothing that matters should exist only on a 240x280
// panel.
//
// Flow: CONFIRM (glasses must be off the disc) -> RUNNING -> RESULTS.
//
// The run is blocking and lives in loop(), not here: the input
// handler only asks for it. While it runs, selftest.cpp calls the
// progress hook registered below between positions, which is what
// keeps this screen updating.
// ============================================================

enum StState { ST_CONFIRM, ST_RUNNING, ST_RESULTS };

static StState stState = ST_CONFIRM;

// ============================================================
// Pages
// ============================================================

static void drawConfirm() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("AUTO DIAG", COL_ACCENT);

    uiDrawCenteredText("Remove all", CONTENT_Y + 20, FONT_BODY, COL_ERROR);
    uiDrawCenteredText("glasses first", CONTENT_Y + 45, FONT_BODY, COL_ERROR);

    uiDrawHint("Measures alignment at", CONTENT_Y + 82);
    uiDrawHint("each position against", CONTENT_Y + 100);
    uiDrawHint("the home magnet.", CONTENT_Y + 118);

    uiDrawHint("Many revolutions,", CONTENT_Y + 146);
    uiDrawHint("about 2 minutes.", CONTENT_Y + 164);

    uiDrawSoftButtons("CANCEL", "START");
}

static void drawRunning() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RUNNING", COL_MOVING);

    char buf[32];
    snprintf(buf, sizeof(buf), "Pass %d / %d",
             selfTestCurrentPass(), SELFTEST_TOTAL_PASSES);
    uiDrawCenteredText(buf, CONTENT_Y + 30, FONT_BODY, COL_TEXT);

    snprintf(buf, sizeof(buf), "%d", selfTestCurrentGlass());
    uiDrawCenteredText(buf, CONTENT_Y + 80, FONT_LARGE, COL_ACCENT);
    uiDrawCenteredText("position", CONTENT_Y + 112, FONT_SMALL, COL_DIM);

    const char* kind;
    switch (selfTestCurrentOrder()) {
        case SELFTEST_ORDER_RND:   kind = "randomised order"; break;
        case SELFTEST_ORDER_CHAIN: kind = "chain (no reads)"; break;
        default:                   kind = "sequential order"; break;
    }
    uiDrawHint(kind, CONTENT_Y + 140);

    uiDrawHint("Keep hands clear", CONTENT_Y + 168);
    uiDrawSoftButtons("STOP", "");
}

static void drawResults() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    const SelfTestSummary* s = selfTestGetSummary();

    uiDrawTitleBar(s->aborted ? "STOPPED" : "RESULTS",
                   s->aborted ? COL_ERROR : COL_SELECTED);

    if (!s->valid) {
        uiDrawCenteredText("No data", CONTENT_Y + 60, FONT_BODY, COL_ERROR);
        uiDrawHint("Homing failed", CONTENT_Y + 95);
        uiDrawSoftButtons("BACK", "RETRY");
        return;
    }

    int labelX = 10;
    int meanX  = 74;
    int sprX   = 140;
    int chainX = 205;
    int y      = CONTENT_Y + 2;

    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString("Pos", labelX, y);
    tft->drawString("Mean", meanX, y);
    tft->drawString("Scat", sprX, y);
    tft->drawString("Chain", chainX - 30, y);
    y += 16;
    tft->drawFastHLine(8, y, SCREEN_W - 16, COL_DIM);
    y += 6;

    char buf[16];
    for (int p = 0; p < NUM_GLASSES; p++) {
        const SelfTestPosStats& st = s->pos[p];

        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, COL_BG);
        snprintf(buf, sizeof(buf), "G%d", p + 1);
        tft->drawString(buf, labelX, y);

        // Mean is per-position bias. Calibration removes it, so it is
        // shown neutrally — only the spread between positions matters.
        tft->setTextColor(COL_ACCENT, COL_BG);
        if (st.samples) snprintf(buf, sizeof(buf), "%+d", st.mean);
        else            snprintf(buf, sizeof(buf), "--");
        tft->drawString(buf, meanX, y);

        // Scatter is the number nothing can remove. Colour it by the
        // roadmap's acceptance thresholds: 8 goal, 12 acceptable.
        uint16_t col = (st.spread > 12) ? COL_ERROR
                     : (st.spread > 8)  ? COL_MOVING : COL_SELECTED;
        tft->setTextColor(col, COL_BG);
        if (st.samples) snprintf(buf, sizeof(buf), "%d", st.spread);
        else            snprintf(buf, sizeof(buf), "--");
        tft->drawString(buf, sprX, y);

        tft->setTextColor(COL_DIM, COL_BG);
        if (s->chainValid[p]) snprintf(buf, sizeof(buf), "%+d", s->chainErr[p]);
        else                  snprintf(buf, sizeof(buf), "--");
        tft->drawString(buf, chainX - 30, y);

        y += 26;
    }

    y += 4;
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);

    char line[48];
    // The primary acceptance number: goal 8 microsteps, acceptable 15.
    uint16_t sCol = (s->interGlassSpread > 15) ? COL_ERROR
                  : (s->interGlassSpread > 8)  ? COL_MOVING : COL_SELECTED;
    snprintf(line, sizeof(line), "Inter-glass spread: %d (%.1f mm)",
             s->interGlassSpread, s->interGlassSpread * 0.275f);
    tft->setTextColor(sCol, COL_BG);
    tft->drawString(line, labelX, y);
    y += 14;

    snprintf(line, sizeof(line), "Worst scatter: %d   Accum: %d",
             s->worstScatter, s->accumMax);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString(line, labelX, y);
    y += 14;

    snprintf(line, sizeof(line), "Reads %d/%d fail  Mag %d-%d",
             s->reads, s->failedReads,
             s->magnetWidthMin, s->magnetWidthMax);
    tft->drawString(line, labelX, y);
    y += 14;

    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString("Full data: /log", labelX, y);

    uiDrawSoftButtons("BACK", "RE-RUN");
}

// ============================================================
// Progress hook — called by selftest.cpp between positions
// ============================================================

static void stProgress() {
    // The hook stays registered after this screen is popped, and the
    // phone can start a run from anywhere, so check before painting —
    // otherwise a phone-triggered run draws over whatever is on screen.
    if (uiActiveScreen() != &screenSelfTest) return;

    if (selfTestIsRunning()) {
        drawRunning();
    } else if (stState == ST_RUNNING) {
        // The final call, made by selftest.cpp once the run has ended.
        // This is where the results page goes up: the run blocks loop()
        // from start to finish, so an ordinary draw pass never once sees
        // the test running, and cannot detect the edge itself.
        stState = ST_RESULTS;
        drawResults();
    }
}

// ============================================================
// Screen callbacks
// ============================================================

static void stDraw(bool fullRedraw) {
    // Backstop. stProgress() normally puts the results up as the run
    // ends; this catches any path where it did not fire, so the screen
    // can never be left stranded on the progress page with no working
    // buttons. Testing busy rather than running matters: a request is
    // queued in one loop() pass and executed in the next, and in that
    // gap "not running" does not yet mean "finished".
    if (stState == ST_RUNNING && !selfTestIsBusy()) {
        stState = ST_RESULTS;
        fullRedraw = true;
    }

    if (!fullRedraw) return;

    switch (stState) {
        case ST_CONFIRM: drawConfirm(); break;
        case ST_RUNNING: drawRunning(); break;
        case ST_RESULTS: drawResults(); break;
    }
}

static void stInput(InputEvent evt) {
    switch (stState) {

        case ST_CONFIRM:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                if (selfTestRequest()) {
                    audioPlayTone(TONE_CONFIRM);
                    stState = ST_RUNNING;
                    uiRequestRedraw();
                } else {
                    audioPlayTone(TONE_ERROR);
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;

        case ST_RUNNING:
            // Unreachable in practice — the run blocks loop(), and
            // selftest.cpp drains the input queue itself, treating the
            // left button as the abort. Kept so a stray event during
            // the request/start gap cannot wedge the screen.
            if (evt == INPUT_BTN_LEFT) selfTestAbort();
            break;

        case ST_RESULTS:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                stState = ST_CONFIRM;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                uiPopScreenT(TRANS_WIPE_LEFT);
            }
            break;
    }
}

static void stOnEnter() {
    stState = ST_CONFIRM;
    selfTestSetProgressHook(stProgress);
}

const Screen screenSelfTest = {
    "SelfTest",
    stDraw,
    stInput,
    stOnEnter
};
