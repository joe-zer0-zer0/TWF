#pragma once

// ============================================================
// Blind Flight — Configuration
// ============================================================
// Central header for pin assignments, motor constants,
// color palette, layout dimensions, and font sizes.
// All hardware-specific values live here.
// ============================================================

#include <Arduino.h>

// --- Firmware version ---
#define FW_VERSION  "1.6.0"

// --- Pin definitions (from hardware spec) ---

// Rotary encoder
#define PIN_ENC_CLK     32
#define PIN_ENC_DT      33
#define PIN_ENC_SW      34   // Input-only GPIO, external 10k pull-up to 3.3V

// Soft buttons
#define PIN_BTN_LEFT    14
#define PIN_BTN_RIGHT   12

// Stepper motor (TMC2209 standalone mode)
#define PIN_MOTOR_STEP  25
#define PIN_MOTOR_DIR   26
#define PIN_MOTOR_EN    27

// Hall effect sensor
#define PIN_HALL        35   // Input-only GPIO, external 10k pull-up to 3.3V

// Passive buzzer
#define PIN_BUZZER      13

// Battery voltage divider (Session 17)
#define PIN_BATT_ADC    36      // ADC1_CH0, input-only, voltage divider from battery+
#define BATT_DIV_RATIO  0.3197f // R2 / (R1 + R2) — 47k / 147k
#define BATT_FULL_V     8.40f   // 2S fully charged
#define BATT_EMPTY_V    6.00f   // 2S cutoff (3.0 V/cell)

// --- Buzzer LEDC config ---
#define BUZZER_CHANNEL      0    // LEDC channel (0–15)
#define BUZZER_RESOLUTION   8    // 8-bit duty resolution

// --- Motor constants ---
#define MICROSTEPS_PER_REV      1600    // 200 full steps × 8 microsteps (TMC2209 default)
#define MICROSTEPS_PER_GLASS    (MICROSTEPS_PER_REV / 4)   // 400
#define NUM_GLASSES             4

// Motor direction mapping — based on physical wiring.
// DIR pin level that produces clockwise disc rotation viewed from top.
// If positions 2 & 4 appear swapped, flip these values.
#define MOTOR_CW_DIR    LOW
#define MOTOR_CCW_DIR   HIGH

// Pour spout offset from home position (microsteps).
// The pour spout is 135° CCW from the Hall sensor.
// As an absolute CW motor position: (360° - 135°) / 360° × 1600 = 1000.
// motorMoveToGlass adds this to each glass's base position so that
// the selected glass aligns with the pour spout, not the sensor.
#define POUR_OFFSET     1000

// Acceleration profile (trapezoidal)
#define MOTOR_MAX_SPEED     1600    // microsteps/sec at full speed
#define MOTOR_MIN_SPEED     500     // microsteps/sec at start/end (raised from 400 for torque margin)
#define MOTOR_ACCEL         1600    // microsteps/sec² — halved from 3200 to reduce lost steps under load
#define HOMING_SPEED        400     // microsteps/sec — slow for homing

// --- Hall noise rejection during homing (v1.5.4) ---
// The real magnet measures 51-53 microsteps wide at HOMING_SPEED, very
// consistently across runs. A 2026-07-28 capture recorded two homings
// that reported a width of 1 and returned SUCCESS: a single noise spike
// on the Hall line was accepted as the magnet, anchoring the disc ~78°
// out with nothing downstream able to notice.
//
// Two independent guards, because either alone leaves a hole. The
// confirm count rejects spikes shorter than the sampling window; the
// width floor catches anything that survives it. 20 sits far below the
// observed 51 (so a genuinely weak magnet read still homes) and far
// above the 1 that got through.
#define HOME_MAGNET_WIDTH_MIN   20      // microsteps; narrower = noise
#define HOME_HALL_CONFIRM       4       // consecutive agreeing reads at an edge
#define HOME_HALL_CONFIRM_US    40      // spacing between those reads

// --- Manual nudge (Session 2, v1.4.1) ---
// Nudges are short bursts from a dead stop, so they get no acceleration ramp.
// 800 sps (the old value) is above the motor's pull-in rate at rest, which
// loses the first step or two of every burst. 300 sps sits below pull-in.
// This is NOT in conflict with the MOTOR_MIN_SPEED >= 400 rule: that rule is
// about ramped moves, where the disc spends enough time at the start speed to
// excite low-speed resonance. A 4–10 step burst lasting 13–33 ms is over
// before resonance can build.
#define NUDGE_SPEED         300     // microsteps/sec for all manual nudges

// Pour-time nudge (game.cpp / h2h.cpp): coarse and quick, used to slide a
// glass under the spout mid-pour. 10 microsteps = 2.25° = ~2.8 mm at the
// 70 mm glass radius.
#define NUDGE_STEPS         10

// Calibration / diagnostic nudge (screen_calibrate, screen_glass_diag):
// fine resolution, because these screens MEASURE alignment error rather than
// just correcting it. 4 microsteps = 0.9° = ~1.1 mm at the glass.
#define CAL_NUDGE_STEPS     4

// --- Display geometry ---
#define SCREEN_W        240
#define SCREEN_H        280

// Layout zones
#define TITLE_BAR_H     36
#define TITLE_BAR_Y     0
#define CONTENT_Y       (TITLE_BAR_H + 4)      // 40
#define SOFT_BTN_Y      248
#define SOFT_BTN_H      28
#define SOFT_BTN_W      100
#define SOFT_BTN_R      4       // corner radius
#define CONTENT_H       (SOFT_BTN_Y - CONTENT_Y - 4)   // usable content height (204)

