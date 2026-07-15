#include "ui.h"
#include "config.h"
#include "audio.h"
#include "game.h"
#include "motor.h"
#include "settings.h"
#include "transitions.h"

// ============================================================
// Internal state
// ============================================================
static TFT_eSPI* tft = nullptr;

static const Screen* screenStack[MAX_SCREEN_DEPTH];
static int stackDepth = 0;
static bool needsRedraw = false;

// --- Idle timer state ---
enum IdleState { IDLE_ACTIVE, IDLE_DIM, IDLE_OFF };
static IdleState idleState = IDLE_ACTIVE;
static unsigned long lastActivityMs = 0;

// ============================================================
// Screen stack management
// ============================================================

void uiInit(TFT_eSPI* tftPtr) {
    tft = tftPtr;
    stackDepth = 0;
    needsRedraw = false;
    lastActivityMs = millis();
    idleState = IDLE_ACTIVE;

    // Share TFT pointer with transition module
    transSetTFT(tftPtr);
}

void uiPushScreen(const Screen* screen) {
    if (stackDepth >= MAX_SCREEN_DEPTH) {
        Serial.println("[UI] Screen stack overflow!");
        return;
    }
    screenStack[stackDepth] = screen;
    stackDepth++;
    needsRedraw = true;

    Serial.printf("[UI] Push: %s (depth %d)\n", screen->name, stackDepth);

    if (screen->onEnter) {
        screen->onEnter();
    }
}

void uiPopScreen() {
    if (stackDepth <= 0) {
        Serial.println("[UI] Screen stack underflow!");
        return;
    }
    stackDepth--;
    Serial.printf("[UI] Pop → %s (depth %d)\n",
                  stackDepth > 0 ? screenStack[stackDepth - 1]->name : "(empty)",
                  stackDepth);

    if (stackDepth > 0) {
        needsRedraw = true;
        const Screen* top = screenStack[stackDepth - 1];
        if (top->onEnter) {
            top->onEnter();
        }
    }
}

void uiGoHome() {
    if (stackDepth <= 1) return;

    Serial.printf("[UI] GoHome (depth %d → 1)\n", stackDepth);
    gameAbort();
    stackDepth = 1;
    needsRedraw = true;

    const Screen* home = screenStack[0];
    if (home->onEnter) {
        home->onEnter();
    }
}

void uiRequestRedraw() {
    needsRedraw = true;
}

const Screen* uiActiveScreen() {
    if (stackDepth <= 0) return nullptr;
    return screenStack[stackDepth - 1];
}

// ============================================================
// Transition-aware navigation (Session 9)
// ============================================================
// These run synchronously: the caller blocks until the
// transition + draw completes. Input events that arrive
// during the animation are left in the queue for uiUpdate().
// ============================================================

void uiPushScreenT(const Screen* screen, TransitionType trans) {
    if (stackDepth >= MAX_SCREEN_DEPTH) {
        Serial.println("[UI] Screen stack overflow!");
        return;
    }

    Serial.printf("[UI] PushT: %s (trans %d, depth %d→%d)\n",
                  screen->name, (int)trans, stackDepth, stackDepth + 1);

    // Phase out: animate over current screen content
    transRunOut(trans);

    // Stack change
    screenStack[stackDepth] = screen;
    stackDepth++;

    // onEnter (sets up screen state, should not draw)
    if (screen->onEnter) {
        screen->onEnter();
    }

    // Draw new screen (display may be dark/black at this point)
    if (screen->draw) {
        screen->draw(true);
    }

    // Phase in: reveal new content
    transRunIn(trans);

    // Already drawn — suppress the deferred redraw
    needsRedraw = false;
}

