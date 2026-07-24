#include "h2h.h"
#include "game.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "wifi_portal.h"
#include "settings.h"
#include "battery.h"

#ifndef HEADLESS_BUILD
#include "browse.h"
#include "screens.h"
#else
extern bool runHomingSequence();
#endif

// ============================================================
// Blind Flight — Head-to-Head Module (Session 23)
// ============================================================

// --- Module state ---
static bool       h2hActive = false;
static H2HPhase   phase     = H2H_MODE_SELECT;
static H2HSubMode subMode   = H2H_SUB_2X2;

// --- Player registry ---
static H2HPlayer  players[H2H_MAX_PLAYERS];
static int         playerCount = 0;

// --- Session data (shared across sub-modes) ---
static char    h2hBottles[NUM_GLASSES][MAX_GLASS_NAME]; // up to 4 bottles
static int     bottleCount = 0;
static int     h2hGlassCount = 4;                       // 2-4, set during count select

static int     pourOrder[NUM_GLASSES];           // pourOrder[pourIdx] = glass# (1-based)
static int     bottleForGlass[NUM_GLASSES];      // bottleForGlass[glass_0based] = bottle index
static int     pourCount = 0;
static int     currentGlass = 0;
static bool    glassUsed[NUM_GLASSES];

// --- 2x2 constants ---
#define H2H_2X2_BOTTLES  2
#define H2H_2X2_GLASSES  4
#define H2H_2X2_POURS    4

// Reveal map (glass-number order)
struct H2HRevealEntry { int glass; int pourIdx; };
static H2HRevealEntry revealMap[NUM_GLASSES];
static int revealMapCount = 0;
static int revealIndex = 0;

// --- Browse integration ---
static bool awaitingBrowseReturn = false;
static bool homedThisFlight = false;

// --- Sub-mode selector ---
static ScrollList subModeList;
static const int SUB_MODE_COUNT = 3;
static const char* subModeLabels[SUB_MODE_COUNT] = {
    "2x2",
    "Multiplayer Random",
    "Premium Pour"
};

// --- Glass count selector (Random/Premium) ---
static ScrollList countList;
static const int COUNT_OPTIONS = 3;
static const char* countLabels[COUNT_OPTIONS] = { "2 Glasses", "3 Glasses", "4 Glasses" };

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

static void resetSession() {
    playerCount = 0;
    bottleCount = 0;
    pourCount   = 0;
    currentGlass = 0;
    revealMapCount = 0;
    revealIndex = 0;
    homedThisFlight = false;
    for (int i = 0; i < H2H_MAX_PLAYERS; i++) {
        players[i].name[0] = '\0';
        players[i].connected = false;
        players[i].submitted = false;
        players[i].correct = false;
        players[i].clientNum = 0;
        players[i].guess2x2[0] = -1;
        players[i].guess2x2[1] = -1;
        players[i].assignedGlasses[0] = 0;
        players[i].assignedGlasses[1] = 0;
        players[i].correctPerGlass[0] = false;
        players[i].correctPerGlass[1] = false;
        players[i].claimedGlass = 0;
        players[i].guessBottleIdx = -1;
        players[i].guessPremium = false;
        players[i].guessedPremium = false;
    }
    for (int i = 0; i < NUM_GLASSES; i++) {
        h2hBottles[i][0] = '\0';
        pourOrder[i] = 0;
        bottleForGlass[i] = -1;
        glassUsed[i] = false;
    }
}

// Build pour order: A, A, B, B into random glass positions
static void build2x2PourOrder() {
    int bottleSeq[4] = {0, 0, 1, 1};

    // Shuffle bottle assignment
    for (int i = 3; i > 0; i--) {
        int j = random(i + 1);
        int tmp = bottleSeq[i]; bottleSeq[i] = bottleSeq[j]; bottleSeq[j] = tmp;
    }

    // Shuffle glass positions
    int glassPool[4] = {1, 2, 3, 4};
    for (int i = 3; i > 0; i--) {
        int j = random(i + 1);
        int tmp = glassPool[i]; glassPool[i] = glassPool[j]; glassPool[j] = tmp;
    }

    for (int i = 0; i < 4; i++) {
        pourOrder[i] = glassPool[i];
        bottleForGlass[glassPool[i] - 1] = bottleSeq[i];
    }
}

// Assign glasses so each player gets one of each bottle
static void assignGlasses() {
    int bottleAGlasses[2];
    int bottleBGlasses[2];
    int aCount = 0, bCount = 0;

    for (int g = 0; g < NUM_GLASSES; g++) {
        if (bottleForGlass[g] == 0) bottleAGlasses[aCount++] = g + 1;
        else                        bottleBGlasses[bCount++] = g + 1;
    }

    // Randomly decide which A-glass goes to which player
    if (random(2) == 0) {
        players[0].assignedGlasses[0] = bottleAGlasses[0];
        players[1].assignedGlasses[0] = bottleAGlasses[1];
    } else {
        players[0].assignedGlasses[0] = bottleAGlasses[1];
        players[1].assignedGlasses[0] = bottleAGlasses[0];
    }

    // The B-glasses go to the other player
    if (random(2) == 0) {
        players[0].assignedGlasses[1] = bottleBGlasses[0];
        players[1].assignedGlasses[1] = bottleBGlasses[1];
    } else {
        players[0].assignedGlasses[1] = bottleBGlasses[1];
        players[1].assignedGlasses[1] = bottleBGlasses[0];
    }
}

