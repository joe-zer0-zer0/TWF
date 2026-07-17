#include "game.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "transitions.h"
#include "browse.h"
#include "wifi_portal.h"
#include "screens.h"
#include "diagnostics.h"
#include "settings.h"
#include "battery.h"
#include "persist.h"

// ============================================================
// Blind Flight — Game Module (Session 10, updated Session 16)
// ============================================================
// Session 12 revisions:
//   - Reveal and results show glasses in POSITION order (1–4),
//     not pour order. Glass number at top, pour # below name.
//   - Reveal uses FONT_BODY for names (fits ~20 chars).
//   - Results are left-aligned with glass number prefix.
//   - Manual entry integrated via browse screen.
//
// Session 13 revisions:
//   - Added GAME_MODE_GUESS (Best Guess mode).
//   - Guessing round: user matches whiskey names to glasses
//     before seeing the reveal. Pool management removes locked
//     names and restores them on back-tracking.
//   - Score screen shows X of Y correct.
//   - Detail screen shows checkmark/X per glass.
//   - Tasting screen text changes for guess mode.
//   - modeUsesNames() helper replaces GAME_MODE_NAMED checks.
//
// Session 16 revisions:
//   - Added state getter functions for Wi-Fi phone interface.
//   - Added deferred phone action handlers (name submit, guess
//     select, game start) processed at top of gameDraw().
//   - Phone and physical controls work simultaneously.
// ============================================================

// --- Module state ---
static GameSession session;
static GameState   state;
static GameMode    currentMode = GAME_MODE_BASIC;

// True while runPourCycle()/runFinalSpin() is blocked inside a
// motor spin (bug 14). Set immediately before the blocking call
// and cleared immediately after, with an explicit
// wifiPortalBroadcastNow() bracketing each so phones see a
// "spinning" snapshot instead of going unserviced mid-move.
static bool spinningNow = false;

// --- Lifecycle flags ---
static bool gameActive     = false;
static bool pendingBrowse  = false;
static bool awaitingBrowseReturn = false;  // true when browse was pushed; gate for return handler

// --- Reveal mapping (built once when entering REVEAL state) ---
// Poured glasses sorted by glass number (position order).
// revealMap[i] = { glassNum (1-based), pourIndex (0-based) }
struct RevealEntry {
    int glass;      // glass position (1–4)
    int pourIdx;    // index into pourOrder/glassName (0-based)
};
static RevealEntry revealMap[NUM_GLASSES];
static int revealMapCount;

// --- Guess+Ranked: per-glass correctness (computed before revealMap rebuild) ---
static bool guessCorrectForGlass[NUM_GLASSES]; // indexed by glass number - 1

// --- Guessing round pool (Session 13) ---
static const char* guessPoolLabels[NUM_GLASSES];
static int guessPoolPourIdx[NUM_GLASSES];
static int guessPoolCount;
static ScrollList guessList;

// --- Ranking round pool (Session 20) ---
static const char* rankPoolLabels[NUM_GLASSES];
static int rankPoolGlass[NUM_GLASSES];
static int rankPoolCount;
static ScrollList rankList;

// --- Phone deferred actions (Session 16) ---
// phoneNameReady: name is already in session.glassName and browse
//   has been cleaned up — gameDraw just needs to run the pour cycle.
// phonePendingGuess: pool index for a guess selection.
// phonePendingStart: set by gameStartFromPhone; gameOnEnter defers
//   to let gameDraw handle custom phone initialization.
static bool phoneNameReady    = false;
static int  phonePendingGuess = -1;
static bool phonePendingStart = false;
static GameMode phoneStartMode = GAME_MODE_BASIC;
static bool pendingResumePour = false;
static bool homedThisFlight  = false;

// ============================================================
// Helpers
// ============================================================

static void delayWithAudio(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        audioUpdate();
        delay(1);
    }
}

static void flushInput() {
    inputUpdate();
    while (inputGetEvent() != INPUT_NONE) {}
}

// Returns true for modes that use the browse library for names (per-glass)
static bool modeUsesNames() {
    return currentMode == GAME_MODE_NAMED || currentMode == GAME_MODE_GUESS
        || currentMode == GAME_MODE_RANK  || currentMode == GAME_MODE_GUESS_RANK;
}

// Returns true for challenge modes (Duplicate/Decoy)
static bool modeIsChallenge() {
    return currentMode == GAME_MODE_DUPLICATE || currentMode == GAME_MODE_DECOY;
}

// Title bar text for the current mode
static const char* getModeTitleText() {
    switch (currentMode) {
        case GAME_MODE_NAMED:     return "NAMED FLIGHT";
        case GAME_MODE_GUESS:     return "BEST GUESS";
        case GAME_MODE_RANK:      return "RANKED FLIGHT";
        case GAME_MODE_GUESS_RANK: return "GUESS + RANKED";
        case GAME_MODE_DUPLICATE: return "TWIN POUR";
        case GAME_MODE_DECOY:     return "FIND THE RINGER";
        default:                  return "QUICK FLIGHT";
    }
}

static void resetSession() {
    session.pourCount    = 0;
    session.currentGlass = 0;
    session.revealIndex  = 0;
    session.guessIndex   = 0;
    session.glassCount   = settingsGetGlassCount();
    revealMapCount       = 0;
    session.rankIndex    = 0;
    session.ratingIndex  = 0;
    homedThisFlight      = false;
    for (int i = 0; i < NUM_GLASSES; i++) {
        session.pourOrder[i] = 0;
        session.glassUsed[i] = false;
        session.glassName[i][0] = '\0';
        session.guessForGlass[i] = -1;
        session.rankOrder[i] = 0;
        session.glassEntry[i] = nullptr;
        session.glassRating[i] = 0;
        session.bottleForGlass[i] = -1;
        session.challengePourOrder[i] = -1;
    }
    session.bottleCount = 0;
    session.duplicateIdx = -1;
    session.challengeGuess[0] = 0;
    session.challengeGuess[1] = 0;
    session.challengeGuessCount = 0;
    session.challengeCorrect = false;
    for (int i = 0; i < MAX_BOTTLES; i++) {
        session.bottleName[i][0] = '\0';
    }
}

static int selectRandomGlass() {
    int available[NUM_GLASSES];
    int count = 0;
    for (int i = 0; i < session.glassCount; i++) {
        if (!session.glassUsed[i]) {
            available[count++] = i + 1;
        }
    }
    if (count == 0) return 0;
    return available[random(count)];
}

// Build the reveal map: poured glasses in glass-number order.
// Must be called before entering GAME_REVEAL state.
static void buildRevealMap() {
    revealMapCount = 0;
    for (int g = 1; g <= session.glassCount; g++) {
        if (session.glassUsed[g - 1]) {
            // Find which pour used this glass
            for (int p = 0; p < session.pourCount; p++) {
                if (session.pourOrder[p] == g) {
                    revealMap[revealMapCount].glass   = g;
                    revealMap[revealMapCount].pourIdx = p;
                    revealMapCount++;
                    break;
                }
            }
        }
    }
}

// ============================================================
// Guessing pool management (Session 13)
// ============================================================

// Build the pool of available whiskey names for the current
// guess position. Names locked in at positions 0..guessIndex-1
// are excluded. If the current position has a previous guess,
// pre-highlight it in the list.
static void buildGuessPool() {
    guessPoolCount = 0;
    for (int p = 0; p < session.pourCount; p++) {
        // Check if this pour index is locked by an earlier position
        bool taken = false;
        for (int j = 0; j < session.guessIndex; j++) {
            if (session.guessForGlass[j] == p) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            guessPoolPourIdx[guessPoolCount] = p;
            guessPoolLabels[guessPoolCount] = session.glassName[p];
            guessPoolCount++;
        }
    }

    // Pre-select previous guess if it's still in the pool
    int preSelect = 0;
    int prevGuess = session.guessForGlass[session.guessIndex];
    if (prevGuess >= 0) {
        for (int i = 0; i < guessPoolCount; i++) {
            if (guessPoolPourIdx[i] == prevGuess) {
                preSelect = i;
                break;
            }
        }
    }

    uiScrollListInit(&guessList, guessPoolLabels, guessPoolCount);
    guessList.selected = preSelect;
}

// Count how many guesses match the actual glass contents
static int countCorrectGuesses() {
    int correct = 0;
    for (int i = 0; i < revealMapCount; i++) {
        if (session.guessForGlass[i] == revealMap[i].pourIdx) {
            correct++;
        }
    }
    return correct;
}

// Enter the guessing round. If preserveGuesses is true,
// previous guesses are kept for pre-highlighting (used when
// returning from the score/detail screen to re-guess).
static void initGuessing(bool preserveGuesses) {
    buildRevealMap();
    session.guessIndex = 0;
    if (!preserveGuesses) {
        for (int i = 0; i < NUM_GLASSES; i++) {
            session.guessForGlass[i] = -1;
        }
    }
    buildGuessPool();
    state = GAME_GUESSING;
    flushInput();
    uiRequestRedraw();
}

