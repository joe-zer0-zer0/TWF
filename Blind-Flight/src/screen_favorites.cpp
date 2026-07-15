#include "screens.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "favorites.h"

static ScrollList favList;
static const char* favLabels[FAV_MAX_COUNT];
static int favListCount = 0;

static bool deleteConfirm = false;
static int  deleteIndex   = 0;

static void rebuildList() {
    favListCount = favoritesGetCount();
    for (int i = 0; i < favListCount; i++) {
        favLabels[i] = favoritesGetName(i);
    }
}

static void favDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    char title[20];
    snprintf(title, sizeof(title), "FAVORITES (%d/%d)", favoritesGetCount(), FAV_MAX_COUNT);
    uiDrawTitleBar(title, COL_ACCENT);

    if (deleteConfirm) {
        uiDrawCenteredText("Delete?", 90, FONT_BODY, COL_TEXT);
        uiDrawCenteredText(favLabels[deleteIndex], 120, FONT_BODY, COL_ACCENT);
        uiDrawSoftButtons("NO", "YES");
        return;
    }

    if (favListCount == 0) {
        uiDrawCenteredText("(No favorites)", 120, FONT_BODY, COL_DIM);
        uiDrawSoftButtons("BACK", "");
        return;
    }

    uiScrollListDraw(&favList);
    uiDrawSoftButtons("BACK", "DELETE");
}

static void favOnEnter() {
    deleteConfirm = false;
    rebuildList();
    if (favListCount > 0) {
        uiScrollListInit(&favList, favLabels, favListCount);
    }
}

static void favInput(InputEvent evt) {
    if (deleteConfirm) {
        if (evt == INPUT_BTN_RIGHT || evt == INPUT_ENC_CLICK) {
            audioPlayTone(TONE_CONFIRM);
            favoritesRemove(deleteIndex);
            deleteConfirm = false;
            rebuildList();
            if (favListCount > 0) {
                uiScrollListInit(&favList, favLabels, favListCount);
            }
            uiRequestRedraw();
        } else if (evt == INPUT_BTN_LEFT) {
            audioPlayTone(TONE_SELECT);
            deleteConfirm = false;
            uiRequestRedraw();
        }
        return;
    }

    if (favListCount == 0) {
        if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
            audioPlayTone(TONE_SELECT);
            uiPopScreenT(TRANS_WIPE_LEFT);
        }
        return;
    }

    int sel = uiScrollListHandleInput(&favList, evt);

    if (evt == INPUT_BTN_RIGHT) {
        int idx = uiScrollListGetSelection(&favList);
        audioPlayTone(TONE_SELECT);
        deleteIndex = idx;
        deleteConfirm = true;
        uiRequestRedraw();
        return;
    }

    if (sel >= 0 && evt == INPUT_ENC_CLICK) {
        audioPlayTone(TONE_SELECT);
        deleteIndex = sel;
        deleteConfirm = true;
        uiRequestRedraw();
        return;
    }

    if (evt == INPUT_BTN_LEFT) {
        audioPlayTone(TONE_SELECT);
        uiPopScreenT(TRANS_WIPE_LEFT);
        return;
    }
}

const Screen screenFavorites = {
    "Favorites",
    favDraw,
    favInput,
    favOnEnter
};
