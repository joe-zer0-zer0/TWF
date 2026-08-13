#include "party.h"
#include "game.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "roster.h"
#include "settings.h"
#include "battery.h"
#include "persist.h"
#include "wifi_portal.h"

#ifndef HEADLESS_BUILD
#include "browse.h"
#include "screens.h"
#else
extern bool runHomingSequence();
#endif

// ============================================================
// Blind Flight — Flight Club (Phase 1)
// ============================================================
// See party.h and docs/specs/flight_club_spec.md.
//
// Layout note: the engine, the aggregate, and the getters live in the
// always-compiled part of this file; only the drawing and the screen
// callbacks sit inside #ifndef HEADLESS_BUILD. That ordering is
// deliberate. h2h.cpp put its input handler inside the screen-only
// block, which is why Head-to-Head still cannot be played on a headless
// unit. Phase 3 adds phone actions on top of what is already shared
// here rather than having to move it first.
// ============================================================

// --- Module state ---
static bool         partyActive = false;
static PartyPhase   phase       = PARTY_SETUP;
static PartyScoring scoring     = PARTY_SCORE_RANK;

static PartyPlayer  players[MAX_PARTY_PLAYERS];
static int          playerCount   = 4;   // participants in this event
static int          currentPlayer = 0;   // whose flight is being poured

// --- Event-level session data (identical for every participant) ---
static int  partyGlassCount = 4;
static char bottleName[NUM_GLASSES][MAX_GLASS_NAME];
static int  bottleCount = 0;

// --- Per-flight pour state ---
static bool glassUsed[NUM_GLASSES];
static int  pourCount    = 0;
static int  currentGlass = 0;    // NEVER drawn during a pour — see C1 below
static int  collectIndex = 0;

// --- Host-proxy input round ---
static int  entryPlayer = 0;
static int  rankIndex   = 0;
static int  entryCursor = 0;     // which remaining glass is highlighted

// --- Reveal ---
static int  revealIndex  = 0;    // 0 = worst bottle, counts up toward best
static bool revealDetail = false;
static int  revealDetailPage = 0;
static bool revealExcluded = false;   // final page listing who was left out

// The device screen cannot hold eight participants at once, so the
// per-bottle breakdown pages rather than clipping.
#define PARTY_DETAIL_ROWS  6

// Aggregate result, one per bottle. rankSum rather than a mean, so the
// tiebreak compares integers — the mean is derived for display only.
struct PartyResult {
    uint8_t  bottle;
    uint16_t rankSum;
    uint8_t  voteAtRank[NUM_GLASSES];   // voteAtRank[r] = how many placed it r+1
    bool     unanimous;
};
static PartyResult results[NUM_GLASSES];
static int resultCount    = 0;
static int countedPlayers = 0;

// ============================================================
// Helpers
// ============================================================

// Same rationale as the copies in game.cpp and h2h.cpp: service the
// portal so a connected phone is not starved for the length of every
// pause. wifiPortalService(), never wifiPortalUpdate() — the latter
// pushes and pops screens and would corrupt the stack mid-operation.
static void delayWithAudio(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        audioUpdate();
        wifiPortalService();
        delay(1);
    }
}

static void flushInput() {
    inputUpdate();
    while (inputGetEvent() != INPUT_NONE) {}
}

// The engine drives the event into PARTY_COLLECTING from several places,
// and each one has to leave the host menu ready to draw. Defined with the
// screen code below; there is no list to prepare on a headless build.
#ifndef HEADLESS_BUILD
static void prepareHostMenu();
#else
static inline void prepareHostMenu() {}
#endif

static void savePartyState() {
    persistSaveParty((uint8_t)phase, (uint8_t)scoring, (uint8_t)partyGlassCount,
                     (uint8_t)playerCount, (uint8_t)currentPlayer,
                     bottleName, players);
}

static void resetPlayer(PartyPlayer& p) {
    p.name[0]   = '\0';
    p.token     = 0;
    p.clientNum = 0xFF;
    p.poured    = false;
    p.rankDone  = false;
    p.guessDone = false;
    for (int g = 0; g < NUM_GLASSES; g++) {
        p.bottleForGlass[g] = 0;
        p.rankOrder[g]      = 0;
        p.guessForGlass[g]  = -1;
    }
}

static void resetEvent() {
    playerCount   = 4;
    currentPlayer = 0;
    partyGlassCount = settingsGetGlassCount();
    if (partyGlassCount < 2) partyGlassCount = 2;
    if (partyGlassCount > NUM_GLASSES) partyGlassCount = NUM_GLASSES;
    bottleCount   = 0;
    pourCount     = 0;
    currentGlass  = 0;
    collectIndex  = 0;
    entryPlayer   = 0;
    rankIndex     = 0;
    entryCursor   = 0;
    revealIndex   = 0;
    revealDetail  = false;
    revealDetailPage = 0;
    revealExcluded = false;
    resultCount   = 0;
    countedPlayers = 0;
    scoring       = PARTY_SCORE_RANK;

    for (int i = 0; i < NUM_GLASSES; i++) {
        bottleName[i][0] = '\0';
        glassUsed[i] = false;
    }
    for (int i = 0; i < MAX_PARTY_PLAYERS; i++) resetPlayer(players[i]);
}