void uiPopScreenT(TransitionType trans) {
    if (stackDepth <= 0) {
        Serial.println("[UI] Screen stack underflow!");
        return;
    }

    Serial.printf("[UI] PopT (trans %d, depth %d→%d)\n",
                  (int)trans, stackDepth, stackDepth - 1);

    // Phase out
    transRunOut(trans);

    // Stack change
    stackDepth--;

    if (stackDepth > 0) {
        const Screen* top = screenStack[stackDepth - 1];

        Serial.printf("[UI] PopT → %s\n", top->name);

        if (top->onEnter) {
            top->onEnter();
        }

        if (top->draw) {
            top->draw(true);
        }

        transRunIn(trans);
    }

    needsRedraw = false;
}

void uiReplaceScreenT(const Screen* screen, TransitionType trans) {
    if (stackDepth <= 0) {
        uiPushScreenT(screen, trans);
        return;
    }

    Serial.printf("[UI] ReplaceT → %s (trans %d, depth %d)\n",
                  screen->name, (int)trans, stackDepth);

    // Phase out
    transRunOut(trans);

    // Stack swap
    screenStack[stackDepth - 1] = screen;

    if (screen->onEnter) {
        screen->onEnter();
    }

    if (screen->draw) {
        screen->draw(true);
    }

    // Phase in
    transRunIn(trans);

    needsRedraw = false;
}

// ============================================================
// Main update loop
// ============================================================
void uiUpdate() {
    const Screen* active = uiActiveScreen();
    if (!active) return;

    // --- Idle timer check ---
    unsigned long now = millis();
    unsigned long idleMs = now - lastActivityMs;

    if (idleState == IDLE_ACTIVE) {
        uint16_t dimDelay = settingsGetDimDelay();
        if (idleMs >= (unsigned long)dimDelay * 1000UL) {
            int userBright = BRIGHT_MAP[settingsGetBrightness()];
            int dimLevel = userBright / 4;
            if (dimLevel < 10) dimLevel = 10;
            setBacklight(dimLevel);
            idleState = IDLE_DIM;
        }
    } else if (idleState == IDLE_DIM) {
        uint16_t offDelay = settingsGetOffDelay();
        if (idleMs >= (unsigned long)offDelay * 1000UL) {
            setBacklight(0);
            motorDisable();
            idleState = IDLE_OFF;
        }
    }

    // Drain all pending input events to active screen
    InputEvent evt;
    while ((evt = inputGetEvent()) != INPUT_NONE) {
        // Wake from idle — consume the event, restore brightness
        if (idleState != IDLE_ACTIVE) {
            bool wasOff = (idleState == IDLE_OFF);
            setBacklight(BRIGHT_MAP[settingsGetBrightness()]);
            idleState = IDLE_ACTIVE;
            lastActivityMs = now;

            if (wasOff) {
                tft->fillScreen(COL_BG);
                uiDrawCenteredText("Restoring...", SCREEN_H / 2, FONT_TITLE, COL_TEXT);
                motorHome();
                needsRedraw = true;
            }

            continue;  // consume this event
        }

        lastActivityMs = now;

        if (evt == INPUT_BTN_LEFT_LONG && stackDepth > 1) {
            audioPlayTone(TONE_SELECT);
            uiGoHome();
            return;
        }

        active->handleInput(evt);

        // Active screen may have changed (push/pop inside handler),
        // so re-fetch for subsequent events
        active = uiActiveScreen();
        if (!active) return;
    }

    // Redraw if flagged
    if (needsRedraw) {
        needsRedraw = false;
        active = uiActiveScreen();
        if (active && active->draw) {
            active->draw(true);     // full redraw
        }
    } else {
        // Partial update pass — screens that need periodic refresh
        // (e.g. About screen battery/Wi-Fi) handle it here.
        // Most screens return immediately when fullRedraw is false.
        if (active && active->draw) {
            active->draw(false);
        }
    }
}

// ============================================================
// Idle timer API
// ============================================================

void uiResetIdleTimer() {
    lastActivityMs = millis();
    if (idleState != IDLE_ACTIVE) {
        setBacklight(BRIGHT_MAP[settingsGetBrightness()]);
        idleState = IDLE_ACTIVE;
    }
}

