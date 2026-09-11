#include "led.h"
#include "config.h"
#include "battery.h"
#include "game.h"

// ============================================================
// PWM setup — LEDC channels 4 (red) and 5 (green)
// ============================================================

#define LED_RED_CHANNEL    4
#define LED_GREEN_CHANNEL  5
#define LED_PWM_FREQ       5000
#define LED_PWM_RES        8       // 0–255

static LedState baseState   = LED_OFF;
static LedState activeState = LED_OFF;
static unsigned long animStart = 0;
static bool otaOverride = false;

// ============================================================
// Charging detection
// ============================================================
// The TP4056 CHRG pin is open-drain, active LOW when charging.
// Needs an external pull-up to 3.3V on the GPIO.

#ifdef PIN_CHRG_SENSE
static bool chargingDetected = false;
static unsigned long lastChrgRead = 0;
#define CHRG_SAMPLE_MS  500
#endif

bool ledIsCharging() {
#ifdef PIN_CHRG_SENSE
    return chargingDetected;
#else
    return false;
#endif
}

// ============================================================
// Raw PWM output
// ============================================================

static void setRed(uint8_t duty) {
#ifdef PIN_LED_RED
    ledcWrite(LED_RED_CHANNEL, duty);
#endif
}

static void setGreen(uint8_t duty) {
#ifdef PIN_LED_GREEN
    ledcWrite(LED_GREEN_CHANNEL, duty);
#endif
}

static void setBoth(uint8_t r, uint8_t g) {
    setRed(r);
    setGreen(g);
}

// ============================================================
// Animation helpers
// ============================================================

// Smooth sine pulse 0→255→0 over periodMs
static uint8_t pulse(unsigned long periodMs) {
    unsigned long t = (millis() - animStart) % periodMs;
    float phase = (float)t / periodMs * 2.0f * PI;
    return (uint8_t)((sin(phase - PI / 2.0f) + 1.0f) * 127.5f);
}

// Square-wave blink: on for onMs, off for offMs
static bool blinkOn(unsigned long onMs, unsigned long offMs) {
    unsigned long t = (millis() - animStart) % (onMs + offMs);
    return t < onMs;
}

// Double blink: two short flashes then a pause
static bool doubleBlink(unsigned long flashMs, unsigned long gapMs, unsigned long pauseMs) {
    unsigned long period = flashMs + gapMs + flashMs + pauseMs;
    unsigned long t = (millis() - animStart) % period;
    if (t < flashMs) return true;
    t -= flashMs;
    if (t < gapMs) return false;
    t -= gapMs;
    if (t < flashMs) return true;
    return false;
}

// ============================================================
// Animation driver — one frame
// ============================================================

static void animateState(LedState state) {
    uint8_t v;

    switch (state) {
        case LED_OFF:
            setBoth(0, 0);
            break;

        case LED_BOOT:
            setBoth(200, 120);
            break;

        case LED_HOMING:
            v = pulse(1500);
            setBoth(v, (uint8_t)(v * 0.6f));
            break;

        case LED_READY:
            setBoth(0, 180);
            break;

        case LED_GAME_ACTIVE:
            setBoth(0, blinkOn(1500, 500) ? 180 : 0);
            break;

        case LED_SPINNING:
            v = blinkOn(150, 150) ? 220 : 0;
            setBoth(v, (uint8_t)(v * 0.5f));
            break;

        case LED_POURING:
            v = pulse(2000);
            setBoth(0, v);
            break;

        case LED_TASTING:
            setBoth(0, doubleBlink(200, 150, 1200) ? 180 : 0);
            break;

        case LED_LOW_BATTERY:
            setBoth(blinkOn(500, 1500) ? 220 : 0, 0);
            break;

        case LED_LOCKOUT:
            setBoth(220, 0);
            break;

        case LED_CHARGING: {
            v = pulse(3000);
            setBoth(v, (uint8_t)(v * 0.6f));
            break;
        }

        case LED_OTA: {
            // Alternating red/green every 300ms
            unsigned long t = (millis() - animStart) % 600;
            if (t < 300) setBoth(220, 0);
            else         setBoth(0, 220);
            break;
        }

        case LED_ERROR:
            setBoth(blinkOn(100, 100) ? 255 : 0, 0);
            break;
    }
}

// ============================================================
// State resolution
// ============================================================
// Determines the effective state by layering overrides on top
// of the caller-set base state.

static LedState resolveState() {
    if (otaOverride) return LED_OTA;
    if (batteryIsLockout()) return LED_LOCKOUT;
    if (batteryIsLow() && !gameIsActive()) return LED_LOW_BATTERY;
    if (gameIsSpinning()) return LED_SPINNING;

    // Game-state refinement: if a game is active, map the game
    // sub-state to a more specific LED pattern.
    if (gameIsActive()) {
        GameState gs = gameGetState();
        if (gs == GAME_POURING) return LED_POURING;
        if (gs == GAME_TASTING || gs == GAME_RATING) return LED_TASTING;
        return LED_GAME_ACTIVE;
    }

#ifdef PIN_CHRG_SENSE
    if (chargingDetected) return LED_CHARGING;
#endif

    return baseState;
}

// ============================================================
// Public API
// ============================================================

void ledInit() {
#ifdef PIN_LED_RED
    ledcSetup(LED_RED_CHANNEL, LED_PWM_FREQ, LED_PWM_RES);
    ledcAttachPin(PIN_LED_RED, LED_RED_CHANNEL);
    ledcWrite(LED_RED_CHANNEL, 0);
#endif

#ifdef PIN_LED_GREEN
    ledcSetup(LED_GREEN_CHANNEL, LED_PWM_FREQ, LED_PWM_RES);
    ledcAttachPin(PIN_LED_GREEN, LED_GREEN_CHANNEL);
    ledcWrite(LED_GREEN_CHANNEL, 0);
#endif

#ifdef PIN_CHRG_SENSE
    pinMode(PIN_CHRG_SENSE, INPUT);
    chargingDetected = (digitalRead(PIN_CHRG_SENSE) == LOW);
    lastChrgRead = millis();
    Serial.printf("[LED] CHRG pin %d: %s\n", PIN_CHRG_SENSE,
                  chargingDetected ? "charging" : "not charging");
#endif

    baseState = LED_BOOT;
    activeState = LED_BOOT;
    animStart = millis();

    Serial.println("[LED] Init complete");
}

void ledSetState(LedState state) {
    baseState = state;
}

LedState ledGetState() {
    return activeState;
}

void ledUpdate() {
#ifdef PIN_CHRG_SENSE
    unsigned long now = millis();
    if (now - lastChrgRead >= CHRG_SAMPLE_MS) {
        lastChrgRead = now;
        chargingDetected = (digitalRead(PIN_CHRG_SENSE) == LOW);
    }
#endif

    LedState resolved = resolveState();
    if (resolved != activeState) {
        activeState = resolved;
        animStart = millis();
    }

    animateState(activeState);
}

void ledSetOtaOverride(bool active) {
    otaOverride = active;
    if (active) {
        activeState = LED_OTA;
        animStart = millis();
        animateState(activeState);
    }
}

void ledTick() {
    animateState(activeState);
}