// Start one participant's flight. Forces the once-per-flight home:
// glasses are loaded and the disc handled between participants, so a new
// flight never inherits the previous one's position.
static void beginFlight() {
    pourCount    = 0;
    currentGlass = 0;
    collectIndex = 0;
    for (int i = 0; i < NUM_GLASSES; i++) glassUsed[i] = false;
    motorInvalidatePosition();
}

static int selectRandomGlass() {
    int avail[NUM_GLASSES];
    int n = 0;
    for (int g = 0; g < partyGlassCount; g++) {
        if (!glassUsed[g]) avail[n++] = g + 1;
    }
    if (n == 0) return 0;
    int pick = avail[random(n)];
    glassUsed[pick - 1] = true;
    return pick;
}

// ============================================================
// Aggregate — rank scoring
// ============================================================
// Each participant ranked GLASSES. Their own bottleForGlass resolves
// that into a ranking of BOTTLES, and those are averaged across every
// participant who finished. Lower mean = better.
//
// Participants without a complete input round are excluded entirely
// rather than partially counted, and are named on the reveal so their
// absence reads as a decision and not a bug.
// ============================================================

// Ties are routine at 3-4 participants — two bottles at 2.5 is normal.
// Order: lower mean, then most 1st-place votes, then most 2nd, and so
// on, then bottle entry order. Deterministic and explainable out loud,
// which is what matters when someone contests the result at a party.
static bool resultBeats(const PartyResult& a, const PartyResult& b) {
    if (a.rankSum != b.rankSum) return a.rankSum < b.rankSum;
    for (int r = 0; r < NUM_GLASSES; r++) {
        if (a.voteAtRank[r] != b.voteAtRank[r]) return a.voteAtRank[r] > b.voteAtRank[r];
    }
    return a.bottle < b.bottle;
}

static void computeResults() {
    countedPlayers = 0;
    resultCount    = partyGlassCount;

    for (int b = 0; b < partyGlassCount; b++) {
        results[b].bottle    = (uint8_t)b;
        results[b].rankSum   = 0;
        results[b].unanimous = false;
        for (int r = 0; r < NUM_GLASSES; r++) results[b].voteAtRank[r] = 0;
    }

    for (int p = 0; p < playerCount; p++) {
        if (!players[p].rankDone) continue;
        countedPlayers++;
        for (int slot = 0; slot < partyGlassCount; slot++) {
            int g = players[p].rankOrder[slot];
            if (g < 1 || g > partyGlassCount) continue;
            int b = players[p].bottleForGlass[g - 1];
            if (b < 0 || b >= partyGlassCount) continue;
            results[b].rankSum += (uint16_t)(slot + 1);
            results[b].voteAtRank[slot]++;
        }
    }

    // A bottle every counted participant placed at the same rank.
    // Meaningless with a single participant, so it needs at least two.
    if (countedPlayers > 1) {
        for (int b = 0; b < partyGlassCount; b++) {
            for (int r = 0; r < partyGlassCount; r++) {
                if (results[b].voteAtRank[r] == countedPlayers) {
                    results[b].unanimous = true;
                    break;
                }
            }
        }
    }

    // Insertion sort, best first. resultCount is at most 4.
    for (int i = 1; i < resultCount; i++) {
        PartyResult key = results[i];
        int j = i - 1;
        while (j >= 0 && resultBeats(key, results[j])) {
            results[j + 1] = results[j];
            j--;
        }
        results[j + 1] = key;
    }
}

static int excludedCount() {
    int n = 0;
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].rankDone) n++;
    }
    return n;
}

// Reveal walks worst -> best, so page 0 is the last entry of a
// best-first array.
static const PartyResult& resultAtRevealPage(int page) {
    int idx = resultCount - 1 - page;
    if (idx >= resultCount) idx = resultCount - 1;
    if (idx < 0) idx = 0;
    return results[idx];
}

// ============================================================
// Blocking pour cycle
// ============================================================

static void runPartyPourCycle() {
    if (batteryIsLockout()) {
        Serial.println("[Party] Battery lockout — refusing pour");
#ifndef HEADLESS_BUILD
        gameShowLockoutScreen();
#else
        audioPlayTone(TONE_ERROR);
#endif
        motorDisable();
        // Bank whatever is already in the glasses rather than losing the
        // participant's flight: the pours that happened are still valid.
        phase = (pourCount > 0) ? PARTY_COLLECT : PARTY_NAME;
        collectIndex = 0;
        flushInput();
        savePartyState();
        uiRequestRedraw();
        wifiPortalBroadcastNow();
        return;
    }

    uiResetIdleTimer();

    if (!motorPositionIsVerified()) {
        runHomingSequence();
    }

    // Bottles are poured in entry order; the GLASS is what gets
    // randomized. That produces an independent permutation per
    // participant, which is what makes comparing notes across the room
    // leak nothing.
    currentGlass = selectRandomGlass();
    players[currentPlayer].bottleForGlass[currentGlass - 1] = (uint8_t)pourCount;

    Serial.printf("[Party] %s — pour %d/%d (bottle: %s)\n",
                  players[currentPlayer].name, pourCount + 1, partyGlassCount,
                  bottleName[pourCount]);

#ifndef HEADLESS_BUILD
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("FLIGHT CLUB", COL_ACCENT);

    char pourNum[4];
    snprintf(pourNum, sizeof(pourNum), "%d", pourCount + 1);
    uiDrawCenteredText(pourNum, CONTENT_Y + 60, FONT_XLARGE, COL_ACCENT);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 120, FONT_BODY, COL_TEXT);
    uiDrawSoftButtons("", "");
