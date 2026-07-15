#pragma once

// ============================================================
// Blind Flight — UI Module
// ============================================================
// Screen manager with push/pop navigation and common drawing
// primitives. Each screen is a struct of function pointers
// (draw, handleInput, onEnter). The screen stack supports
// up to MAX_SCREEN_DEPTH nested screens.
//
// Drawing primitives provide consistent title bars, soft
// button labels, menu items, and text rendering across
// all screens.
//
// Session 6 addition: ScrollList — a reusable scrollable
// list component with encoder-driven selection and a
// scroll position indicator.
//
// Session 7 addition: TextEntry — a reusable character-by-
// character text input component with a visual grid picker
// and live text preview.
//
// Session 9 addition: Transition-aware screen navigation.
// The T-suffixed functions (uiPushScreenT, uiPopScreenT,
// uiReplaceScreenT) run a visual transition around the
// screen swap. The original non-T functions still work as
// instant swaps for backward compatibility.
// ============================================================

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "input.h"
#include "transitions.h"

// --- Screen definition ---
// Each screen provides:
//   draw()        — render the screen (fullRedraw=true on first show)
//   handleInput() — process one input event, may call uiPush/uiPop
//   onEnter()     — called when screen becomes active (optional)

struct Screen {
    const char* name;
    void (*draw)(bool fullRedraw);
    void (*handleInput)(InputEvent evt);
    void (*onEnter)();      // Can be nullptr
};

// --- Screen stack API ---
#define MAX_SCREEN_DEPTH 8

void uiInit(TFT_eSPI* tftPtr);             // Pass the TFT instance
void uiPushScreen(const Screen* screen);    // Navigate forward (instant)
void uiPopScreen();                         // Navigate back (instant)
void uiGoHome();                            // Unwind stack to bottom (home screen)
void uiRequestRedraw();                     // Flag active screen for full redraw
void uiUpdate();                            // Call each loop — drains input events, redraws if needed
const Screen* uiActiveScreen();             // Current top-of-stack (nullptr if empty)

// --- Transition-aware navigation (Session 9) ---
// These run a visual transition synchronously: phase-out over
// the old screen, stack change + onEnter + draw, then phase-in.
// Input events are not processed during the transition.
void uiPushScreenT(const Screen* screen, TransitionType trans);
void uiPopScreenT(TransitionType trans);
void uiReplaceScreenT(const Screen* screen, TransitionType trans);

// --- Drawing primitives ---
// These use the TFT instance passed to uiInit().

// Title bar: full-width colored bar with centered text
void uiDrawTitleBar(const char* title, uint16_t bgColor);

// Soft button labels at bottom of screen
// Pass "" for a side with no button
void uiDrawSoftButtons(const char* leftLabel, const char* rightLabel);

// Menu item: full-width row with text, optional highlight
// (used for simple short lists — for scrollable lists, use ScrollList)
void uiDrawMenuItem(int index, const char* text, bool selected);

// Centered text at a given Y position
void uiDrawCenteredText(const char* text, int y, int fontSize, uint16_t color);

// Centered text with auto-wrap: if text exceeds maxWidth, it wraps to a
// second line (splitting at the last space that fits). Returns the Y
// coordinate below the last line drawn, so callers can position
// subsequent content. Pass 0 for maxWidth to use SCREEN_W - 16.
int uiDrawCenteredTextWrap(const char* text, int y, int fontSize,
                           uint16_t color, int maxWidth = 0);

// Hint text (small, dimmed) centered at Y
void uiDrawHint(const char* text, int y);

// Clear just the content area (between title bar and soft buttons)
void uiClearContent();

// Get the TFT pointer (for screens that need direct drawing)
TFT_eSPI* uiGetTFT();

// --- Idle timer / display timeout ---
void uiResetIdleTimer();       // Call before motor ops or active WS traffic

