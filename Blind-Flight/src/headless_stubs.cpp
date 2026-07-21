#ifdef HEADLESS_BUILD

#include "ui.h"
#include "transitions.h"
#include "motor.h"
#include "audio.h"
#include "config.h"

// ============================================================
// Headless stubs — no-op implementations of screen-only APIs
// ============================================================
// In the headless (phone-only) build, these modules are excluded
// via src_filter: ui.cpp, transitions.cpp, screens.cpp, splash.cpp,
// browse.cpp, h2h.cpp, palate_training.cpp, screen_*.cpp.
//
// This file provides the minimal stubs so shared code (game.cpp,
// wifi_portal.cpp, main.cpp, persist.cpp) links without errors.

// ============================================================
// UI stubs
// ============================================================

static bool redrawRequested = false;

void uiInit(TFT_eSPI*) {}
void uiPushScreen(const Screen*) {}
void uiPopScreen() {}
void uiGoHome() {}

void uiRequestRedraw() {
    redrawRequested = true;
}

void uiUpdate() {
    if (redrawRequested) {
        redrawRequested = false;
    }
}

const Screen* uiActiveScreen() { return nullptr; }

void uiPushScreenT(const Screen* screen, TransitionType) { uiPushScreen(screen); }
void uiPopScreenT(TransitionType) { uiPopScreen(); }
void uiReplaceScreenT(const Screen*, TransitionType) {}

void uiDrawTitleBar(const char*, uint16_t) {}
void uiDrawSoftButtons(const char*, const char*) {}
void uiDrawMenuItem(int, const char*, bool) {}
void uiDrawCenteredText(const char*, int, int, uint16_t) {}
int  uiDrawCenteredTextWrap(const char*, int, int, uint16_t, int) { return 0; }
void uiDrawHint(const char*, int) {}
void uiClearContent() {}
TFT_eSPI* uiGetTFT() { return nullptr; }
void uiResetIdleTimer() {}

void uiScrollListInit(ScrollList* list, const char** items, int count) {
    list->items = items;
    list->count = count;
    list->selected = 0;
    list->scrollOffset = 0;
    list->visibleCount = count;
}
void uiScrollListDraw(ScrollList*) {}
int  uiScrollListHandleInput(ScrollList*, InputEvent) { return -1; }
int  uiScrollListGetSelection(ScrollList* list) { return list->selected; }

void uiTextEntryInit(TextEntry* entry, int maxLen, const char* initial) {
    entry->bufLen = 0;
    entry->maxLen = (maxLen > TEXT_ENTRY_MAX_LEN) ? TEXT_ENTRY_MAX_LEN : maxLen;
    entry->charIndex = 0;
    if (initial && initial[0]) {
        strncpy(entry->buffer, initial, entry->maxLen);
        entry->buffer[entry->maxLen] = '\0';
        entry->bufLen = strlen(entry->buffer);
    } else {
        entry->buffer[0] = '\0';
    }
}
void uiTextEntryDraw(TextEntry*) {}
int  uiTextEntryHandleInput(TextEntry*, InputEvent) { return 0; }
const char* uiTextEntryGetText(TextEntry* entry) { return entry->buffer; }

// ============================================================
// Transition stubs
// ============================================================

void transInit() {}
void transSetTFT(TFT_eSPI*) {}
void setBacklight(int) {}
int  getBacklight() { return 0; }
void transRunOut(TransitionType) {}
void transRunIn(TransitionType) {}
void transSlotRoll(int, int, int, int, const char*, uint16_t, uint16_t, int) {}

// ============================================================
// Homing sequence (headless version)
// ============================================================
// Calls motorHome() with buzzer feedback. Auto-retries up to 3
// times on failure (no screen to prompt the user).

bool runHomingSequence() {
    const int MAX_RETRIES = 3;
    for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
        Serial.printf("[Headless] Homing attempt %d/%d\n", attempt + 1, MAX_RETRIES);
        if (motorHome()) {
            audioPlayTone(TONE_HOME_FOUND);
            Serial.println("[Headless] Homing succeeded");
            return true;
        }
        audioPlayTone(TONE_ERROR);
        Serial.println("[Headless] Homing failed, retrying...");
        delay(500);
    }
    Serial.println("[Headless] Homing failed after all retries");
    return false;
}

// ============================================================
// Screen externs referenced by shared code
// ============================================================
// game.cpp references screenBrowse (via browse.h, excluded).
// The screenGame extern is defined in game.cpp itself.
// No other screen externs are needed in headless.

#endif // HEADLESS_BUILD
