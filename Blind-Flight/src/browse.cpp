#include "browse.h"
#include "categories.h"
#include "favorites.h"
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
static const LibraryEntry* resultEntry;  // library entry, or nullptr (Session 21)

// Manual entry via TextEntry (Session 12 revision)
static bool textEntryMode = false;
static TextEntry textEntry;
static char manualNameBuf[TEXT_ENTRY_MAX_LEN + 1];  // persists after pop

// "Save to Favorites?" prompt after manual entry
static bool favPromptMode = false;

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

// The type list has "Manual Entry" at index 0, "Favorites" at index 1,
// then the whiskey types starting at index 2.
#define TYPE_OFFSET 2

// Favorites sublevel: reuse browseLabels with pointers into favNames
static const char* favLabelPtrs[FAV_MAX_COUNT];

static void buildLevel() {
    browseCount = 0;

    switch (browseLevel) {
        case 0:
            browseLabels[browseCount++] = "-- Manual Entry --";
            browseLabels[browseCount++] = "-- Favorites --";
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
                browseLabels[i] = wd.products[i].name;
                browseCount++;
            }
            break;
        }

        case 10: {
            // Favorites flat list
            int fc = favoritesGetCount();
            if (fc == 0) {
                browseLabels[browseCount++] = "(No favorites saved)";
            } else {
                for (int i = 0; i < fc && browseCount < BROWSE_MAX_ITEMS; i++) {
                    favLabelPtrs[i] = favoritesGetName(i);
                    browseLabels[browseCount++] = favLabelPtrs[i];
                }
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
    resultEntry    = nullptr;
    textEntryMode  = false;
    favPromptMode  = false;
}

const char* browseGetResult() {
    return resultName;
}

const LibraryEntry* browseGetEntry() {
    return resultEntry;
}

static void selectProduct(int idx) {
    const WhiskeyDistillery& wd =
        WHISKEY_TYPES[selectedType].distilleries[selectedDist];
    resultName  = wd.products[idx].name;
    resultEntry = &wd.products[idx];
}

static void selectNonProduct(const char* name) {
    resultName  = name;
    resultEntry = nullptr;
}

// ============================================================
// Screen callbacks
// ============================================================

static void browseDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();
    tft->fillScreen(COL_BG);

    // --- "Save to Favorites?" prompt ---
    if (favPromptMode) {
        uiDrawTitleBar("SAVE FAVORITE?", COL_ACCENT);
        uiDrawCenteredText(manualNameBuf, 110, FONT_BODY, COL_TEXT);
        uiDrawHint("Save to favorites list?", 150);
        uiDrawSoftButtons("NO", "SAVE");
        return;
    }

    // --- Text entry mode ---
    if (textEntryMode) {
        uiDrawTitleBar("ENTER NAME", COL_ACCENT);
        uiTextEntryDraw(&textEntry);
        return;
    }

    // --- Normal list mode ---
    if (browseLevel == 10) {
        uiDrawTitleBar("FAVORITES", COL_ACCENT);
    } else {
        uiDrawTitleBar(browseTitles[browseLevel], COL_ACCENT);
    }
    uiScrollListDraw(&browseList);

    if (browseLevel == 10) {
        if (favoritesGetCount() == 0) {
            uiDrawSoftButtons("BACK", "");
        } else {
            uiDrawSoftButtons("BACK", "SELECT");
        }
    } else if (browseLevel < 2) {
        uiDrawSoftButtons("BACK", "USE THIS");
    } else {
        uiDrawSoftButtons("BACK", "SELECT");
    }
}

