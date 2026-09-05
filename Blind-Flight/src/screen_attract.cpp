#include "screen_attract.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "screens.h"
#include "game.h"
#include "transitions.h"
#include "settings.h"
#include "wifi_portal.h"
#include "splash.h"
#include "battery.h"

// ============================================================
// Blind Flight — Attract Mode
// ============================================================
// A blocking demo loop that plays a ~2:15 scripted sequence
// showcasing all major features. Each phase draws simulated
// screens using the real UI primitives, runs real motor
// movement where indicated, and checks for long-press exit.
//
// Architecture: the entire sequence runs as a blocking function
// inside the screen's onEnter callback (same pattern as the
// splash animation and self-test). Between every wait and motor
// operation, wifiPortalService() keeps the phone alive and
// inputUpdate() checks for long-press exit.
// ============================================================

static bool running = false;

bool attractIsRunning() { return running; }

// ============================================================
// Timing helpers
// ============================================================

// Wait for `ms` milliseconds, servicing audio/wifi/input.
// Returns true if long-press exit was detected.
static bool waitMs(unsigned long ms) {
    unsigned long end = millis() + ms;
    while (millis() < end) {
        audioUpdate();
        wifiPortalService();
        inputUpdate();
        InputEvent evt = inputGetEvent();
        if (evt == INPUT_ENC_LONG) return true;
        delay(1);
    }
    return false;
}

// Wait, but also drain any stale input events first.
static bool waitClean(unsigned long ms) {
    inputUpdate();
    while (inputGetEvent() != INPUT_NONE) { inputUpdate(); }
    return waitMs(ms);
}

// ============================================================
// Title card — shown at the start of each section
// ============================================================

static bool showTitle(const char* line1, const char* line2 = nullptr) {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    int y = line2 ? (SCREEN_H / 2 - 20) : (SCREEN_H / 2 - 8);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(FONT_BODY);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString(line1, SCREEN_W / 2, y);
    if (line2) {
        tft->drawString(line2, SCREEN_W / 2, y + 28);
    }

    audioPlayTone(TONE_SELECT);
    return waitMs(1500);
}

// ============================================================
// Simulated screen drawing helpers
// ============================================================

static void drawSimTitleBar(const char* title, uint16_t col) {
    uiDrawTitleBar(title, col);
}

static void drawSimSoftButtons(const char* left, const char* right) {
    uiDrawSoftButtons(left, right);
}

// Draw a menu list with one item highlighted
static void drawSimMenuList(const char** items, int count,
                            int selected, int scrollOff = 0) {
    TFT_eSPI* tft = uiGetTFT();
    int visible = CONTENT_H / MENU_ITEM_H;

    for (int i = 0; i < visible && (scrollOff + i) < count; i++) {
        int idx = scrollOff + i;
        int y = CONTENT_Y + i * MENU_ITEM_H;
        bool sel = (idx == selected);
        uint16_t bg = sel ? COL_HIGHLIGHT : COL_BG;
        tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H, bg);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(ML_DATUM);
        tft->setTextColor(COL_TEXT, bg);
        tft->drawString(items[idx], MENU_ITEM_X + 4, y + MENU_ITEM_H / 2);
        tft->setTextDatum(MR_DATUM);
        tft->setTextColor(COL_DIM, bg);
        tft->drawString(">", MENU_ITEM_X + MENU_ITEM_W - 4, y + MENU_ITEM_H / 2);
    }
}

// Draw a glass grid (home screen style)
static void drawSimGlassGrid(int glassCount, int poursDone,
                              const uint16_t* glassColors = nullptr) {
    TFT_eSPI* tft = uiGetTFT();
    int boxSize = 40;
    int gap = 12;
    int totalW = glassCount * boxSize + (glassCount - 1) * gap;
    int startX = (SCREEN_W - totalW) / 2;
    int y = CONTENT_Y + 30;

    for (int i = 0; i < glassCount; i++) {
        int x = startX + i * (boxSize + gap);
        uint16_t col = (i < poursDone) ? COL_SELECTED : COL_DIM;
        if (glassColors) col = glassColors[i];
        tft->drawRect(x, y, boxSize, boxSize, col);
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(FONT_BODY);
        tft->setTextColor(col, COL_BG);
        tft->drawString(num, x + boxSize / 2, y + boxSize / 2);
    }
}