// ============================================================
// Drawing primitives
// ============================================================

void uiDrawTitleBar(const char* title, uint16_t bgColor) {
    tft->fillRect(0, TITLE_BAR_Y, SCREEN_W, TITLE_BAR_H, bgColor);
    tft->setTextColor(COL_TEXT, bgColor);
    tft->setTextSize(FONT_TITLE);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(title, SCREEN_W / 2, TITLE_BAR_Y + TITLE_BAR_H / 2);
}

void uiDrawSoftButtons(const char* leftLabel, const char* rightLabel) {
    // Clear the button row
    tft->fillRect(0, SOFT_BTN_Y - 2, SCREEN_W, SOFT_BTN_H + 4, COL_BG);

    if (leftLabel[0] != '\0') {
        tft->fillRoundRect(8, SOFT_BTN_Y, SOFT_BTN_W, SOFT_BTN_H,
                           SOFT_BTN_R, COL_DIM);
        tft->setTextColor(COL_TEXT, COL_DIM);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(leftLabel, 8 + SOFT_BTN_W / 2,
                        SOFT_BTN_Y + SOFT_BTN_H / 2);
    }

    if (rightLabel[0] != '\0') {
        int rx = SCREEN_W - 8 - SOFT_BTN_W;
        tft->fillRoundRect(rx, SOFT_BTN_Y, SOFT_BTN_W, SOFT_BTN_H,
                           SOFT_BTN_R, COL_ACCENT);
        tft->setTextColor(COL_BG, COL_ACCENT);
        tft->setTextSize(FONT_BODY);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(rightLabel, rx + SOFT_BTN_W / 2,
                        SOFT_BTN_Y + SOFT_BTN_H / 2);
    }
}

void uiDrawMenuItem(int index, const char* text, bool selected) {
    int y = CONTENT_Y + 4 + index * MENU_ITEM_H;

    uint16_t bgColor = selected ? COL_HIGHLIGHT : COL_BG;
    uint16_t fgColor = selected ? COL_ACCENT : COL_TEXT;

    tft->fillRoundRect(MENU_ITEM_X, y, MENU_ITEM_W, MENU_ITEM_H - 2,
                       4, bgColor);

    if (selected) {
        tft->fillRoundRect(MENU_ITEM_X, y, 4, MENU_ITEM_H - 2, 2, COL_ACCENT);
    }

    int menuTextMaxW = MENU_ITEM_W - 18;
    tft->setTextColor(fgColor, bgColor);
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(ML_DATUM);

    if (tft->textWidth(text) > menuTextMaxW) {
        char truncBuf[32];
        int maxChars = 0;
        for (int i = 0; text[i] && i < 28; i++) {
            truncBuf[i] = text[i];
            truncBuf[i + 1] = '\0';
            if (tft->textWidth(truncBuf) > menuTextMaxW - tft->textWidth("..")) {
                break;
            }
            maxChars = i + 1;
        }
        truncBuf[maxChars] = '.';
        truncBuf[maxChars + 1] = '.';
        truncBuf[maxChars + 2] = '\0';
        tft->drawString(truncBuf, MENU_ITEM_X + 14, y + (MENU_ITEM_H - 2) / 2);
    } else {
        tft->drawString(text, MENU_ITEM_X + 14, y + (MENU_ITEM_H - 2) / 2);
    }
}

void uiDrawCenteredText(const char* text, int y, int fontSize, uint16_t color) {
    tft->setTextColor(color, COL_BG);
    tft->setTextSize(fontSize);
    tft->setTextDatum(MC_DATUM);
    tft->drawString(text, SCREEN_W / 2, y);
}