// Score 2x2 guesses
static void score2x2() {
    for (int p = 0; p < 2; p++) {
        if (!players[p].submitted) continue;
        for (int g = 0; g < 2; g++) {
            int glassNum = players[p].assignedGlasses[g];
            int actualBottle = bottleForGlass[glassNum - 1];
            players[p].correctPerGlass[g] = (players[p].guess2x2[g] == actualBottle);
        }
    }
}

// Build Random pour order: one bottle per glass, shuffled positions
static void buildRandomPourOrder() {
    int glassPool[NUM_GLASSES];
    for (int i = 0; i < h2hGlassCount; i++) glassPool[i] = i + 1;

    // Shuffle glass positions
    for (int i = h2hGlassCount - 1; i > 0; i--) {
        int j = random(i + 1);
        int tmp = glassPool[i]; glassPool[i] = glassPool[j]; glassPool[j] = tmp;
    }

    for (int i = 0; i < h2hGlassCount; i++) {
        pourOrder[i] = glassPool[i];
        bottleForGlass[glassPool[i] - 1] = i; // bottle i goes to shuffled glass
    }
}

// Select a random unused glass (for Random mode pour cycle)
static int h2hSelectRandomGlass() {
    int available[NUM_GLASSES];
    int count = 0;
    for (int i = 0; i < h2hGlassCount; i++) {
        if (!glassUsed[i]) available[count++] = i + 1;
    }
    if (count == 0) return 0;
    return available[random(count)];
}

// Score Multiplayer Random guesses
static void scoreRandom() {
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].submitted) continue;
        int glass = players[p].claimedGlass;
        if (glass < 1 || glass > NUM_GLASSES) continue;
        int actualBottle = bottleForGlass[glass - 1];
        players[p].correct = (players[p].guessBottleIdx == actualBottle);
    }
}

// Build Premium pour order: premium bottle (index 0) goes first into a random glass,
// remaining bottles go into remaining glasses in random order
static void buildPremiumPourOrder() {
    int glassPool[NUM_GLASSES];
    for (int i = 0; i < h2hGlassCount; i++) glassPool[i] = i + 1;

    // Shuffle glass positions
    for (int i = h2hGlassCount - 1; i > 0; i--) {
        int j = random(i + 1);
        int tmp = glassPool[i]; glassPool[i] = glassPool[j]; glassPool[j] = tmp;
    }

    // Premium (bottle 0) goes to first shuffled position
    pourOrder[0] = glassPool[0];
    bottleForGlass[glassPool[0] - 1] = 0;

    // Remaining bottles fill remaining glasses
    for (int i = 1; i < h2hGlassCount; i++) {
        pourOrder[i] = glassPool[i];
        bottleForGlass[glassPool[i] - 1] = i;
    }
}

// Score Premium Pour guesses
static void scorePremium() {
    int premiumGlass = 0;
    for (int g = 0; g < NUM_GLASSES; g++) {
        if (bottleForGlass[g] == 0) { premiumGlass = g + 1; break; }
    }
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].submitted) continue;
        bool hasPremium = (players[p].claimedGlass == premiumGlass);
        players[p].correct = (players[p].guessPremium == hasPremium);
    }
}

// Build reveal map (glass-number order)
static void buildRevealMap() {
    revealMapCount = 0;
    for (int g = 1; g <= NUM_GLASSES; g++) {
        if (glassUsed[g - 1]) {
            for (int p = 0; p < pourCount; p++) {
                if (pourOrder[p] == g) {
                    revealMap[revealMapCount].glass = g;
                    revealMap[revealMapCount].pourIdx = p;
                    revealMapCount++;
                    break;
                }
            }
        }
    }
}

// ============================================================
// Pour cycle (blocking motor spin, same pattern as game.cpp)
// ============================================================

static void runH2HPourCycle() {
#ifndef HEADLESS_BUILD
    if (batteryIsLockout()) {
        Serial.println("[H2H] Battery lockout during pour");
        return;
    }
#endif

    uiResetIdleTimer();

    if (!homedThisFlight) {
        runHomingSequence();
        homedThisFlight = true;
    }

    if (subMode == H2H_SUB_2X2 || subMode == H2H_SUB_PREMIUM) {
        currentGlass = pourOrder[pourCount];
    } else {
        currentGlass = h2hSelectRandomGlass();
        pourOrder[pourCount] = currentGlass;
        bottleForGlass[currentGlass - 1] = pourCount;
    }

    int bottleIdx = bottleForGlass[currentGlass - 1];
    Serial.printf("[H2H] Pour %d/%d — glass %d (bottle: %s)\n",
                  pourCount + 1, h2hGlassCount, currentGlass,
                  h2hBottles[bottleIdx]);

#ifndef HEADLESS_BUILD
    TFT_eSPI* tft = uiGetTFT();

    char pourNum[4];
    snprintf(pourNum, sizeof(pourNum), "%d", pourCount + 1);

    tft->fillScreen(COL_BG);
    uiDrawTitleBar("HEAD TO HEAD", COL_ACCENT);
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

    int extraRevs = motorGetExtraRevs();
    wifiPortalBroadcastNow();
    motorSpinToGlass(currentGlass, extraRevs);
    wifiPortalBroadcastNow();

    audioPlayTone(TONE_ARRIVE);
    delayWithAudio(GAME_SPIN_SETTLE_MS);

    flushInput();
    phase = H2H_POURING;
    uiRequestRedraw();
}