// Draw star rating for a glass
static void drawSimStars(int y, int filled, int total = 5) {
    TFT_eSPI* tft = uiGetTFT();
    int starW = 24, starH = 20, gap = 6;
    int totalW = total * starW + (total - 1) * gap;
    int startX = (SCREEN_W - totalW) / 2;

    for (int i = 0; i < total; i++) {
        int x = startX + i * (starW + gap);
        uint16_t col = (i < filled) ? COL_ACCENT : COL_DIM;
        if (i < filled) {
            tft->fillRoundRect(x, y, starW, starH, 3, col);
            tft->setTextColor(COL_BG, col);
        } else {
            tft->drawRoundRect(x, y, starW, starH, 3, col);
            tft->setTextColor(col, COL_BG);
        }
        tft->setTextDatum(MC_DATUM);
        tft->setTextSize(FONT_BODY);
        tft->drawString("*", x + starW / 2, y + starH / 2);
    }
}

// Draw a simulated battery indicator with specific values
static void drawSimBatteryIndicator(int pct, bool low, bool lockout) {
    TFT_eSPI* tft = uiGetTFT();
    int bw = 24, bh = 14;
    int tw = 3, th = 6;

    uint16_t col = COL_SELECTED;
    if (lockout) col = COL_ERROR;
    else if (low) col = COL_MOVING;

    int totalW = bw + tw + 4 + 36;
    int bx = (SCREEN_W - totalW) / 2;
    int by = CONTENT_Y + 60;

    tft->drawRect(bx, by, bw, bh, col);
    tft->fillRect(bx + bw, by + (bh - th) / 2, tw, th, col);
    int fillW = (bw - 4) * pct / 100;
    if (fillW > 0) {
        tft->fillRect(bx + 2, by + 2, fillW, bh - 4, col);
    }

    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    tft->setTextColor(col, COL_BG);
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(ML_DATUM);
    tft->drawString(buf, bx + bw + tw + 4, by + bh / 2);
}

// ============================================================
// Phase: Splash (reuses the real animation)
// ============================================================

static bool phaseSplash() {
    splashRunAnimation();
    return waitClean(800);
}

// ============================================================
// Phase: Mode showcase with 20s continuous spin
// ============================================================

static const char* modeNames[] = {
    "QUICK FLIGHT", "FULL FLIGHT", "BEST GUESS",
    "RANKED FLIGHT", "GUESS + RANKED", "TWIN POUR",
    "FIND THE RINGER", "HEAD TO HEAD", "FLIGHT CLUB"
};
static const char* modeDescs[] = {
    "Numbered blind tasting",
    "Named whiskey selection",
    "Match names to glasses",
    "Order your favorites",
    "The ultimate challenge",
    "Find the matching pair",
    "Spot the odd one out",
    "Competitive multiplayer",
    "Group tasting events"
};
static const int MODE_COUNT = 9;

static void drawModeCard(int idx) {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    // Mode number
    char numBuf[8];
    snprintf(numBuf, sizeof(numBuf), "%d of %d", idx + 1, MODE_COUNT);
    tft->setTextDatum(MC_DATUM);
    tft->setTextSize(FONT_SMALL);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->drawString(numBuf, SCREEN_W / 2, 30);

    // Mode name
    tft->setTextSize(FONT_BODY);
    tft->setTextColor(COL_ACCENT, COL_BG);
    tft->drawString(modeNames[idx], SCREEN_W / 2, SCREEN_H / 2 - 15);

    // Description
    tft->setTextSize(FONT_BODY);
    tft->setTextColor(COL_TEXT, COL_BG);
    tft->drawString(modeDescs[idx], SCREEN_W / 2, SCREEN_H / 2 + 20);

    // Decorative line
    int lineW = 80;
    tft->drawFastHLine((SCREEN_W - lineW) / 2, SCREEN_H / 2 + 45,
                       lineW, COL_DIM);
}