int uiDrawCenteredTextWrap(const char* text, int y, int fontSize,
                           uint16_t color, int maxWidth) {
    if (!text || !text[0]) return y;
    if (maxWidth <= 0) maxWidth = SCREEN_W - 16;

    tft->setTextSize(fontSize);
    int textW = tft->textWidth(text);

    if (textW <= maxWidth) {
        uiDrawCenteredText(text, y, fontSize, color);
        int lineH = fontSize * 8 + 4;
        return y + lineH;
    }

    // Try smaller font first
    if (fontSize > FONT_SMALL) {
        int smallW = 0;
        tft->setTextSize(fontSize - 1);
        smallW = tft->textWidth(text);
        if (smallW <= maxWidth) {
            uiDrawCenteredText(text, y, fontSize - 1, color);
            int lineH = (fontSize - 1) * 8 + 4;
            return y + lineH;
        }
    }

    // Word-wrap: find the last space that keeps line 1 within maxWidth
    tft->setTextSize(fontSize);
    int len = strlen(text);
    int splitAt = -1;
    char line1[64];

    for (int i = 0; i < len && i < 63; i++) {
        if (text[i] == ' ') {
            memcpy(line1, text, i);
            line1[i] = '\0';
            if (tft->textWidth(line1) <= maxWidth) {
                splitAt = i;
            } else {
                break;
            }
        }
    }

    if (splitAt > 0) {
        memcpy(line1, text, splitAt);
        line1[splitAt] = '\0';
        const char* line2 = text + splitAt + 1;
        int lineH = fontSize * 8 + 4;
        uiDrawCenteredText(line1, y, fontSize, color);
        uiDrawCenteredText(line2, y + lineH, fontSize, color);
        return y + lineH * 2;
    }

    // No space found — just draw it (clipped)
    uiDrawCenteredText(text, y, fontSize, color);
    int lineH = fontSize * 8 + 4;
    return y + lineH;
}

void uiDrawHint(const char* text, int y) {
    uiDrawCenteredTextWrap(text, y, FONT_BODY, COL_DIM);
}

void uiClearContent() {
    tft->fillRect(0, CONTENT_Y, SCREEN_W, CONTENT_H, COL_BG);
}

TFT_eSPI* uiGetTFT() {
    return tft;
}

// ============================================================
// ScrollList implementation (Session 6)
// ============================================================

void uiScrollListInit(ScrollList* list, const char** items, int count) {
    list->items = items;
    list->count = count;
    list->selected = 0;
    list->scrollOffset = 0;
    // Calculate how many items fit in the content area
    list->visibleCount = CONTENT_H / MENU_ITEM_H;
    if (list->visibleCount < 1) list->visibleCount = 1;
    if (list->visibleCount > count) list->visibleCount = count;
}

// --- Internal: draw the scroll position indicator ---
static void drawScrollBar(ScrollList* list) {
    // Only draw if list is longer than visible window
    if (list->count <= list->visibleCount) return;

    int trackTop = CONTENT_Y + SCROLL_BAR_PAD;
    int trackH = CONTENT_H - 2 * SCROLL_BAR_PAD;

    // Draw track background
    tft->fillRect(SCROLL_BAR_X, trackTop, SCROLL_BAR_W, trackH, COL_SCROLL_BG);

    // Calculate thumb size and position
    // Thumb height proportional to visible/total ratio, minimum 8px
    int thumbH = (trackH * list->visibleCount) / list->count;
    if (thumbH < 8) thumbH = 8;

    // Thumb position: map scrollOffset to track range
    int scrollRange = list->count - list->visibleCount;
    int thumbRange = trackH - thumbH;
    int thumbY = trackTop;
    if (scrollRange > 0) {
        thumbY = trackTop + (list->scrollOffset * thumbRange) / scrollRange;
    }

    // Draw thumb
    tft->fillRoundRect(SCROLL_BAR_X, thumbY, SCROLL_BAR_W, thumbH,
                       SCROLL_BAR_W / 2, COL_SCROLL_FG);
}