// ============================================================
// ScrollList — Reusable scrollable list component (Session 6)
// ============================================================
// A scrollable list of text items driven by encoder rotation.
// Manages its own selection cursor, scroll offset, and drawing.
//
// Usage:
//   1. Declare a ScrollList in your screen's static state
//   2. Call uiScrollListInit() in your screen's onEnter
//   3. Call uiScrollListDraw() from your screen's draw function
//   4. Call uiScrollListHandleInput() from your input handler
//      — it returns the selected index on ENC_CLICK, or -1
//
// The list draws within the standard content area (CONTENT_Y
// to SOFT_BTN_Y) and renders a scroll indicator bar on the
// right edge when the list is longer than the visible window.
// ============================================================

#define SCROLL_LIST_MAX_ITEMS 64

struct ScrollList {
    const char** items;     // pointer to array of item labels
    int count;              // total number of items
    int selected;           // cursor position (0-based index into items[])
    int scrollOffset;       // index of first visible item
    int visibleCount;       // how many items fit on screen (auto-calculated)
};

// Initialize list state. Call when entering a screen that uses this list.
// Resets selection to 0 and calculates visible count from layout constants.
void uiScrollListInit(ScrollList* list, const char** items, int count);

// Draw the visible window of items plus scroll indicator.
// Only redraws the content area — caller handles title bar and soft buttons.
void uiScrollListDraw(ScrollList* list);

// Feed an input event to the list.
// Returns: -1 on encoder rotation or non-list events (list redraws itself)
//          >= 0 (the selected index) on ENC_CLICK or BTN_RIGHT
int uiScrollListHandleInput(ScrollList* list, InputEvent evt);

// Read current selection without consuming it
int uiScrollListGetSelection(ScrollList* list);

// ============================================================
// TextEntry — Reusable text input component (Session 7)
// ============================================================
// Character-by-character text entry via a visual grid picker.
// The encoder scrolls through a 10-column grid of characters
// (A–Z, 0–9, space, backspace, done). Encoder click selects
// the highlighted character. The entered text is shown above
// the grid with a blinking cursor.
//
// Usage:
//   1. Declare a TextEntry in your screen's static state
//   2. Call uiTextEntryInit() in onEnter (optionally with seed text)
//   3. Call uiTextEntryDraw() from draw (draws text field + grid)
//   4. Call uiTextEntryHandleInput() from input handler:
//        returns  0  → still editing
//        returns  1  → user confirmed (read text via uiTextEntryGetText)
//        returns -1  → user cancelled
//   5. Soft buttons are drawn by the component — caller only
//      draws the title bar.
//
// Hosting screen is responsible for the title bar. The
// component draws the text field, character grid, and soft
// button labels (which change based on buffer state).
// ============================================================

#define TEXT_ENTRY_MAX_LEN  20

// Grid layout
#define TE_GRID_COLS    10
#define TE_GRID_ROWS    4
#define TE_TOTAL_CHARS  39      // A-Z(26) + 0-9(10) + space + BS + OK
#define TE_IDX_SPACE    36
#define TE_IDX_BS       37
#define TE_IDX_DONE     38

struct TextEntry {
    char buffer[TEXT_ENTRY_MAX_LEN + 1];    // entered text + null terminator
    int  bufLen;                             // current string length
    int  maxLen;                             // configured max (≤ TEXT_ENTRY_MAX_LEN)
    int  charIndex;                          // cursor position in character set (0–38)
};

// Initialize. maxLen is clamped to TEXT_ENTRY_MAX_LEN.
// Pass initial text (or "" for empty). Cursor starts on 'A'.
void uiTextEntryInit(TextEntry* entry, int maxLen, const char* initial);

// Draw text field, character grid, and soft button labels.
// Caller is responsible for the title bar only.
void uiTextEntryDraw(TextEntry* entry);

// Feed an input event.
// Returns:  0  still editing
//           1  confirmed (buffer contains final text)
//          -1  cancelled
int uiTextEntryHandleInput(TextEntry* entry, InputEvent evt);

// Read the current buffer contents (null-terminated).
const char* uiTextEntryGetText(TextEntry* entry);