static bool phaseModeShowcase() {
    if (showTitle("NINE WAYS TO", "TASTE BLIND")) return true;

    motorEnable();
    unsigned long spinStart = millis();
    unsigned long stepTimer = micros();
    int modeIdx = 0;
    unsigned long lastModeSwitch = millis();
    const unsigned long SPIN_DURATION = 20000;
    const unsigned long MODE_INTERVAL = SPIN_DURATION / MODE_COUNT;
    // 800 sps for a stately pace — matches the theatrical preset
    const unsigned long STEP_INTERVAL_US = 1250;

    drawModeCard(0);

    // Soft ramp: start at 3000µs/step, ramp to target over 400ms
    unsigned long rampEnd = spinStart + 400;
    const unsigned long RAMP_START_US = 3000;

    while (millis() - spinStart < SPIN_DURATION) {
        unsigned long now = micros();
        unsigned long interval = STEP_INTERVAL_US;
        if (millis() < rampEnd) {
            float t = (float)(millis() - spinStart) / 400.0f;
            interval = RAMP_START_US + (unsigned long)((float)(STEP_INTERVAL_US - RAMP_START_US) * t);
        }

        if (now - stepTimer >= interval) {
            stepTimer = now;
            digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
            digitalWrite(PIN_MOTOR_STEP, HIGH);
            delayMicroseconds(2);
            digitalWrite(PIN_MOTOR_STEP, LOW);
        }

        // Switch mode display
        if (millis() - lastModeSwitch >= MODE_INTERVAL && modeIdx < MODE_COUNT - 1) {
            lastModeSwitch = millis();
            modeIdx++;
            drawModeCard(modeIdx);
            audioPlayTone(TONE_CLICK);
        }

        // Check exit
        inputUpdate();
        if (inputGetEvent() == INPUT_ENC_LONG) {
            motorDisable();
            return true;
        }

        audioUpdate();
        // Service WiFi every ~50ms (don't call on every step)
        static unsigned long lastWifi = 0;
        if (millis() - lastWifi > 50) {
            lastWifi = millis();
            wifiPortalService();
        }
    }

    // Soft stop: ramp down over 300ms
    unsigned long stopStart = millis();
    unsigned long stopInterval = STEP_INTERVAL_US;
    while (millis() - stopStart < 300) {
        unsigned long now = micros();
        float t = (float)(millis() - stopStart) / 300.0f;
        stopInterval = STEP_INTERVAL_US + (unsigned long)((float)(RAMP_START_US - STEP_INTERVAL_US) * t);
        if (now - stepTimer >= stopInterval) {
            stepTimer = now;
            digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
            digitalWrite(PIN_MOTOR_STEP, HIGH);
            delayMicroseconds(2);
            digitalWrite(PIN_MOTOR_STEP, LOW);
        }
        audioUpdate();
    }

    motorDisable();
    return waitMs(500);
}

// ============================================================
// Phase: Choose your flight (simulated menu navigation)
// ============================================================

static bool phaseChooseFlight() {
    if (showTitle("CHOOSE YOUR", "FLIGHT")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Show home screen briefly
    tft->fillScreen(COL_BG);
    drawSimTitleBar("BLIND FLIGHT", COL_HOME);
    drawSimGlassGrid(4, 0);
    uiDrawHint("Ready", CONTENT_Y + 100);
    drawSimSoftButtons("SETTINGS", "START");
    if (waitMs(1500)) return true;

    // Show menu with "Full Flight" selected
    static const char* menuItems[] = {
        "Quick Flight", "Full Flight", "Ranked Flight",
        "Palate Training", "Head to Head", "Flight Club"
    };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("SELECT MODE", COL_HOME);
    drawSimMenuList(menuItems, 6, 1);
    drawSimSoftButtons("BACK", "SELECT");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2000)) return true;

    return false;
}

// ============================================================
// Phase: Browse library (simulated navigation)
// ============================================================

static bool phaseBrowseLibrary() {
    if (showTitle("BROWSE THE", "LIBRARY")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Level 1: Type selection
    static const char* types[] = {
        "Bourbon", "Rye", "Scotch", "Irish", "Japanese"
    };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("SELECT TYPE", COL_ACCENT);
    drawSimMenuList(types, 5, 0);
    drawSimSoftButtons("BACK", "SELECT");
    audioPlayTone(TONE_CLICK);
    if (waitMs(2000)) return true;

    // Level 2: Distillery
    static const char* distilleries[] = {
        "Buffalo Trace", "Heaven Hill", "Maker's Mark",
        "Wild Turkey", "Woodford"
    };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("BOURBON", COL_ACCENT);
    drawSimMenuList(distilleries, 5, 0);
    drawSimSoftButtons("BACK", "SELECT");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2000)) return true;

    // Level 3: Product
    static const char* products[] = {
        "Eagle Rare", "Blanton's", "E.H. Taylor",
        "Stagg Jr"
    };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("BUFFALO TRACE", COL_ACCENT);
    drawSimMenuList(products, 4, 0);
    drawSimSoftButtons("BACK", "SELECT");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2000)) return true;

    audioPlayTone(TONE_CONFIRM);
    return false;
}

// ============================================================
// Phase: Text entry (simulated typing)
// ============================================================