// --- Internal: draw a single list item at a visual slot ---
// slot is 0-based position within the visible window.
static void drawListItem(ScrollList* list, int slot, int itemIndex, bool selected) {
    int y = CONTENT_Y + 4 + slot * MENU_ITEM_H;

    // Item width: narrower when scroll bar is visible to avoid overlap
    int itemW = MENU_ITEM_W;
    if (list->count > list->visibleCount) {
        itemW = SCROLL_BAR_X - MENU_ITEM_X - 2;    // leave gap before scroll bar
    }

    uint16_t bgColor = selected ? COL_HIGHLIGHT : COL_BG;
    uint16_t fgColor = selected ? COL_ACCENT : COL_TEXT;

    tft->fillRoundRect(MENU_ITEM_X, y, itemW, MENU_ITEM_H - 2, 4, bgColor);

    // Selection indicator bar on left edge
    if (selected) {
        tft->fillRoundRect(MENU_ITEM_X, y, 4, MENU_ITEM_H - 2, 2, COL_ACCENT);
    }

    int textX = MENU_ITEM_X + 14;
    int textMaxW = itemW - 18;
    tft->setTextColor(fgColor, bgColor);
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(ML_DATUM);

    const char* itemText = list->items[itemIndex];
    if (tft->textWidth(itemText) > textMaxW) {
        char truncBuf[32];
        int maxChars = 0;
        for (int i = 0; itemText[i] && i < 28; i++) {
            truncBuf[i] = itemText[i];
            truncBuf[i + 1] = '\0';
            if (tft->textWidth(truncBuf) > textMaxW - tft->textWidth("..")) {
                break;
            }
            maxChars = i + 1;
        }
        truncBuf[maxChars] = '.';
        truncBuf[maxChars + 1] = '.';
        truncBuf[maxChars + 2] = '\0';
        tft->drawString(truncBuf, textX, y + (MENU_ITEM_H - 2) / 2);
    } else {
        tft->drawString(itemText, textX, y + (MENU_ITEM_H - 2) / 2);
    }
}

void uiScrollListDraw(ScrollList* list) {
    // Clear content area
    uiClearContent();

    // Draw visible items
    int end = list->scrollOffset + list->visibleCount;
    if (end > list->count) end = list->count;

    for (int i = list->scrollOffset; i < end; i++) {
        int slot = i - list->scrollOffset;
        drawListItem(list, slot, i, i == list->selected);
    }

    // Scroll bar
    drawScrollBar(list);
}

// --- Internal: ensure the selected item is visible ---
static bool ensureVisible(ScrollList* list) {
    bool scrolled = false;

    // If selection is above the visible window, scroll up
    if (list->selected < list->scrollOffset) {
        list->scrollOffset = list->selected;
        scrolled = true;
    }

    // If selection is below the visible window, scroll down
    if (list->selected >= list->scrollOffset + list->visibleCount) {
        list->scrollOffset = list->selected - list->visibleCount + 1;
        scrolled = true;
    }

    return scrolled;
}

int uiScrollListHandleInput(ScrollList* list, InputEvent evt) {
    switch (evt) {
        case INPUT_ENC_CW:
            if (list->selected < list->count - 1) {
                list->selected++;
                ensureVisible(list);
                audioPlayTone(TONE_CLICK);
                uiScrollListDraw(list);     // redraw visible items
            }
            return -1;

        case INPUT_ENC_CCW:
            if (list->selected > 0) {
                list->selected--;
                ensureVisible(list);
                audioPlayTone(TONE_CLICK);
                uiScrollListDraw(list);
            }
            return -1;

        case INPUT_ENC_CLICK:
        case INPUT_BTN_RIGHT:
            return list->selected;

        default:
            return -1;
    }
}

int uiScrollListGetSelection(ScrollList* list) {
    return list->selected;
}

// ============================================================
// TextEntry implementation (Session 7)
// ============================================================