// ============================================================
// Ranking pool management (Session 20)
// ============================================================

static const char* ordinalStr(int rank) {
    switch (rank) {
        case 0: return "1ST PLACE?";
        case 1: return "2ND PLACE?";
        case 2: return "3RD PLACE?";
        default: return "4TH PLACE?";
    }
}

static const char* ordinalRevealStr(int rank) {
    switch (rank) {
        case 0: return "1st Place";
        case 1: return "2nd Place";
        case 2: return "3rd Place";
        default: return "4th Place";
    }
}

static char rankGlassLabels[NUM_GLASSES][10];

static void buildRankPool() {
    rankPoolCount = 0;
    for (int g = 1; g <= session.glassCount; g++) {
        if (!session.glassUsed[g - 1]) continue;
        bool taken = false;
        for (int j = 0; j < session.rankIndex; j++) {
            if (session.rankOrder[j] == g) {
                taken = true;
                break;
            }
        }
        if (!taken) {
            rankPoolGlass[rankPoolCount] = g;
            snprintf(rankGlassLabels[rankPoolCount], sizeof(rankGlassLabels[0]),
                     "Glass %d", g);
            rankPoolLabels[rankPoolCount] = rankGlassLabels[rankPoolCount];
            rankPoolCount++;
        }
    }

    int preSelect = 0;
    int prevPick = session.rankOrder[session.rankIndex];
    if (prevPick > 0) {
        for (int i = 0; i < rankPoolCount; i++) {
            if (rankPoolGlass[i] == prevPick) {
                preSelect = i;
                break;
            }
        }
    }

    uiScrollListInit(&rankList, rankPoolLabels, rankPoolCount);
    rankList.selected = preSelect;
}

static void buildRevealMapFromRank() {
    revealMapCount = 0;
    // Reverse order: last place revealed first, building to 1st place
    for (int r = session.pourCount - 1; r >= 0; r--) {
        int g = session.rankOrder[r];
        if (g <= 0) continue;
        for (int p = 0; p < session.pourCount; p++) {
            if (session.pourOrder[p] == g) {
                revealMap[revealMapCount].glass   = g;
                revealMap[revealMapCount].pourIdx = p;
                revealMapCount++;
                break;
            }
        }
    }
}

static void initRanking(bool preserveRanks) {
    buildRevealMap();
    session.rankIndex = 0;
    if (!preserveRanks) {
        for (int i = 0; i < NUM_GLASSES; i++) {
            session.rankOrder[i] = 0;
        }
    }
    buildRankPool();
    state = GAME_RANKING;
    flushInput();
    uiRequestRedraw();
}

// ============================================================
// Duplicate/Decoy challenge logic (Session 22)
// ============================================================

// How many bottles does this mode need?
static int challengeBottleTarget() {
    return (currentMode == GAME_MODE_DUPLICATE) ? 3 : 2;
}

// Build randomized pour order for challenge modes.
// Duplicate: 3 bottles, one gets 2 glasses, the other two get 1 each.
// Decoy: 2 bottles, main gets 3 glasses, ringer gets 1.
// Fills challengePourOrder[0..3] with bottle indices and assigns
// random glass positions via glassUsed/pourOrder.
static void buildChallengePourOrder() {
    session.glassCount = NUM_GLASSES;

    if (currentMode == GAME_MODE_DUPLICATE) {
        // Pick which bottle to duplicate (0, 1, or 2)
        session.duplicateIdx = random(3);

        // Build a 4-element array of bottle indices
        int pours[4];
        int idx = 0;
        for (int b = 0; b < 3; b++) {
            pours[idx++] = b;
            if (b == session.duplicateIdx) {
                pours[idx++] = b;  // duplicated
            }
        }

        // Fisher-Yates shuffle
        for (int i = 3; i > 0; i--) {
            int j = random(i + 1);
            int tmp = pours[i]; pours[i] = pours[j]; pours[j] = tmp;
        }
        for (int i = 0; i < 4; i++) {
            session.challengePourOrder[i] = pours[i];
        }

    } else {
        // Decoy: bottle 0 = main (3 glasses), bottle 1 = ringer (1 glass)
        int pours[4] = {0, 0, 0, 1};

        for (int i = 3; i > 0; i--) {
            int j = random(i + 1);
            int tmp = pours[i]; pours[i] = pours[j]; pours[j] = tmp;
        }
        for (int i = 0; i < 4; i++) {
            session.challengePourOrder[i] = pours[i];
        }
    }

    // Assign random glass positions for each pour
    int glassPool[4] = {1, 2, 3, 4};
    for (int i = 3; i > 0; i--) {
        int j = random(i + 1);
        int tmp = glassPool[i]; glassPool[i] = glassPool[j]; glassPool[j] = tmp;
    }

    for (int i = 0; i < 4; i++) {
        session.pourOrder[i] = glassPool[i];
        session.bottleForGlass[glassPool[i] - 1] = session.challengePourOrder[i];
    }
}

// Check if the user's challenge guess is correct
static bool checkChallengeGuess() {
    if (currentMode == GAME_MODE_DUPLICATE) {
        // User selected two glasses — check if both contain the duplicated bottle
        int g1 = session.challengeGuess[0] - 1;
        int g2 = session.challengeGuess[1] - 1;
        return (session.bottleForGlass[g1] == session.duplicateIdx &&
                session.bottleForGlass[g2] == session.duplicateIdx);
    } else {
        // Decoy: user selected one glass — check if it's the ringer (bottle 1)
        int g = session.challengeGuess[0] - 1;
        return (session.bottleForGlass[g] == 1);
    }
}

// ============================================================
// Challenge mode draw functions (Session 22)
// ============================================================

static void drawBottleSelect(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar(getModeTitleText(), COL_ACCENT);

    int target = challengeBottleTarget();
    char progBuf[24];
    snprintf(progBuf, sizeof(progBuf), "Select Bottle %d of %d",
             session.bottleCount + 1, target);
    uiDrawCenteredText(progBuf, CONTENT_Y + 20, FONT_BODY, COL_TEXT);

    if (currentMode == GAME_MODE_DECOY && session.bottleCount == 0) {
        uiDrawHint("This is the MAIN bottle", CONTENT_Y + 50);
        uiDrawHint("(poured into 3 glasses)", CONTENT_Y + 68);
    } else if (currentMode == GAME_MODE_DECOY && session.bottleCount == 1) {
        uiDrawHint("This is the RINGER", CONTENT_Y + 50);
        uiDrawHint("(poured into 1 glass)", CONTENT_Y + 68);
    }

    // Show already-selected bottles
    int y = CONTENT_Y + 95;
    for (int i = 0; i < session.bottleCount; i++) {
        char label[30];
        snprintf(label, sizeof(label), "%d: %s", i + 1, session.bottleName[i]);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(label, 16, y);
        y += 16;
    }

    uiDrawHint("Press to browse library", CONTENT_Y + 150);
    uiDrawSoftButtons("BACK", "BROWSE");
}

static void drawChallengePouring(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("POUR", COL_SELECTED);

    // Show which bottle to pour (by name), not which glass
    int bottleIdx = session.challengePourOrder[session.pourCount];
    const char* bottleName = session.bottleName[bottleIdx];

    char pourCounter[16];
    snprintf(pourCounter, sizeof(pourCounter), "Pour %d of 4", session.pourCount + 1);
    uiDrawCenteredText(pourCounter, CONTENT_Y + 15, FONT_BODY, COL_DIM);

    uiDrawCenteredTextWrap(bottleName, CONTENT_Y + 55, FONT_BODY, COL_ACCENT);

    uiDrawCenteredText("Ready To Pour", CONTENT_Y + 100, FONT_BODY, COL_TEXT);

    uiDrawHint("DONE When Poured", CONTENT_Y + 135);
    uiDrawHint("SKIP To End Flight", CONTENT_Y + 155);

    uiDrawSoftButtons("SKIP", "DONE");
}

