#include "telemetry.h"
#include "battery.h"
#include "config.h"

#include <Preferences.h>
#include <esp_system.h>
#include <stdarg.h>
#include <string.h>

// ============================================================
// Blind Flight — Telemetry Module (Session 5, v1.5.0)
// ============================================================

static const char* TELEM_NS = "bftel";

// Circular byte buffer. sHead is the next write index; once sWrapped
// is set, the oldest readable byte is the one at sHead.
static char   sRing[TELEM_RING_BYTES];
static size_t sHead    = 0;
static bool   sWrapped = false;

static uint32_t sBytesWritten = 0;
static uint32_t sRunId        = 0;

// Longest line we will emit. Records are ~55 bytes; free-form lines
// are prose. Anything longer is truncated rather than split.
#define TELEM_LINE_MAX 192

// ============================================================
// Ring buffer
// ============================================================

static void ringPut(const char* s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        sRing[sHead++] = s[i];
        if (sHead >= TELEM_RING_BYTES) {
            sHead = 0;
            sWrapped = true;
        }
    }
    sBytesWritten += n;
}

void telemetrySegments(const char** seg1, size_t* len1,
                       const char** seg2, size_t* len2) {
    *seg1 = nullptr; *len1 = 0;
    *seg2 = nullptr; *len2 = 0;

    if (!sWrapped) {
        *seg1 = sRing;
        *len1 = sHead;
        return;
    }

    // Wrapped: oldest byte sits at sHead, and it is almost certainly
    // the middle of a line that got half overwritten. Skip to just
    // after the first newline so the output starts on a record
    // boundary.
    const char* tail     = sRing + sHead;
    size_t      tailLen  = TELEM_RING_BYTES - sHead;
    const char* nl       = (const char*)memchr(tail, '\n', tailLen);

    if (nl) {
        size_t skip = (size_t)(nl - tail) + 1;
        *seg1 = tail + skip;
        *len1 = tailLen - skip;
        *seg2 = sRing;
        *len2 = sHead;
        return;
    }

    // No newline in the tail segment at all — the partial line runs
    // past the wrap point. Look for the boundary in the head segment.
    const char* nl2 = (const char*)memchr(sRing, '\n', sHead);
    if (nl2) {
        size_t skip = (size_t)(nl2 - sRing) + 1;
        *seg1 = sRing + skip;
        *len1 = sHead - skip;
    }
}

// ============================================================
// Writing
// ============================================================

void telemetryPrintf(const char* fmt, ...) {
    char line[TELEM_LINE_MAX];

    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    if (n < 0) return;
    if (n >= (int)sizeof(line)) n = sizeof(line) - 1;   // truncated

    Serial.write((const uint8_t*)line, (size_t)n);
    Serial.write('\n');

    ringPut(line, (size_t)n);
    ringPut("\n", 1);
}

// Battery in millivolts. batteryGetVoltage() is the smoothed pack
// voltage, sampled every 2 s in loop() — near enough for correlating
// alignment against state of charge, and it costs no ADC time here.
static int battMv() {
    return (int)(batteryGetVoltage() * 1000.0f + 0.5f);
}

void telemetryLogHoming(int magnetWidth, int attempt, int failPhase) {
    telemetryPrintf("H,%lu,%lu,%d,%d,%d,%d",
                    (unsigned long)sRunId, (unsigned long)millis(), battMv(),
                    magnetWidth, attempt, failPhase);
}

void telemetryLogCrossing(int glass, int crossIdx, int expected,
                          int actual, int drift) {
    telemetryPrintf("X,%lu,%lu,%d,%d,%d,%d,%d,%d",
                    (unsigned long)sRunId, (unsigned long)millis(), battMv(),
                    glass, crossIdx, expected, actual, drift);
}

void telemetryLogMove(int fromPos, int toPos, bool clockwise, int steps) {
    telemetryPrintf("M,%lu,%lu,%d,%d,%d,%s,%d",
                    (unsigned long)sRunId, (unsigned long)millis(), battMv(),
                    fromPos, toPos, clockwise ? "CW" : "CCW", steps);
}

void telemetryLogSelfTest(int pass, int order, int visit, int glass,
                          int predicted, int measured, int err,
                          int magnetWidth, bool ok) {
    telemetryPrintf("S,%lu,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                    (unsigned long)sRunId, (unsigned long)millis(), battMv(),
                    pass, order, visit, glass,
                    predicted, measured, err, magnetWidth, ok ? 1 : 0);
}

// ============================================================
// Init / accessors
// ============================================================

void telemetryInit() {
    sHead = 0;
    sWrapped = false;
    sBytesWritten = 0;

    // One NVS write per boot. Wear is negligible at that rate, and a
    // monotonic ID is worth far more than a random one when several
    // captures from different power cycles get pasted together.
    Preferences p;
    p.begin(TELEM_NS, false);
    sRunId = p.getUInt("run", 0) + 1;
    p.putUInt("run", sRunId);
    p.end();

    // Reset reason distinguishes an ordinary power cycle from a
    // brownout, a panic or a watchdog. Without it a rebooted device is
    // indistinguishable from one the user simply switched off, and the
    // ring buffer it cleared looks like a short session rather than a
    // fault. POWERON/SW are benign; BROWNOUT/PANIC/WDT are not.
    const char* why;
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  why = "POWERON";  break;
        case ESP_RST_SW:       why = "SW";       break;   // esp_restart(), e.g. after OTA
        case ESP_RST_PANIC:    why = "PANIC";    break;
        case ESP_RST_INT_WDT:  why = "INT_WDT";  break;
        case ESP_RST_TASK_WDT: why = "TASK_WDT"; break;
        case ESP_RST_WDT:      why = "WDT";      break;
        case ESP_RST_BROWNOUT: why = "BROWNOUT"; break;
        case ESP_RST_DEEPSLEEP:why = "DEEPSLEEP";break;
        case ESP_RST_EXT:      why = "EXT";      break;
        default:               why = "UNKNOWN";  break;
    }

    telemetryPrintf("# boot run=%lu fw=%s ring=%d reset=%s",
                    (unsigned long)sRunId, FW_VERSION, TELEM_RING_BYTES, why);
}

uint32_t telemetryGetRunId()        { return sRunId; }
uint32_t telemetryGetBytesWritten() { return sBytesWritten; }
bool     telemetryHasWrapped()      { return sWrapped; }