static void browseInput(InputEvent evt) {

    // --- "Save to Favorites?" prompt ---
    if (favPromptMode) {
        if (evt == INPUT_BTN_RIGHT) {
            // Save and continue
            favoritesAdd(manualNameBuf);
            audioPlayTone(TONE_CONFIRM);
            favPromptMode = false;
            selectNonProduct(manualNameBuf);
            uiPopScreen();
        } else if (evt == INPUT_BTN_LEFT || evt == INPUT_ENC_CLICK) {
            // Don't save, just use the name
            audioPlayTone(TONE_SELECT);
            favPromptMode = false;
            selectNonProduct(manualNameBuf);
            uiPopScreen();
        }
        return;
    }

    // --- Text entry mode ---
    if (textEntryMode) {
        int result = uiTextEntryHandleInput(&textEntry, evt);
        if (result == 1) {
            // Confirmed — copy text
            const char* text = uiTextEntryGetText(&textEntry);
            strncpy(manualNameBuf, text, TEXT_ENTRY_MAX_LEN);
            manualNameBuf[TEXT_ENTRY_MAX_LEN] = '\0';
            textEntryMode = false;

            // Offer to save if not already a favorite and list isn't full
            if (manualNameBuf[0] &&
                !favoritesContains(manualNameBuf) &&
                favoritesGetCount() < FAV_MAX_COUNT) {
                favPromptMode = true;
                uiRequestRedraw();
            } else {
                selectNonProduct(manualNameBuf);
                uiPopScreen();
            }
        } else if (result == -1) {
            // Cancelled — back to type list
            textEntryMode = false;
            browseLevel = 0;
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
                // "Manual Entry"
                audioPlayTone(TONE_CONFIRM);
                uiTextEntryInit(&textEntry, TEXT_ENTRY_MAX_LEN, "");
                textEntryMode = true;
                uiRequestRedraw();
                return;
            }
            if (sel == 1) {
                // "Favorites" — open flat list
                audioPlayTone(TONE_CONFIRM);
                browseLevel = 10;
                buildLevel();
                uiRequestRedraw();
                return;
            }
            // Drill into distilleries
            audioPlayTone(TONE_CONFIRM);
            selectedType = sel - TYPE_OFFSET;
            browseLevel = 1;
            buildLevel();
            uiRequestRedraw();
            return;
        }
        else if (browseLevel == 10) {
            // Favorites list — select a favorite
            if (favoritesGetCount() == 0) return;
            audioPlayTone(TONE_CONFIRM);
            selectNonProduct(browseLabels[sel]);
            uiPopScreen();
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
            selectProduct(sel);
            uiPopScreen();
            return;
        }
    }

    // Right button: USE THIS / SELECT
    if (evt == INPUT_BTN_RIGHT) {
        int idx = uiScrollListGetSelection(&browseList);
        if (browseLevel == 0 && idx == 0) {
            // "Manual Entry"
            audioPlayTone(TONE_CONFIRM);
            uiTextEntryInit(&textEntry, TEXT_ENTRY_MAX_LEN, "");
            textEntryMode = true;
            uiRequestRedraw();
            return;
        }
        if (browseLevel == 0 && idx == 1) {
            // "Favorites" — open flat list
            audioPlayTone(TONE_CONFIRM);
            browseLevel = 10;
            buildLevel();
            uiRequestRedraw();
            return;
        }
        if (browseLevel == 10) {
            if (favoritesGetCount() == 0) return;
            audioPlayTone(TONE_CONFIRM);
            selectNonProduct(browseLabels[idx]);
            uiPopScreen();
            return;
        }
        audioPlayTone(TONE_CONFIRM);
        if (browseLevel == 2) {
            selectProduct(idx);
        } else {
            selectNonProduct(browseLabels[idx]);
        }
        uiPopScreen();
        return;
    }

    // Left button: BACK
    if (evt == INPUT_BTN_LEFT) {
        audioPlayTone(TONE_SELECT);
        if (browseLevel == 10) {
            // Back from favorites to type list
            browseLevel = 0;
            buildLevel();
            uiRequestRedraw();
        } else if (browseLevel > 0) {
            browseLevel--;
            buildLevel();
            uiRequestRedraw();
        } else {
            resultName  = nullptr;
            resultEntry = nullptr;
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
