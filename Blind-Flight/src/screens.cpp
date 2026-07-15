#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "game.h"
#include "h2h.h"
#include "palate_training.h"
#include "settings.h"
#include "battery.h"
#include "persist.h"

// ============================================================
// Shared state between screens
// ============================================================

// Menu screen — now uses ScrollList
static ScrollList menuList;
static const int MENU_COUNT = 5;
static const char* menuLabels[MENU_COUNT] = {
    "Quick Flight",
    "Full Flight",
    "Ranked Flight",
    "Palate Training",
    "Head to Head"
};

// Detail screen — set by menu before pushing
static const char* detailTitle = "";
static const char* detailNote = "";

// ============================================================
// HOME SCREEN
// ============================================================

static unsigned long homeLastBattRefresh = 0;

static void homeDrawBattery() {
    TFT_eSPI* tft = uiGetTFT();
    int pct = batteryGetPercent();

    int bw = 24, bh = 14;      // body
    int tw = 3, th = 6;        // terminal nub

    // Color by tier
    uint16_t col = COL_SELECTED;
    if (batteryIsLockout())     col = COL_ERROR;
    else if (batteryIsLow())    col = COL_MOVING;

    // Centered below hint text
    int totalW = bw + tw + 4 + 30; // glyph + gap + percent text
    int bx = (SCREEN_W - totalW) / 2;
    int by = 210;

    // Clear the battery area
    tft->fillRect(bx - 4, by - 2, totalW + 8, bh + 4, COL_BG);

    // Battery outline
    tft->drawRect(bx, by, bw, bh, col);
    // Terminal nub
    tft->fillRect(bx + bw, by + (bh - th) / 2, tw, th, col);
    // Fill proportional to percent
    int fillW = (bw - 4) * pct / 100;
    if (fillW > 0) {
        tft->fillRect(bx + 2, by + 2, fillW, bh - 4, col);
    }

    // Percent text right of glyph
    char buf[6];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    tft->setTextColor(col, COL_BG);
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(ML_DATUM);
    tft->drawString(buf, bx + bw + tw + 4, by + bh / 2);
}

static void homeDraw(bool fullRedraw) {
    TFT_eSPI* tft = uiGetTFT();

    if (fullRedraw) {
        tft->fillScreen(COL_BG);
        uiDrawTitleBar("BLIND FLIGHT", COL_ACCENT);

        // Decorative glass position indicators
        int y = 90;
        int boxW = 44, boxH = 44, gap = 10;
        int totalW = 4 * boxW + 3 * gap;
        int startX = (SCREEN_W - totalW) / 2;

        for (int i = 0; i < NUM_GLASSES; i++) {
            int x = startX + i * (boxW + gap);
            tft->fillRoundRect(x, y, boxW, boxH, 6, COL_DIM);
            tft->setTextColor(COL_TEXT, COL_DIM);
            tft->setTextSize(3);
            tft->setTextDatum(MC_DATUM);
            tft->drawString(String(i + 1), x + boxW / 2, y + boxH / 2);
        }

        uiDrawCenteredText("Ready", 160, FONT_BODY, COL_SELECTED);

        if (batteryIsLow()) {
            uiDrawCenteredText("Battery Low", 185, FONT_BODY, COL_MOVING);
        } else {
            uiDrawHint("Press MENU to begin", 190);
        }

        homeDrawBattery();

        uiDrawSoftButtons("SETTINGS", "MENU");
        homeLastBattRefresh = millis();
        return;
    }

    // Partial redraw: refresh battery indicator every 10s
    if (millis() - homeLastBattRefresh >= 10000) {
        homeLastBattRefresh = millis();
        homeDrawBattery();
    }
}

static void homeInput(InputEvent evt) {
    switch (evt) {
        case INPUT_BTN_RIGHT:
        case INPUT_ENC_CLICK:
            audioPlayTone(TONE_SELECT);
            uiPushScreen(&screenMenu);
            break;

        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            uiPushScreenT(&screenSettings, TRANS_WIPE_LEFT);
            break;

        default:
            break;
    }
}

const Screen screenHome = {
    "Home",
    homeDraw,
    homeInput,
    nullptr
};

// ============================================================
// MENU SCREEN
// ============================================================
// Now uses the ScrollList component. With 4 items and 4
// visible slots, all items are visible without scrolling.
// ============================================================

