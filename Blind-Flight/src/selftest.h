#pragma once

// ============================================================
// Blind Flight — Alignment Self-Test (Session 9, v1.5.1)
// ============================================================
// Automatic characterisation of per-glass positional error.
//
// THE MEASUREMENT. Move to a glass the way the pour sequence would,
// stop, then step CW until the home magnet has been crossed, and
// compare the actual step count against the count the firmware's
// tracked position predicts.
//
// This is the strongest measurement this hardware supports, because
// the magnet is embedded in the DISC rather than on the motor shaft.
// The Hall sensor therefore reports true disc angle including lash and
// hub wind-up; every other signal reports where the firmware believes
// the shaft to be. It also takes the human out of the loop — unlike
// Glass Diag, nobody has to judge alignment by eye.
//
// WHAT IT IS OPTIMISING FOR. Not absolute error. A constant offset
// shared by all four glasses is invisible during a tasting and is
// calibrated out by a single trim. What breaks the blind tasting is
// being able to TELL WHICH GLASS is under the spout, so the numbers
// that matter are inter-glass spread and run-to-run scatter — and
// scatter is the one no amount of calibration can remove.
//
// ------------------------------------------------------------
// WHY THE RUN HAS TWO PHASES
// ------------------------------------------------------------
// The open question this session exists to settle is whether physical
// position 4 is genuinely worst, or whether the LAST position visited
// is worst whichever one it is. Every existing observation comes from
// tools that traverse in order, where those two are the same thing.
//
// A measurement cannot be taken without traversing to the magnet, and
// that traverse re-anchors the tracked position exactly. So a routine
// that reads every position in turn destroys accumulated error as it
// goes — it can never observe accumulation, no matter what order it
// visits in. Randomising the order does not fix that; after a traverse
// the disc always sits just past the magnet, so each position is
// always approached over the same distance in the same direction.
//
// Hence:
//
//   PHASE A — ISOLATED. Move to a position, measure, repeat. Every
//   reading is one move from a freshly anchored zero. This yields
//   per-position bias and, across passes, per-position scatter. Some
//   passes run sequentially and some randomised: under the reasoning
//   above those should agree, which makes the randomised passes a
//   control on the re-anchoring assumption rather than new data.
//
//   PHASE B — CHAIN. Home once, walk all four positions in a random
//   order with NO measurement in between — exactly what a flight does
//   — and measure only at the final position. One reading per pass,
//   with the final position rotated so each gets a turn arriving last.
//
// chain error at position p, minus isolated mean at position p, is the
// error that accumulated over the three preceding moves. If that
// difference is ~0 the problem is per-position geometry (Session 10's
// calibration table). If it is large the problem is accumulation
// (Sessions 3 and 4).
// ------------------------------------------------------------
//
// This module is compiled into BOTH build environments. The screen is
// a thin front-end; the phone triggers the identical routine. A
// self-test that existed only on the screen build would be missing
// from exactly the variant where remote troubleshooting matters most.
//
// THE RUN IS BLOCKING and takes roughly two minutes. Between positions
// it services the Wi-Fi portal and calls the progress hook, which keeps
// the phone's socket alive and the notice updating; never inside a
// step loop, which would drop steps.
// ============================================================

#include <Arduino.h>
#include "config.h"

// Isolated passes. Five is enough for a peak-to-peak scatter figure per
// position without pushing the run past a couple of minutes.
#define SELFTEST_ISO_PASSES     5

// Chain passes — one per position, so every position is measured once
// after arriving last in a full four-move sequence.
#define SELFTEST_CHAIN_PASSES   NUM_GLASSES

#define SELFTEST_TOTAL_PASSES   (SELFTEST_ISO_PASSES + SELFTEST_CHAIN_PASSES)

// Settle time after an operational move, before the measuring traverse
// begins. Long enough for the post-stop relaxation of the hub to have
// happened, as it would during a pour.
#define SELFTEST_SETTLE_MS      400

// order field in the S telemetry record
#define SELFTEST_ORDER_SEQ      0
#define SELFTEST_ORDER_RND      1
#define SELFTEST_ORDER_CHAIN    2

// --- Per-position statistics, in microsteps ---
struct SelfTestPosStats {
    int samples;
    int mean;       // rounded
    int minErr;
    int maxErr;
    int spread;     // maxErr - minErr — run-to-run scatter at this position
};

struct SelfTestSummary {
    bool valid;             // a run completed (possibly with failed reads)
    bool aborted;
    int  passesDone;
    int  reads;             // successful measurements
    int  failedReads;

    // Phase A, by physical position
    SelfTestPosStats pos[NUM_GLASSES];

    // Phase B, by physical position (one reading each)
    bool chainValid[NUM_GLASSES];
    int  chainErr[NUM_GLASSES];

    int  interGlassSpread;  // max(pos.mean) - min(pos.mean) — the primary number
    int  worstScatter;      // max(pos.spread) — the floor calibration cannot lift
    int  accumMax;          // largest |chainErr - pos.mean| seen
    int  magnetWidthMin;
    int  magnetWidthMax;
};

// Progress hook, called between positions from loop() context. Safe to
// draw from. May be null.
typedef void (*SelfTestProgressFn)(void);
void selfTestSetProgressHook(SelfTestProgressFn fn);

// Request a run. Returns false if one is already pending or running, or
// if a flight is in progress. The run itself happens in
// selfTestUpdate(), i.e. from loop() context, so this is safe to call
// from a WebSocket handler.
bool selfTestRequest();

// Ask a running test to stop at the next position boundary.
void selfTestAbort();

// Called every loop(). Runs the whole sequence when one is pending.
void selfTestUpdate();

bool selfTestIsRunning();

// True from the moment a run is requested until it has finished — i.e.
// pending OR running. A caller that only checks selfTestIsRunning()
// cannot tell "not started yet" from "already done", because the run is
// queued in one loop() pass and executed in the next.
bool selfTestIsBusy();

// Live progress — valid while running.
int  selfTestCurrentPass();     // 1-based, across both phases
int  selfTestCurrentVisit();    // 1-based within the pass
int  selfTestCurrentGlass();
int  selfTestCurrentOrder();    // SELFTEST_ORDER_*

// Result of the last completed run.
const SelfTestSummary* selfTestGetSummary();