#endif

    audioPlayTone(TONE_SELECT);
    delayWithAudio(GAME_SELECT_PAUSE_MS);
    audioPlayTone(TONE_CONFIRM);
    delayWithAudio(GAME_SELECT_REVEAL_MS);

#ifndef HEADLESS_BUILD
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("SPINNING", COL_MOVING);
    uiDrawCenteredText(pourNum, CONTENT_Y + 45, FONT_XLARGE, COL_MOVING);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 105, FONT_BODY, COL_TEXT);
    uiDrawHint("Please wait...", CONTENT_Y + 135);
    uiDrawSoftButtons("", "");
#endif

    gameSetSpinning(true);
    wifiPortalBroadcastNow();
    motorSpinToGlass(currentGlass, motorGetExtraRevs());
    gameSetSpinning(false);
    wifiPortalBroadcastNow();

    audioPlayTone(TONE_ARRIVE);
    delayWithAudio(GAME_SPIN_SETTLE_MS);

    flushInput();
    phase = PARTY_POURING;
    uiRequestRedraw();
}

// The pour is confirmed; either queue the next one or move to collection.
static void confirmPour() {
    pourCount++;
    if (pourCount < partyGlassCount) {
        runPartyPourCycle();
        return;
    }

    // Every glass is filled. Release the disc HERE rather than waiting
    // ten minutes for the idle timeout to do it by accident: this is the
    // known-safe moment, the host is about to handle glasses, and holding
    // current through the swap gap is pure waste over a 40-minute event.
    motorDisable();
    collectIndex = 0;
    phase = PARTY_COLLECT;
    savePartyState();
    flushInput();
    uiRequestRedraw();
}

static void finishFlight() {
    players[currentPlayer].poured = true;
    currentPlayer++;

    if (currentPlayer >= playerCount) {
        prepareHostMenu();
        phase = PARTY_COLLECTING;
        Serial.println("[Party] All flights poured");
    } else {
        phase = PARTY_NAME;
    }
    savePartyState();
    flushInput();
    uiRequestRedraw();
}

// ============================================================
// Getters
// ============================================================

bool partyIsActive()          { return partyActive; }
PartyPhase partyGetPhase()    { return phase; }
int  partyGetGlassCount()     { return partyGlassCount; }
int  partyGetPlayerCount()    { return playerCount; }
int  partyGetCurrentPlayer()  { return currentPlayer; }
int  partyGetBottleCount()    { return bottleCount; }

const char* partyGetPlayerName(int idx) {
    if (idx < 0 || idx >= MAX_PARTY_PLAYERS) return "";
    return players[idx].name;
}

const char* partyGetBottleName(int idx) {
    if (idx < 0 || idx >= NUM_GLASSES) return "";
    return bottleName[idx];
}

void partyAbort() {
    if (!partyActive) return;
    Serial.println("[Party] Aborted");
    persistClearParty();
    partyActive = false;
    motorDisable();
}

#ifndef HEADLESS_BUILD

// ============================================================
// Screen state
// ============================================================

static ScrollList playerCountList;
static const char* playerCountLabels[MAX_PARTY_PLAYERS - 1] = {
    "2 People", "3 People", "4 People", "5 People",
    "6 People", "7 People", "8 People"
};

static ScrollList countList;
static const int COUNT_OPTIONS = 3;
static const char* countLabels[COUNT_OPTIONS] = { "2 Glasses", "3 Glasses", "4 Glasses" };

static ScrollList resumeList;
static const int RESUME_OPTIONS = 2;
static const char* resumeLabels[RESUME_OPTIONS] = { "Resume Event", "Start Over" };

// Name entry: roster names, then "Type a name..."
static ScrollList nameList;
static const char* nameItems[ROSTER_MAX_COUNT + 1];
static int         nameItemCount = 0;
static bool        nameTyping = false;
static TextEntry   nameEntry;

// Host menu during PARTY_COLLECTING
static ScrollList hostList;
static const char* hostLabels[3] = { "Enter Rankings", "Show Answers", "Skip To Reveal" };

static void prepareHostMenu() {
    uiScrollListInit(&hostList, hostLabels, 3);
}

// Participant picker, reused for both host-menu actions
static ScrollList pickList;
static const char* pickItems[MAX_PARTY_PLAYERS];
static int         pickCount = 0;
static bool        pickForAnswers = false;   // false = ranking entry
static bool        answersConfirm = false;   // gate before showing an answer key
static bool        showingAnswers = false;
static int         answersPlayer  = 0;

static bool awaitingBrowseReturn = false;

static void buildNameList() {
    nameItemCount = 0;
    for (int i = 0; i < rosterGetCount() && nameItemCount < ROSTER_MAX_COUNT; i++) {
        nameItems[nameItemCount++] = rosterGetName(i);
    }
    nameItems[nameItemCount++] = "Type a name...";
    uiScrollListInit(&nameList, nameItems, nameItemCount);
}