static void menuDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar("SELECT MODE", COL_HOME);
    uiScrollListDraw(&menuList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void menuOnEnter() {
    uiScrollListInit(&menuList, menuLabels, MENU_COUNT);
}

static void menuInput(InputEvent evt) {
    // Let ScrollList handle encoder rotation and click
    int sel = uiScrollListHandleInput(&menuList, evt);

    if (sel >= 0) {
        audioPlayTone(TONE_CONFIRM);

        if (sel == 0) {
            gameSetMode(GAME_MODE_BASIC);
            uiPushScreenT(&screenGame, TRANS_FADE);
        } else if (sel == 1) {
            gameSetMode(GAME_MODE_NAMED);
            uiPushScreen(&screenGame);
        } else if (sel == 2) {
            gameSetMode(GAME_MODE_RANK);
            uiPushScreen(&screenGame);
        } else if (sel == 3) {
            uiPushScreen(&screenPalateTraining);
        } else if (sel == 4) {
            uiPushScreen(&screenH2H);
        }
        return;
    }

    // Handle non-list events
    switch (evt) {
        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            uiPopScreen();
            break;

        default:
            break;
    }
}

const Screen screenMenu = {
    "Menu",
    menuDraw,
    menuInput,
    menuOnEnter
};

// ============================================================
// DETAIL SCREEN (placeholder)
// ============================================================

static void detailDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar(detailTitle, COL_MOVING);

    uiDrawCenteredText("Coming in", 90, FONT_BODY, COL_DIM);
    uiDrawCenteredText(detailNote, 120, FONT_LARGE, COL_ACCENT);

    int boxY = 160;
    int boxH = 40;
    tft->fillRoundRect(20, boxY, SCREEN_W - 40, boxH, 6, COL_HIGHLIGHT);
    tft->setTextColor(COL_TEXT, COL_HIGHLIGHT);
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(detailTitle, SCREEN_W / 2, boxY + boxH / 2);

    uiDrawHint("This mode is not yet implemented", 220);

    uiDrawSoftButtons("BACK", "");
}

static void detailInput(InputEvent evt) {
    switch (evt) {
        case INPUT_BTN_LEFT:
        case INPUT_ENC_CLICK:
            audioPlayTone(TONE_SELECT);
            uiPopScreen();
            break;

        default:
            break;
    }
}

const Screen screenDetail = {
    "Detail",
    detailDraw,
    detailInput,
    nullptr
};

// ============================================================
// HOMING SEQUENCE (blocking helper)
// ============================================================

bool runHomingSequence() {
    TFT_eSPI* tft = uiGetTFT();

    while (true) {
        tft->fillScreen(COL_BG);
        uiDrawTitleBar("HOMING", COL_HOME);
        uiDrawCenteredText("Searching for", 100, FONT_BODY, COL_TEXT);
        uiDrawCenteredText("home position", 125, FONT_BODY, COL_TEXT);
        uiDrawHint("Motor is scanning...", 170);

        unsigned long waitStart = millis();
        while (millis() - waitStart < 300) {
            audioUpdate();
            delay(1);
        }

        Serial.println("[Homing] Starting...");
        bool found = motorHome();

        if (found) {
            Serial.println("[Homing] Home found!");
            audioPlayTone(TONE_HOME_FOUND);

            tft->fillScreen(COL_BG);
            uiDrawTitleBar("HOMED", COL_SELECTED);
            uiDrawCenteredText("Home found", 110, FONT_BODY, COL_SELECTED);

            unsigned long doneWait = millis();
            while (millis() - doneWait < 800) {
                audioUpdate();
                delay(1);
            }
            return true;

        } else {
            Serial.println("[Homing] FAILED — magnet not detected");
            audioPlayTone(TONE_ERROR);

            tft->fillScreen(COL_BG);
            uiDrawTitleBar("HOME FAILED", COL_ERROR);
            uiDrawCenteredText("Magnet not", 90, FONT_BODY, COL_TEXT);
            uiDrawCenteredText("detected", 115, FONT_BODY, COL_TEXT);
            uiDrawHint("Press any button to retry", 160);
            uiDrawSoftButtons("RETRY", "");

            inputUpdate();
            while (inputGetEvent() != INPUT_NONE) {}

            bool retry = false;
            while (!retry) {
                audioUpdate();
                inputUpdate();
                InputEvent evt = inputGetEvent();
                if (evt == INPUT_BTN_LEFT || evt == INPUT_BTN_RIGHT ||
                    evt == INPUT_ENC_CLICK) {
                    audioPlayTone(TONE_SELECT);
                    retry = true;
                }
                delay(10);
            }
            // Loop back to top and try homing again.
        }
    }
}
