// ============================================================
// Blind Flight — Splash Screen (Session 8, updated Session 9)
// ============================================================
// Session 9 changes:
//   - Removed local backlight PWM code (initBacklightPWM,
//     setBacklight) — now uses shared functions from
//     transitions.h
//   - Splash→home transition uses uiReplaceScreenT with
//     TRANS_FADE for a smooth handoff after homing
// ============================================================

#include "splash.h"
#include "config.h"
#include "audio.h"
#include "input.h"
#include "screens.h"
#include "transitions.h"
#include "settings.h"

// --- Image assets (auto-generated headers) ---
#include "img_logo_full.h"
#include "img_logo_text.h"
#include "img_plane_25.h"
#include "flight_path.h"

// ============================================================
// Animation constants
// ============================================================

// Dot trail: draw a dot every DOT_INTERVAL waypoints
#define DOT_INTERVAL        3
#define DOT_RADIUS          2
#define DOT_COLOR           COL_TEXT

// Timing (milliseconds)
#define FRAME_DELAY_MS      18      // ~55 fps, ~2.2s for 120 waypoints
#define POST_TRAIL_PAUSE    300     // Pause after trail completes
#define TEXT_FADE_STEPS     8       // PWM fade-in steps for text
#define TEXT_FADE_STEP_MS   40      // Delay between fade steps
#define POST_TEXT_PAUSE     400     // Pause before showing hint
#define SKIP_GRACE_MS       600     // Ignore input for first N ms (prevent accidental skip)

// Logo / text vertical position (centered on screen)
#define LOGO_Y              ((SCREEN_H - LOGO_FULL_H) / 2)

// Plane sprite is drawn centered on the waypoint
#define PLANE_OFFSET_X      (PLANE_25_W / 2)
#define PLANE_OFFSET_Y      (PLANE_25_H / 2)

// ============================================================
// Internal state
// ============================================================
static bool animationDone = false;

// Track which dot positions have been drawn (for redraw after plane erase)
static bool dotDrawn[FLIGHT_PATH_LEN];

// ============================================================
// Helper: read a waypoint from PROGMEM
// ============================================================
static void getWaypoint(int index, int16_t* x, int16_t* y) {
    *x = pgm_read_word(&flight_path[index][0]);
    *y = pgm_read_word(&flight_path[index][1]);
}

// ============================================================
// Helper: draw a single dot at a waypoint position
// ============================================================
static void drawDot(TFT_eSPI* tft, int wpIndex) {
    int16_t x, y;
    getWaypoint(wpIndex, &x, &y);
    tft->fillCircle(x, y, DOT_RADIUS, DOT_COLOR);
}

// ============================================================
// Helper: draw the plane sprite at a waypoint position
// Uses pushImage with transparent color 0x0000
// ============================================================
static void drawPlane(TFT_eSPI* tft, int16_t wx, int16_t wy) {
    int px = wx - PLANE_OFFSET_X;
    int py = wy - PLANE_OFFSET_Y;
    tft->pushImage(px, py, PLANE_25_W, PLANE_25_H,
                   plane_25_data, (uint16_t)0x0000);
}

// ============================================================
// Helper: erase the plane sprite area and redraw any dots
// that were under it
// ============================================================
static void erasePlane(TFT_eSPI* tft, int16_t wx, int16_t wy, int currentWp) {
    int px = wx - PLANE_OFFSET_X;
    int py = wy - PLANE_OFFSET_Y;

    // Black out the old plane area
    tft->fillRect(px, py, PLANE_25_W, PLANE_25_H, COL_BG);

    // Redraw any dots that fell within that rectangle
    int x1 = px;
    int y1 = py;
    int x2 = px + PLANE_25_W;
    int y2 = py + PLANE_25_H;

    for (int i = 0; i <= currentWp && i < FLIGHT_PATH_LEN; i++) {
        if (!dotDrawn[i]) continue;

        int16_t dx, dy;
        getWaypoint(i, &dx, &dy);

        // Check if this dot overlaps the erased rectangle
        if (dx + DOT_RADIUS >= x1 && dx - DOT_RADIUS <= x2 &&
            dy + DOT_RADIUS >= y1 && dy - DOT_RADIUS <= y2) {
            tft->fillCircle(dx, dy, DOT_RADIUS, DOT_COLOR);
        }
    }
}