static bool phaseTextEntry() {
    if (showTitle("NAME YOUR OWN", "")) return true;

    TFT_eSPI* tft = uiGetTFT();
    const char* target = "WOODFORD";
    int targetLen = strlen(target);

    // Grid layout matching the real TextEntry
    static const char charSet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

    for (int charIdx = 0; charIdx <= targetLen; charIdx++) {
        tft->fillScreen(COL_BG);
        drawSimTitleBar("ENTER NAME", COL_ACCENT);

        // Text field with typed-so-far
        int fieldY = CONTENT_Y + 10;
        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(FONT_BODY);
        tft->setTextColor(COL_TEXT, COL_BG);
        char typed[22] = "";
        strncpy(typed, target, charIdx);
        typed[charIdx] = '\0';
        tft->drawString(typed, 20, fieldY + 8);

        // Blinking cursor
        int cursorX = 20 + tft->textWidth(typed);
        tft->drawFastVLine(cursorX, fieldY + 2, 16, COL_ACCENT);

        // Separator line
        tft->drawFastHLine(16, fieldY + 24, SCREEN_W - 32, COL_DIM);

        // Draw a simplified grid
        int gridY = fieldY + 34;
        int cellW = 20, cellH = 22;
        int cols = 10;
        for (int i = 0; i < 37; i++) {
            int row = i / cols;
            int col = i % cols;
            int cx = 10 + col * (cellW + 2);
            int cy = gridY + row * (cellH + 2);
            bool highlight = false;
            if (charIdx < targetLen) {
                int targetCharPos = -1;
                for (int j = 0; j < 37; j++) {
                    if (charSet[j] == target[charIdx]) { targetCharPos = j; break; }
                }
                highlight = (i == targetCharPos);
            }
            uint16_t bg = highlight ? COL_HIGHLIGHT : COL_BG;
            uint16_t fg = highlight ? COL_TEXT : COL_DIM;
            tft->fillRect(cx, cy, cellW, cellH, bg);
            char ch[2] = { charSet[i], '\0' };
            tft->setTextDatum(MC_DATUM);
            tft->setTextSize(FONT_SMALL);
            tft->setTextColor(fg, bg);
            tft->drawString(ch, cx + cellW / 2, cy + cellH / 2);
        }

        drawSimSoftButtons("CANCEL", charIdx == targetLen ? "DONE" : "");

        if (charIdx < targetLen) {
            audioPlayTone(TONE_CLICK);
        } else {
            audioPlayTone(TONE_CONFIRM);
        }

        if (waitMs(charIdx < targetLen ? 350 : 600)) return true;
    }

    return false;
}

// ============================================================
// Phase: Pour sequence (real motor movement)
// ============================================================