// --- Character set ---
// Flat array: A-Z (0–25), 0-9 (26–35), then 3 special entries.
// Special entries are identified by index, not stored as chars.
static const char TE_CHARSET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// Labels for the 3 special grid cells
static const char* TE_SPECIAL_LABELS[] = { "SP", "<-", "OK" };

// --- Grid geometry ---
// The grid sits below a text preview field, within the content area.
#define TE_TEXT_Y       (CONTENT_Y + 4)         // top of text field
#define TE_TEXT_H       40                       // text field height
#define TE_GRID_Y       (TE_TEXT_Y + TE_TEXT_H + 8)  // top of character grid
#define TE_CELL_W       (SCREEN_W / TE_GRID_COLS)    // 24
#define TE_CELL_H       30

// --- Internal: get the display label for a character index ---
static void teGetLabel(int index, char* out, int outSize) {
    if (index < 36) {
        // Normal alphanumeric character
        out[0] = TE_CHARSET[index];
        out[1] = '\0';
    } else {
        // Special entry (SP, <-, OK)
        int special = index - 36;   // 0=space, 1=backspace, 2=done
        strncpy(out, TE_SPECIAL_LABELS[special], outSize - 1);
        out[outSize - 1] = '\0';
    }
}

// --- Internal: get cell x-position and width ---
// The space key (index 36) spans 2 columns; backspace and done
// shift right to compensate. All other cells are standard width.
static void teGetCellRect(int index, int* outX, int* outY, int* outW) {
    int row = index / TE_GRID_COLS;
    int col = index % TE_GRID_COLS;
    *outY = TE_GRID_Y + row * TE_CELL_H;

    if (index == TE_IDX_SPACE) {
        // Space spans columns 6–7
        *outX = 6 * TE_CELL_W;
        *outW = 2 * TE_CELL_W;
    } else if (index == TE_IDX_BS) {
        // Backspace shifts to column 8
        *outX = 8 * TE_CELL_W;
        *outW = TE_CELL_W;
    } else if (index == TE_IDX_DONE) {
        // Done shifts to column 9
        *outX = 9 * TE_CELL_W;
        *outW = TE_CELL_W;
    } else {
        *outX = col * TE_CELL_W;
        *outW = TE_CELL_W;
    }
}

// --- Internal: draw the spacebar bracket icon (wide U shape) ---
static void teDrawSpacebarIcon(int cx, int cy, uint16_t color) {
    // Draws:  |           |
    //         +-----------+
    int halfW = 14;     // half-width of the bracket
    int armH = 6;       // height of vertical arms
    int baseY = cy + 2; // bottom of the U

    tft->drawFastHLine(cx - halfW, baseY, halfW * 2 + 1, color);       // bottom bar
    tft->drawFastVLine(cx - halfW, baseY - armH, armH, color);          // left arm
    tft->drawFastVLine(cx + halfW, baseY - armH, armH, color);          // right arm
}

// --- Internal: draw a single grid cell ---
static void teDrawCell(int index, bool selected) {
    int x, y, w;
    teGetCellRect(index, &x, &y, &w);

    // Cell colors
    uint16_t bgColor, fgColor;
    if (selected) {
        bgColor = COL_ACCENT;
        fgColor = COL_BG;
    } else if (index >= TE_IDX_SPACE) {
        // Special keys get a subtle background to stand out
        bgColor = COL_HIGHLIGHT;
        fgColor = COL_TEXT;
    } else {
        bgColor = COL_BG;
        fgColor = COL_DIM;
    }

    // Draw cell background
    tft->fillRoundRect(x + 1, y + 1, w - 2, TE_CELL_H - 2, 3, bgColor);

    if (index == TE_IDX_SPACE) {
        // Draw spacebar bracket icon centered in the wide cell
        teDrawSpacebarIcon(x + w / 2, y + TE_CELL_H / 2, fgColor);
    } else {
        // Draw text label centered in cell
        char label[4];
        teGetLabel(index, label, sizeof(label));

        bool isSpecial = (index > TE_IDX_SPACE);
        int fontSize = isSpecial ? FONT_SMALL : FONT_BODY;

        tft->setTextColor(fgColor, bgColor);
        tft->setTextSize(fontSize);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(label, x + w / 2, y + TE_CELL_H / 2);
    }
}