static void drawChallengeGuess(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    if (currentMode == GAME_MODE_DUPLICATE) {
        uiDrawTitleBar("FIND THE PAIR", COL_ACCENT);
        uiDrawCenteredText("Which two glasses", CONTENT_Y + 8, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("are the same?", CONTENT_Y + 28, FONT_BODY, COL_TEXT);
    } else {
        uiDrawTitleBar("FIND THE RINGER", COL_ACCENT);
        uiDrawCenteredText("Which glass is", CONTENT_Y + 8, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("the odd one out?", CONTENT_Y + 28, FONT_BODY, COL_TEXT);
    }

    // Draw 4 glass boxes — highlight selected ones
    const int boxW = 44, boxH = 44, gap = 10;
    int totalW = 4 * boxW + 3 * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int boxY = CONTENT_Y + 60;

    for (int i = 0; i < 4; i++) {
        int x = startX + i * (boxW + gap);
        int glassNum = i + 1;

        bool selected = false;
        for (int s = 0; s < session.challengeGuessCount; s++) {
            if (session.challengeGuess[s] == glassNum) selected = true;
        }

        bool isCursor = (glassNum == session.currentGlass);

        uint16_t boxCol = COL_DIM;
        uint16_t txtCol = COL_TEXT;
        if (selected) {
            boxCol = COL_ACCENT;
            txtCol = COL_BG;
        } else if (isCursor) {
            boxCol = COL_HIGHLIGHT;
            txtCol = COL_TEXT;
        }

        tft->fillRoundRect(x, boxY, boxW, boxH, 6, boxCol);
        tft->setTextColor(txtCol, boxCol);
        tft->setTextSize(3);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(String(glassNum), x + boxW / 2, boxY + boxH / 2);
    }

    // Hint text
    if (currentMode == GAME_MODE_DUPLICATE) {
        char hintBuf[24];
        snprintf(hintBuf, sizeof(hintBuf), "Selected: %d of 2", session.challengeGuessCount);
        uiDrawCenteredText(hintBuf, CONTENT_Y + 120, FONT_SMALL, COL_DIM);
    }

    uiDrawHint("Turn to move, click to select", CONTENT_Y + 145);
    uiDrawSoftButtons("BACK", "CONFIRM");
}

static void drawChallengeResult(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    if (session.challengeCorrect) {
        uiDrawTitleBar("CORRECT!", COL_SELECTED);
        uiDrawCenteredText("Nice", CONTENT_Y + 40, FONT_XLARGE, COL_SELECTED);
        uiDrawCenteredText("palate!", CONTENT_Y + 90, FONT_BODY, COL_TEXT);
    } else {
        uiDrawTitleBar("NOT QUITE", COL_ERROR);
        uiDrawCenteredText("Oops", CONTENT_Y + 40, FONT_XLARGE, COL_ERROR);
        uiDrawCenteredText("Better luck next time", CONTENT_Y + 90, FONT_BODY, COL_TEXT);
    }

    uiDrawHint("Press to see the reveal", CONTENT_Y + 140);
    uiDrawSoftButtons("", "REVEAL");
}

// ============================================================
// Flight completion logging
// ============================================================

static void enterGameDone() {
    if (session.pourCount > 0) {
        diagLogFlight((uint8_t)currentMode, session.pourCount, session.pourOrder);
    }
    persistClearSession();
    state = GAME_DONE;
    flushInput();
    uiRequestRedraw();
}

// ============================================================
// Blocking pour cycle
// ============================================================

static void showLockoutScreen() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("LOW BATTERY", COL_ERROR);
    uiDrawCenteredText("Battery Too", CONTENT_Y + 40, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("Low", CONTENT_Y + 65, FONT_BODY, COL_TEXT);
    uiDrawHint("Charge before pouring", CONTENT_Y + 110);
    uiDrawSoftButtons("BACK", "");

    flushInput();
    while (true) {
        audioUpdate();
        inputUpdate();
        InputEvent evt = inputGetEvent();
        if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK || evt == INPUT_BTN_RIGHT) {
            audioPlayTone(TONE_SELECT);
            break;
        }
        delay(10);
    }
}

static void runPourCycle() {
    if (batteryIsLockout()) {
        showLockoutScreen();
        if (session.pourCount > 0) {
            motorDisable();
            flushInput();
            state = GAME_TASTING;
            persistSaveGame(currentMode, state, session);
            uiRequestRedraw();
        } else {
            gameActive = false;
            uiPopScreenT(TRANS_FADE);
        }
        return;
    }

    uiResetIdleTimer();

    if (!homedThisFlight) {
        runHomingSequence();
        homedThisFlight = true;
    }

    TFT_eSPI* tft = uiGetTFT();

    if (modeIsChallenge()) {
        // Challenge modes: glass assignment is pre-computed
        session.currentGlass = session.pourOrder[session.pourCount];
    } else {
        session.currentGlass = selectRandomGlass();
    }

    Serial.printf("[Game] Pour %d/%d — glass %d",
                  session.pourCount + 1, session.glassCount, session.currentGlass);
    if (modeUsesNames() && session.glassName[session.pourCount][0]) {
        Serial.printf(" (%s)", session.glassName[session.pourCount]);
    } else if (modeIsChallenge()) {
        int bi = session.challengePourOrder[session.pourCount];
        Serial.printf(" (bottle: %s)", session.bottleName[bi]);
    }
    Serial.println();

    // --- 1. Selection animation ---
    tft->fillScreen(COL_BG);
    uiDrawTitleBar(getModeTitleText(), COL_ACCENT);

    char pourNum[4];
    snprintf(pourNum, sizeof(pourNum), "%d", session.pourCount + 1);
    uiDrawCenteredText(pourNum, CONTENT_Y + 60, FONT_XLARGE, COL_ACCENT);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 120, FONT_BODY, COL_TEXT);
    uiDrawSoftButtons("", "");

    audioPlayTone(TONE_SELECT);
    delayWithAudio(GAME_SELECT_PAUSE_MS);

    audioPlayTone(TONE_CONFIRM);
    delayWithAudio(GAME_SELECT_REVEAL_MS);

    // --- 2. Spin the motor ---
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("SPINNING", COL_MOVING);

    uiDrawCenteredText(pourNum, CONTENT_Y + 45, FONT_XLARGE, COL_MOVING);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 105, FONT_BODY, COL_TEXT);
    uiDrawHint("Please wait...", CONTENT_Y + 135);
    uiDrawSoftButtons("", "");

    int extraRevs = motorGetExtraRevs();

    // Bug 14: motorSpinToGlass() blocks the main loop for the
    // duration of the spin (worst case ~8-10s on Theatrical), so
    // the Wi-Fi portal can't service phones until it returns. Push
    // a "spinning" snapshot right before the blocking call so
    // connected phones show a spinner instead of "Reconnecting...".
    spinningNow = true;
    wifiPortalBroadcastNow();
    motorSpinToGlass(session.currentGlass, extraRevs);
    spinningNow = false;
    wifiPortalBroadcastNow();

    audioPlayTone(TONE_ARRIVE);
    delayWithAudio(GAME_SPIN_SETTLE_MS);

    // --- 3. Transition to interactive pour screen ---
    flushInput();
    state = GAME_POURING;
    uiRequestRedraw();
}

static void runFinalSpin() {
    uiResetIdleTimer();
    TFT_eSPI* tft = uiGetTFT();

    tft->fillScreen(COL_BG);
    uiDrawTitleBar("SPINNING", COL_MOVING);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 70, FONT_BODY, COL_TEXT);
    uiDrawHint("Please wait...", CONTENT_Y + 100);
    uiDrawSoftButtons("", "");

    delayWithAudio(400);

    int steps = 3 * MICROSTEPS_PER_REV + random(MICROSTEPS_PER_REV);

    spinningNow = true;
    wifiPortalBroadcastNow();
    motorSpinSteps(steps);
    spinningNow = false;
    wifiPortalBroadcastNow();

    audioPlayTone(TONE_ARRIVE);
    delayWithAudio(GAME_SPIN_SETTLE_MS);
    motorDisable();  // release holding torque — carousel doesn't need it while tasting

    flushInput();
    state = GAME_TASTING;
    persistSaveGame(currentMode, state, session);
    uiRequestRedraw();
}

// ============================================================
// Browse request (deferred or immediate)
// ============================================================

static void requestBrowse(bool deferred) {
    state = GAME_NAMING;
    awaitingBrowseReturn = true;
    if (deferred) {
        pendingBrowse = true;
    } else {
        browseReset();
        uiPushScreen(&screenBrowse);
    }
}

// ============================================================
// Draw functions for interactive states
// ============================================================

static void drawPouring(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("POUR", COL_SELECTED);

    char pourNum[4];
    snprintf(pourNum, sizeof(pourNum), "%d", session.pourCount + 1);
    uiDrawCenteredText(pourNum, CONTENT_Y + 20, FONT_XLARGE, COL_SELECTED);

    if (modeUsesNames() && session.glassName[session.pourCount][0]) {
        int nextY = uiDrawCenteredTextWrap(session.glassName[session.pourCount],
                                           CONTENT_Y + 80, FONT_BODY, COL_ACCENT);
        uiDrawCenteredText("Ready To Pour", nextY + 10, FONT_BODY, COL_TEXT);
    } else {
        uiDrawCenteredText("Ready To Pour", CONTENT_Y + 105, FONT_BODY, COL_TEXT);
    }

    uiDrawHint("DONE When Poured", CONTENT_Y + 140);
    uiDrawHint("SKIP To End Flight", CONTENT_Y + 160);

    uiDrawSoftButtons("SKIP", "DONE");
}