// Menu item layout
#define MENU_ITEM_H     42
#define MENU_ITEM_PAD   6       // vertical padding inside item
#define MENU_ITEM_X     8       // left margin
#define MENU_ITEM_W     (SCREEN_W - 2 * MENU_ITEM_X)   // 224

// Scroll bar layout (Session 6)
#define SCROLL_BAR_W    4       // scroll indicator width
#define SCROLL_BAR_X    (SCREEN_W - SCROLL_BAR_W - 2)  // right edge with 2px margin
#define SCROLL_BAR_PAD  4       // top/bottom padding within content area

// --- Color palette (RGB565) ---
#define COL_BG          0x0000      // Black
#define COL_TEXT        0xFFFF      // White
#define COL_ACCENT      0x5E1F      // Warm amber/gold
#define COL_DIM         0x4208      // Dark gray
#define COL_SELECTED    0x07E0      // Green
#define COL_MOVING      0xFD20      // Orange
#define COL_HOME        0x001F      // Blue
#define COL_HIGHLIGHT   0x1A3F      // Deep blue (list selection bg)
#define COL_ERROR       0xF800      // Red
#define COL_SCROLL_BG   0x2104      // Very dark gray (scroll track)
#define COL_SCROLL_FG   0x6B4D      // Medium gray (scroll thumb)
#define COL_CORRECT     0x07E0      // Pure green (RGB565) — correct guess indicator

// --- Font sizes (TFT_eSPI textSize multiplier) ---
#define FONT_TITLE      2       // Title bar text
#define FONT_BODY       2       // Body text, menu items
#define FONT_SMALL      1       // Hints, instructions
#define FONT_LARGE      4       // Big numbers, emphasis
#define FONT_XLARGE     5       // Hero numbers

// --- Display timeout defaults (seconds) ---
#define DEFAULT_DIM_DELAY   120     // 2 minutes idle → dim to 25%
#define DEFAULT_OFF_DELAY   600     // 10 minutes idle → backlight off

// --- Rating ---
#define RATING_MAX          5       // Maximum star rating (1-5)

// Price tier boundaries: 1=$ (<$30), 2=$$ ($30-60), 3=$$$ ($60-120), 4=$$$$ ($120+)

// --- Timing ---
#define DEBOUNCE_MS         50
#define LONG_PRESS_MS       600

// --- Rotary encoder quadrature decode (Session 1, v1.4.0) ---
// Raw quadrature transitions per mechanical detent click. KY-040 modules
// vary — some report 4 transitions/detent, some 2. UNVERIFIED: this value
// is a placeholder pending bench confirmation. ENCODER_DEBUG_SERIAL prints
// the measured transition count per detent on every flash so it can be
// corrected before relying on it. See docs/specs/alignment_recovery_roadmap.md
// Session 1.
#define ENC_COUNTS_PER_DETENT   4
// Flip to 1 if menu navigation moves opposite the physical turn direction.
#define ENC_INVERT_DIR          0
// Temporary: prints a cumulative raw quadrature transition count plus the
// decoded detent position to Serial. Turn the encoder exactly 10 clicks and
// divide the change in `raw` by 10 to get the true value for this module.
// Leave on until ENC_COUNTS_PER_DETENT is confirmed, then set to 0.
#define ENCODER_DEBUG_SERIAL    1

// --- Head-to-Head multiplayer ---
#define H2H_MAX_PLAYERS     4
#define H2H_NAME_LEN        13      // 12 display chars + null

// --- Wi-Fi STA mode ---
#define WIFI_STA_TIMEOUT        10000   // ms to wait for STA connection
#define WIFI_MAX_SCAN_RESULTS   15      // cap network scan list
#define WIFI_SCAN_TIMEOUT       20000   // ms before an async scan is abandoned
#define NVS_NS_WIFI             "bfwifi"

// --- OTA firmware updates ---
#define OTA_MANIFEST_URL    "https://raw.githubusercontent.com/joe-zer0-zer0/TWF/master/release/version.json"
#define OTA_CHECK_TIMEOUT   10000       // ms for manifest fetch
// HTTPClient::setTimeout() takes a uint16_t, so the old 120000 here
// silently wrapped to 54464 ms — the "120 second" download timeout has
// always actually been 54 seconds. 60000 is the honest value and is
// within a whisker of the behaviour that has been shipping; overall
// stall detection is OTA_STALL_TIMEOUT's job, not this one's.
#define OTA_DOWNLOAD_TIMEOUT 60000      // ms for binary download (max 65535)
// HTTPClient's timeout covers a socket that goes quiet, not one that stays
// open and simply stops delivering. Without this the download loop spins
// forever on a stalled connection and the only way out is a power cycle.
#define OTA_STALL_TIMEOUT   30000       // ms of no progress before abort

// Healthy uptime a freshly-installed image must accumulate before it
// confirms itself and cancels bootloader rollback. Long enough to cover
// splash, homing, and settling into idle — the window where a bad build
// realistically crashes. See ota.h.
//
// TRADE-OFF: power-cycling the device inside this window makes the
// bootloader revert to the previous firmware. That is the mechanism
// working as designed, but it means "update, then immediately switch
// off" silently downgrades the unit. Raising this value widens that
// trap; lowering it shrinks the crash window actually being checked.
#define OTA_VALIDATE_UPTIME_MS  30000
