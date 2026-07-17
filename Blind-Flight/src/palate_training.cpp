#include "palate_training.h"
#include "config.h"
#include "audio.h"
#include "game.h"
#include "screens.h"

static ScrollList ptList;
static const int PT_MODE_COUNT = 4;
static const char* ptLabels[PT_MODE_COUNT] = {
    "Best Guess",
    "Guess + Ranked",
    "Twin Pour",
    "Find the Ringer"
};

static bool ptActive = false;

static void ptDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);
    uiDrawTitleBar("PALATE TRAINING", COL_ACCENT);
    uiScrollListDraw(&ptList);
    uiDrawSoftButtons("BACK", "SELECT");
}

static void ptInput(InputEvent evt) {
    int sel = uiScrollListHandleInput(&ptList, evt);

    if (sel >= 0) {
        audioPlayTone(TONE_CONFIRM);
        if (sel == 0) {
            gameSetMode(GAME_MODE_GUESS);
        } else if (sel == 1) {
            gameSetMode(GAME_MODE_GUESS_RANK);
        } else if (sel == 2) {
            gameSetMode(GAME_MODE_DUPLICATE);
        } else {
            gameSetMode(GAME_MODE_DECOY);
        }
        uiPushScreen(&screenGame);
        return;
    }

    if (evt == INPUT_BTN_LEFT) {
        audioPlayTone(TONE_SELECT);
        ptActive = false;
        uiPopScreenT(TRANS_FADE);
    }
}

static void ptOnEnter() {
    if (ptActive) return;
    ptActive = true;
    uiScrollListInit(&ptList, ptLabels, PT_MODE_COUNT);
    uiRequestRedraw();
}

const Screen screenPalateTraining = {
    "PalateTraining",
    ptDraw,
    ptInput,
    ptOnEnter
};