static void drawTasting(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("TASTING TIME", COL_ACCENT);

    char countStr[24];
    snprintf(countStr, sizeof(countStr), "%d Glasses Poured", session.pourCount);
    uiDrawCenteredText(countStr, CONTENT_Y + 20, FONT_BODY, COL_TEXT);

    // Glass position indicators (centered for glass count)
    {
        const int boxW = 44, boxH = 44, gap = 10;
        int n = session.glassCount;
        int totalW = n * boxW + (n - 1) * gap;
        int startX = (SCREEN_W - totalW) / 2;
        int y = CONTENT_Y + 55;
        for (int i = 0; i < n; i++) {
            int x = startX + i * (boxW + gap);
            uint16_t boxCol = session.glassUsed[i] ? COL_ACCENT : COL_DIM;
            uint16_t txtCol = session.glassUsed[i] ? COL_BG     : COL_TEXT;
            tft->fillRoundRect(x, y, boxW, boxH, 6, boxCol);
            tft->setTextColor(txtCol, boxCol);
            tft->setTextSize(3);
            tft->setTextDatum(MC_DATUM);
            tft->drawString(String(i + 1), x + boxW / 2, y + boxH / 2);
        }
    }

    uiDrawCenteredText("Enjoy Your Flight", CONTENT_Y + 120, FONT_BODY, COL_TEXT);

    // Mode-specific hint and button text
    if (modeIsChallenge()) {
        if (currentMode == GAME_MODE_DUPLICATE) {
            uiDrawHint("Find the matching", CONTENT_Y + 142);
            uiDrawHint("pair!",             CONTENT_Y + 160);
        } else {
            uiDrawHint("Find the odd",      CONTENT_Y + 142);
            uiDrawHint("one out!",          CONTENT_Y + 160);
        }
        uiDrawSoftButtons("", "GUESS");
    } else if (currentMode == GAME_MODE_GUESS) {
        uiDrawHint("Press to Guess",    CONTENT_Y + 150);
        uiDrawSoftButtons("", "NEXT");
    } else if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
        uiDrawHint("Press to Rank",     CONTENT_Y + 150);
        uiDrawSoftButtons("", "NEXT");
    } else {
        uiDrawHint("Press for Reveal",  CONTENT_Y + 150);
        uiDrawSoftButtons("", "NEXT");
    }
}

// ============================================================
// Guessing round draw functions (Session 13)
// ============================================================

static void drawGuessing(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    // Title bar shows which glass is being guessed
    int glass = revealMap[session.guessIndex].glass;
    char titleBuf[20];
    snprintf(titleBuf, sizeof(titleBuf), "GLASS %d", glass);
    uiDrawTitleBar(titleBuf, COL_ACCENT);

    // ScrollList of available whiskey names
    uiScrollListDraw(&guessList);

    uiDrawSoftButtons("BACK", "LOCK IN");
}

static void drawGuessScore(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESULTS", COL_ACCENT);

    int correct = countCorrectGuesses();

    // "X of Y" in large font
    char scoreBuf[8];
    snprintf(scoreBuf, sizeof(scoreBuf), "%d of %d", correct, revealMapCount);
    uiDrawCenteredText(scoreBuf, CONTENT_Y + 40, FONT_XLARGE, COL_TEXT);

    uiDrawCenteredText("Correct", CONTENT_Y + 90, FONT_BODY, COL_TEXT);

    uiDrawHint("BACK to change", CONTENT_Y + 130);
    uiDrawHint("NEXT for hint", CONTENT_Y + 150);

    uiDrawSoftButtons("BACK", "NEXT");
}

// Helper: draw a 2px-thick checkmark centered at (cx, cy)
static void drawCheckmark(TFT_eSPI* tft, int cx, int cy, uint16_t color) {
    for (int t = 0; t <= 1; t++) {
        tft->drawLine(cx - 5, cy - 1 + t, cx - 1, cy + 3 + t, color);
        tft->drawLine(cx - 1, cy + 3 + t, cx + 6, cy - 5 + t, color);
    }
}

// Helper: draw a 2px-thick X centered at (cx, cy)
static void drawXMark(TFT_eSPI* tft, int cx, int cy, uint16_t color) {
    for (int t = 0; t <= 1; t++) {
        tft->drawLine(cx - 4 + t, cy - 4, cx + 4 + t, cy + 4, color);
        tft->drawLine(cx + 4 - t, cy - 4, cx - 4 - t, cy + 4, color);
    }
}

static void drawGuessDetail(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESULTS", COL_ACCENT);

    uiDrawCenteredText("Your Guesses", CONTENT_Y + 10, FONT_BODY, COL_TEXT);

    // Glass position boxes (centered for glass count)
    const int boxW = 44, boxH = 44, gap = 10;
    int n = session.glassCount;
    int totalW = n * boxW + (n - 1) * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int boxY = CONTENT_Y + 45;

    for (int i = 0; i < n; i++) {
        int x = startX + i * (boxW + gap);

        if (session.glassUsed[i]) {
            // Poured glass — show number + correct/incorrect indicator
            tft->fillRoundRect(x, boxY, boxW, boxH, 6, COL_ACCENT);
            tft->setTextColor(COL_BG, COL_ACCENT);
            tft->setTextSize(FONT_BODY);
            tft->setTextDatum(MC_DATUM);
            tft->drawString(String(i + 1), x + boxW / 2, boxY + 14);

            // Find this glass in revealMap to check guess
            for (int r = 0; r < revealMapCount; r++) {
                if (revealMap[r].glass == i + 1) {
                    int indicatorCx = x + boxW / 2;
                    int indicatorCy = boxY + 33;
                    if (session.guessForGlass[r] == revealMap[r].pourIdx) {
                        drawCheckmark(tft, indicatorCx, indicatorCy, COL_CORRECT);
                    } else {
                        drawXMark(tft, indicatorCx, indicatorCy, COL_BG);
                    }
                    break;
                }
            }
        } else {
            // Unpoured glass — dimmed, no indicator
            tft->fillRoundRect(x, boxY, boxW, boxH, 6, COL_DIM);
            tft->setTextColor(COL_TEXT, COL_DIM);
            tft->setTextSize(FONT_BODY);
            tft->setTextDatum(MC_DATUM);
            tft->drawString(String(i + 1), x + boxW / 2, boxY + boxH / 2);
        }
    }

    // Score reminder below boxes
    int correct = countCorrectGuesses();
    char scoreBuf[16];
    snprintf(scoreBuf, sizeof(scoreBuf), "%d of %d Correct", correct, revealMapCount);
    uiDrawCenteredText(scoreBuf, CONTENT_Y + 110, FONT_BODY, COL_TEXT);

    uiDrawHint("BACK to re-guess", CONTENT_Y + 145);

    uiDrawSoftButtons("BACK", "REVEAL");
}

// ============================================================
// Ranking round draw function (Session 20)
// ============================================================

static void drawRanking(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar(ordinalStr(session.rankIndex), COL_ACCENT);

    uiScrollListDraw(&rankList);

    uiDrawSoftButtons("BACK", "LOCK IN");
}

// ============================================================
// Rating round (Session 21)
// ============================================================

static void initRating() {
    buildRevealMap();
    session.ratingIndex = 0;
    for (int i = 0; i < NUM_GLASSES; i++) {
        session.glassRating[i] = 0;
    }
    state = GAME_RATING;
    flushInput();
    uiRequestRedraw();
}

static void drawRating(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    int idx = session.ratingIndex;
    if (idx >= revealMapCount) return;

    int pourIdx = revealMap[idx].pourIdx;
    int glass   = revealMap[idx].glass;

    char titleBuf[20];
    snprintf(titleBuf, sizeof(titleBuf), "RATE GLASS %d", glass);
    uiDrawTitleBar(titleBuf, COL_ACCENT);

    if (session.glassName[pourIdx][0]) {
        uiDrawCenteredTextWrap(session.glassName[pourIdx],
                               CONTENT_Y + 15, FONT_BODY, COL_TEXT);
    } else {
        char pourLabel[16];
        snprintf(pourLabel, sizeof(pourLabel), "Pour #%d", pourIdx + 1);
        uiDrawCenteredText(pourLabel, CONTENT_Y + 15, FONT_BODY, COL_TEXT);
    }

    uint8_t rating = session.glassRating[pourIdx];

    // Draw star row
    const int starW = 28, starGap = 8;
    int totalStarW = RATING_MAX * starW + (RATING_MAX - 1) * starGap;
    int starX0 = (SCREEN_W - totalStarW) / 2;
    int starY  = CONTENT_Y + 55;

    for (int s = 0; s < RATING_MAX; s++) {
        int x = starX0 + s * (starW + starGap);
        bool filled = (s < rating);
        if (filled) {
            tft->fillRoundRect(x, starY, starW, starW, 4, COL_ACCENT);
            tft->setTextColor(COL_BG, COL_ACCENT);
        } else {
            tft->drawRoundRect(x, starY, starW, starW, 4, COL_DIM);
            tft->setTextColor(COL_DIM, COL_BG);
        }
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawChar('*', x + starW / 2, starY + starW / 2);
    }

    // Progress
    char progBuf[16];
    snprintf(progBuf, sizeof(progBuf), "%d of %d", idx + 1, revealMapCount);
    uiDrawCenteredText(progBuf, CONTENT_Y + 105, FONT_SMALL, COL_DIM);

    uiDrawHint("Turn to rate", CONTENT_Y + 130);
    uiDrawHint("Click to confirm", CONTENT_Y + 148);

    uiDrawSoftButtons("SKIP", "OK");
}

