// ============================================================
// Blind Flight — Transition Effects (Session 9)
// ============================================================

#include "transitions.h"
#include "config.h"
#include "audio.h"
#include "settings.h"

// ============================================================
// Backlight PWM state
// ============================================================
// Uses LEDC channel 2. Channel 0 is the buzzer (see config.h).
// NOTE: ESP32 Arduino core 2.x uses ledcSetup/ledcAttachPin.
//       Core 3.x uses ledcAttach/ledcWrite with pin.
//       Uncomment the appropriate lines if compilation fails.
// ============================================================

#define BL_CHANNEL  2
#define BL_FREQ     5000
#define BL_RES      8       // 8-bit → 0–255

static bool blInitialized = false;
static int  blBrightness  = 255;

// Internal TFT pointer, set via transSetTFT() called by uiInit.
// Avoids circular dependency (ui.h includes transitions.h, so
// transitions.cpp can't include ui.h to call uiGetTFT).
static TFT_eSPI* tft = nullptr;

// ============================================================
// Initialization
// ============================================================

void transInit() {
    if (blInitialized) return;

    // --- Arduino core 2.x API (default) ---
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_RES);
    ledcAttachPin(TFT_BL, BL_CHANNEL);
    // --- Arduino core 3.x API (uncomment if needed) ---
    // ledcAttach(TFT_BL, BL_FREQ, BL_RES);

    blInitialized = true;
    blBrightness = 255;

    // Set full brightness immediately so the pin isn't left at 0
    // (ledcAttachPin resets duty cycle — this was the Session 8 bug)
    ledcWrite(BL_CHANNEL, 255);
    // Core 3.x: ledcWrite(TFT_BL, 255);
}

// Called by ui.cpp after uiInit so transitions can draw
void transSetTFT(TFT_eSPI* tftPtr) {
    tft = tftPtr;
}

// ============================================================
// Backlight control
// ============================================================

void setBacklight(int brightness) {
    if (!blInitialized) transInit();
    if (brightness < 0)   brightness = 0;
    if (brightness > 255) brightness = 255;
    blBrightness = brightness;

    // --- Core 2.x: write to channel ---
    ledcWrite(BL_CHANNEL, brightness);
    // --- Core 3.x: write to pin ---
    // ledcWrite(TFT_BL, brightness);
}

int getBacklight() {
    return blBrightness;
}

// ============================================================
// Internal: yield-friendly delay that keeps audio alive
// ============================================================
static void busyWait(unsigned long ms) {
    unsigned long end = millis() + ms;
    while (millis() < end) {
        audioUpdate();
        delay(1);
    }
}

// ============================================================
// Fade transition
// ============================================================

static void fadeOut() {
    // Ramp brightness from current level down to 0
    int start = blBrightness;
    for (int i = 1; i <= TRANS_FADE_STEPS; i++) {
        int b = start - (start * i) / TRANS_FADE_STEPS;
        setBacklight(b);
        busyWait(TRANS_FADE_STEP_MS);
    }
    setBacklight(0);

    // Hold dark briefly so the new draw doesn't flash
    busyWait(TRANS_FADE_DARK_MS);
}

static void fadeIn() {
    // Ramp brightness from 0 up to the user's configured level
    int target = BRIGHT_MAP[settingsGetBrightness()];
    for (int i = 1; i <= TRANS_FADE_STEPS; i++) {
        int b = (target * i) / TRANS_FADE_STEPS;
        setBacklight(b);
        busyWait(TRANS_FADE_STEP_MS);
    }
    setBacklight(target);
}

// ============================================================
// Wipe-down transition
// ============================================================

static void wipeDownOut() {
    if (!tft) return;

    // Sweep horizontal black bars from top to bottom
    for (int y = 0; y < SCREEN_H; y += TRANS_WIPE_STRIP_PX) {
        int h = TRANS_WIPE_STRIP_PX;
        if (y + h > SCREEN_H) h = SCREEN_H - y;
        tft->fillRect(0, y, SCREEN_W, h, COL_BG);
        busyWait(TRANS_WIPE_STEP_MS);
    }
}

static void wipeDownIn() {
    // Screen was drawn while dark (backlight dimmed during wipe).
    // Quick fade-in to reveal, up to the user's configured level.
    int target = BRIGHT_MAP[settingsGetBrightness()];
    for (int i = 1; i <= TRANS_WIPE_FADE_STEPS; i++) {
        int b = (target * i) / TRANS_WIPE_FADE_STEPS;
        setBacklight(b);
        busyWait(TRANS_WIPE_FADE_MS);
    }
    setBacklight(target);
}

// ============================================================
// Wipe-left transition
// ============================================================

static void wipeLeftOut() {
    if (!tft) return;

    // Sweep vertical black bars from right to left
    for (int x = SCREEN_W - TRANS_WIPE_STRIP_PX; x >= 0; x -= TRANS_WIPE_STRIP_PX) {
        int w = TRANS_WIPE_STRIP_PX;
        if (x < 0) { w += x; x = 0; }
        tft->fillRect(x, 0, w, SCREEN_H, COL_BG);
        busyWait(TRANS_WIPE_STEP_MS);
    }
}

static void wipeLeftIn() {
    // Same reveal as wipe-down: quick backlight fade-in
    wipeDownIn();
}

// ============================================================
// Phase dispatch
// ============================================================

