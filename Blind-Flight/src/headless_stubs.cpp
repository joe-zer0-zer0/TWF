#ifdef HEADLESS_BUILD

#include "ui.h"
#include "transitions.h"
#include "motor.h"
#include "audio.h"
#include "config.h"
#include "game.h"
#include "h2h.h"
#include "settings.h"

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
// Screen stack — a real one, not a no-op
// ============================================================
// There is no display here, but the screen stack is not only a display
// concern: it is what drives the game.
//
//   * game.cpp's gameDraw() is the deferred-action pump. Every phone
//     action that must not run inside the WebSocket callback —
//     phonePendingStart, phoneNameReady, phonePendingGuess,
//     pendingResumePour — is consumed there, and gameDraw() only ever
//     runs as the active screen's draw().
//   * gameInput() is what advances a flight. The phone's "Done — Poured",
//     "Reveal", "Back" and "Skip" controls all inject INPUT_BTN_RIGHT /
//     INPUT_BTN_LEFT through inputInjectEvent(), and those events reach
//     the game only by being drained here and handed to the active screen.
//
// Through v1.5.4 both were stubbed to nothing, so a headless device would
// accept a phone, authenticate it, and then do nothing at all: no flight
// could start and no pour could be confirmed. The stack below draws
// nothing — it is pure bookkeeping — but the dispatch it enables is the
// entire game loop.

static const Screen* screenStack[MAX_SCREEN_DEPTH];
static int  stackDepth   = 0;
static bool needsRedraw  = false;

static unsigned long lastActivityMs = 0;
static bool          motorIdleOff   = false;

void uiInit(TFT_eSPI*) {
    stackDepth = 0;
    needsRedraw = false;
    lastActivityMs = millis();
}

// NOTE: unlike ui.cpp, push does NOT call draw(). On the screen build the
// synchronous draw is what paints the new screen, and running the pump
// there is harmless. Here the caller is almost always a WebSocket handler
// (gameStartFromPhone), and gameDraw() can start a blocking pour cycle —
// which would run the motor from inside the socket callback. Setting
// needsRedraw instead defers the pump to the next uiUpdate() on loop(),
// which is the same "push the work into the next draw loop" pattern the
// screen build uses for pendingBrowse / phoneNameReady.
void uiPushScreen(const Screen* screen) {
    if (!screen) return;
    if (stackDepth >= MAX_SCREEN_DEPTH) {
        Serial.println("[UI] Screen stack overflow!");
        return;
    }
    screenStack[stackDepth++] = screen;
    Serial.printf("[UI] Push → %s (depth %d)\n", screen->name, stackDepth);

    if (screen->onEnter) screen->onEnter();
    needsRedraw = true;
}

void uiPopScreen() {
    if (stackDepth <= 0) return;
    stackDepth--;
    Serial.printf("[UI] Pop → %s (depth %d)\n",
                  stackDepth > 0 ? screenStack[stackDepth - 1]->name : "(idle)",
                  stackDepth);

    if (stackDepth > 0) {
        const Screen* top = screenStack[stackDepth - 1];
        if (top->onEnter) top->onEnter();
        needsRedraw = true;
    }
}

// The screen build unwinds to stack[0], its home menu. Headless has no
// home screen — the stack starts empty and an empty stack *is* idle,
// which is what the phone renders as "Ready".
void uiGoHome() {
    if (stackDepth == 0) return;
    Serial.printf("[UI] GoHome (depth %d → 0)\n", stackDepth);
    gameAbort();
    stackDepth = 0;
    needsRedraw = false;
}

void uiRequestRedraw() {
    needsRedraw = true;
}

const Screen* uiActiveScreen() {
    if (stackDepth <= 0) return nullptr;
    return screenStack[stackDepth - 1];
}

void uiUpdate() {
    // --- Idle motor release ---
    // Mirrors the IDLE_OFF branch of ui.cpp: a stepper left energised
    // holds current indefinitely, which this battery cannot afford. The
    // backlight/dim tiers above it have no meaning without a display, so
    // only the motor half is kept. Dropping the hold means the disc can
    // be turned by hand, so the tracked position is no longer trustworthy
    // — invalidate it and let the next pour cycle re-home (item 7k).
    unsigned long now = millis();
    if (!motorIdleOff) {
        unsigned long offMs = (unsigned long)settingsGetOffDelay() * 1000UL;
        if (now - lastActivityMs >= offMs) {
            motorDisable();
            gameInvalidateHoming();
            h2hInvalidateHoming();
            motorIdleOff = true;
            Serial.println("[UI] Idle — motor released, position unverified");
        }
    }

    const Screen* active = uiActiveScreen();
    if (!active) return;

    // Drain all pending input events to the active screen. Everything in
    // this queue on a headless build arrived from the phone via
    // inputInjectEvent().
    InputEvent evt;
    while ((evt = inputGetEvent()) != INPUT_NONE) {
        uiResetIdleTimer();

        if (evt == INPUT_BTN_LEFT_LONG && stackDepth > 1) {
            uiGoHome();
            return;
        }

        active->handleInput(evt);

        // The handler may have pushed or popped, so re-fetch before the
        // next event rather than dispatching into a stale screen.
        active = uiActiveScreen();
        if (!active) return;
    }

    if (active->draw) {
        bool full = needsRedraw;
        needsRedraw = false;
        active->draw(full);
    }
}

void uiPushScreenT(const Screen* screen, TransitionType) { uiPushScreen(screen); }
void uiPopScreenT(TransitionType) { uiPopScreen(); }
void uiReplaceScreenT(const Screen* screen, TransitionType) {
    if (!screen) return;
    if (stackDepth > 0) stackDepth--;
    uiPushScreen(screen);
}

void uiDrawTitleBar(const char*, uint16_t) {}
void uiDrawSoftButtons(const char*, const char*) {}
void uiDrawMenuItem(int, const char*, bool) {}
void uiDrawCenteredText(const char*, int, int, uint16_t) {}
int  uiDrawCenteredTextWrap(const char*, int, int, uint16_t, int) { return 0; }
void uiDrawHint(const char*, int) {}
void uiClearContent() {}
TFT_eSPI* uiGetTFT() { return nullptr; }

void uiResetIdleTimer() {
    lastActivityMs = millis();
    motorIdleOff = false;
}

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
        if (motorHome(attempt)) {
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