// ============================================================
// Metadata formatting helpers (Session 21)
// ============================================================

static const char* priceTierStr(uint8_t tier) {
    switch (tier) {
        case 1: return "$";
        case 2: return "$$";
        case 3: return "$$$";
        case 4: return "$$$$";
        default: return nullptr;
    }
}

static void drawStarRow(TFT_eSPI* tft, int cx, int y, uint8_t rating) {
    const int starGap = 10;
    int totalW = (rating - 1) * starGap;
    int x0 = cx - totalW / 2;
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(MC_DATUM);
    tft->setTextColor(COL_ACCENT, COL_BG);
    for (int s = 0; s < rating; s++) {
        tft->drawChar('*', x0 + s * starGap, y);
    }
}

// ============================================================
// Reveal and Done draw functions
// ============================================================

static void drawReveal(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("REVEAL", COL_ACCENT);

    int idx = session.revealIndex;
    if (idx >= revealMapCount) return;

    int glass   = revealMap[idx].glass;
    int pourIdx = revealMap[idx].pourIdx;

    // --- Glass number at top ---
    char glassLabel[16];
    snprintf(glassLabel, sizeof(glassLabel), "Glass %d", glass);
    uiDrawCenteredText(glassLabel, CONTENT_Y + 8, FONT_BODY, COL_TEXT);

    // --- Name reveal (slot roll at FONT_BODY for fit) ---
    const char* revealStr;
    char numStr[8];
    if (modeIsChallenge()) {
        int bottleIdx = session.bottleForGlass[glass - 1];
        revealStr = session.bottleName[bottleIdx];
    } else if (session.glassName[pourIdx][0]) {
        revealStr = session.glassName[pourIdx];
    } else {
        snprintf(numStr, sizeof(numStr), "Pour %d", pourIdx + 1);
        revealStr = numStr;
    }

    int rollY = CONTENT_Y + 35;
    int rollH = 36;
    transSlotRoll(0, rollY, SCREEN_W, rollH,
                  revealStr, COL_ACCENT, COL_BG, FONT_BODY);

    // --- Rank or pour number below the name ---
    int metaY;
    if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
        // revealMap is reversed: idx 0 = last place, so map back to rank
        int rank = revealMapCount - 1 - idx;
        uiDrawCenteredText(ordinalRevealStr(rank), CONTENT_Y + 80, FONT_SMALL, COL_DIM);
        metaY = CONTENT_Y + 95;
    } else {
        char pourLabel[16];
        snprintf(pourLabel, sizeof(pourLabel), "Pour #%d", pourIdx + 1);
        uiDrawCenteredText(pourLabel, CONTENT_Y + 80, FONT_SMALL, COL_DIM);
        metaY = CONTENT_Y + 95;
    }

    // --- Metadata line (Session 21) ---
    const LibraryEntry* entry = session.glassEntry[pourIdx];
    if (entry) {
        char metaBuf[32];
        metaBuf[0] = '\0';
        if (entry->proof > 0) {
            snprintf(metaBuf, sizeof(metaBuf), "%d proof", entry->proof);
        }
        const char* pt = priceTierStr(entry->priceTier);
        if (pt) {
            if (metaBuf[0]) strncat(metaBuf, "  ", sizeof(metaBuf) - strlen(metaBuf) - 1);
            strncat(metaBuf, pt, sizeof(metaBuf) - strlen(metaBuf) - 1);
        }
        if (entry->ageYears > 0) {
            char ageBuf[8];
            snprintf(ageBuf, sizeof(ageBuf), "  %dyr", entry->ageYears);
            strncat(metaBuf, ageBuf, sizeof(metaBuf) - strlen(metaBuf) - 1);
        }
        if (metaBuf[0]) {
            uiDrawCenteredText(metaBuf, metaY, FONT_SMALL, COL_DIM);
        }
    }

    // --- Star rating (Session 21) ---
    if (session.glassRating[pourIdx] > 0) {
        drawStarRow(tft, SCREEN_W / 2, metaY + 12, session.glassRating[pourIdx]);
    }

    // --- Progress dots ---
    int dotY   = CONTENT_Y + 135;
    int dotR   = 6;
    int dotGap = 24;
    int dotsW  = (revealMapCount - 1) * dotGap;
    int dotX0  = (SCREEN_W - dotsW) / 2;

    for (int i = 0; i < revealMapCount; i++) {
        int cx = dotX0 + i * dotGap;
        uint16_t col = (i <= idx) ? COL_ACCENT : COL_DIM;
        tft->fillCircle(cx, dotY, dotR, col);
    }

    bool isLast = (idx >= revealMapCount - 1);
    uiDrawSoftButtons("", isLast ? "FINISH" : "NEXT");
}

static void drawDone(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("FLIGHT COMPLETE", COL_SELECTED);

    uiDrawCenteredText("Results", CONTENT_Y + 2, FONT_BODY, COL_TEXT);

    int y = CONTENT_Y + 30;
    int leftX = 16;

    if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
        // Results in rank order (1st to last): revealMap is reversed,
        // so iterate backwards to show #1 first
        for (int i = revealMapCount - 1; i >= 0; i--) {
            int rank    = revealMapCount - i;  // 1-based rank
            int glass   = revealMap[i].glass;
            int pourIdx = revealMap[i].pourIdx;
            const char* name = session.glassName[pourIdx][0] ? session.glassName[pourIdx] : "?";

            char rankStr[8];
            snprintf(rankStr, sizeof(rankStr), "#%d ", rank);
            tft->setTextSize(FONT_BODY);
            tft->setTextDatum(ML_DATUM);
            tft->setTextColor(COL_DIM, COL_BG);
            tft->drawString(rankStr, leftX, y + 10);
            int rankW = tft->textWidth(rankStr);

            uint16_t nameCol = COL_ACCENT;
            if (currentMode == GAME_MODE_GUESS_RANK) {
                nameCol = guessCorrectForGlass[glass - 1] ? COL_CORRECT : COL_ERROR;
            }

            char glassName[40];
            snprintf(glassName, sizeof(glassName), "%d. %s", glass, name);
            int lineMaxW = SCREEN_W - leftX - 16 - rankW;
            tft->setTextColor(nameCol, COL_BG);
            if (tft->textWidth(glassName) > lineMaxW) {
                char truncBuf[40];
                int maxChars = 0;
                for (int c = 0; glassName[c] && c < 36; c++) {
                    truncBuf[c] = glassName[c];
                    truncBuf[c + 1] = '\0';
                    if (tft->textWidth(truncBuf) > lineMaxW - tft->textWidth("..")) break;
                    maxChars = c + 1;
                }
                truncBuf[maxChars] = '.';
                truncBuf[maxChars + 1] = '.';
                truncBuf[maxChars + 2] = '\0';
                tft->drawString(truncBuf, leftX + rankW, y + 10);
            } else {
                tft->drawString(glassName, leftX + rankW, y + 10);
            }
            y += 28;
        }
    } else {
        for (int i = 0; i < revealMapCount; i++) {
            int glass   = revealMap[i].glass;
            int pourIdx = revealMap[i].pourIdx;

            char line[40];
            if (modeIsChallenge()) {
                int bottleIdx = session.bottleForGlass[glass - 1];
                snprintf(line, sizeof(line), "%d. %s", glass, session.bottleName[bottleIdx]);
            } else if (session.glassName[pourIdx][0]) {
                snprintf(line, sizeof(line), "%d. %s", glass, session.glassName[pourIdx]);
            } else {
                snprintf(line, sizeof(line), "%d. Pour #%d", glass, pourIdx + 1);
            }

            tft->setTextColor(COL_ACCENT, COL_BG);
            tft->setTextSize(FONT_BODY);
            int lineMaxW = SCREEN_W - leftX * 2;
            tft->setTextDatum(ML_DATUM);
            if (tft->textWidth(line) > lineMaxW) {
                char truncBuf[40];
                int maxChars = 0;
                for (int c = 0; line[c] && c < 36; c++) {
                    truncBuf[c] = line[c];
                    truncBuf[c + 1] = '\0';
                    if (tft->textWidth(truncBuf) > lineMaxW - tft->textWidth("..")) break;
                    maxChars = c + 1;
                }
                truncBuf[maxChars] = '.';
                truncBuf[maxChars + 1] = '.';
                truncBuf[maxChars + 2] = '\0';
                tft->drawString(truncBuf, leftX, y + 10);
            } else {
                tft->drawString(line, leftX, y + 10);
            }
            y += 28;
        }
    }

    if (currentMode == GAME_MODE_GUESS_RANK) {
        int correct = 0;
        for (int i = 0; i < NUM_GLASSES; i++) {
            if (guessCorrectForGlass[i]) correct++;
        }
        char scoreBuf[20];
        snprintf(scoreBuf, sizeof(scoreBuf), "Guessed %d of %d", correct, revealMapCount);
        uiDrawCenteredText(scoreBuf, y + 4, FONT_SMALL, COL_DIM);
        y += 14;
    }

    uiDrawHint("New flight or exit", y + 4);
    uiDrawSoftButtons("EXIT", "NEW");
}

