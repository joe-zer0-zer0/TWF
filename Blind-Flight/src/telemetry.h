#pragma once

// ============================================================
// Blind Flight — Telemetry Module (Session 5, v1.5.0)
// ============================================================
// A RAM ring buffer of structured, machine-parseable records,
// retrievable over HTTP at GET /log.
//
// Why RAM and not NVS: the log is grabbed while the device is
// still powered, so flash write endurance buys nothing.
//
// Why pull and not push: blocking motor loops freeze the HTTP
// and WebSocket servers, so a live stream would stutter exactly
// while the interesting data is being produced. Reading the
// buffer after the run sidesteps that entirely.
//
// Record formats (one line each, CSV-shaped). Every record
// carries the boot run ID, uptime in ms, and battery millivolts
// — battery state is an uncontrolled variable that changes motor
// torque, so alignment data is uninterpretable without it.
//
//   H,<run>,<ms>,<mV>,<magnetWidth>,<attempt>,<failPhase>
//   X,<run>,<ms>,<mV>,<glass>,<crossIdx>,<expected>,<actual>,<drift>
//   M,<run>,<ms>,<mV>,<from>,<to>,<dir>,<steps>
//
// Anything else written to the ring is prefixed '#', so a parser
// that drops '#' lines sees pure CSV.
//
// THREADING: ring writes happen only from loop() context and the
// HTTP handler reads from the same context. Neither is called
// from an ISR, and nothing here may be called from inside a motor
// step loop — buffer values locally during a move and flush after
// it completes.
// ============================================================

#include <Arduino.h>

// Ring capacity in bytes. Records run ~45–55 bytes, so this holds
// roughly 300 of them — around a dozen full flights.
#define TELEM_RING_BYTES 16384

void telemetryInit();

// Free-form line. Written to Serial and the ring. Do NOT include a
// trailing newline. Prefix the text with '#' so CSV parsers skip it.
void telemetryPrintf(const char* fmt, ...);

// --- Structured records ---

// failPhase: 0 = success, 1 = stuck on magnet, 2 = magnet not found,
// 3 = magnet wider than one glass. attempt is 0-based within one
// runHomingSequence() call.
void telemetryLogHoming(int magnetWidth, int attempt, int failPhase);

// One record per Hall crossing seen during a verified spin.
// crossIdx counts crossings within that single move, from 0.
void telemetryLogCrossing(int glass, int crossIdx, int expected,
                          int actual, int drift);

void telemetryLogMove(int fromPos, int toPos, bool clockwise, int steps);

// --- Accessors ---

// Monotonic boot counter, persisted in NVS, so captures taken across
// power cycles can be ordered and separated.
uint32_t telemetryGetRunId();

// Total bytes ever written, and whether the ring has wrapped (i.e.
// whether the capture is missing its oldest lines).
uint32_t telemetryGetBytesWritten();
bool     telemetryHasWrapped();

// Readable log as at most two contiguous segments, oldest first.
// A partial leading line left by a wrap is trimmed off. Either
// segment may be empty.
void telemetrySegments(const char** seg1, size_t* len1,
                       const char** seg2, size_t* len2);
