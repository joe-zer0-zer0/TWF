#include "screens.h"
#include "config.h"
#include "audio.h"
#include "motor.h"
#include "input.h"
#include "game.h"
#include "settings.h"

// ============================================================
// Shared state between screens
// ============================================================

// Menu screen — now uses ScrollList
static ScrollList menuList;
static const int MENU_COUNT = 5;
static const char* menuLabels[MENU_COUNT] = {
    "Basic Flight",
    "Full Flight",
    "Best Guess",
    "Ranked Flight",
    "Settings"
};
static const char* menuSessionNotes[MENU_COUNT] = {
    "Session 10",
    "Session 12",
    "Session 13",
    "Session 20",
    "Session 17"
};

// Detail screen — set by menu before pushing
static const char* detailTitle = "";
static const char* detailNote = "";

// List demo — 24 items to exercise scrolling
static ScrollList demoList;
static const int DEMO_COUNT = 24;
static const char* demoLabels[DEMO_COUNT] = {
    "Bourbon",
    "Rye Whiskey",
    "Scotch Single Malt",
    "Scotch Blend",
    "Irish Whiskey",
    "Japanese Whisky",
    "Canadian Whisky",
    "Tennessee Whiskey",
    "Corn Whiskey",
    "Wheat Whiskey",
    "Malt Whiskey",
    "Pot Still Whiskey",
    "Blended Malt",
    "Cask Strength",
    "Single Barrel",
    "Small Batch",
    "Bottled in Bond",
    "Peated",
    "Sherried",
    "Port Finish",
    "Rum Cask",
    "Wine Cask",
    "Double Barrel",
    "Infinity Bottle"
};

// ============================================================
// HOME SCREEN
// ============================================================

static void homeDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar("BLIND FLIGHT", COL_ACCENT);

    // Decorative glass position indicators
    int y = 90;
    int boxW = 44;
    int boxH = 44;
    int gap = 10;
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
    uiDrawHint("Press MENU to begin", 190);

    uiDrawSoftButtons("HOME", "MENU");
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
            runHomingSequence();
            uiRequestRedraw();
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
        // Item selected
        audioPlayTone(TONE_CONFIRM);

        if (sel == 0) {
            gameSetMode(GAME_MODE_BASIC);
            uiPushScreenT(&screenGame, TRANS_FADE);
        } else if (sel == 1) {
            gameSetMode(GAME_MODE_NAMED);
            uiPushScreen(&screenGame);
        } else if (sel == 2) {
            gameSetMode(GAME_MODE_GUESS);
            uiPushScreen(&screenGame);
        } else if (sel == 3) {
            gameSetMode(GAME_MODE_RANK);
            uiPushScreen(&screenGame);
        } else if (sel == 4) {
            uiPushScreenT(&screenSettings, TRANS_WIPE_LEFT);
        } else {
            // Other modes — push detail placeholder
            detailTitle = menuLabels[sel];
            detailNote = menuSessionNotes[sel];
            uiPushScreen(&screenDetail);
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
// LIST DEMO SCREEN (Session 6)
// ============================================================
// 24-item scrollable list of whiskey categories.
// Selecting an item pushes the detail screen showing the
// selected item name. Exercises the full ScrollList API:
// scrolling, scroll bar, selection, and clamping.
// ============================================================

static void listDemoDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    uiDrawTitleBar("WHISKEY TYPES", COL_ACCENT);
    uiScrollListDraw(&demoList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void listDemoOnEnter() {
    uiScrollListInit(&demoList, demoLabels, DEMO_COUNT);
}

static void listDemoInput(InputEvent evt) {
    int sel = uiScrollListHandleInput(&demoList, evt);

    if (sel >= 0) {
        audioPlayTone(TONE_CONFIRM);
        detailTitle = demoLabels[sel];
        detailNote = "Selected!";
        uiPushScreen(&screenDetail);
        return;
    }

    switch (evt) {
        case INPUT_BTN_LEFT:
            audioPlayTone(TONE_SELECT);
            uiPopScreen();
            break;

        default:
            break;
    }
}

const Screen screenListDemo = {
    "ListDemo",
    listDemoDraw,
    listDemoInput,
    listDemoOnEnter
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
