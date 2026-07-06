// ============================================================
// Blind Flight — Text Entry Demo Screen (Session 7)
// ============================================================
// A test screen that exercises the TextEntry component.
// Enter a name → confirm → see the result → press to go again.
// ============================================================

#include "ui.h"
#include "config.h"
#include "audio.h"
#include "screens.h"

// --- Demo state ---
static TextEntry entry;
static bool showingResult = false;

// --- Forward declarations ---
static void textDemoDraw(bool fullRedraw);
static void textDemoInput(InputEvent evt);
static void textDemoEnter();

// --- Result sub-screen drawing ---
static void drawResult() {
    TFT_eSPI* tft = uiGetTFT();

    uiDrawTitleBar("Result", COL_ACCENT);
    uiClearContent();

    // "You entered:" label
    uiDrawHint("You entered:", CONTENT_Y + 30);

    // The entered text, large and centered
    const char* text = uiTextEntryGetText(&entry);
    if (text[0] != '\0') {
        uiDrawCenteredText(text, CONTENT_Y + 70, FONT_BODY, COL_TEXT);
    } else {
        uiDrawCenteredText("(empty)", CONTENT_Y + 70, FONT_BODY, COL_DIM);
    }

    // Character count
    char countBuf[24];
    snprintf(countBuf, sizeof(countBuf), "%d characters", entry.bufLen);
    uiDrawHint(countBuf, CONTENT_Y + 110);

    uiDrawSoftButtons("Back", "Again");
}

// --- Screen callbacks ---

static void textDemoDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    if (showingResult) {
        drawResult();
    } else {
        uiDrawTitleBar("Enter Name", COL_ACCENT);
        uiTextEntryDraw(&entry);
    }
}

static void textDemoInput(InputEvent evt) {
    if (showingResult) {
        // Result screen: any button goes somewhere
        switch (evt) {
            case INPUT_BTN_LEFT:
            case INPUT_ENC_CLICK:
                // Back to previous screen
                audioPlayTone(TONE_CLICK);
                showingResult = false;
                uiPopScreen();
                return;

            case INPUT_BTN_RIGHT:
                // Try again — reset entry and redraw
                audioPlayTone(TONE_CLICK);
                showingResult = false;
                uiTextEntryInit(&entry, TEXT_ENTRY_MAX_LEN, "");
                uiRequestRedraw();
                return;

            default:
                return;
        }
    }

    // Text entry mode — delegate to component
    int result = uiTextEntryHandleInput(&entry, evt);

    if (result == 1) {
        // Confirmed — show result
        showingResult = true;
        uiRequestRedraw();
    } else if (result == -1) {
        // Cancelled — pop back
        showingResult = false;
        uiPopScreen();
    }
}

static void textDemoEnter() {
    showingResult = false;
    uiTextEntryInit(&entry, TEXT_ENTRY_MAX_LEN, "");
    Serial.println("[Screen] Text entry demo entered");
}

// --- Screen definition ---
const Screen screenTextDemo = {
    "TextDemo",
    textDemoDraw,
    textDemoInput,
    textDemoEnter
};