static void runFinalSpin() {
    uiResetIdleTimer();

#ifndef HEADLESS_BUILD
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("SPINNING", COL_MOVING);
    uiDrawCenteredText("Randomizing", CONTENT_Y + 70, FONT_BODY, COL_TEXT);
    uiDrawHint("Please wait...", CONTENT_Y + 100);
    uiDrawSoftButtons("", "");
#endif

    delayWithAudio(400);
    int steps = 3 * MICROSTEPS_PER_REV + random(MICROSTEPS_PER_REV);

    wifiPortalBroadcastNow();
    motorSpinSteps(steps);
    wifiPortalBroadcastNow();

    audioPlayTone(TONE_ARRIVE);
    delayWithAudio(GAME_SPIN_SETTLE_MS);
    motorDisable();
    flushInput();
}

// ============================================================
// Draw functions (screen build only)
// ============================================================

#ifndef HEADLESS_BUILD

static void drawModeSelect(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("HEAD TO HEAD", COL_ACCENT);
    uiScrollListDraw(&subModeList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawCountSelect(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("HOW MANY?", COL_ACCENT);
    uiScrollListDraw(&countList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void drawBottleSelect(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    int targetCount = (subMode == H2H_SUB_2X2) ? H2H_2X2_BOTTLES : h2hGlassCount;
    const char* title = (subMode == H2H_SUB_2X2) ? "2x2 SETUP" :
                        (subMode == H2H_SUB_PREMIUM) ? "PREMIUM SETUP" : "RANDOM SETUP";
    uiDrawTitleBar(title, COL_ACCENT);

    char progBuf[28];
    if (subMode == H2H_SUB_PREMIUM && bottleCount == 0) {
        snprintf(progBuf, sizeof(progBuf), "PREMIUM BOTTLE");
    } else if (subMode == H2H_SUB_PREMIUM) {
        snprintf(progBuf, sizeof(progBuf), "Standard %d of %d",
                 bottleCount, targetCount - 1);
    } else {
        snprintf(progBuf, sizeof(progBuf), "Select Bottle %d of %d",
                 bottleCount + 1, targetCount);
    }
    uiDrawCenteredText(progBuf, CONTENT_Y + 20, FONT_BODY, COL_TEXT);

    int y = CONTENT_Y + 60;
    for (int i = 0; i < bottleCount; i++) {
        char label[30];
        snprintf(label, sizeof(label), "%d: %s", i + 1, h2hBottles[i]);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(label, 16, y);
        y += 16;
    }

    uiDrawHint("Press to browse library", CONTENT_Y + 120);
    uiDrawSoftButtons("BACK", "BROWSE");
}

static void drawLobby(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    int required = h2hGetRequiredPlayers();
    const char* lobbyTitle = (subMode == H2H_SUB_2X2) ? "2x2 LOBBY" :
                             (subMode == H2H_SUB_PREMIUM) ? "PREMIUM LOBBY" : "RANDOM LOBBY";
    uiDrawTitleBar(lobbyTitle, COL_ACCENT);

    uiDrawCenteredText("Waiting for", CONTENT_Y + 10, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("players...", CONTENT_Y + 30, FONT_BODY, COL_TEXT);

    int y = CONTENT_Y + 65;
    int connectedCount = 0;
    for (int i = 0; i < playerCount; i++) {
        if (players[i].connected) connectedCount++;
        uint16_t col = players[i].connected ? COL_SELECTED : COL_ERROR;
        char label[32];
        snprintf(label, sizeof(label), "%d. %s%s",
                 i + 1, players[i].name,
                 players[i].connected ? "" : " (disconnected)");
        tft->setTextColor(col, COL_BG);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(label, 16, y);
        y += 28;
    }

    if (playerCount < required) {
        char waitBuf[32];
        snprintf(waitBuf, sizeof(waitBuf), "%d of %d joined",
                 connectedCount, required);
        uiDrawCenteredText(waitBuf, CONTENT_Y + 140, FONT_BODY, COL_DIM);
    }

    uiDrawHint("Connect phones to BlindFlight Wi-Fi", CONTENT_Y + 170);

    bool canStart = (connectedCount >= required);
    uiDrawSoftButtons("BACK", canStart ? "START" : "");
}

static void drawPouring(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("POUR", COL_SELECTED);

    int bottleIdx = bottleForGlass[currentGlass - 1];

    char pourCounter[16];
    snprintf(pourCounter, sizeof(pourCounter), "Pour %d of %d",
             pourCount + 1, h2hGlassCount);
    uiDrawCenteredText(pourCounter, CONTENT_Y + 15, FONT_BODY, COL_DIM);

    if (subMode == H2H_SUB_PREMIUM && bottleIdx == 0) {
        uiDrawCenteredText("PREMIUM", CONTENT_Y + 40, FONT_SMALL, COL_SELECTED);
    }
    uiDrawCenteredTextWrap(h2hBottles[bottleIdx], CONTENT_Y + 55, FONT_BODY, COL_ACCENT);
    uiDrawCenteredText("Ready To Pour", CONTENT_Y + 100, FONT_BODY, COL_TEXT);

    uiDrawHint("DONE When Poured", CONTENT_Y + 135);
    uiDrawSoftButtons("", "DONE");
}

static void drawTasting(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("TASTING TIME", COL_ACCENT);

    char countBuf[16];
    snprintf(countBuf, sizeof(countBuf), "%d", h2hGlassCount);
    uiDrawCenteredText(countBuf, CONTENT_Y + 30, FONT_XLARGE, COL_ACCENT);
    uiDrawCenteredText("Glasses Poured", CONTENT_Y + 80, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("Take one glass each", CONTENT_Y + 110, FONT_BODY, COL_DIM);

    uiDrawHint("Press when ready to guess", CONTENT_Y + 160);
    uiDrawSoftButtons("", "GUESS");
}

static void drawAssign(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("TAKE YOUR GLASSES", COL_ACCENT);

    int y = CONTENT_Y + 20;
    for (int p = 0; p < 2; p++) {
        char label[48];
        snprintf(label, sizeof(label), "%s: Glass %d & %d",
                 players[p].name,
                 players[p].assignedGlasses[0],
                 players[p].assignedGlasses[1]);

        tft->setTextColor(COL_TEXT, COL_BG);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(label, 16, y);
        y += 32;
    }

    // Glass position boxes
    const int boxW = 44, boxH = 44, gap = 10;
    int totalW = 4 * boxW + 3 * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int boxY = CONTENT_Y + 100;

    for (int i = 0; i < 4; i++) {
        int x = startX + i * (boxW + gap);
        int glassNum = i + 1;

        // Color by player assignment
        uint16_t boxCol = COL_DIM;
        const char* playerLabel = "";
        for (int p = 0; p < 2; p++) {
            if (players[p].assignedGlasses[0] == glassNum ||
                players[p].assignedGlasses[1] == glassNum) {
                boxCol = (p == 0) ? COL_ACCENT : COL_HOME;
                playerLabel = (p == 0) ? "P1" : "P2";
            }
        }

        tft->fillRoundRect(x, boxY, boxW, boxH, 6, boxCol);
        tft->setTextColor(COL_BG, boxCol);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(String(glassNum), x + boxW / 2, boxY + 14);
        tft->setTextSize(FONT_SMALL);
        tft->drawString(playerLabel, x + boxW / 2, boxY + 33);
    }

    uiDrawHint("Press when ready", CONTENT_Y + 165);
    uiDrawSoftButtons("", "NEXT");
}

static void drawWaiting(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("WAITING", COL_ACCENT);

    int required = h2hGetRequiredPlayers();
    int submitted = h2hGetSubmittedCount();
    char waitBuf[32];
    snprintf(waitBuf, sizeof(waitBuf), "%d of %d", submitted, required);
    uiDrawCenteredText(waitBuf, CONTENT_Y + 40, FONT_XLARGE, COL_ACCENT);
    uiDrawCenteredText("Submitted", CONTENT_Y + 90, FONT_BODY, COL_TEXT);

    int y = CONTENT_Y + 120;
    for (int p = 0; p < playerCount; p++) {
        uint16_t col = players[p].submitted ? COL_SELECTED : COL_DIM;
        const char* status = players[p].submitted ? "Ready" :
                             (players[p].connected ? "Guessing..." : "Disconnected");
        char label[32];
        snprintf(label, sizeof(label), "%s: %s", players[p].name, status);
        tft->setTextColor(col, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(label, 16, y);
        y += 18;
    }

    uiDrawHint("Waiting for guesses...", CONTENT_Y + 175);
    uiDrawSoftButtons("", "");
}

static void drawResults2x2(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESULTS", COL_ACCENT);

    int y = CONTENT_Y + 10;
    for (int p = 0; p < 2; p++) {
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(players[p].name, 16, y);
        y += 22;

        for (int g = 0; g < 2; g++) {
            int glassNum = players[p].assignedGlasses[g];
            bool correct = players[p].correctPerGlass[g];
            char line[40];
            snprintf(line, sizeof(line), "  Glass %d: %s",
                     glassNum, correct ? "Correct" : "Wrong");
            tft->setTextColor(correct ? COL_SELECTED : COL_ERROR, COL_BG);
            tft->setTextSize(FONT_SMALL);
            tft->setTextDatum(ML_DATUM);
            tft->drawString(line, 16, y);
            y += 16;
        }
        y += 8;
    }

    uiDrawSoftButtons("", "REVEAL");
}

static void drawResultsRandom(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("RESULTS", COL_ACCENT);

    // Green banner — correct guesses
    int y = CONTENT_Y + 5;
    tft->fillRoundRect(8, y, SCREEN_W - 16, 18, 4, COL_SELECTED);
    tft->setTextColor(COL_BG, COL_SELECTED);
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("CORRECT", SCREEN_W / 2, y + 9);
    y += 24;

    bool anyCorrect = false;
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].submitted || !players[p].correct) continue;
        anyCorrect = true;
        char line[40];
        snprintf(line, sizeof(line), "Glass %d: %s",
                 players[p].claimedGlass, players[p].name);
        tft->setTextColor(COL_SELECTED, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(line, 16, y);
        y += 16;
    }
    if (!anyCorrect) {
        tft->setTextColor(COL_DIM, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("None", 16, y);
        y += 16;
    }

    // Red banner — incorrect guesses
    y += 8;
    tft->fillRoundRect(8, y, SCREEN_W - 16, 18, 4, COL_ERROR);
    tft->setTextColor(COL_BG, COL_ERROR);
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(MC_DATUM);
    tft->drawString("INCORRECT", SCREEN_W / 2, y + 9);
    y += 24;

    bool anyIncorrect = false;
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].submitted || players[p].correct) continue;
        anyIncorrect = true;
        char line[40];
        snprintf(line, sizeof(line), "Glass %d: %s",
                 players[p].claimedGlass, players[p].name);
        tft->setTextColor(COL_ERROR, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(line, 16, y);
        y += 16;
    }
    if (!anyIncorrect) {
        tft->setTextColor(COL_DIM, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString("None", 16, y);
    }

    uiDrawSoftButtons("", "REVEAL");
}

static void drawResultsPremium(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("PREMIUM RESULTS", COL_ACCENT);

    // Show which glass was premium
    int premGlass = 0;
    for (int g = 0; g < NUM_GLASSES; g++) {
        if (bottleForGlass[g] == 0) { premGlass = g + 1; break; }
    }
    char premBuf[32];
    snprintf(premBuf, sizeof(premBuf), "Glass %d was premium", premGlass);
    uiDrawCenteredText(premBuf, CONTENT_Y + 5, FONT_SMALL, COL_DIM);

    int y = CONTENT_Y + 28;
    for (int p = 0; p < playerCount; p++) {
        if (!players[p].submitted) continue;
        bool hasPremium = (players[p].claimedGlass == premGlass);
        const char* guessLabel = players[p].guessPremium ? "Yes" : "No";
        const char* hadLabel = hasPremium ? "Yes" : "No";

        char line[48];
        snprintf(line, sizeof(line), "%s: %s (%s)",
                 players[p].name, guessLabel,
                 players[p].correct ? "Correct" : "Wrong");
        uint16_t col = players[p].correct ? COL_SELECTED : COL_ERROR;
        tft->setTextColor(col, COL_BG);
        tft->setTextSize(FONT_SMALL);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(line, 16, y);

        char detail[32];
        snprintf(detail, sizeof(detail), "  Glass %d — had premium: %s",
                 players[p].claimedGlass, hadLabel);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString(detail, 16, y + 14);
        y += 34;
    }

    uiDrawSoftButtons("", "REVEAL");
}

static void drawResults(bool fullRedraw) {
    if (subMode == H2H_SUB_PREMIUM) drawResultsPremium(fullRedraw);
    else if (subMode == H2H_SUB_RANDOM) drawResultsRandom(fullRedraw);
    else drawResults2x2(fullRedraw);
}

static void drawReveal(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("REVEAL", COL_ACCENT);

    int idx = revealIndex;
    if (idx >= revealMapCount) return;

    int glass = revealMap[idx].glass;
    int bottleIdx = bottleForGlass[glass - 1];

    char glassLabel[16];
    snprintf(glassLabel, sizeof(glassLabel), "Glass %d", glass);
    uiDrawCenteredText(glassLabel, CONTENT_Y + 8, FONT_BODY, COL_TEXT);

    uiDrawCenteredTextWrap(h2hBottles[bottleIdx], CONTENT_Y + 40, FONT_BODY, COL_ACCENT);

    // Progress dots
    int dotY   = CONTENT_Y + 135;
    int dotGap = 24;
    int dotsW  = (revealMapCount - 1) * dotGap;
    int dotX0  = (SCREEN_W - dotsW) / 2;
    for (int i = 0; i < revealMapCount; i++) {
        uint16_t col = (i <= idx) ? COL_ACCENT : COL_DIM;
        tft->fillCircle(dotX0 + i * dotGap, dotY, 6, col);
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
    for (int i = 0; i < revealMapCount; i++) {
        int glass = revealMap[i].glass;
        int bIdx = bottleForGlass[glass - 1];
        char line[40];
        snprintf(line, sizeof(line), "%d. %s", glass, h2hBottles[bIdx]);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->drawString(line, 16, y + 10);
        y += 28;
    }

    uiDrawSoftButtons("EXIT", "NEW");
}

// ============================================================
// Screen callbacks
// ============================================================

static void h2hDraw(bool fullRedraw) {
    // Handle browse return
    if (phase == H2H_BOTTLE_SELECT && awaitingBrowseReturn) {
        awaitingBrowseReturn = false;
        const char* name = browseGetResult();
        int targetBottles = (subMode == H2H_SUB_2X2) ? H2H_2X2_BOTTLES : h2hGlassCount;
        if (name) {
            strncpy(h2hBottles[bottleCount], name, MAX_GLASS_NAME - 1);
            h2hBottles[bottleCount][MAX_GLASS_NAME - 1] = '\0';
            Serial.printf("[H2H] Bottle %d: %s\n", bottleCount + 1, h2hBottles[bottleCount]);
            bottleCount++;

            if (bottleCount >= targetBottles) {
                if (subMode == H2H_SUB_2X2) {
                    build2x2PourOrder();
                } else if (subMode == H2H_SUB_PREMIUM) {
                    buildPremiumPourOrder();
                } else {
                    buildRandomPourOrder();
                }
                phase = H2H_LOBBY;
                wifiPortalBroadcastNow();
                Serial.println("[H2H] Bottles selected, entering lobby");
            }
            uiRequestRedraw();
        } else {
            if (bottleCount > 0) {
                uiRequestRedraw();
            } else {
                Serial.println("[H2H] Bottle select cancelled");
                if (subMode == H2H_SUB_RANDOM || subMode == H2H_SUB_PREMIUM) {
                    phase = H2H_COUNT_SELECT;
                    uiRequestRedraw();
                } else {
                    phase = H2H_MODE_SELECT;
                    uiRequestRedraw();
                }
            }
        }
        return;
    }

    switch (phase) {
        case H2H_MODE_SELECT:   drawModeSelect(fullRedraw);   break;
        case H2H_COUNT_SELECT:  drawCountSelect(fullRedraw);  break;
        case H2H_BOTTLE_SELECT: drawBottleSelect(fullRedraw); break;
        case H2H_LOBBY:         drawLobby(fullRedraw);        break;
        case H2H_POURING:       drawPouring(fullRedraw);      break;
        case H2H_TASTING:       drawTasting(fullRedraw);      break;
        case H2H_ASSIGN:        drawAssign(fullRedraw);       break;
        case H2H_WAITING:       drawWaiting(fullRedraw);      break;
        case H2H_RESULTS:       drawResults(fullRedraw);      break;
        case H2H_REVEAL:        drawReveal(fullRedraw);       break;
        case H2H_DONE:          drawDone(fullRedraw);         break;
    }
}

static void h2hInput(InputEvent evt) {
    switch (phase) {

        case H2H_MODE_SELECT: {
            int sel = uiScrollListHandleInput(&subModeList, evt);
            if (sel >= 0) {
                if (sel == 0) {
                    audioPlayTone(TONE_CONFIRM);
                    subMode = H2H_SUB_2X2;
                    h2hGlassCount = 4;
                    phase = H2H_BOTTLE_SELECT;
                    uiRequestRedraw();
                } else if (sel == 1) {
                    audioPlayTone(TONE_CONFIRM);
                    subMode = H2H_SUB_RANDOM;
                    uiScrollListInit(&countList, countLabels, COUNT_OPTIONS);
                    phase = H2H_COUNT_SELECT;
                    uiRequestRedraw();
                } else {
                    audioPlayTone(TONE_CONFIRM);
                    subMode = H2H_SUB_PREMIUM;
                    uiScrollListInit(&countList, countLabels, COUNT_OPTIONS);
                    phase = H2H_COUNT_SELECT;
                    uiRequestRedraw();
                }
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                h2hActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;
        }

        case H2H_COUNT_SELECT: {
            int sel = uiScrollListHandleInput(&countList, evt);
            if (sel >= 0) {
                audioPlayTone(TONE_CONFIRM);
                h2hGlassCount = sel + 2; // 0→2, 1→3, 2→4
                phase = H2H_BOTTLE_SELECT;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                phase = H2H_MODE_SELECT;
                uiRequestRedraw();
            }
            break;
        }

        case H2H_BOTTLE_SELECT:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                awaitingBrowseReturn = true;
                browseReset();
                uiPushScreen(&screenBrowse);
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                if (bottleCount > 0) {
                    bottleCount--;
                    h2hBottles[bottleCount][0] = '\0';
                    uiRequestRedraw();
                } else {
                    phase = (subMode == H2H_SUB_RANDOM || subMode == H2H_SUB_PREMIUM)
                            ? H2H_COUNT_SELECT : H2H_MODE_SELECT;
                    uiRequestRedraw();
                }
            }
            break;

        case H2H_LOBBY: {
            int connectedCount = 0;
            for (int i = 0; i < playerCount; i++) {
                if (players[i].connected) connectedCount++;
            }
            int required = h2hGetRequiredPlayers();

            if ((evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) && connectedCount >= required) {
                audioPlayTone(TONE_CONFIRM);
                Serial.println("[H2H] Lobby start — beginning pours");
                runH2HPourCycle();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                resetSession();
                phase = H2H_BOTTLE_SELECT;
                uiRequestRedraw();
            }
            break;
        }

        case H2H_POURING:
            if (evt == INPUT_ENC_CW) {
                motorMoveToPosition(motorGetPosition() + NUDGE_STEPS);
            } else if (evt == INPUT_ENC_CCW) {
                motorMoveToPosition(motorGetPosition() - NUDGE_STEPS);
            } else if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                glassUsed[currentGlass - 1] = true;
                pourCount++;
                Serial.printf("[H2H] Poured glass %d (pour %d/%d)\n",
                              currentGlass, pourCount, h2hGlassCount);

                if (pourCount >= h2hGlassCount) {
                    runFinalSpin();

                    if (subMode == H2H_SUB_2X2) {
                        assignGlasses();
                        phase = H2H_ASSIGN;
                        wifiPortalBroadcastNow();
                        Serial.printf("[H2H] Assignments: %s → G%d,G%d | %s → G%d,G%d\n",
                                      players[0].name,
                                      players[0].assignedGlasses[0], players[0].assignedGlasses[1],
                                      players[1].name,
                                      players[1].assignedGlasses[0], players[1].assignedGlasses[1]);
                    } else {
                        phase = H2H_TASTING;
                        wifiPortalBroadcastNow();
                        Serial.println("[H2H] All poured — tasting time");
                    }
                    uiRequestRedraw();
                } else {
                    runH2HPourCycle();
                }
            }
            break;

        case H2H_TASTING:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                phase = H2H_WAITING;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            break;

        case H2H_ASSIGN:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_CONFIRM);
                phase = H2H_WAITING;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            break;

        case H2H_WAITING: {
            int required = h2hGetRequiredPlayers();
            int submitted = h2hGetSubmittedCount();
            if (submitted >= required) {
                audioPlayTone(TONE_CONFIRM);
                if (subMode == H2H_SUB_2X2) score2x2();
                else if (subMode == H2H_SUB_PREMIUM) scorePremium();
                else scoreRandom();
                phase = H2H_RESULTS;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            break;
        }

        case H2H_RESULTS:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_HOME_FOUND);
                buildRevealMap();
                revealIndex = 0;
                phase = H2H_REVEAL;
                uiRequestRedraw();
            }
            break;

        case H2H_REVEAL:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                if (revealIndex < revealMapCount - 1) {
                    revealIndex++;
                    audioPlayTone(TONE_HOME_FOUND);
                    uiRequestRedraw();
                } else {
                    audioPlayTone(TONE_CONFIRM);
                    phase = H2H_DONE;
                    uiRequestRedraw();
                }
            }
            break;

        case H2H_DONE:
            if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
                audioPlayTone(TONE_SELECT);
                resetSession();
                phase = H2H_BOTTLE_SELECT;
                uiRequestRedraw();
            } else if (evt == INPUT_BTN_LEFT) {
                audioPlayTone(TONE_SELECT);
                h2hActive = false;
                uiPopScreenT(TRANS_FADE);
            }
            break;
    }
}

static void h2hOnEnter() {
    if (h2hActive) {
        Serial.println("[H2H] onEnter (re-entry — skipping init)");
        return;
    }

    h2hActive = true;
    awaitingBrowseReturn = false;
    resetSession();
    phase = H2H_MODE_SELECT;
    uiScrollListInit(&subModeList, subModeLabels, SUB_MODE_COUNT);
    Serial.println("[H2H] Entered Head-to-Head mode");
    uiRequestRedraw();
}

// ============================================================
// Public API
// ============================================================

const Screen screenH2H = {
    "H2H",
    h2hDraw,
    h2hInput,
    h2hOnEnter
};

#endif // !HEADLESS_BUILD

// ============================================================
// Public getters (both builds)
// ============================================================

bool h2hIsActive() {
    return h2hActive;
}

H2HPhase h2hGetPhase() {
    return phase;
}

H2HSubMode h2hGetSubMode() {
    return subMode;
}

int h2hGetPlayerCount() {
    return playerCount;
}

const H2HPlayer* h2hGetPlayer(int idx) {
    if (idx < 0 || idx >= playerCount) return nullptr;
    return &players[idx];
}

int h2hGetSubmittedCount() {
    int count = 0;
    for (int i = 0; i < playerCount; i++) {
        if (players[i].submitted) count++;
    }
    return count;
}

int h2hGetRequiredPlayers() {
    if (subMode == H2H_SUB_2X2) return 2;
    return h2hGlassCount; // Random/Premium: player count matches glass count
}

int h2hGetBottleCount() {
    return bottleCount;
}

const char* h2hGetBottleName(int idx) {
    if (idx < 0 || idx >= NUM_GLASSES) return "";
    return h2hBottles[idx];
}

int h2hGetGlassCount() {
    return h2hGlassCount;
}

int h2hGetBottleForGlass(int glassNum) {
    if (glassNum < 1 || glassNum > NUM_GLASSES) return -1;
    return bottleForGlass[glassNum - 1];
}

int h2hGetClaimedBy(int glassNum) {
    for (int i = 0; i < playerCount; i++) {
        if (players[i].claimedGlass == glassNum) return i;
    }
    return -1;
}

// ============================================================
// Phone action handlers
// ============================================================

int h2hPhoneJoin(uint8_t clientNum, const char* name) {
    if (!h2hActive) return -1;
    if (phase != H2H_LOBBY && phase != H2H_BOTTLE_SELECT &&
        phase != H2H_MODE_SELECT && phase != H2H_COUNT_SELECT) return -1;

    // Check for reconnect by name
    for (int i = 0; i < playerCount; i++) {
        if (strcmp(players[i].name, name) == 0) {
            players[i].clientNum = clientNum;
            players[i].connected = true;
            Serial.printf("[H2H] Player '%s' reconnected (client %u)\n", name, clientNum);
            uiRequestRedraw();
            wifiPortalBroadcastNow();
            return i;
        }
    }

    // New player
    if (playerCount >= H2H_MAX_PLAYERS) return -1;
    int required = h2hGetRequiredPlayers();
    if (playerCount >= required) return -1;

    int idx = playerCount++;
    players[idx].clientNum = clientNum;
    strncpy(players[idx].name, name, H2H_NAME_LEN - 1);
    players[idx].name[H2H_NAME_LEN - 1] = '\0';
    players[idx].connected = true;
    players[idx].submitted = false;
    players[idx].guess2x2[0] = -1;
    players[idx].guess2x2[1] = -1;

    Serial.printf("[H2H] Player '%s' joined (client %u, slot %d)\n",
                  name, clientNum, idx);

    uiRequestRedraw();
    wifiPortalBroadcastNow();
    return idx;
}

void h2hPhoneGuess2x2(uint8_t clientNum, int guess0, int guess1) {
    if (!h2hActive || phase != H2H_WAITING || subMode != H2H_SUB_2X2) return;

    for (int i = 0; i < playerCount; i++) {
        if (players[i].clientNum == clientNum && players[i].connected) {
            players[i].guess2x2[0] = guess0;
            players[i].guess2x2[1] = guess1;
            players[i].submitted = true;

            Serial.printf("[H2H] Player '%s' guessed: [%d, %d]\n",
                          players[i].name, guess0, guess1);

            wifiPortalBroadcastNow();
            uiRequestRedraw();

            int submitted = h2hGetSubmittedCount();
            if (submitted >= h2hGetRequiredPlayers()) {
                score2x2();
                phase = H2H_RESULTS;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            return;
        }
    }
}

void h2hPhoneClaimGlass(uint8_t clientNum, int glassNum) {
    if (!h2hActive || phase != H2H_WAITING) return;
    if (glassNum < 1 || glassNum > h2hGlassCount) return;

    // Check if already claimed by someone else
    for (int i = 0; i < playerCount; i++) {
        if (players[i].claimedGlass == glassNum && players[i].clientNum != clientNum) {
            Serial.printf("[H2H] Glass %d already claimed by '%s'\n",
                          glassNum, players[i].name);
            return;
        }
    }

    for (int i = 0; i < playerCount; i++) {
        if (players[i].clientNum == clientNum && players[i].connected) {
            players[i].claimedGlass = glassNum;
            Serial.printf("[H2H] Player '%s' claimed glass %d\n",
                          players[i].name, glassNum);
            wifiPortalBroadcastNow();
            return;
        }
    }
}

void h2hPhoneGuessRandom(uint8_t clientNum, int bottleIdx) {
    if (!h2hActive || phase != H2H_WAITING || subMode != H2H_SUB_RANDOM) return;
    if (bottleIdx < 0 || bottleIdx >= bottleCount) return;

    for (int i = 0; i < playerCount; i++) {
        if (players[i].clientNum == clientNum && players[i].connected) {
            if (players[i].claimedGlass == 0) {
                Serial.printf("[H2H] Player '%s' hasn't claimed a glass yet\n",
                              players[i].name);
                return;
            }
            players[i].guessBottleIdx = bottleIdx;
            players[i].submitted = true;

            Serial.printf("[H2H] Player '%s' guessed bottle %d (%s)\n",
                          players[i].name, bottleIdx, h2hBottles[bottleIdx]);

            wifiPortalBroadcastNow();
            uiRequestRedraw();

            int submitted = h2hGetSubmittedCount();
            if (submitted >= h2hGetRequiredPlayers()) {
                scoreRandom();
                phase = H2H_RESULTS;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            return;
        }
    }
}

void h2hPhoneGuessPremium(uint8_t clientNum, bool guess) {
    if (!h2hActive || phase != H2H_WAITING || subMode != H2H_SUB_PREMIUM) return;

    for (int i = 0; i < playerCount; i++) {
        if (players[i].clientNum == clientNum && players[i].connected) {
            if (players[i].claimedGlass == 0) {
                Serial.printf("[H2H] Player '%s' hasn't claimed a glass yet\n",
                              players[i].name);
                return;
            }
            players[i].guessPremium = guess;
            players[i].guessedPremium = true;
            players[i].submitted = true;

            Serial.printf("[H2H] Player '%s' guessed premium: %s\n",
                          players[i].name, guess ? "YES" : "NO");

            wifiPortalBroadcastNow();
            uiRequestRedraw();

            int submitted = h2hGetSubmittedCount();
            if (submitted >= h2hGetRequiredPlayers()) {
                scorePremium();
                phase = H2H_RESULTS;
                wifiPortalBroadcastNow();
                uiRequestRedraw();
            }
            return;
        }
    }
}

int h2hGetPremiumGlassNum() {
    for (int g = 0; g < NUM_GLASSES; g++) {
        if (bottleForGlass[g] == 0) return g + 1;
    }
    return 0;
}

void h2hPhoneDisconnect(uint8_t clientNum) {
    for (int i = 0; i < playerCount; i++) {
        if (players[i].clientNum == clientNum) {
            players[i].connected = false;
            Serial.printf("[H2H] Player '%s' disconnected\n", players[i].name);
            uiRequestRedraw();
            return;
        }
    }
}

// ============================================================
// Phone-based H2H creation (Session 25)
// ============================================================

void h2hStartFromPhone(uint8_t clientNum, H2HSubMode mode) {
    if (h2hActive || gameIsActive()) return;

    h2hActive = true;
    resetSession();
    subMode = mode;

    if (mode == H2H_SUB_2X2) {
        h2hGlassCount = 4;
        phase = H2H_BOTTLE_SELECT;
        Serial.printf("[H2H] Phone created 2x2 (need %d bottles)\n", H2H_2X2_BOTTLES);
    } else {
        phase = H2H_COUNT_SELECT;
        Serial.printf("[H2H] Phone created %s (need glass count)\n",
                      mode == H2H_SUB_PREMIUM ? "Premium" : "Random");
    }

    uiRequestRedraw();
    wifiPortalBroadcastNow();
}

void h2hPhoneSetGlassCount(int count) {
    if (!h2hActive || phase != H2H_COUNT_SELECT) return;
    if (count < 2 || count > 4) return;

    h2hGlassCount = count;
    phase = H2H_BOTTLE_SELECT;

    Serial.printf("[H2H] Phone set glass count: %d\n", count);
    uiRequestRedraw();
    wifiPortalBroadcastNow();
}

void h2hPhoneAddBottle(const char* name) {
    if (!h2hActive || phase != H2H_BOTTLE_SELECT) return;
    if (!name || !name[0]) return;

    int needed;
    if (subMode == H2H_SUB_2X2) needed = H2H_2X2_BOTTLES;
    else needed = h2hGlassCount;

    if (bottleCount >= needed) return;

    strncpy(h2hBottles[bottleCount], name, MAX_GLASS_NAME - 1);
    h2hBottles[bottleCount][MAX_GLASS_NAME - 1] = '\0';

    Serial.printf("[H2H] Phone bottle %d: %s\n", bottleCount + 1, h2hBottles[bottleCount]);

    bottleCount++;
    if (bottleCount >= needed) {
        if (subMode == H2H_SUB_2X2)         build2x2PourOrder();
        else if (subMode == H2H_SUB_RANDOM)  buildRandomPourOrder();
        else                                  buildPremiumPourOrder();

        phase = H2H_LOBBY;
        Serial.println("[H2H] All bottles entered — lobby open");
    }

    uiRequestRedraw();
    wifiPortalBroadcastNow();
}

void h2hPhoneStartGame(uint8_t clientNum) {
    if (!h2hActive || phase != H2H_LOBBY) return;

    int connectedCount = 0;
    for (int i = 0; i < playerCount; i++) {
        if (players[i].connected) connectedCount++;
    }
    int required = h2hGetRequiredPlayers();
    if (connectedCount < required) {
        Serial.printf("[H2H] Not enough players: %d/%d\n", connectedCount, required);
        return;
    }

    Serial.println("[H2H] Phone start — beginning pours");
    audioPlayTone(TONE_CONFIRM);
    runH2HPourCycle();
}