static bool phasePour() {
    if (showTitle("THE BLIND POUR", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Re-home the disc (position unknown after the showcase spin)
    tft->fillScreen(COL_BG);
    drawSimTitleBar("HOMING", COL_MOVING);
    uiDrawCenteredText("Aligning disc...", SCREEN_H / 2, FONT_BODY, COL_TEXT);
    motorHome();
    audioPlayTone(TONE_HOME_FOUND);
    if (waitMs(300)) return true;

    const char* pourNames[] = { "Eagle Rare", "Woodford" };

    for (int pour = 0; pour < 2; pour++) {
        // Spin to glass
        tft->fillScreen(COL_BG);
        drawSimTitleBar("SPINNING", COL_MOVING);
        uiDrawCenteredText("Finding glass...", SCREEN_H / 2, FONT_BODY, COL_TEXT);
        gameSetSpinning(true);
        wifiPortalBroadcastNow();

        motorSetSpinSpeed(1);  // Normal
        motorSpinToGlass(pour + 1, motorGetExtraRevs());

        gameSetSpinning(false);
        wifiPortalBroadcastNow();
        audioPlayTone(TONE_ARRIVE);

        // "Ready to Pour" screen
        tft->fillScreen(COL_BG);
        drawSimTitleBar("READY TO POUR", COL_ACCENT);

        char pourLabel[32];
        snprintf(pourLabel, sizeof(pourLabel), "Glass %d of 2", pour + 1);
        uiDrawCenteredText(pourLabel, CONTENT_Y + 30, FONT_BODY, COL_TEXT);

        uiDrawCenteredText(pourNames[pour], CONTENT_Y + 70, FONT_BODY, COL_ACCENT);
        uiDrawHint("Pour and press DONE", CONTENT_Y + 120);
        drawSimSoftButtons("SKIP", "DONE");

        if (waitMs(2500)) return true;

        audioPlayTone(TONE_CONFIRM);
        if (waitMs(300)) return true;
    }

    return false;
}

// ============================================================
// Phase: Star rating
// ============================================================

static bool phaseRating() {
    if (showTitle("RATE YOUR POUR", "")) return true;

    TFT_eSPI* tft = uiGetTFT();
    int ratings[] = { 4, 5 };
    const char* names[] = { "Eagle Rare", "Woodford" };

    for (int g = 0; g < 2; g++) {
        // Animate filling stars
        for (int s = 1; s <= ratings[g]; s++) {
            tft->fillScreen(COL_BG);
            drawSimTitleBar("RATE THIS POUR", COL_ACCENT);

            char label[32];
            snprintf(label, sizeof(label), "Glass %d: %s", g + 1, names[g]);
            uiDrawCenteredText(label, CONTENT_Y + 30, FONT_BODY, COL_TEXT);

            drawSimStars(CONTENT_Y + 70, s);

            char progress[16];
            snprintf(progress, sizeof(progress), "%d of 2", g + 1);
            uiDrawHint(progress, CONTENT_Y + 120);
            drawSimSoftButtons("SKIP", "CONFIRM");

            audioPlayTone(TONE_CLICK);
            if (waitMs(300)) return true;
        }
        audioPlayTone(TONE_CONFIRM);
        if (waitMs(400)) return true;
    }

    return false;
}

// ============================================================
// Phase: Guessing round
// ============================================================

static bool phaseGuessing() {
    if (showTitle("GUESS THE GLASS", "")) return true;

    TFT_eSPI* tft = uiGetTFT();
    const char* names[] = { "Eagle Rare", "Woodford" };

    // Show guess pool for glass 1
    for (int g = 0; g < 2; g++) {
        tft->fillScreen(COL_BG);
        drawSimTitleBar("WHICH WHISKEY?", COL_ACCENT);

        char label[16];
        snprintf(label, sizeof(label), "Glass %d", g + 1);
        uiDrawCenteredText(label, CONTENT_Y + 20, FONT_LARGE, COL_TEXT);

        // Pool of names
        for (int i = 0; i < 2; i++) {
            int y = CONTENT_Y + 70 + i * 36;
            bool sel = (i == g);  // correct guess: glass 1 = name 0, glass 2 = name 1
            uint16_t bg = sel ? COL_HIGHLIGHT : COL_BG;
            tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, 30, bg);
            tft->setTextDatum(ML_DATUM);
            tft->setTextSize(FONT_BODY);
            tft->setTextColor(COL_TEXT, bg);
            tft->drawString(names[i], MENU_ITEM_X + 8, y + 15);
        }

        drawSimSoftButtons("BACK", "SELECT");
        audioPlayTone(TONE_SELECT);
        if (waitMs(1500)) return true;
    }

    // Score card
    tft->fillScreen(COL_BG);
    drawSimTitleBar("RESULTS", COL_SELECTED);
    uiDrawCenteredText("2 of 2", CONTENT_Y + 30, FONT_XLARGE, COL_SELECTED);
    uiDrawCenteredText("Correct!", CONTENT_Y + 90, FONT_BODY, COL_TEXT);
    drawSimSoftButtons("", "NEXT");
    audioPlayTone(TONE_CONFIRM);
    if (waitMs(2000)) return true;

    return false;
}

// ============================================================
// Phase: Reveal (with motor spin and slot animation)
// ============================================================

static bool phaseReveal() {
    if (showTitle("THE REVEAL", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Final theatrical spin
    tft->fillScreen(COL_BG);
    drawSimTitleBar("FINAL SPIN", COL_MOVING);
    uiDrawCenteredText("Revealing...", SCREEN_H / 2, FONT_BODY, COL_ACCENT);

    gameSetSpinning(true);
    wifiPortalBroadcastNow();
    int steps = 3 * MICROSTEPS_PER_REV + random(2 * MICROSTEPS_PER_REV);
    motorSpinSteps(steps);
    gameSetSpinning(false);
    wifiPortalBroadcastNow();
    audioPlayTone(TONE_ARRIVE);
    if (waitMs(500)) return true;

    // Per-glass reveal (simulated slot-machine text)
    const char* names[] = { "Eagle Rare", "Woodford" };
    const char* meta[] = { "90 proof  |  $$", "90.4 proof  |  $$" };

    for (int g = 0; g < 2; g++) {
        tft->fillScreen(COL_BG);
        drawSimTitleBar("REVEAL", COL_ACCENT);

        char label[16];
        snprintf(label, sizeof(label), "Glass %d", g + 1);
        uiDrawCenteredText(label, CONTENT_Y + 20, FONT_BODY, COL_DIM);

        // Simulate slot-roll effect with a few intermediate frames
        const char* scramble[] = { "Blanton's", "Stagg Jr", "E.H. Taylor" };
        for (int f = 0; f < 3; f++) {
            tft->fillRect(0, CONTENT_Y + 50, SCREEN_W, 30, COL_BG);
            uiDrawCenteredText(scramble[f], CONTENT_Y + 60, FONT_BODY, COL_DIM);
            if (waitMs(120)) return true;
        }

        // Final reveal
        tft->fillRect(0, CONTENT_Y + 50, SCREEN_W, 30, COL_BG);
        uiDrawCenteredText(names[g], CONTENT_Y + 60, FONT_BODY, COL_ACCENT);

        // Metadata
        uiDrawCenteredText(meta[g], CONTENT_Y + 100, FONT_SMALL, COL_DIM);

        // Stars
        int ratings[] = { 4, 5 };
        drawSimStars(CONTENT_Y + 125, ratings[g]);

        // Progress dots
        char dots[8];
        snprintf(dots, sizeof(dots), "%d of 2", g + 1);
        uiDrawHint(dots, CONTENT_Y + 165);

        drawSimSoftButtons("", "NEXT");
        audioPlayTone(TONE_HOME_FOUND);
        if (waitMs(2500)) return true;
    }

    // Flight complete
    tft->fillScreen(COL_BG);
    drawSimTitleBar("FLIGHT COMPLETE", COL_SELECTED);

    for (int g = 0; g < 2; g++) {
        int y = CONTENT_Y + 20 + g * 55;
        char label[8];
        snprintf(label, sizeof(label), "%d.", g + 1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(FONT_BODY);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString(label, 16, y);
        tft->setTextColor(COL_ACCENT, COL_BG);
        tft->drawString(names[g], 40, y);
        int ratings[] = { 4, 5 };
        drawSimStars(y + 22, ratings[g]);
    }

    drawSimSoftButtons("EXIT", "NEW");
    audioPlayTone(TONE_CONFIRM);
    if (waitMs(2500)) return true;

    return false;
}

// ============================================================
// Phase: Head to Head
// ============================================================

static bool phaseH2H() {
    if (showTitle("HEAD TO HEAD", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Sub-mode menu
    static const char* h2hModes[] = { "2 x 2", "Random", "Premium" };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("HEAD TO HEAD", COL_ACCENT);
    drawSimMenuList(h2hModes, 3, 1);
    drawSimSoftButtons("BACK", "SELECT");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2000)) return true;

    // Lobby with simulated players joining
    const char* players[] = { "Jeremy", "Sarah", "Mike" };
    for (int joined = 1; joined <= 3; joined++) {
        tft->fillScreen(COL_BG);
        drawSimTitleBar("LOBBY", COL_HOME);

        for (int p = 0; p < joined; p++) {
            int y = CONTENT_Y + 20 + p * 36;
            tft->fillCircle(30, y + 10, 5, COL_SELECTED);
            tft->setTextDatum(ML_DATUM);
            tft->setTextSize(FONT_BODY);
            tft->setTextColor(COL_TEXT, COL_BG);
            tft->drawString(players[p], 46, y + 10);
        }

        char countBuf[16];
        snprintf(countBuf, sizeof(countBuf), "%d players", joined);
        uiDrawHint(countBuf, CONTENT_Y + 150);
        drawSimSoftButtons("CANCEL", joined >= 2 ? "START" : "");
        audioPlayTone(TONE_CLICK);
        if (waitMs(1200)) return true;
    }

    audioPlayTone(TONE_CONFIRM);
    if (waitMs(500)) return true;

    return false;
}

// ============================================================
// Phase: Flight Club
// ============================================================

static bool phaseFlightClub() {
    if (showTitle("FLIGHT CLUB", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Roster
    const char* roster[] = { "Jeremy", "Sarah", "Mike", "Alex" };
    tft->fillScreen(COL_BG);
    drawSimTitleBar("FLIGHT CLUB", COL_ACCENT);
    uiDrawCenteredText("ROSTER", CONTENT_Y + 10, FONT_BODY, COL_DIM);

    for (int i = 0; i < 4; i++) {
        int y = CONTENT_Y + 35 + i * 30;
        char numBuf[4];
        snprintf(numBuf, sizeof(numBuf), "%d.", i + 1);
        tft->setTextDatum(ML_DATUM);
        tft->setTextSize(FONT_BODY);
        tft->setTextColor(COL_DIM, COL_BG);
        tft->drawString(numBuf, 20, y);
        tft->setTextColor(COL_TEXT, COL_BG);
        tft->drawString(roster[i], 50, y);
    }
    drawSimSoftButtons("BACK", "START");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2500)) return true;

    // Round indicator
    tft->fillScreen(COL_BG);
    drawSimTitleBar("FLIGHT CLUB", COL_ACCENT);
    uiDrawCenteredText("Round 1 of 4", CONTENT_Y + 30, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("Jeremy", CONTENT_Y + 70, FONT_LARGE, COL_ACCENT);
    uiDrawHint("Step up to the device", CONTENT_Y + 120);
    drawSimSoftButtons("", "POUR");
    audioPlayTone(TONE_CONFIRM);
    if (waitMs(2500)) return true;

    return false;
}

// ============================================================
// Phase: Phone control / QR / Wi-Fi
// ============================================================

static bool phasePhone() {
    if (showTitle("CONNECT YOUR", "PHONE")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Simulated QR code screen
    tft->fillScreen(COL_BG);
    drawSimTitleBar("JOIN WI-FI", COL_HOME);

    // Draw a placeholder QR-like pattern
    int qrSize = 100;
    int qrX = (SCREEN_W - qrSize) / 2;
    int qrY = CONTENT_Y + 10;
    tft->drawRect(qrX, qrY, qrSize, qrSize, COL_TEXT);
    // Corner markers
    int mSize = 20;
    tft->fillRect(qrX + 4, qrY + 4, mSize, mSize, COL_TEXT);
    tft->fillRect(qrX + qrSize - 4 - mSize, qrY + 4, mSize, mSize, COL_TEXT);
    tft->fillRect(qrX + 4, qrY + qrSize - 4 - mSize, mSize, mSize, COL_TEXT);
    // Inner dots pattern
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            if ((r + c) % 2 == 0) {
                int dx = qrX + 30 + c * 10;
                int dy = qrY + 30 + r * 10;
                tft->fillRect(dx, dy, 6, 6, COL_TEXT);
            }
        }
    }

    uiDrawHint("BlindFlight", CONTENT_Y + 120);
    uiDrawHint("PIN: 4829", CONTENT_Y + 140);
    drawSimSoftButtons("BACK", "");
    if (waitMs(2500)) return true;

    // STA mode info
    tft->fillScreen(COL_BG);
    drawSimTitleBar("WI-FI CONNECTED", COL_SELECTED);
    uiDrawCenteredText("Local Network", CONTENT_Y + 30, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("MyHomeWiFi", CONTENT_Y + 60, FONT_BODY, COL_ACCENT);
    uiDrawCenteredText("http://flight.local", CONTENT_Y + 100, FONT_BODY, COL_DIM);
    uiDrawHint("All phones on same network", CONTENT_Y + 140);
    drawSimSoftButtons("BACK", "");
    audioPlayTone(TONE_CONFIRM);
    if (waitMs(2500)) return true;

    return false;
}

// ============================================================
// Phase: Settings showcase
// ============================================================

static bool phaseSettings() {
    if (showTitle("YOUR SETTINGS", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    struct SettingDemo {
        const char* label;
        const char* values[4];
        int valueCount;
    };

    SettingDemo settings[] = {
        { "Pour Side",  { "Front", "Right", "Rear", "Left" }, 4 },
        { "Spin Speed", { "Fast", "Normal", "Slow", "" },     3 },
        { "Sound",      { "ON ####", "ON ###.", "ON ##..", "" }, 3 },
        { "Brightness", { "#####", "####.", "###..", "" },     3 },
    };

    for (int s = 0; s < 4; s++) {
        for (int v = 0; v < settings[s].valueCount; v++) {
            tft->fillScreen(COL_BG);
            drawSimTitleBar("SETTINGS", COL_HOME);

            // Draw a few settings items with current one highlighted
            for (int row = 0; row < 4; row++) {
                int y = CONTENT_Y + row * MENU_ITEM_H;
                bool sel = (row == s);
                uint16_t bg = sel ? COL_HIGHLIGHT : COL_BG;
                tft->fillRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H, bg);
                tft->setTextDatum(ML_DATUM);
                tft->setTextSize(FONT_BODY);
                tft->setTextColor(COL_TEXT, bg);
                tft->drawString(settings[row].label, MENU_ITEM_X + 4,
                               y + MENU_ITEM_H / 2);
                tft->setTextDatum(MR_DATUM);
                tft->setTextColor(COL_ACCENT, bg);
                const char* val;
                if (row == s) {
                    val = settings[s].values[v];
                } else {
                    val = settings[row].values[0];
                }
                tft->drawString(val, MENU_ITEM_X + MENU_ITEM_W - 4,
                               y + MENU_ITEM_H / 2);
            }

            drawSimSoftButtons("BACK", "EDIT");
            audioPlayTone(TONE_CLICK);
            if (waitMs(600)) return true;
        }
        if (waitMs(200)) return true;
    }

    return false;
}

// ============================================================
// Phase: OTA updates
// ============================================================

static bool phaseOTA() {
    if (showTitle("ALWAYS CURRENT", "")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Checking phase
    tft->fillScreen(COL_BG);
    drawSimTitleBar("FIRMWARE UPDATE", COL_HOME);
    uiDrawCenteredText("Checking...", CONTENT_Y + 50, FONT_BODY, COL_TEXT);
    uiDrawHint("Fetching manifest", CONTENT_Y + 90);
    drawSimSoftButtons("BACK", "");
    if (waitMs(2000)) return true;

    // Up to date
    tft->fillScreen(COL_BG);
    drawSimTitleBar("UP TO DATE", COL_SELECTED);
    uiDrawCenteredText(FW_VERSION, CONTENT_Y + 40, FONT_LARGE, COL_ACCENT);
    uiDrawCenteredText("No update available", CONTENT_Y + 90, FONT_BODY, COL_TEXT);
    uiDrawHint("Updates delivered over Wi-Fi", CONTENT_Y + 130);
    drawSimSoftButtons("BACK", "");
    audioPlayTone(TONE_CONFIRM);
    if (waitMs(2500)) return true;

    return false;
}

// ============================================================
// Phase: Battery & session recovery (simulated readings)
// ============================================================

static bool phaseBattery() {
    if (showTitle("BUILT FOR THE", "SESSION")) return true;

    TFT_eSPI* tft = uiGetTFT();

    // Normal battery state
    tft->fillScreen(COL_BG);
    drawSimTitleBar("BATTERY", COL_HOME);
    drawSimBatteryIndicator(72, false, false);
    uiDrawCenteredText("7.82V", CONTENT_Y + 95, FONT_BODY, COL_TEXT);
    uiDrawHint("Rechargeable Li-ion pack", CONTENT_Y + 130);
    uiDrawHint("USB-C charging", CONTENT_Y + 150);
    if (waitMs(2500)) return true;

    // Low battery warning
    tft->fillScreen(COL_BG);
    drawSimTitleBar("LOW BATTERY", COL_MOVING);
    drawSimBatteryIndicator(18, true, false);
    uiDrawCenteredText("6.95V", CONTENT_Y + 95, FONT_BODY, COL_MOVING);
    uiDrawHint("Charge before pouring", CONTENT_Y + 130);
    audioPlayTone(TONE_ERROR);
    if (waitMs(2000)) return true;

    // Session recovery
    tft->fillScreen(COL_BG);
    drawSimTitleBar("RESUME FLIGHT?", COL_ACCENT);
    uiDrawCenteredText("Named Flight", CONTENT_Y + 30, FONT_BODY, COL_TEXT);
    uiDrawCenteredText("2 of 4 poured", CONTENT_Y + 65, FONT_BODY, COL_ACCENT);
    uiDrawHint("Power was lost mid-flight", CONTENT_Y + 110);
    drawSimSoftButtons("DISCARD", "RESUME");
    audioPlayTone(TONE_SELECT);
    if (waitMs(2500)) return true;

    return false;
}

// ============================================================
// Phase: Closing (brief logo)
// ============================================================

static bool phaseClosing() {
    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawCenteredText("BLIND FLIGHT", SCREEN_H / 2 - 10, FONT_BODY, COL_ACCENT);
    audioPlayTone(TONE_CONFIRM);
    return waitMs(3000);
}

// ============================================================
// Main attract sequence
// ============================================================

static void runAttractSequence() {
    running = true;
    motorDisable();

    while (running) {
        if (phaseSplash())       break;
        if (phaseModeShowcase()) break;
        if (phaseChooseFlight()) break;
        if (phaseBrowseLibrary())break;
        if (phaseTextEntry())    break;
        if (phasePour())         break;
        if (phaseRating())       break;
        if (phaseGuessing())     break;
        if (phaseReveal())       break;
        if (phaseH2H())          break;
        if (phaseFlightClub())   break;
        if (phasePhone())        break;
        if (phaseSettings())     break;
        if (phaseOTA())          break;
        if (phaseBattery())      break;
        if (phaseClosing())      break;
        // Loop back to start
    }

    running = false;
    motorDisable();
}

// ============================================================
// Screen callbacks
// ============================================================

static bool pendingStart = false;

static void attractDraw(bool fullRedraw) {
    if (pendingStart) {
        pendingStart = false;
        runAttractSequence();
        // After attract mode ends, pop back to diagnostics
        uiPopScreenT(TRANS_FADE);
    }
}

static void attractInput(InputEvent evt) {
    // All input is handled inside runAttractSequence
}

static void attractOnEnter() {
    pendingStart = true;
    uiRequestRedraw();
}

const Screen screenAttract = {
    "Attract",
    attractDraw,
    attractInput,
    attractOnEnter
};