void transRunOut(TransitionType type) {
    switch (type) {
        case TRANS_FADE:
            fadeOut();
            break;
        case TRANS_WIPE_DOWN:
            wipeDownOut();
            // Dim backlight so new screen draw is invisible
            setBacklight(0);
            break;
        case TRANS_WIPE_LEFT:
            wipeLeftOut();
            setBacklight(0);
            break;
        case TRANS_NONE:
        default:
            break;
    }
}

void transRunIn(TransitionType type) {
    switch (type) {
        case TRANS_FADE:
            fadeIn();
            break;
        case TRANS_WIPE_DOWN:
            wipeDownIn();
            break;
        case TRANS_WIPE_LEFT:
            wipeLeftIn();
            break;
        case TRANS_NONE:
        default:
            // Ensure backlight is on (safety net)
            {
                int target = BRIGHT_MAP[settingsGetBrightness()];
                if (blBrightness < target) setBacklight(target);
            }
            break;
    }
}

// ============================================================
// Slot-machine text roll
// ============================================================
// Uses a TFT_eSprite as a clipping window. The text starts
// offset below the visible area and decelerates upward into
// its final position, like a slot reel settling.
// ============================================================

void transSlotRoll(int x, int y, int w, int h,
                   const char* text,
                   uint16_t fgColor, uint16_t bgColor,
                   int fontSize) {
    if (!tft) return;

    // Create a sprite sized to the bounding rect
    TFT_eSprite spr(tft);
    spr.createSprite(w, h);
    if (!spr.created()) {
        // Fallback: just draw the text directly (no animation)
        Serial.println("[Trans] SlotRoll: sprite alloc failed, drawing static");
        tft->fillRect(x, y, w, h, bgColor);
        tft->setTextColor(fgColor, bgColor);
        tft->setTextSize(fontSize);
        tft->setTextDatum(MC_DATUM);
        tft->drawString(text, x + w / 2, y + h / 2);
        return;
    }

    spr.setTextColor(fgColor, bgColor);
    spr.setTextSize(fontSize);
    spr.setTextDatum(MC_DATUM);

    int centerX = w / 2;
    int centerY = h / 2;

    // Animate: text starts offset downward and decelerates to center.
    // Use ease-out (quadratic): offset = maxOffset * (1 - t)^2
    for (int frame = 0; frame <= TRANS_SLOT_FRAMES; frame++) {
        float t = (float)frame / TRANS_SLOT_FRAMES;     // 0.0 → 1.0
        float ease = (1.0f - t) * (1.0f - t);            // ease-out quadratic
        int offset = (int)(TRANS_SLOT_MAX_OFFSET * ease); // pixels below center

        spr.fillSprite(bgColor);
        spr.drawString(text, centerX, centerY + offset);
        spr.pushSprite(x, y);

        // Adaptive delay: faster at start, slower near end (enhances decel feel)
        int delayMs = 8 + (int)(30.0f * t * t);
        busyWait(delayMs);
    }

    // Final frame: ensure text is exactly centered
    spr.fillSprite(bgColor);
    spr.drawString(text, centerX, centerY);
    spr.pushSprite(x, y);

    spr.deleteSprite();
}

// ============================================================
// Spin indicator
// ============================================================
// Draws a short arc that rotates around a center point.
// Each call erases the previous arc and draws the next one.
// The arc spans ARC_SWEEP degrees. Call in a timed loop
// (e.g., every 30–50ms) for smooth rotation.
// ============================================================

#define ARC_SWEEP       60      // degrees of visible arc
#define ARC_STEP        15      // degrees to advance per call

// Internal: draw an arc segment on the TFT
// Angles in degrees, 0 = top, clockwise.
static void drawArcSegment(int cx, int cy, int r, int thickness,
                           int startAngle, int endAngle,
                           uint16_t color) {
    // Convert to radians and draw pixel-by-pixel
    // Simple approach: step through angles, draw filled circles at each point
    float startRad = (startAngle - 90) * DEG_TO_RAD;   // -90 so 0° = top
    float endRad   = (endAngle - 90) * DEG_TO_RAD;

    // Step in 2° increments for smoothness vs. speed balance
    float stepRad = 2.0f * DEG_TO_RAD;
    for (float a = startRad; a <= endRad; a += stepRad) {
        int px = cx + (int)(r * cos(a));
        int py = cy + (int)(r * sin(a));
        tft->fillCircle(px, py, thickness / 2, color);
    }
}

void transSpinStep(int cx, int cy,
                   uint16_t color, uint16_t bgColor,
                   int* angle) {
    if (!tft) return;

    // Erase previous arc
    int prevStart = *angle;
    int prevEnd   = prevStart + ARC_SWEEP;
    drawArcSegment(cx, cy, TRANS_SPIN_RADIUS, TRANS_SPIN_THICKNESS,
                   prevStart, prevEnd, bgColor);

    // Advance angle
    *angle = (*angle + ARC_STEP) % 360;

    // Draw new arc
    int newStart = *angle;
    int newEnd   = newStart + ARC_SWEEP;
    drawArcSegment(cx, cy, TRANS_SPIN_RADIUS, TRANS_SPIN_THICKNESS,
                   newStart, newEnd, color);
}

void transSpinClear(int cx, int cy, uint16_t bgColor) {
    if (!tft) return;
    // Clear the full circle area the spin indicator occupies
    int outer = TRANS_SPIN_RADIUS + TRANS_SPIN_THICKNESS / 2 + 1;
    tft->fillCircle(cx, cy, outer, bgColor);
}
