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
#define FW_VERSION  "1.3.3"

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
#define MOTOR_MIN_SPEED     400     // microsteps/sec at start/end (above resonance zone)
#define MOTOR_ACCEL         1600    // microsteps/sec² — halved from 3200 to reduce lost steps under load
#define HOMING_SPEED        400     // microsteps/sec — slow for homing

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

// --- Head-to-Head multiplayer ---
#define H2H_MAX_PLAYERS     4
#define H2H_NAME_LEN        13      // 12 display chars + null

// --- Wi-Fi STA mode ---
#define WIFI_STA_TIMEOUT        10000   // ms to wait for STA connection
#define WIFI_MAX_SCAN_RESULTS   15      // cap network scan list
#define NVS_NS_WIFI             "bfwifi"

// --- OTA firmware updates ---
#define OTA_MANIFEST_URL    "https://raw.githubusercontent.com/joe-zer0-zer0/TWF/master/release/version.json"
#define OTA_CHECK_TIMEOUT   10000       // ms for manifest fetch
#define OTA_DOWNLOAD_TIMEOUT 120000     // ms for binary download
