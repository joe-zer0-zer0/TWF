#include "input.h"
#include "config.h"

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

// ============================================================
// Button debounce state
// ============================================================
static bool lastBtnLeftState = HIGH;
static bool lastBtnRightState = HIGH;
static bool lastEncSwState = HIGH;
static unsigned long lastBtnLeftTime = 0;
static unsigned long lastBtnRightTime = 0;
static unsigned long lastEncSwTime = 0;

// Simple debounced read — returns true on falling edge (press)
static bool readButton(int pin, bool &lastState, unsigned long &lastTime) {
    bool reading = digitalRead(pin);
    unsigned long now = millis();
    if (reading != lastState && (now - lastTime) > DEBOUNCE_MS) {
        lastTime = now;
        lastState = reading;
        if (reading == LOW) return true;
    }
    return false;
}

// ============================================================
// Event queue — small ring buffer for pending input events
// ============================================================
#define EVENT_QUEUE_SIZE 8
static InputEvent eventQueue[EVENT_QUEUE_SIZE];
static int eventHead = 0;
static int eventTail = 0;

static void enqueueEvent(InputEvent evt) {
    int next = (eventHead + 1) % EVENT_QUEUE_SIZE;
    if (next != eventTail) {    // Drop if full (shouldn't happen at 1ms poll)
        eventQueue[eventHead] = evt;
        eventHead = next;
    }
}

// ============================================================
// Snapshot of encoder position for delta tracking
// ============================================================
static int lastEncoderSnapshot = 0;

// ============================================================
// API implementation
// ============================================================

void inputInit() {
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
}

void inputUpdate() {
    // --- Encoder rotation ---
    // Read atomically and convert to detent events.
    // Each physical detent is ~2 raw counts on a KY-040.
    int raw;
    noInterrupts();
    raw = encoderPos;
    interrupts();

    int delta = raw - lastEncoderSnapshot;
    // Generate one event per 2 raw counts (one detent)
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
    if (readButton(PIN_ENC_SW, lastEncSwState, lastEncSwTime)) {
        enqueueEvent(INPUT_ENC_CLICK);
    }
    if (readButton(PIN_BTN_LEFT, lastBtnLeftState, lastBtnLeftTime)) {
        enqueueEvent(INPUT_BTN_LEFT);
    }
    if (readButton(PIN_BTN_RIGHT, lastBtnRightState, lastBtnRightTime)) {
        enqueueEvent(INPUT_BTN_RIGHT);
    }
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
// Wraps the same ring buffer as hardware events. Call from
// the main loop context only (not from ISRs).

void inputInjectEvent(InputEvent evt) {
    enqueueEvent(evt);
}

int inputGetEncoderPos() {
    int raw;
    noInterrupts();
    raw = encoderPos;
    interrupts();
    return raw;
}

void inputSetEncoderPos(int pos) {
    noInterrupts();
    encoderPos = pos;
    interrupts();
    lastEncoderSnapshot = pos;
}