// ============================================================
// The main animation sequence (blocking)
// ============================================================
void splashRunAnimation() {
    TFT_eSPI* tft = uiGetTFT();

    // Ensure backlight is on (transInit in main.cpp already set this,
    // but be safe in case splash is re-entered)
    setBacklight(BRIGHT_MAP[settingsGetBrightness()]);

    // Clear screen
    tft->fillScreen(COL_BG);

    // Reset dot tracking
    memset(dotDrawn, false, sizeof(dotDrawn));

    unsigned long animStart = millis();

    // --- Phase 1: Plane flies along path, trailing dots ---

    int16_t prevX = -100, prevY = -100;  // off-screen initially

    for (int wp = 0; wp < FLIGHT_PATH_LEN; wp++) {
        int16_t wx, wy;
        getWaypoint(wp, &wx, &wy);

        // Erase old plane position (skip first frame)
        if (prevX >= 0) {
            erasePlane(tft, prevX, prevY, wp - 1);
        }

        // Draw dot at this position if it's a dot interval
        if (wp % DOT_INTERVAL == 0) {
            dotDrawn[wp] = true;
            drawDot(tft, wp);
        }

        // Draw plane at new position
        drawPlane(tft, wx, wy);
        prevX = wx;
        prevY = wy;

        // Frame pacing
        unsigned long frameEnd = millis() + FRAME_DELAY_MS;
        while (millis() < frameEnd) {
            audioUpdate();
            delay(1);
        }

        // Allow skip after grace period
        if (millis() - animStart > SKIP_GRACE_MS) {
            inputUpdate();
            InputEvent evt = inputGetEvent();
            if (evt == INPUT_ENC_CLICK || evt == INPUT_BTN_LEFT || evt == INPUT_BTN_RIGHT) {
                // Skip animation — jump to final state
                break;
            }
        }
    }

    // Erase the plane from its final position
    erasePlane(tft, prevX, prevY, FLIGHT_PATH_LEN - 1);

    // Brief pause after trail completes
    unsigned long pauseEnd = millis() + POST_TRAIL_PAUSE;
    while (millis() < pauseEnd) {
        audioUpdate();
        delay(1);
    }

    // --- Phase 2: Text fade-in via backlight ---

    int targetBright = BRIGHT_MAP[settingsGetBrightness()];

    // Dim backlight
    for (int b = targetBright; b >= 0; b -= 32) {
        setBacklight(b);
        delay(15);
    }
    setBacklight(0);

    // Draw the text image (while screen is dark)
    // Position matches the full logo's vertical centering
    tft->pushImage(0, LOGO_Y, LOGO_TEXT_W, LOGO_TEXT_H,
                   logo_text_data, (uint16_t)0x0000);

    // Brief dark pause
    delay(80);

    // Fade backlight back up
    for (int b = 0; b <= targetBright; b += 32) {
        setBacklight(b);
        delay(TEXT_FADE_STEP_MS);
    }
    setBacklight(targetBright);

    // Pause before showing hint
    pauseEnd = millis() + POST_TEXT_PAUSE;
    while (millis() < pauseEnd) {
        audioUpdate();
        delay(1);
    }

    // Play a soft chime
    audioPlayTone(TONE_CONFIRM);

    // Show the hint immediately (redraw will clean it up later)
    uiDrawHint("Press to start", LOGO_Y + LOGO_FULL_H + 30);

    animationDone = true;
}

// ============================================================
// Splash screen — static display after animation
// ============================================================

static void splashDraw(bool fullRedraw) {
    if (!fullRedraw) return;

    TFT_eSPI* tft = uiGetTFT();

    if (animationDone) {
        // Static splash: show the full logo centered, plus hint
        tft->fillScreen(COL_BG);
        tft->pushImage(0, LOGO_Y, LOGO_FULL_W, LOGO_FULL_H,
                       logo_full_data, (uint16_t)0x0000);

        // "Press to start" hint below the logo
        uiDrawHint("Press to start", LOGO_Y + LOGO_FULL_H + 30);
    }
}

static void splashInput(InputEvent evt) {
    if (!animationDone) return;

    switch (evt) {
        case INPUT_ENC_CLICK:
        case INPUT_BTN_LEFT:
        case INPUT_BTN_RIGHT:
            audioPlayTone(TONE_SELECT);
            // Run homing (blocking), then fade-transition to home screen
            runHomingSequence();
            uiReplaceScreenT(&screenHome, TRANS_FADE);
            break;

        default:
            break;
    }
}

static void splashEnter() {
    animationDone = false;
    splashRunAnimation();
    // After animation, trigger a redraw to show the static splash
    uiRequestRedraw();
}

// ============================================================
// Screen definition
// ============================================================
const Screen screenSplash = {
    "Splash",
    splashDraw,
    splashInput,
    splashEnter
};
