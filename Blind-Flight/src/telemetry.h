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
//   S,<run>,<ms>,<mV>,<pass>,<order>,<visit>,<glass>,<predicted>,<measured>,<err>,<magW>,<ok>
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
// 3 = magnet wider than one glass, 4 = narrow pulse rejected as noise
// (informational — the scan continued and a later record reports the
// outcome). attempt is 0-based within one runHomingSequence() call.
void telemetryLogHoming(int magnetWidth, int attempt, int failPhase);

// One record per Hall crossing seen during a verified spin.
// crossIdx counts crossings within that single move, from 0.
void telemetryLogCrossing(int glass, int crossIdx, int expected,
                          int actual, int drift);

void telemetryLogMove(int fromPos, int toPos, bool clockwise, int steps);

// One record per position visited during the Session 9 self-test.
//
// order: 0 = sequential (1,2,3,4), 1 = randomised. visit is the 0-based
// index within the pass, glass is the physical position. Reporting both
// separately is the whole point — the existing "glass 4 is worst" evidence
// came from tools that always traverse in order, so position and
// visit-index are confounded in it.
//
// predicted = CW microsteps from the firmware's believed position to home;
// measured = what the Hall actually reported; err = predicted - measured,
// wrapped to +/- half a revolution. Positive err = the disc overshot CW.
void telemetryLogSelfTest(int pass, int order, int visit, int glass,
                          int predicted, int measured, int err,
                          int magnetWidth, bool ok);

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
