#include "browse.h"
#include "categories.h"
#include "config.h"
#include "audio.h"
#include "input.h"

// ============================================================
// Blind Flight — Browse Screen (Session 12)
// ============================================================
// Three-level hierarchy: Type → Distillery → Product
// Plus "Manual Entry" at the top of the type list for
// free-form text input via the TextEntry component.
// ============================================================

// --- Module state ---
static int browseLevel;          // 0=types, 1=distilleries, 2=products
static int selectedType;         // index into WHISKEY_TYPES[]
static int selectedDist;         // index into distilleries[]
static const char* resultName;   // selected name, or nullptr

// Manual entry via TextEntry (Session 12 revision)
static bool textEntryMode = false;
static TextEntry textEntry;
static char manualNameBuf[TEXT_ENTRY_MAX_LEN + 1];  // persists after pop

// Dynamic label array for ScrollList
#define BROWSE_MAX_ITEMS 64
static const char* browseLabels[BROWSE_MAX_ITEMS];
static int browseCount;

static ScrollList browseList;

static const char* browseTitles[] = {
    "WHISKEY TYPE", "DISTILLERY", "PRODUCT"
};

// ============================================================
// Helpers
// ============================================================

// The type list has "Manual Entry" at index 0, then the
// whiskey types starting at index 1. This offset converts
// a list selection to a WHISKEY_TYPES[] index.
#define TYPE_OFFSET 1

static void buildLevel() {
    browseCount = 0;

    switch (browseLevel) {
        case 0:
            // "Manual Entry" first, then whiskey types
            browseLabels[browseCount++] = "-- Manual Entry --";
            for (int i = 0; i < WHISKEY_TYPE_COUNT && browseCount < BROWSE_MAX_ITEMS; i++) {
                browseLabels[browseCount++] = WHISKEY_TYPES[i].name;
            }
            break;

        case 1: {
            const WhiskeyType& wt = WHISKEY_TYPES[selectedType];
            for (int i = 0; i < wt.distilleryCount && i < BROWSE_MAX_ITEMS; i++) {
                browseLabels[i] = wt.distilleries[i].name;
                browseCount++;
            }
            break;
        }

        case 2: {
            const WhiskeyDistillery& wd =
                WHISKEY_TYPES[selectedType].distilleries[selectedDist];
            for (int i = 0; i < wd.productCount && i < BROWSE_MAX_ITEMS; i++) {
                browseLabels[i] = wd.products[i];
                browseCount++;
            }
            break;
        }
    }

    uiScrollListInit(&browseList, browseLabels, browseCount);
}

// ============================================================
// Public API
// ============================================================

void browseReset() {
    browseLevel    = 0;
    selectedType   = 0;
    selectedDist   = 0;
    resultName     = nullptr;
    textEntryMode  = false;
}

const char* browseGetResult() {
    return resultName;
}

// ============================================================
// Screen callbacks
// ============================================================

static void browseDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    // --- Text entry mode ---
    if (textEntryMode) {
        uiDrawTitleBar("ENTER NAME", COL_ACCENT);
        uiTextEntryDraw(&textEntry);
        return;
    }

    // --- Normal list mode ---
    uiDrawTitleBar(browseTitles[browseLevel], COL_ACCENT);
    uiScrollListDraw(&browseList);

    if (browseLevel < 2) {
        uiDrawSoftButtons("BACK", "USE THIS");
    } else {
        uiDrawSoftButtons("BACK", "SELECT");
    }
}

static void browseInput(InputEvent evt) {

    // --- Text entry mode ---
    if (textEntryMode) {
        int result = uiTextEntryHandleInput(&textEntry, evt);
        if (result == 1) {
            // Confirmed — copy text and pop back to game
            const char* text = uiTextEntryGetText(&textEntry);
            strncpy(manualNameBuf, text, TEXT_ENTRY_MAX_LEN);
            manualNameBuf[TEXT_ENTRY_MAX_LEN] = '\0';
            resultName = manualNameBuf;
            textEntryMode = false;
            uiPopScreen();
        } else if (result == -1) {
            // Cancelled — back to type list
            textEntryMode = false;
            buildLevel();
            uiRequestRedraw();
        }
        return;
    }

    // --- Normal list mode ---
    int sel = uiScrollListHandleInput(&browseList, evt);

    // Encoder click: drill down (or select at product level)
    if (sel >= 0 && evt == INPUT_ENC_CLICK) {
        if (browseLevel == 0) {
            if (sel == 0) {
                // "Manual Entry" — switch to text entry mode
                audioPlayTone(TONE_CONFIRM);
                uiTextEntryInit(&textEntry, TEXT_ENTRY_MAX_LEN, "");
                textEntryMode = true;
                uiRequestRedraw();
                return;
            }
            // Drill into distilleries (offset by 1 for Manual Entry)
            audioPlayTone(TONE_CONFIRM);
            selectedType = sel - TYPE_OFFSET;
            browseLevel = 1;
            buildLevel();
            uiRequestRedraw();
            return;
        }
        else if (browseLevel == 1) {
            audioPlayTone(TONE_CONFIRM);
            selectedDist = sel;
            browseLevel = 2;
            buildLevel();
            uiRequestRedraw();
            return;
        }
        else {
            // Product level — encoder click selects
            audioPlayTone(TONE_CONFIRM);
            resultName = browseLabels[sel];
            uiPopScreen();
            return;
        }
    }

    // Right button: USE THIS (accept current highlight as name)
    if (evt == INPUT_BTN_RIGHT) {
        int idx = uiScrollListGetSelection(&browseList);
        if (browseLevel == 0 && idx == 0) {
            // "Manual Entry" — enter text entry mode
            audioPlayTone(TONE_CONFIRM);
            uiTextEntryInit(&textEntry, TEXT_ENTRY_MAX_LEN, "");
            textEntryMode = true;
            uiRequestRedraw();
            return;
        }
        audioPlayTone(TONE_CONFIRM);
        resultName = browseLabels[idx];
        uiPopScreen();
        return;
    }

    // Left button: BACK
    if (evt == INPUT_BTN_LEFT) {
        audioPlayTone(TONE_SELECT);
        if (browseLevel > 0) {
            browseLevel--;
            buildLevel();
            uiRequestRedraw();
        } else {
            resultName = nullptr;
            uiPopScreen();
        }
        return;
    }
}

static void browseOnEnter() {
    if (!textEntryMode) {
        buildLevel();
    }
}

// ============================================================
// Screen definition
// ============================================================

const Screen screenBrowse = {
    "Browse",
    browseDraw,
    browseInput,
    browseOnEnter
};