// --- Internal: draw the text preview field ---
static void teDrawTextField(TextEntry* entry) {
    int fieldX = 12;
    int fieldW = SCREEN_W - 24;
    int fieldY = TE_TEXT_Y;

    // Clear field area
    tft->fillRect(fieldX, fieldY, fieldW, TE_TEXT_H, COL_BG);

    // Draw underline
    int lineY = fieldY + TE_TEXT_H - 4;
    tft->drawFastHLine(fieldX, lineY, fieldW, COL_DIM);

    // Draw entered text (scroll to show tail if too long for field)
    tft->setTextSize(FONT_BODY);
    tft->setTextDatum(ML_DATUM);

    int charW = 12;     // pixel width per char at textSize 2
    int textPad = 4;
    int maxVisible = (fieldW - textPad * 2) / charW;  // ~17 chars

    if (entry->bufLen > 0) {
        tft->setTextColor(COL_TEXT, COL_BG);
        const char* displayStr = entry->buffer;
        int displayLen = entry->bufLen;

        // If text exceeds visible area, show the rightmost portion
        if (displayLen > maxVisible) {
            displayStr = entry->buffer + (displayLen - maxVisible);
            displayLen = maxVisible;
        }

        tft->drawString(displayStr, fieldX + textPad,
                        fieldY + (TE_TEXT_H - 4) / 2);
    }

    // Solid cursor bar at insertion point
    int visibleLen = (entry->bufLen > maxVisible) ? maxVisible : entry->bufLen;
    if (entry->bufLen < entry->maxLen) {
        int cursorX = fieldX + textPad + visibleLen * charW;
        tft->fillRect(cursorX, fieldY + 8, 2, TE_TEXT_H - 16, COL_ACCENT);
    }

    // Character count indicator (right-aligned, small)
    char countBuf[8];
    snprintf(countBuf, sizeof(countBuf), "%d/%d", entry->bufLen, entry->maxLen);
    tft->setTextColor(COL_DIM, COL_BG);
    tft->setTextSize(FONT_SMALL);
    tft->setTextDatum(MR_DATUM);
    tft->drawString(countBuf, fieldX + fieldW - 4, fieldY + 8);
}

// --- Internal: draw the full character grid ---
static void teDrawGrid(TextEntry* entry) {
    for (int i = 0; i < TE_TOTAL_CHARS; i++) {
        teDrawCell(i, i == entry->charIndex);
    }
}

// --- Internal: redraw only the previously-selected and newly-selected cells ---
static void teUpdateSelection(int oldIndex, int newIndex) {
    if (oldIndex >= 0 && oldIndex < TE_TOTAL_CHARS) {
        teDrawCell(oldIndex, false);
    }
    if (newIndex >= 0 && newIndex < TE_TOTAL_CHARS) {
        teDrawCell(newIndex, true);
    }
}

// --- Internal: draw soft buttons based on buffer state ---
static void teDrawSoftButtons(TextEntry* entry) {
    const char* leftLabel = (entry->bufLen > 0) ? "Delete" : "Cancel";
    uiDrawSoftButtons(leftLabel, "Done");
}

// --- Public API ---

