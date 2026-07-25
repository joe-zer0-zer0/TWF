#include "input.h"
#include "config.h"

#ifndef HEADLESS_BUILD
// ============================================================
// Encoder ISR state (volatile — modified in interrupt)
// ============================================================
// Full quadrature Gray-code decode. index = (prevState << 2) | curState,
// where state = (CLK << 1) | DT. A bounce returns to the previous state
// and nets zero in the table, so it's rejected by construction — no
// time-based debounce needed.
// DRAM_ATTR: the ISR is installed with ESP_INTR_FLAG_IRAM, so it can run
// while flash is busy (NVS write, OTA download). A flash-resident lookup
// table would fault there — force it into RAM.
static const int8_t DRAM_ATTR QTAB[16] = { 0,-1, 1, 0,  1, 0, 0,-1,
                                          -1, 0, 0, 1,  0, 1,-1, 0 };

static volatile uint8_t encPrev  = 0;
static volatile int8_t  encAccum = 0;
static volatile int     encoderPos = 0;

// Debug only (ENCODER_DEBUG_SERIAL) — cumulative count of valid quadrature
// transitions. Deliberately NOT reset per detent: the whole point is to
// compare it against a physically counted number of detent clicks, which
// a per-detent count cannot reveal (it always equals the configured value).
static volatile uint32_t encRawTransitions = 0;

void IRAM_ATTR encoderISR() {
    uint8_t cur = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
    int8_t delta = QTAB[(encPrev << 2) | cur];
    encPrev = cur;
    if (delta == 0) return;   // illegal/bounce transition — ignore

    encAccum += ENC_INVERT_DIR ? (int8_t)-delta : delta;
    encRawTransitions++;

    if (encAccum >= ENC_COUNTS_PER_DETENT) {
        encoderPos++;
        encAccum = 0;
    } else if (encAccum <= -ENC_COUNTS_PER_DETENT) {
        encoderPos--;
        encAccum = 0;
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
    encPrev = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), encoderISR, CHANGE);

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
    while (delta >= 1) {
        enqueueEvent(INPUT_ENC_CW);
        lastEncoderSnapshot += 1;
        delta -= 1;
    }
    while (delta <= -1) {
        enqueueEvent(INPUT_ENC_CCW);
        lastEncoderSnapshot -= 1;
        delta += 1;
    }

#if ENCODER_DEBUG_SERIAL
    // Temporary — see ENC_COUNTS_PER_DETENT in config.h.
    // Procedure: note the printed values, turn the encoder exactly 10 detent
    // clicks in one direction, then read the last line. (raw_after - raw_before)
    // divided by 10 is the true transitions-per-detent for this module.
    // `pos` should have moved by exactly 10 once the constant is correct.
    static uint32_t lastPrintedRaw = 0;
    uint32_t rawCount;
    int      posNow;
    noInterrupts();
    rawCount = encRawTransitions;
    posNow   = encoderPos;
    interrupts();
    if (rawCount != lastPrintedRaw) {
        lastPrintedRaw = rawCount;
        Serial.printf("[Encoder] raw=%lu pos=%d (ENC_COUNTS_PER_DETENT=%d)\n",
                      (unsigned long)rawCount, posNow, ENC_COUNTS_PER_DETENT);
    }
#endif

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