static void buildPickList() {
    pickCount = 0;
    for (int p = 0; p < playerCount; p++) {
        pickItems[pickCount++] = players[p].name;
    }
    uiScrollListInit(&pickList, pickItems, pickCount);
}

static void beginRankEntry(int playerIdx) {
    entryPlayer = playerIdx;
    rankIndex   = 0;
    entryCursor = 0;
    for (int i = 0; i < NUM_GLASSES; i++) players[entryPlayer].rankOrder[i] = 0;
    phase = PARTY_ENTRY;
}

// Glasses not yet placed in this participant's ranking.
static int rankPoolGlass(int poolIdx) {
    int n = 0;
    for (int g = 1; g <= partyGlassCount; g++) {
        bool taken = false;
        for (int s = 0; s < rankIndex; s++) {
            if (players[entryPlayer].rankOrder[s] == g) { taken = true; break; }
        }
        if (taken) continue;
        if (n == poolIdx) return g;
        n++;
    }
    return 0;
}

static int rankPoolCount() {
    return partyGlassCount - rankIndex;
}

// ============================================================
// Draw
// ============================================================

static void drawResume(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESUME EVENT?", COL_HOME);

    char buf[40];
    snprintf(buf, sizeof(buf), "%d of %d flights poured",
             persistGetPartyPouredCount(), persistGetPartyPlayerCount());
    uiDrawCenteredText(buf, CONTENT_Y + 8, FONT_SMALL, COL_DIM);

    uiScrollListDraw(&resumeList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawSetup(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("PARTICIPANTS", COL_ACCENT);
    uiScrollListDraw(&playerCountList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawCountSelect(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("GLASSES", COL_ACCENT);
    uiScrollListDraw(&countList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawBottleSelect(bool full) {
    // uiUpdate() calls draw(false) every loop iteration, so every draw
    // here has to bail out unless a full redraw was actually requested.
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("BOTTLES", COL_ACCENT);

    char buf[32];
    snprintf(buf, sizeof(buf), "Bottle %d of %d", bottleCount + 1, partyGlassCount);
    uiDrawCenteredText(buf, CONTENT_Y + 20, FONT_BODY, COL_TEXT);

    int y = CONTENT_Y + 60;
    for (int i = 0; i < bottleCount && i < partyGlassCount; i++) {
        uiDrawCenteredTextWrap(bottleName[i], y, FONT_SMALL, COL_DIM);
        y += 20;
    }

    uiDrawHint("Same bottles for everyone", 224);
    uiDrawSoftButtons(bottleCount > 0 ? "UNDO" : "BACK", "ADD");
}

static void drawName(bool full) {
    if (!full) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    // The grid repaints its own selection in place from
    // uiTextEntryHandleInput(), so it is only drawn on a full redraw.
    if (nameTyping) {
        uiDrawTitleBar("NAME", COL_ACCENT);
        uiTextEntryDraw(&nameEntry);
        return;
    }

    char title[24];
    snprintf(title, sizeof(title), "TASTER %d/%d", currentPlayer + 1, playerCount);
    uiDrawTitleBar(title, COL_ACCENT);
    uiScrollListDraw(&nameList);
    uiDrawSoftButtons("BACK", "SELECT");
}

// C1 — THE POUR VIEW MUST NEVER NAME A GLASS.
//
// The blind property depends entirely on glass identity being assigned
// at COLLECTION, not at pour. currentGlass is tracked here and must not
// be drawn, logged to the screen, or sent to a phone during this phase.
// A regression here silently destroys the mode while everything still
// appears to work.
static void drawPouring(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("POUR", COL_SELECTED);

    char buf[32];
    snprintf(buf, sizeof(buf), "Pour %d of %d", pourCount + 1, partyGlassCount);
    uiDrawCenteredText(buf, CONTENT_Y + 10, FONT_SMALL, COL_DIM);

    int y = uiDrawCenteredTextWrap(bottleName[pourCount], CONTENT_Y + 45,
                                  FONT_BODY, COL_ACCENT);

    uiDrawCenteredText(players[currentPlayer].name, y + 18, FONT_SMALL, COL_DIM);
    uiDrawHint("Pour into the glass", 210);
    uiDrawHint("at the spout", 226);
    uiDrawSoftButtons("NUDGE", "POURED");
}

static void drawCollect(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("COLLECT", COL_HOME);

    uiDrawCenteredText(players[currentPlayer].name, CONTENT_Y + 8, FONT_SMALL, COL_DIM);

    char buf[24];
    snprintf(buf, sizeof(buf), "%d", collectIndex + 1);
    uiDrawCenteredText(buf, CONTENT_Y + 50, FONT_XLARGE, COL_ACCENT);

    snprintf(buf, sizeof(buf), "Glass %d to tray %d", collectIndex + 1, collectIndex + 1);
    uiDrawCenteredText(buf, CONTENT_Y + 130, FONT_SMALL, COL_TEXT);

    uiDrawHint("Disc is released", 224);
    uiDrawSoftButtons("BACK", "NEXT");
}

static void drawCollecting(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("TASTING", COL_ACCENT);

    int done = 0;
    for (int p = 0; p < playerCount; p++) if (players[p].rankDone) done++;

    char buf[32];
    snprintf(buf, sizeof(buf), "%d of %d submitted", done, playerCount);
    uiDrawCenteredText(buf, CONTENT_Y + 8, FONT_SMALL, COL_DIM);

    uiScrollListDraw(&hostList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawPick(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar(pickForAnswers ? "WHOSE ANSWERS?" : "WHOSE RANKING?", COL_ACCENT);
    uiScrollListDraw(&pickList);
    uiDrawSoftButtons("BACK", "SELECT");
}

// Handing one person their answer key while everyone else is still
// tasting puts the bottles in the room. Gated, not blocked — it is a
// legitimate thing to want for someone who has to leave early.
static void drawAnswersConfirm(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("REVEAL ANSWERS", COL_ERROR);

    char buf[48];
    snprintf(buf, sizeof(buf), "Show %s the answers?", players[answersPlayer].name);
    uiDrawCenteredTextWrap(buf, CONTENT_Y + 30, FONT_BODY, COL_TEXT);
    uiDrawHint("Others are still tasting", 200);
    uiDrawSoftButtons("CANCEL", "SHOW");
}

static void drawAnswers(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar(players[answersPlayer].name, COL_SELECTED);

    int y = CONTENT_Y + 12;
    for (int g = 0; g < partyGlassCount; g++) {
        int b = players[answersPlayer].bottleForGlass[g];
        char buf[MAX_GLASS_NAME + 12];
        snprintf(buf, sizeof(buf), "%d: %s", g + 1,
                 (b >= 0 && b < partyGlassCount) ? bottleName[b] : "?");
        y = uiDrawCenteredTextWrap(buf, y, FONT_SMALL, COL_TEXT) + 8;
    }
    uiDrawSoftButtons("BACK", "");
}

static void drawEntry(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    char title[24];
    snprintf(title, sizeof(title), "RANK %d/%d", rankIndex + 1, partyGlassCount);
    uiDrawTitleBar(title, COL_ACCENT);

    uiDrawCenteredText(players[entryPlayer].name, CONTENT_Y + 6, FONT_SMALL, COL_DIM);

    const char* prompt = (rankIndex == 0) ? "Favourite glass?" : "Next favourite?";
    uiDrawCenteredText(prompt, CONTENT_Y + 30, FONT_SMALL, COL_TEXT);

    // Remaining glasses as tappable numbers; encoder picks, click confirms.
    int n = rankPoolCount();
    int boxW = 44, boxH = 44, gap = 10;
    int totalW = n * boxW + (n - 1) * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int y = CONTENT_Y + 70;

    for (int i = 0; i < n; i++) {
        int x = startX + i * (boxW + gap);
        bool on = (i == entryCursor);
        tft->fillRoundRect(x, y, boxW, boxH, 6, on ? COL_HIGHLIGHT : COL_DIM);
        tft->setTextColor(COL_TEXT, on ? COL_HIGHLIGHT : COL_DIM);
        tft->setTextSize(3);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(String(rankPoolGlass(i)), x + boxW / 2, y + boxH / 2);
    }

    uiDrawHint("Turn to choose, press to pick", 224);
    uiDrawSoftButtons("BACK", "PICK");
}

static void drawReveal(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    if (revealExcluded) {
        uiDrawTitleBar("NOT COUNTED", COL_ERROR);
        int y = CONTENT_Y + 20;
        for (int p = 0; p < playerCount; p++) {
            if (players[p].rankDone) continue;
            y = uiDrawCenteredTextWrap(players[p].name, y, FONT_BODY, COL_TEXT) + 6;
        }
        uiDrawHint("No ranking submitted", 214);
        uiDrawSoftButtons("BACK", "DONE");
        return;
    }

    const PartyResult& r = resultAtRevealPage(revealIndex);
    int placement = resultCount - revealIndex;   // page 0 is last place

    if (revealDetail) {
        uiDrawTitleBar("WHO HAD IT", COL_HOME);
        int y = CONTENT_Y + 6;
        y = uiDrawCenteredTextWrap(bottleName[r.bottle], y, FONT_SMALL, COL_ACCENT) + 8;

        int shown  = 0;
        int placed = 0;   // counted participants walked past so far
        int skip   = revealDetailPage * PARTY_DETAIL_ROWS;

        for (int p = 0; p < playerCount; p++) {
            if (!players[p].rankDone) continue;
            if (placed++ < skip) continue;
            if (shown >= PARTY_DETAIL_ROWS) break;

            // Which glass held this bottle for this participant, and
            // where they placed it.
            int glass = 0, place = 0;
            for (int g = 0; g < partyGlassCount; g++) {
                if (players[p].bottleForGlass[g] == r.bottle) { glass = g + 1; break; }
            }
            for (int s = 0; s < partyGlassCount; s++) {
                if (players[p].rankOrder[s] == glass) { place = s + 1; break; }
            }
            char buf[PARTY_NAME_LEN + 24];
            snprintf(buf, sizeof(buf), "%s  glass %d  #%d",
                     players[p].name, glass, place);
            uiDrawCenteredText(buf, y, FONT_SMALL, COL_TEXT);
            y += 18;
            shown++;
        }

        int pages = (countedPlayers + PARTY_DETAIL_ROWS - 1) / PARTY_DETAIL_ROWS;
        if (pages > 1) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d / %d", revealDetailPage + 1, pages);
            uiDrawHint(buf, 224);
        }
        uiDrawSoftButtons("BACK", pages > 1 ? "MORE" : "NEXT");
        return;
    }

    uiDrawTitleBar(placement == 1 ? "WINNER" : "RESULTS", COL_ACCENT);

    char buf[16];
    snprintf(buf, sizeof(buf), "#%d", placement);
    uiDrawCenteredText(buf, CONTENT_Y + 6, FONT_BODY, COL_DIM);

    int y = uiDrawCenteredTextWrap(bottleName[r.bottle], CONTENT_Y + 42,
                                  FONT_BODY, COL_ACCENT) + 10;

    if (countedPlayers > 0) {
        float mean = (float)r.rankSum / countedPlayers;
        snprintf(buf, sizeof(buf), "%.2f", mean);
        uiDrawCenteredText(buf, y, FONT_LARGE, COL_TEXT);
        uiDrawHint("mean rank", y + 42);
    }

    if (r.unanimous) {
        uiDrawCenteredText("Unanimous", 206, FONT_SMALL, COL_SELECTED);
    }

    uiDrawHint("Press knob for details", 224);
    uiDrawSoftButtons("BACK", "NEXT");
}

static void drawDone(bool full) {
    if (!full) return;
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("FLIGHT CLUB", COL_SELECTED);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d participants", countedPlayers);
    uiDrawCenteredText("Event complete", CONTENT_Y + 40, FONT_BODY, COL_TEXT);
    uiDrawCenteredText(buf, CONTENT_Y + 75, FONT_SMALL, COL_DIM);
    uiDrawSoftButtons("EXIT", "REPLAY");
}

// ============================================================
// Screen callbacks
// ============================================================

static void partyDraw(bool fullRedraw) {
    // Browse return, handled here rather than in onEnter — pushing or
    // popping screens from inside a transition corrupts the stack, so the
    // established pattern is a deferred flag picked up on the next draw.
    if (phase == PARTY_BOTTLES && awaitingBrowseReturn) {
        awaitingBrowseReturn = false;
        const char* name = browseGetResult();
        if (name) {
            strncpy(bottleName[bottleCount], name, MAX_GLASS_NAME - 1);
            bottleName[bottleCount][MAX_GLASS_NAME - 1] = '\0';
            Serial.printf("[Party] Bottle %d: %s\n", bottleCount + 1, bottleName[bottleCount]);
            bottleCount++;

            if (bottleCount >= partyGlassCount) {
                currentPlayer = 0;
                buildNameList();
                phase = PARTY_NAME;
                savePartyState();
            }
        } else if (bottleCount == 0) {
            phase = PARTY_COUNT;
        }
        uiRequestRedraw();
        return;
    }

    switch (phase) {
        case PARTY_RESUME:     drawResume(fullRedraw);     break;
        case PARTY_SETUP:      drawSetup(fullRedraw);      break;
        case PARTY_COUNT:      drawCountSelect(fullRedraw); break;
        case PARTY_BOTTLES:    drawBottleSelect(fullRedraw); break;
        case PARTY_NAME:       drawName(fullRedraw);       break;
        case PARTY_POURING:    drawPouring(fullRedraw);    break;
        case PARTY_COLLECT:    drawCollect(fullRedraw);    break;
        case PARTY_COLLECTING:
            if (showingAnswers)      drawAnswers(fullRedraw);
            else if (answersConfirm) drawAnswersConfirm(fullRedraw);
            else if (pickCount > 0)  drawPick(fullRedraw);
            else                     drawCollecting(fullRedraw);
            break;
        case PARTY_ENTRY:      drawEntry(fullRedraw);      break;
        case PARTY_REVEAL:     drawReveal(fullRedraw);     break;
        case PARTY_DONE:       drawDone(fullRedraw);       break;
    }
}

static void enterReveal() {
    computeResults();
    revealIndex      = 0;
    revealDetail     = false;
    revealDetailPage = 0;
    revealExcluded   = false;
    phase = PARTY_REVEAL;
    uiRequestRedraw();
}

static void partyInput(InputEvent evt) {
    switch (phase) {

        case PARTY_RESUME: {
            int sel = uiScrollListHandleInput(&resumeList, evt);
            if (sel == 0) {
                audioPlayTone(TONE_CONFIRM);
                uint8_t ph, sc, gc, pc, cp;
                if (persistLoadParty(ph, sc, gc, pc, cp, bottleName, players)) {
                    phase           = (PartyPhase)ph;
                    scoring         = (PartyScoring)sc;
                    partyGlassCount = gc;
                    playerCount     = pc;
                    currentPlayer   = cp;
                    bottleCount     = partyGlassCount;

                    // Only three phases are safe to land on. Anything else
                    // is either mid-pour or an input round whose cursor
                    // state was never persisted, so it maps back to the
                    // nearest point that can be re-entered cleanly.
                    switch (phase) {
                        case PARTY_POURING:
                        case PARTY_COLLECT:
                            // A flight was interrupted partway. Re-pour it
                            // rather than trusting a half-filled set of
                            // glasses whose contents nobody recorded.
                            beginFlight();
                            phase = PARTY_NAME;
                            break;
                        case PARTY_NAME:
                            beginFlight();
                            break;
                        case PARTY_COLLECTING:
                            break;
                        default:
                            phase = (currentPlayer >= playerCount)
                                    ? PARTY_COLLECTING : PARTY_NAME;
                            if (phase == PARTY_NAME) beginFlight();
                            break;
                    }

                    if (phase == PARTY_NAME) {
                        buildNameList();
                    } else {
                        pickCount = 0;
                        prepareHostMenu();
                    }
                    Serial.printf("[Party] Resumed at phase %d, participant %d/%d\n",
                                  (int)phase, currentPlayer + 1, playerCount);
                } else {
                    resetEvent();
                    phase = PARTY_SETUP;
                }
                uiRequestRedraw();
            } else if (sel == 1) {
                audioPlayTone(TONE_CONFIRM);
                persistClearParty();
                resetEvent();
                uiScrollListInit(&playerCountList, playerCountLabels, MAX_PARTY_PLAYERS - 1);
                phase = PARTY_SETUP;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                partyActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;
        }

        case PARTY_SETUP: {
            int sel = uiScrollListHandleInput(&playerCountList, evt);
            if (sel >= 0) {
                audioPlayTone(TONE_CONFIRM);
                playerCount = sel + 2;
                uiScrollListInit(&countList, countLabels, COUNT_OPTIONS);
                phase = PARTY_COUNT;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                partyActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;
        }

        case PARTY_COUNT: {
            int sel = uiScrollListHandleInput(&countList, evt);
            if (sel >= 0) {
                audioPlayTone(TONE_CONFIRM);
                partyGlassCount = sel + 2;
                bottleCount = 0;
                phase = PARTY_BOTTLES;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                phase = PARTY_SETUP;
                uiRequestRedraw();
            }
            break;
        }

        case PARTY_BOTTLES:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                awaitingBrowseReturn = true;
                browseReset();
                uiPushScreen(&screenBrowse);
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (bottleCount > 0) {
                    bottleCount--;
                    bottleName[bottleCount][0] = '\0';
                } else {
                    phase = PARTY_COUNT;
                }
                uiRequestRedraw();
            }
            break;

        case PARTY_NAME: {
            if (nameTyping) {
                int r = uiTextEntryHandleInput(&nameEntry, evt);
                if (r == 1) {
                    const char* txt = uiTextEntryGetText(&nameEntry);
                    if (txt[0]) {
                        strncpy(players[currentPlayer].name, txt, PARTY_NAME_LEN - 1);
                        players[currentPlayer].name[PARTY_NAME_LEN - 1] = '\0';
                        rosterAdd(players[currentPlayer].name);
                        nameTyping = false;
                        audioPlayTone(TONE_CONFIRM);
                        beginFlight();
                        savePartyState();
                        runPartyPourCycle();
                    }
                } else if (r == -1) {
                    nameTyping = false;
                    buildNameList();
                    uiRequestRedraw();
                }
                break;
            }

            int sel = uiScrollListHandleInput(&nameList, evt);
            if (sel >= 0) {
                audioPlayTone(TONE_CONFIRM);
                if (sel == nameItemCount - 1) {
                    nameTyping = true;
                    uiTextEntryInit(&nameEntry, PARTY_NAME_LEN - 1, "");
                    uiRequestRedraw();
                } else {
                    strncpy(players[currentPlayer].name, nameItems[sel], PARTY_NAME_LEN - 1);
                    players[currentPlayer].name[PARTY_NAME_LEN - 1] = '\0';
                    beginFlight();
                    savePartyState();
                    runPartyPourCycle();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                // Only the first participant can back out into setup;
                // after that the event is under way.
                if (currentPlayer == 0) {
                    phase = PARTY_BOTTLES;
                    uiRequestRedraw();
                }
            }
            break;
        }

        case PARTY_POURING:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                confirmPour();
            } else if (evt == INPUT_BTN_LEFT) {
                // Nudge the glass under the spout without disturbing the
                // tracked position beyond the nudge itself.
                motorSpinSteps(NUDGE_STEPS);
            }
            break;

        case PARTY_COLLECT:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                collectIndex++;
                if (collectIndex >= partyGlassCount) {
                    audioPlayTone(TONE_CONFIRM);
                    finishFlight();
                } else {
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_LEFT && collectIndex > 0) {
                audioPlayTone(TONE_SELECT);
                collectIndex--;
                uiRequestRedraw();
            }
            break;

        case PARTY_COLLECTING: {
            if (showingAnswers) {
                if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
                    audioPlayTone(TONE_SELECT);
                    showingAnswers = false;
                    pickCount = 0;
                    uiRequestRedraw();
                }
                break;
            }

            if (answersConfirm) {
                if (evt == INPUT_BTN_RIGHT) {
                    audioPlayTone(TONE_CONFIRM);
                    answersConfirm = false;
                    showingAnswers = true;
                } else if (evt == INPUT_BTN_LEFT) {
                    audioPlayTone(TONE_SELECT);
                    answersConfirm = false;
                    pickCount = 0;
                }
                uiRequestRedraw();
                break;
            }

            if (pickCount > 0) {
                int sel = uiScrollListHandleInput(&pickList, evt);
                if (sel >= 0) {
                    audioPlayTone(TONE_CONFIRM);
                    if (pickForAnswers) {
                        answersPlayer  = sel;
                        answersConfirm = true;
                    } else {
                        pickCount = 0;
                        beginRankEntry(sel);
                    }
                    uiRequestRedraw();
                } else if (evt == INPUT_BTN_LEFT) {
                    audioPlayTone(TONE_SELECT);
                    pickCount = 0;
                    uiRequestRedraw();
                }
                break;
            }

            int sel = uiScrollListHandleInput(&hostList, evt);
            if (sel == 0) {
                audioPlayTone(TONE_CONFIRM);
                pickForAnswers = false;
                buildPickList();
                uiRequestRedraw();
            } else if (sel == 1) {
                audioPlayTone(TONE_CONFIRM);
                pickForAnswers = true;
                buildPickList();
                uiRequestRedraw();
            } else if (sel == 2) {
                audioPlayTone(TONE_CONFIRM);
                enterReveal();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                // No accidental exit from a live event.
            }
            break;
        }

        case PARTY_ENTRY: {
            int n = rankPoolCount();
            if (evt == INPUT_ENC_CW) {
                if (entryCursor < n - 1) entryCursor++;
                uiRequestRedraw();
            } else if (evt == INPUT_ENC_CCW) {
                if (entryCursor > 0) entryCursor--;
                uiRequestRedraw();
            } else if (evt == INPUT_ENC_CLICK || evt == INPUT_BTN_RIGHT) {
                int glass = rankPoolGlass(entryCursor);
                if (glass > 0) {
                    audioPlayTone(TONE_CONFIRM);
                    players[entryPlayer].rankOrder[rankIndex] = (uint8_t)glass;
                    rankIndex++;
                    entryCursor = 0;

                    if (rankIndex >= partyGlassCount) {
                        players[entryPlayer].rankDone = true;
                        pickCount = 0;
                        prepareHostMenu();
                        // Phase first, THEN save: the blob records where to
                        // resume, and PARTY_ENTRY is not a resumable point
                        // (entryPlayer and rankIndex are not persisted).
                        phase = PARTY_COLLECTING;
                        savePartyState();

                        // Everyone in — go straight to the reveal rather
                        // than making the host find the menu item.
                        bool allDone = true;
                        for (int p = 0; p < playerCount; p++) {
                            if (!players[p].rankDone) { allDone = false; break; }
                        }
                        if (allDone) enterReveal();
                    }
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (rankIndex > 0) {
                    rankIndex--;
                    players[entryPlayer].rankOrder[rankIndex] = 0;
                    entryCursor = 0;
                } else {
                    pickCount = 0;
                    prepareHostMenu();
                    phase = PARTY_COLLECTING;
                }
                uiRequestRedraw();
            }
            break;
        }

        case PARTY_REVEAL:
            if (evt == INPUT_ENC_CLICK && !revealExcluded) {
                audioPlayTone(TONE_SELECT);
                revealDetail = !revealDetail;
                revealDetailPage = 0;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_RIGHT) {
                audioPlayTone(TONE_SELECT);

                // Inside the breakdown, RIGHT pages through participants
                // and only falls through to the next bottle once the last
                // page has been seen.
                if (revealDetail) {
                    int pages = (countedPlayers + PARTY_DETAIL_ROWS - 1) / PARTY_DETAIL_ROWS;
                    if (revealDetailPage < pages - 1) {
                        revealDetailPage++;
                        uiRequestRedraw();
                        break;
                    }
                }

                revealDetail = false;
                revealDetailPage = 0;
                if (revealExcluded) {
                    phase = PARTY_DONE;
                } else if (revealIndex < resultCount - 1) {
                    revealIndex++;
                } else if (excludedCount() > 0) {
                    revealExcluded = true;
                } else {
                    phase = PARTY_DONE;
                }
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (revealDetail && revealDetailPage > 0) {
                    revealDetailPage--;
                } else if (revealDetail) {
                    revealDetail = false;
                } else if (revealExcluded) {
                    revealExcluded = false;
                } else if (revealIndex > 0) {
                    revealIndex--;
                }
                uiRequestRedraw();
            }
            break;

        case PARTY_DONE:
            if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                persistClearParty();
                partyActive = false;
                motorDisable();
                uiPopScreenT(TRANS_FADE);
            } else if (evt == INPUT_BTN_RIGHT) {
                audioPlayTone(TONE_CONFIRM);
                persistClearParty();
                resetEvent();
                uiScrollListInit(&playerCountList, playerCountLabels, MAX_PARTY_PLAYERS - 1);
                phase = PARTY_SETUP;
                uiRequestRedraw();
            }
            break;
    }
}

static void partyOnEnter() {
    if (partyActive) {
        Serial.println("[Party] onEnter (re-entry — skipping init)");
        return;
    }

    partyActive = true;
    awaitingBrowseReturn = false;
    nameTyping     = false;
    showingAnswers = false;
    answersConfirm = false;
    pickCount      = 0;
    resetEvent();

    if (persistHasParty()) {
        uiScrollListInit(&resumeList, resumeLabels, RESUME_OPTIONS);
        phase = PARTY_RESUME;
    } else {
        uiScrollListInit(&playerCountList, playerCountLabels, MAX_PARTY_PLAYERS - 1);
        phase = PARTY_SETUP;
    }

    Serial.println("[Party] Entered Flight Club");
    uiRequestRedraw();
}

const Screen screenParty = {
    "Party",
    partyDraw,
    partyInput,
    partyOnEnter
};

#endif // !HEADLESS_BUILD