// ============================================================
// Count selection screen
// ============================================================

static void drawCountSelect(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar(getModeTitleText(), COL_ACCENT);

    // Large glass count number
    char num[4];
    snprintf(num, sizeof(num), "%d", session.glassCount);
    uiDrawCenteredText(num, CONTENT_Y + 40, FONT_XLARGE, COL_ACCENT);

    uiDrawCenteredText("Glasses", CONTENT_Y + 85, FONT_BODY, COL_TEXT);

    // Glass icons showing active positions
    const int boxW = 36, boxH = 36, gap = 8;
    int totalW = 4 * boxW + 3 * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int y = CONTENT_Y + 115;
    for (int i = 0; i < 4; i++) {
        int x = startX + i * (boxW + gap);
        bool active = (i < session.glassCount);
        uint16_t boxCol = active ? COL_ACCENT : COL_DIM;
        uint16_t txtCol = active ? COL_BG     : COL_TEXT;
        tft->fillRoundRect(x, y, boxW, boxH, 4, boxCol);
        tft->setTextColor(txtCol, boxCol);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(String(i + 1), x + boxW / 2, y + boxH / 2);
    }

    uiDrawHint("Turn to adjust", CONTENT_Y + 168);
    uiDrawSoftButtons("BACK", "START");
}

// ============================================================
// Screen callbacks
// ============================================================

static void gameDraw(bool fullRedraw) {

    // ========================================================
    // Phone deferred actions (Session 16)
    // ========================================================
    // Process phone actions at the top of gameDraw so they
    // execute in the same context as normal game operations.
    // This avoids reentrancy issues with blocking motor ops.

    // --- Phone game start ---
    if (phonePendingStart && !gameActive) {
        phonePendingStart = false;
        currentMode = phoneStartMode;
        gameActive = true;
        pendingBrowse = false;
        awaitingBrowseReturn = false;

        if (modeUsesNames()) {
            Serial.printf("[Game] Phone starting %s\n",
                          currentMode == GAME_MODE_GUESS ? "Best Guess" :
                          currentMode == GAME_MODE_RANK  ? "Ranked Flight" : "Named Flight");
            resetSession();
            // Go directly to NAMING state — phone provides the name
            state = GAME_NAMING;
            // Don't push browse screen — phone UI handles name entry
            uiRequestRedraw();
        } else {
            Serial.println("[Game] Phone starting Basic Flight");
            resetSession();
            runPourCycle();
        }
        return;
    }

    // --- Phone name ready (browse already cleaned up) ---
    if (phoneNameReady && state == GAME_NAMING) {
        phoneNameReady = false;
        // Name is already in session.glassName (set by gamePhoneSubmitName).
        // Browse screen was already popped/cancelled by the submit function.
        // Just run the pour cycle.
        runPourCycle();
        drawPouring(true);
        return;
    }

    // --- Phone guess selection ---
    if (phonePendingGuess >= 0 && state == GAME_GUESSING) {
        int sel = phonePendingGuess;
        phonePendingGuess = -1;

        if (sel >= 0 && sel < guessPoolCount) {
            audioPlayTone(TONE_CONFIRM);
            session.guessForGlass[session.guessIndex] = guessPoolPourIdx[sel];

            Serial.printf("[Game] Phone guess: Glass %d = %s\n",
                          revealMap[session.guessIndex].glass,
                          session.glassName[guessPoolPourIdx[sel]]);

            if (session.guessIndex < revealMapCount - 1) {
                session.guessIndex++;
                buildGuessPool();
                uiRequestRedraw();
            } else {
                state = GAME_GUESS_SCORE;
                flushInput();
                uiRequestRedraw();
            }
        }
        return;
    }

    // --- Resume pour (session restore for basic mode) ---
    if (pendingResumePour) {
        pendingResumePour = false;
        runPourCycle();
        return;
    }

    // ========================================================
    // Normal game draw flow
    // ========================================================

    // --- Deferred browse push ---
    if (pendingBrowse) {
        pendingBrowse = false;
        browseReset();
        uiPushScreen(&screenBrowse);
        return;
    }

    // --- Handle browse return in BOTTLE_SELECT state (Session 22) ---
    if (state == GAME_BOTTLE_SELECT && awaitingBrowseReturn) {
        awaitingBrowseReturn = false;
        const char* name = browseGetResult();
        if (name) {
            strncpy(session.bottleName[session.bottleCount], name,
                    MAX_GLASS_NAME - 1);
            session.bottleName[session.bottleCount][MAX_GLASS_NAME - 1] = '\0';

            Serial.printf("[Game] Bottle %d: %s\n",
                          session.bottleCount + 1, session.bottleName[session.bottleCount]);

            session.bottleCount++;
            int target = challengeBottleTarget();
            if (session.bottleCount >= target) {
                // All bottles selected — build pour order and start pouring
                buildChallengePourOrder();
                runPourCycle();
            } else {
                // Need more bottles — show selection screen again
                uiRequestRedraw();
            }
        } else {
            // Cancelled browse
            if (session.bottleCount > 0) {
                // Stay on bottle select screen
                uiRequestRedraw();
            } else {
                Serial.println("[Game] Bottle select cancelled — exiting");
                gameActive = false;
                uiPopScreenT(TRANS_FADE);
            }
        }
        return;
    }

    // --- Handle browse return in NAMING state ---
    // Only runs when browse was actually pushed (device-started or
    // "New Flight" flow). Phone-started naming never sets this flag,
    // so the handler is correctly skipped.
    if (state == GAME_NAMING && awaitingBrowseReturn) {
        awaitingBrowseReturn = false;
        const char* name = browseGetResult();
        if (name) {
            strncpy(session.glassName[session.pourCount], name,
                    MAX_GLASS_NAME - 1);
            session.glassName[session.pourCount][MAX_GLASS_NAME - 1] = '\0';
            session.glassEntry[session.pourCount] = browseGetEntry();

            Serial.printf("[Game] Named pour %d: %s\n",
                          session.pourCount + 1,
                          session.glassName[session.pourCount]);

            runPourCycle();
            drawPouring(true);
        } else {
            if (session.pourCount > 0) {
                Serial.println("[Game] Browse cancelled — moving to tasting");
                runFinalSpin();
                drawTasting(true);
            } else {
                Serial.println("[Game] Browse cancelled — exiting");
                gameActive = false;
                uiPopScreenT(TRANS_FADE);
            }
        }
        return;
    }

    // --- Normal state drawing ---
    switch (state) {
        case GAME_COUNT_SELECT:    drawCountSelect(fullRedraw);    break;
        case GAME_BOTTLE_SELECT:   drawBottleSelect(fullRedraw);   break;
        case GAME_POURING:
            if (modeIsChallenge()) drawChallengePouring(fullRedraw);
            else                   drawPouring(fullRedraw);
            break;
        case GAME_TASTING:         drawTasting(fullRedraw);        break;
        case GAME_RATING:          drawRating(fullRedraw);         break;
        case GAME_GUESSING:        drawGuessing(fullRedraw);       break;
        case GAME_GUESS_SCORE:     drawGuessScore(fullRedraw);     break;
        case GAME_GUESS_DETAIL:    drawGuessDetail(fullRedraw);    break;
        case GAME_RANKING:         drawRanking(fullRedraw);        break;
        case GAME_CHALLENGE_GUESS: drawChallengeGuess(fullRedraw); break;
        case GAME_CHALLENGE_RESULT:drawChallengeResult(fullRedraw);break;
        case GAME_REVEAL:          drawReveal(fullRedraw);         break;
        case GAME_DONE:            drawDone(fullRedraw);           break;
        default: break;
    }
}

static void startAfterCountSelect() {
    settingsSetGlassCount(session.glassCount);
    settingsSave();
    if (modeUsesNames()) {
        requestBrowse(false);
    } else {
        runPourCycle();
    }
}