void uiTextEntryInit(TextEntry* entry, int maxLen, const char* initial) {
    // Clamp maxLen
    if (maxLen > TEXT_ENTRY_MAX_LEN) maxLen = TEXT_ENTRY_MAX_LEN;
    if (maxLen < 1) maxLen = 1;
    entry->maxLen = maxLen;

    // Copy initial text
    entry->bufLen = 0;
    entry->buffer[0] = '\0';
    if (initial && initial[0] != '\0') {
        int len = strlen(initial);
        if (len > maxLen) len = maxLen;
        memcpy(entry->buffer, initial, len);
        entry->buffer[len] = '\0';
        entry->bufLen = len;
    }

    // Start cursor on 'A'
    entry->charIndex = 0;

    Serial.printf("[TextEntry] Init: maxLen=%d, initial=\"%s\"\n",
                  entry->maxLen, entry->buffer);
}

void uiTextEntryDraw(TextEntry* entry) {
    // Clear content area (below title bar)
    uiClearContent();

    // Text preview field
    teDrawTextField(entry);

    // Character grid
    teDrawGrid(entry);

    // Soft buttons
    teDrawSoftButtons(entry);
}

int uiTextEntryHandleInput(TextEntry* entry, InputEvent evt) {
    int oldIndex = entry->charIndex;

    switch (evt) {

        case INPUT_ENC_CW:
            // Move cursor forward, wrap at end
            entry->charIndex++;
            if (entry->charIndex >= TE_TOTAL_CHARS) {
                entry->charIndex = 0;
            }
            audioPlayTone(TONE_CLICK);
            teUpdateSelection(oldIndex, entry->charIndex);
            return 0;

        case INPUT_ENC_CCW:
            // Move cursor backward, wrap at start
            entry->charIndex--;
            if (entry->charIndex < 0) {
                entry->charIndex = TE_TOTAL_CHARS - 1;
            }
            audioPlayTone(TONE_CLICK);
            teUpdateSelection(oldIndex, entry->charIndex);
            return 0;

        case INPUT_ENC_CLICK: {
            int idx = entry->charIndex;

            if (idx == TE_IDX_DONE) {
                // Confirm
                audioPlayTone(TONE_CONFIRM);
                Serial.printf("[TextEntry] Confirmed: \"%s\"\n", entry->buffer);
                return 1;
            }

            if (idx == TE_IDX_BS) {
                // Backspace
                if (entry->bufLen > 0) {
                    entry->bufLen--;
                    entry->buffer[entry->bufLen] = '\0';
                    audioPlayTone(TONE_CLICK);
                    teDrawTextField(entry);
                    teDrawSoftButtons(entry);   // label may change
                } else {
                    audioPlayTone(TONE_ERROR);
                }
                return 0;
            }

            // Regular character or space
            if (entry->bufLen < entry->maxLen) {
                char ch;
                if (idx == TE_IDX_SPACE) {
                    ch = ' ';
                } else {
                    ch = TE_CHARSET[idx];
                }
                entry->buffer[entry->bufLen] = ch;
                entry->bufLen++;
                entry->buffer[entry->bufLen] = '\0';
                audioPlayTone(TONE_CLICK);
                teDrawTextField(entry);
            } else {
                // Buffer full
                audioPlayTone(TONE_ERROR);
            }
            return 0;
        }

        case INPUT_BTN_LEFT:
            // Soft button: backspace, or cancel if empty
            if (entry->bufLen > 0) {
                entry->bufLen--;
                entry->buffer[entry->bufLen] = '\0';
                audioPlayTone(TONE_CLICK);
                teDrawTextField(entry);
                teDrawSoftButtons(entry);
            } else {
                // Cancel — empty buffer + left button = back out
                audioPlayTone(TONE_CLICK);
                Serial.println("[TextEntry] Cancelled");
                return -1;
            }
            return 0;

        case INPUT_BTN_RIGHT:
            // Soft button: confirm/done
            audioPlayTone(TONE_CONFIRM);
            Serial.printf("[TextEntry] Confirmed: \"%s\"\n", entry->buffer);
            return 1;

        default:
            return 0;
    }
}

const char* uiTextEntryGetText(TextEntry* entry) {
    return entry->buffer;
}
