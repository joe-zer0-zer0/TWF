#include "input.h"
#include "config.h"

#ifndef HEADLESS_BUILD
// ============================================================
// Encoder ISR state (volatile — modified in interrupt)
// ============================================================
static volatile int encoderPos = 0;
static volatile bool encoderMoved = false;
static int lastCLK;

void IRAM_ATTR encoderISR() {
    int clk = digitalRead(PIN_ENC_CLK);
    int dt = digitalRead(PIN_ENC_DT);
    if (clk != lastCLK) {
        if (dt != clk) {
            encoderPos++;
        } else {
            encoderPos--;
        }
        encoderMoved = true;
        lastCLK = clk;
    }
}
#endif // !HEADLESS_BUILD

// ============================================================
// Event queue — small ring buffer for pending input events
// ============================================================
#define EVENT_QUEUE_SIZE 8
static InputEvent eventQueue[EVENT_QUEUE_SIZE];
static int eventHead = 0;
static int eventTail = 0;

static void enqueueEvent(InputEvent evt) {
    int next = (eventHead + 1) % EVENT_QUEUE_SIZE;
    if (next != eventTail) {
        eventQueue[eventHead] = evt;
        eventHead = next;
    }
}

#ifndef HEADLESS_BUILD
// ============================================================
// Button debounce and long-press state
// ============================================================
struct ButtonState {
    bool lastReading;
    bool pressed;               // true while held down (debounced)
    unsigned long debounceTime; // last edge timestamp for debounce
    unsigned long pressTime;    // millis() when press was registered
};

static ButtonState btnLeft  = { HIGH, false, 0, 0 };
static ButtonState btnRight = { HIGH, false, 0, 0 };
static ButtonState btnEncSw = { HIGH, false, 0, 0 };

static void pollButton(int pin, ButtonState &bs,
                        InputEvent shortEvt, InputEvent longEvt) {
    bool reading = digitalRead(pin);
    unsigned long now = millis();

    if (reading != bs.lastReading && (now - bs.debounceTime) > DEBOUNCE_MS) {
        bs.debounceTime = now;
        bs.lastReading = reading;

        if (reading == LOW) {
            bs.pressed = true;
            bs.pressTime = now;
        } else if (bs.pressed) {
            unsigned long held = now - bs.pressTime;
            enqueueEvent(held >= LONG_PRESS_MS ? longEvt : shortEvt);
            bs.pressed = false;
        }
    }
}

static void pollEncSw(int pin, ButtonState &bs) {
    bool reading = digitalRead(pin);
    unsigned long now = millis();
    if (reading != bs.lastReading && (now - bs.debounceTime) > DEBOUNCE_MS) {
        bs.debounceTime = now;
        bs.lastReading = reading;
        if (reading == LOW) {
            bs.pressed = true;
            bs.pressTime = now;
        } else if (bs.pressed) {
            unsigned long held = now - bs.pressTime;
            enqueueEvent(held >= LONG_PRESS_MS ? INPUT_ENC_LONG : INPUT_ENC_CLICK);
            bs.pressed = false;
        }
    }
}

// ============================================================
// Snapshot of encoder position for delta tracking
// ============================================================
static int lastEncoderSnapshot = 0;
#endif // !HEADLESS_BUILD

// ============================================================
// API implementation
// ============================================================

void inputInit() {
#ifndef HEADLESS_BUILD
    // Encoder
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT, INPUT_PULLUP);
    pinMode(PIN_ENC_SW, INPUT);     // External pull-up on GPIO 34
    lastCLK = digitalRead(PIN_ENC_CLK);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoderISR, CHANGE);

    // Buttons
    pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
    pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

    // Sync snapshot
    noInterrupts();
    lastEncoderSnapshot = encoderPos;
    interrupts();
#endif
}

void inputUpdate() {
#ifndef HEADLESS_BUILD
    // --- Encoder rotation ---
    int raw;
    noInterrupts();
    raw = encoderPos;
    interrupts();

    int delta = raw - lastEncoderSnapshot;
    while (delta >= 2) {
        enqueueEvent(INPUT_ENC_CW);
        lastEncoderSnapshot += 2;
        delta -= 2;
    }
    while (delta <= -2) {
        enqueueEvent(INPUT_ENC_CCW);
        lastEncoderSnapshot -= 2;
        delta += 2;
    }

    // --- Buttons ---
    pollEncSw(PIN_ENC_SW, btnEncSw);
    pollButton(PIN_BTN_LEFT, btnLeft, INPUT_BTN_LEFT, INPUT_BTN_LEFT_LONG);
    pollButton(PIN_BTN_RIGHT, btnRight, INPUT_BTN_RIGHT, INPUT_BTN_RIGHT_LONG);
#endif
}

InputEvent inputGetEvent() {
    if (eventTail == eventHead) return INPUT_NONE;
    InputEvent evt = eventQueue[eventTail];
    eventTail = (eventTail + 1) % EVENT_QUEUE_SIZE;
    return evt;
}

// ============================================================
// Inject a synthetic event (Session 16 — phone interface)
// ============================================================

void inputInjectEvent(InputEvent evt) {
    enqueueEvent(evt);
}