static void gameInput(InputEvent evt) {
    switch (state) {

        case GAME_COUNT_SELECT:
            if (evt == INPUT_ENC_CW) {
                if (session.glassCount < 4) {
                    session.glassCount++;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CCW) {
                if (session.glassCount > 2) {
                    session.glassCount--;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                persistSaveGame(currentMode, state, session);
                startAfterCountSelect();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                gameActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;

        case GAME_NAMING:
            break;

        case GAME_BOTTLE_SELECT:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                awaitingBrowseReturn = true;
                browseReset();
                uiPushScreen(&screenBrowse);
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (session.bottleCount > 0) {
                    // Undo last bottle selection
                    session.bottleCount--;
                    session.bottleName[session.bottleCount][0] = '\0';
                    uiRequestRedraw();
                } else {
                    gameActive = false;
                    uiPopScreenT(TRANS_FADE);
                }
            }
            break;

        case GAME_POURING:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                session.glassUsed[session.currentGlass - 1] = true;
                if (!modeIsChallenge()) {
                    session.pourOrder[session.pourCount] = session.currentGlass;
                }
                session.pourCount++;
                persistSaveGame(currentMode, state, session);

                Serial.printf("[Game] Poured glass %d (pour %d/%d)\n",
                              session.currentGlass, session.pourCount, session.glassCount);

                if (session.pourCount >= session.glassCount) {
                    runFinalSpin();
                } else if (modeIsChallenge()) {
                    runPourCycle();
                } else if (modeUsesNames()) {
                    requestBrowse(false);
                } else {
                    runPourCycle();
                }

            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                Serial.printf("[Game] Skipping remaining — %d poured\n",
                              session.pourCount);
                runFinalSpin();
            }
            break;

        case GAME_TASTING:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                if (session.pourCount > 0) {
                    if (modeIsChallenge()) {
                        buildRevealMap();
                        session.currentGlass = 1;
                        session.challengeGuessCount = 0;
                        session.challengeGuess[0] = 0;
                        session.challengeGuess[1] = 0;
                        state = GAME_CHALLENGE_GUESS;
                        flushInput();
                        uiRequestRedraw();
                    } else if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
                        initRanking(false);
                    } else if (currentMode == GAME_MODE_GUESS) {
                        initGuessing(false);
                    } else {
                        buildRevealMap();
                        session.revealIndex = 0;
                        state = GAME_REVEAL;
                        audioPlayTone(TONE_HOME_FOUND);
                        flushInput();
                        uiRequestRedraw();
                    }
                } else {
                    enterGameDone();
                }
            }
            break;

        // ============================================================
        // Rating round input handling (Session 21)
        // ============================================================

        case GAME_RATING: {
            int pourIdx = revealMap[session.ratingIndex].pourIdx;

            if (evt == INPUT_ENC_CW) {
                if (session.glassRating[pourIdx] < RATING_MAX) {
                    session.glassRating[pourIdx]++;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CCW) {
                if (session.glassRating[pourIdx] > 0) {
                    session.glassRating[pourIdx]--;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                if (session.ratingIndex < revealMapCount - 1) {
                    session.ratingIndex++;
                    uiRequestRedraw();
                } else {
                    // Rating complete — advance to mode-specific next step
                    if (currentMode == GAME_MODE_GUESS) {
                        initGuessing(false);
                    } else if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
                        initRanking(false);
                    } else {
                        session.revealIndex = 0;
                        state = GAME_REVEAL;
                        audioPlayTone(TONE_HOME_FOUND);
                        flushInput();
                        uiRequestRedraw();
                    }
                }
            } else if (evt == INPUT_BTN_LEFT) {
                // SKIP — skip entire rating round
                audioPlayTone(TONE_SELECT);
                for (int i = 0; i < NUM_GLASSES; i++) {
                    session.glassRating[i] = 0;
                }
                if (currentMode == GAME_MODE_GUESS) {
                    initGuessing(false);
                } else if (currentMode == GAME_MODE_RANK || currentMode == GAME_MODE_GUESS_RANK) {
                    initRanking(false);
                } else {
                    session.revealIndex = 0;
                    state = GAME_REVEAL;
                    audioPlayTone(TONE_HOME_FOUND);
                    flushInput();
                    uiRequestRedraw();
                }
            }
            break;
        }

        // ============================================================
        // Guessing round input handling (Session 13)
        // ============================================================

        case GAME_GUESSING: {
            // Let ScrollList handle encoder rotation and click/right
            int sel = uiScrollListHandleInput(&guessList, evt);

            if (sel >= 0) {
                // Lock in this guess
                audioPlayTone(TONE_CONFIRM);
                session.guessForGlass[session.guessIndex] = guessPoolPourIdx[sel];

                Serial.printf("[Game] Guess: Glass %d = %s\n",
                              revealMap[session.guessIndex].glass,
                              session.glassName[guessPoolPourIdx[sel]]);

                if (session.guessIndex < revealMapCount - 1) {
                    // Advance to next glass
                    session.guessIndex++;
                    buildGuessPool();
                    uiRequestRedraw();
                } else {
                    // All glasses guessed — show score
                    state = GAME_GUESS_SCORE;
                    flushInput();
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (session.guessIndex == 0) {
                    // Back to tasting screen (abandon guessing)
                    state = GAME_TASTING;
                    flushInput();
                    uiRequestRedraw();
                } else {
                    // Back to previous glass — restore its guess to pool
                    session.guessIndex--;
                    buildGuessPool();
                    uiRequestRedraw();
                }
            }
            break;
        }

        case GAME_GUESS_SCORE:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                // Advance to detail screen
                audioPlayTone(TONE_CONFIRM);
                state = GAME_GUESS_DETAIL;
                flushInput();
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                // Back to guessing round at Glass 1, preserving guesses
                audioPlayTone(TONE_SELECT);
                initGuessing(true);
            }
            break;

        case GAME_GUESS_DETAIL:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                if (currentMode == GAME_MODE_GUESS_RANK) {
                    // Save per-glass correctness before revealMap is rebuilt
                    for (int i = 0; i < NUM_GLASSES; i++) guessCorrectForGlass[i] = false;
                    for (int r = 0; r < revealMapCount; r++) {
                        int g = revealMap[r].glass;
                        guessCorrectForGlass[g - 1] = (session.guessForGlass[r] == revealMap[r].pourIdx);
                    }
                    buildRevealMapFromRank();
                }
                session.revealIndex = 0;
                state = GAME_REVEAL;
                audioPlayTone(TONE_HOME_FOUND);
                flushInput();
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                initGuessing(true);
            }
            break;

        case GAME_RANKING: {
            int sel = uiScrollListHandleInput(&rankList, evt);

            if (sel >= 0) {
                audioPlayTone(TONE_CONFIRM);
                session.rankOrder[session.rankIndex] = rankPoolGlass[sel];

                Serial.printf("[Game] Rank #%d = Glass %d\n",
                              session.rankIndex + 1, rankPoolGlass[sel]);

                if (session.rankIndex < session.pourCount - 1) {
                    session.rankIndex++;
                    buildRankPool();
                    uiRequestRedraw();
                } else {
                    if (currentMode == GAME_MODE_GUESS_RANK) {
                        // Ranking done — now enter guessing round
                        initGuessing(false);
                    } else {
                        buildRevealMapFromRank();
                        session.revealIndex = 0;
                        state = GAME_REVEAL;
                        audioPlayTone(TONE_HOME_FOUND);
                        flushInput();
                        uiRequestRedraw();
                    }
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (session.rankIndex == 0) {
                    state = GAME_TASTING;
                    flushInput();
                    uiRequestRedraw();
                } else {
                    session.rankIndex--;
                    buildRankPool();
                    uiRequestRedraw();
                }
            }
            break;
        }

        // ============================================================
        // Challenge guess input handling (Session 22)
        // ============================================================

        case GAME_CHALLENGE_GUESS:
            if (evt == INPUT_ENC_CW) {
                if (session.currentGlass < 4) {
                    session.currentGlass++;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CCW) {
                if (session.currentGlass > 1) {
                    session.currentGlass--;
                    audioPlayTone(TONE_CLICK);
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_ENC_CLICK) {
                // Toggle selection of current glass
                int g = session.currentGlass;

                // Check if already selected — deselect it
                bool deselected = false;
                for (int i = 0; i < session.challengeGuessCount; i++) {
                    if (session.challengeGuess[i] == g) {
                        // Remove this selection
                        for (int j = i; j < session.challengeGuessCount - 1; j++) {
                            session.challengeGuess[j] = session.challengeGuess[j + 1];
                        }
                        session.challengeGuessCount--;
                        session.challengeGuess[session.challengeGuessCount] = 0;
                        audioPlayTone(TONE_SELECT);
                        deselected = true;
                        break;
                    }
                }

                if (!deselected) {
                    int maxSelections = (currentMode == GAME_MODE_DUPLICATE) ? 2 : 1;
                    if (session.challengeGuessCount < maxSelections) {
                        session.challengeGuess[session.challengeGuessCount] = g;
                        session.challengeGuessCount++;
                        audioPlayTone(TONE_CONFIRM);
                    }
                }
                uiRequestRedraw();

            } else if (evt == INPUT_BTN_RIGHT) {
                // Confirm guess — only if enough selections made
                int needed = (currentMode == GAME_MODE_DUPLICATE) ? 2 : 1;
                if (session.challengeGuessCount >= needed) {
                    audioPlayTone(TONE_CONFIRM);
                    session.challengeCorrect = checkChallengeGuess();
                    if (session.challengeCorrect) {
                        audioPlayTone(TONE_HOME_FOUND);
                    } else {
                        audioPlayTone(TONE_ERROR);
                    }
                    state = GAME_CHALLENGE_RESULT;
                    flushInput();
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                // Back to tasting
                audioPlayTone(TONE_SELECT);
                state = GAME_TASTING;
                flushInput();
                uiRequestRedraw();
            }
            break;

        case GAME_CHALLENGE_RESULT:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                session.revealIndex = 0;
                state = GAME_REVEAL;
                audioPlayTone(TONE_HOME_FOUND);
                flushInput();
                uiRequestRedraw();
            }
            break;

        case GAME_REVEAL:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                if (session.revealIndex < revealMapCount - 1) {
                    session.revealIndex++;
                    audioPlayTone(TONE_HOME_FOUND);
                    uiRequestRedraw();
                } else {
                    audioPlayTone(TONE_CONFIRM);
                    enterGameDone();
                }
            }
            break;

        case GAME_DONE:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                resetSession();
                if (modeIsChallenge()) {
                    state = GAME_BOTTLE_SELECT;
                    uiRequestRedraw();
                } else if (modeUsesNames()) {
                    requestBrowse(false);
                } else {
                    runPourCycle();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                gameActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;
    }
}

static void gameOnEnter() {
    if (gameActive) {
        Serial.println("[Game] onEnter (re-entry — skipping init)");
        return;
    }

    // Phone start — defer full initialization to gameDraw()
    // so named modes can skip the browse screen.
    if (phonePendingStart) {
        Serial.println("[Game] onEnter (phone start — deferring to gameDraw)");
        return;
    }

    gameActive = true;
    pendingBrowse = false;
    awaitingBrowseReturn = false;

    Serial.printf("[Game] Starting %s\n", getModeTitleText());
    resetSession();

    if (modeIsChallenge()) {
        // Challenge modes lock to 4 glasses, skip count select
        session.glassCount = NUM_GLASSES;
        state = GAME_BOTTLE_SELECT;
    } else {
        state = GAME_COUNT_SELECT;
    }
    uiRequestRedraw();
}

// ============================================================
// Public API
// ============================================================

void gameSetMode(GameMode mode) {
    currentMode = mode;
}

const Screen screenGame = {
    "Game",
    gameDraw,
    gameInput,
    gameOnEnter
};

// ============================================================
// State getters (Session 16)
// ============================================================

bool gameIsActive() {
    return gameActive;
}

void gameAbort() {
    if (!gameActive) return;
    Serial.println("[Game] Aborted via long-press cancel");
    persistClearSession();
    gameActive = false;
    motorDisable();
}

int gameGetGlassCount() {
    return session.glassCount;
}

GameState gameGetState() {
    return state;
}

GameMode gameGetMode() {
    return currentMode;
}

int gameGetPourCount() {
    return session.pourCount;
}

bool gameGetGlassUsed(int glassIdx) {
    if (glassIdx < 0 || glassIdx >= NUM_GLASSES) return false;
    return session.glassUsed[glassIdx];
}

const char* gameGetGlassName(int pourIdx) {
    if (pourIdx < 0 || pourIdx >= NUM_GLASSES) return "";
    return session.glassName[pourIdx];
}

bool gameIsSpinning() {
    return spinningNow;
}

int gameGetRevealCount() {
    return revealMapCount;
}

int gameGetRevealIndex() {
    return session.revealIndex;
}

int gameGetRevealGlass(int revealIdx) {
    if (revealIdx < 0 || revealIdx >= revealMapCount) return 0;
    return revealMap[revealIdx].glass;
}

int gameGetRevealPourIdx(int revealIdx) {
    if (revealIdx < 0 || revealIdx >= revealMapCount) return 0;
    return revealMap[revealIdx].pourIdx;
}

int gameGetGuessGlass() {
    if (session.guessIndex < 0 || session.guessIndex >= revealMapCount) return 0;
    return revealMap[session.guessIndex].glass;
}

int gameGetGuessPoolCount() {
    return guessPoolCount;
}

const char* gameGetGuessPoolName(int poolIdx) {
    if (poolIdx < 0 || poolIdx >= guessPoolCount) return "";
    return guessPoolLabels[poolIdx];
}

int gameGetGuessCorrect() {
    return countCorrectGuesses();
}

bool gameGetGuessCorrectAt(int revealIdx) {
    if (revealIdx < 0 || revealIdx >= revealMapCount) return false;
    return session.guessForGlass[revealIdx] == revealMap[revealIdx].pourIdx;
}

uint8_t gameGetGlassRating(int pourIdx) {
    if (pourIdx < 0 || pourIdx >= NUM_GLASSES) return 0;
    return session.glassRating[pourIdx];
}

const LibraryEntry* gameGetGlassEntry(int pourIdx) {
    if (pourIdx < 0 || pourIdx >= NUM_GLASSES) return nullptr;
    return session.glassEntry[pourIdx];
}

// ============================================================
// Phone action handlers (Session 16)
// ============================================================

void gamePhoneSubmitName(const char* name) {
    if (!gameActive || state != GAME_NAMING) return;
    if (!name || !name[0]) return;

    // Copy name into session for the current pour
    strncpy(session.glassName[session.pourCount], name,
            MAX_GLASS_NAME - 1);
    session.glassName[session.pourCount][MAX_GLASS_NAME - 1] = '\0';

    Serial.printf("[Game] Phone named pour %d: %s\n",
                  session.pourCount + 1,
                  session.glassName[session.pourCount]);

    // Clean up browse screen if it's pending or active.
    // This runs from the WebSocket handler (main loop context),
    // so uiPopScreen is safe to call here.
    if (pendingBrowse) {
        pendingBrowse = false;
    } else if (uiActiveScreen() == &screenBrowse) {
        uiPopScreen();  // pop browse, game screen becomes active
    }
    // else: phone-started flight, no browse was ever pushed

    // Signal gameDraw to run the pour cycle on next redraw
    phoneNameReady = true;
    uiRequestRedraw();
}

void gamePhoneGuessSelect(int poolIndex) {
    phonePendingGuess = poolIndex;
    uiRequestRedraw();
}

void gameStartFromPhone(GameMode mode) {
    if (gameActive) return;  // already in a game

    gameSetMode(mode);

    if (mode == GAME_MODE_NAMED || mode == GAME_MODE_GUESS
        || mode == GAME_MODE_RANK) {
        gameActive = true;
        pendingBrowse = false;
        awaitingBrowseReturn = false;
        resetSession();
        state = GAME_NAMING;
        Serial.printf("[Game] Phone starting %s\n",
                      mode == GAME_MODE_GUESS ? "Best Guess" :
                      mode == GAME_MODE_RANK  ? "Ranked Flight" : "Named Flight");
        uiPushScreenT(&screenGame, TRANS_FADE);
    } else {
        // Basic mode: defer blocking pour cycle to gameDraw
        phoneStartMode = mode;
        phonePendingStart = true;
        uiPushScreenT(&screenGame, TRANS_FADE);
    }
}

// ============================================================
// Session resume (called from persist module on boot)
// ============================================================

void gameResumeSession(GameMode mode, GameState resumeState,
                       int glassCount, const GameSession& saved) {
    currentMode = mode;
    gameActive = true;
    pendingBrowse = false;
    awaitingBrowseReturn = false;

    session = saved;
    session.glassCount = glassCount;

    // Determine resume state — land on the appropriate interactive screen.
    // Mid-pour interruptions resume at "next pour" since we can't know
    // if liquid landed for the interrupted pour.
    if (resumeState == GAME_TASTING || resumeState == GAME_RATING ||
        resumeState == GAME_GUESSING || resumeState == GAME_RANKING) {
        state = GAME_TASTING;
    } else if (session.pourCount >= session.glassCount) {
        state = GAME_TASTING;
    } else if (modeUsesNames()) {
        state = GAME_NAMING;
        pendingBrowse = true;
    } else {
        // Basic mode: trigger a pour cycle on the next gameDraw
        pendingResumePour = true;
    }

    Serial.printf("[Game] Resumed: mode=%d state=%d pours=%d/%d\n",
                  (int)currentMode, (int)state, session.pourCount, session.glassCount);
}
