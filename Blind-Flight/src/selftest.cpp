#include "selftest.h"
#include "motor.h"
#include "audio.h"
#include "input.h"
#include "settings.h"
#include "telemetry.h"
#include "wifi_portal.h"
#include "game.h"
#include "h2h.h"
#include "battery.h"

#include <string.h>

// runHomingSequence() lives in screens.cpp on the screen build and in
// headless_stubs.cpp on the phone-only build. Same signature, different
// retry policy — declared here rather than pulling in screens.h, which
// does not exist in the headless environment.
extern bool runHomingSequence();

// ============================================================
// State
// ============================================================

static bool sPending = false;
static bool sRunning = false;
static bool sAbortReq = false;

static SelfTestProgressFn sProgress = nullptr;

static int  sPassNum   = 0;    // 1-based across both phases
static int  sVisitNum  = 0;    // 1-based within the pass
static int  sGlass     = 0;
static int  sOrderKind = SELFTEST_ORDER_SEQ;

// Phase A readings, [pass][position]
static int16_t sIsoErr[SELFTEST_ISO_PASSES][NUM_GLASSES];
static bool    sIsoOk [SELFTEST_ISO_PASSES][NUM_GLASSES];

static SelfTestSummary sSummary;

// ============================================================
// Helpers
// ============================================================

// Fold a raw step difference into +/- half a revolution, so a reading
// taken either side of the wrap point reads as a small signed error
// rather than a near-full turn.
static int wrapErr(int e) {
    e %= MICROSTEPS_PER_REV;
    if (e < 0) e += MICROSTEPS_PER_REV;
    if (e >= MICROSTEPS_PER_REV / 2) e -= MICROSTEPS_PER_REV;
    return e;
}

// Called only between moves — never from inside a step loop, where the
// milliseconds it can take would stretch step intervals and cost steps.
static void serviceBreak() {
    if (sProgress) sProgress();
    audioUpdate();
    wifiPortalUpdate();

    // loop() is blocked for the whole run, so without this every record
    // would carry the battery reading taken before the test started —
    // and a two-minute motor run is exactly the kind of load that moves
    // it. Alignment data is uninterpretable against an unknown state of
    // charge (Corrected Fact #11).
    batteryUpdate();

    inputUpdate();
    InputEvent e;
    while ((e = inputGetEvent()) != INPUT_NONE) {
        // All input is suspended for the duration of the run except the
        // abort. Everything else is drained so it cannot fire late,
        // after the run has finished and the screen has moved on.
        if (e == INPUT_BTN_LEFT) sAbortReq = true;
    }
}

// A blocking pause that keeps the portal and audio alive.
static void waitMs(unsigned long ms) {
    unsigned long start = millis();
    while (millis() - start < ms) {
        audioUpdate();
        wifiPortalUpdate();
        delay(2);
    }
}

// Push the "test in progress" notice and give the socket time to
// actually send it. Without the flush it would sit in the queue until
// the run finished, which is precisely when it is no longer useful.
static void flushNotice() {
    wifiPortalBroadcastNow();
    for (int i = 0; i < 40; i++) {
        wifiPortalUpdate();
        delay(5);
    }
}

static void shuffle(int* a, int n) {
    // random() here is the ESP32 hardware RNG — this core defaults to it
    // and it is deliberately never seeded (see the roadmap's Corrected
    // Facts; randomSeed() would switch it to pseudo-random).
    for (int i = n - 1; i > 0; i--) {
        int j = random(i + 1);
        int t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

// ============================================================
// One measurement: traverse CW to the magnet and score the arrival
// ============================================================
// Returns true if the Hall gave a clean reading. `err` is positive when
// the disc overshot clockwise past where the firmware believed it was.

static bool measureHere(int glass, int pass, int order, int visit, int* errOut) {
    int believed  = motorGetPosition() % MICROSTEPS_PER_REV;
    if (believed < 0) believed += MICROSTEPS_PER_REV;

    // CW steps from the believed position to home.
    int predicted = (MICROSTEPS_PER_REV - believed) % MICROSTEPS_PER_REV;

    int measured = 0, width = 0;
    bool ok = motorMeasureHomeCW(&measured, &width);

    int err = ok ? wrapErr(predicted - measured) : 0;

    telemetryLogSelfTest(pass, order, visit, glass,
                         predicted, ok ? measured : -1, err, width, ok);

    if (ok) {
        if (width < sSummary.magnetWidthMin || sSummary.magnetWidthMin == 0)
            sSummary.magnetWidthMin = width;
        if (width > sSummary.magnetWidthMax) sSummary.magnetWidthMax = width;
        sSummary.reads++;
    } else {
        sSummary.failedReads++;
    }

    if (errOut) *errOut = err;
    return ok;
}

// ============================================================
// Summary
// ============================================================

static void computeSummary() {
    for (int p = 0; p < NUM_GLASSES; p++) {
        SelfTestPosStats& st = sSummary.pos[p];
        st.samples = 0;
        st.minErr  = 0;
        st.maxErr  = 0;
        long sum   = 0;

        for (int k = 0; k < SELFTEST_ISO_PASSES; k++) {
            if (!sIsoOk[k][p]) continue;
            int e = sIsoErr[k][p];
            if (st.samples == 0) { st.minErr = e; st.maxErr = e; }
            if (e < st.minErr) st.minErr = e;
            if (e > st.maxErr) st.maxErr = e;
            sum += e;
            st.samples++;
        }

        st.mean   = st.samples ? (int)((sum >= 0 ? sum + st.samples / 2
                                                 : sum - st.samples / 2) / st.samples)
                               : 0;
        st.spread = st.samples ? (st.maxErr - st.minErr) : 0;
    }

    // Inter-glass spread is the primary acceptance number. Common-mode
    // bias is deliberately not part of it — a constant offset shared by
    // all four positions is calibrated out by one trim and is harmless.
    int lo = 0, hi = 0;
    bool any = false;
    for (int p = 0; p < NUM_GLASSES; p++) {
        if (!sSummary.pos[p].samples) continue;
        int m = sSummary.pos[p].mean;
        if (!any) { lo = hi = m; any = true; }
        if (m < lo) lo = m;
        if (m > hi) hi = m;
    }
    sSummary.interGlassSpread = any ? (hi - lo) : 0;

    sSummary.worstScatter = 0;
    for (int p = 0; p < NUM_GLASSES; p++) {
        if (sSummary.pos[p].spread > sSummary.worstScatter)
            sSummary.worstScatter = sSummary.pos[p].spread;
    }

    sSummary.accumMax = 0;
    for (int p = 0; p < NUM_GLASSES; p++) {
        if (!sSummary.chainValid[p] || !sSummary.pos[p].samples) continue;
        int d = sSummary.chainErr[p] - sSummary.pos[p].mean;
        if (d < 0) d = -d;
        if (d > sSummary.accumMax) sSummary.accumMax = d;
    }
}

static void logSummary() {
    telemetryPrintf("# SELFTEST summary reads=%d failed=%d passes=%d%s",
                    sSummary.reads, sSummary.failedReads, sSummary.passesDone,
                    sSummary.aborted ? " ABORTED" : "");
    telemetryPrintf("# pos,glass,samples,mean,min,max,spread,chainErr,accum");
    for (int p = 0; p < NUM_GLASSES; p++) {
        const SelfTestPosStats& st = sSummary.pos[p];
        if (sSummary.chainValid[p] && st.samples) {
            telemetryPrintf("# pos,%d,%d,%d,%d,%d,%d,%d,%d",
                            p + 1, st.samples, st.mean, st.minErr, st.maxErr,
                            st.spread, sSummary.chainErr[p],
                            sSummary.chainErr[p] - st.mean);
        } else {
            telemetryPrintf("# pos,%d,%d,%d,%d,%d,%d,%s,-",
                            p + 1, st.samples, st.mean, st.minErr, st.maxErr,
                            st.spread, sSummary.chainValid[p] ? "?" : "-");
        }
    }
    telemetryPrintf("# interGlassSpread=%d worstScatter=%d accumMax=%d "
                    "magW=%d..%d",
                    sSummary.interGlassSpread, sSummary.worstScatter,
                    sSummary.accumMax,
                    sSummary.magnetWidthMin, sSummary.magnetWidthMax);
    telemetryPrintf("# 1 microstep = 0.225 deg = 0.275 mm at the 70 mm glass radius");
}

// ============================================================
// The run
// ============================================================

// Phase A — isolated per-position readings. Each reading is one move
// from a freshly anchored zero, so this measures per-position bias and,
// across passes, per-position scatter.
static void runIsolatedPhase() {
    for (int pass = 0; pass < SELFTEST_ISO_PASSES; pass++) {
        int order[NUM_GLASSES];
        for (int i = 0; i < NUM_GLASSES; i++) order[i] = i + 1;

        // Alternate sequential and randomised. Under the re-anchoring
        // reasoning in selftest.h these should agree; a disagreement
        // means that reasoning is wrong and is worth knowing about.
        bool randomised = (pass % 2 == 1);
        if (randomised) shuffle(order, NUM_GLASSES);
        sOrderKind = randomised ? SELFTEST_ORDER_RND : SELFTEST_ORDER_SEQ;

        for (int v = 0; v < NUM_GLASSES; v++) {
            int g = order[v];
            sPassNum  = pass + 1;
            sVisitNum = v + 1;
            sGlass    = g;

            serviceBreak();
            if (sAbortReq) return;

            motorMoveToGlass(g);
            waitMs(SELFTEST_SETTLE_MS);

            int err = 0;
            bool ok = measureHere(g, pass, sOrderKind, v, &err);
            sIsoOk[pass][g - 1]  = ok;
            sIsoErr[pass][g - 1] = (int16_t)err;
        }
        sSummary.passesDone++;
    }
}

// Phase B — chain readings. Home, walk all four positions with no
// measurement in between, and read only the last. This is the only
// arrangement that can show error accumulating across a sequence,
// because the measuring traverse re-anchors the tracked position
// wherever it is used.
static void runChainPhase() {
    sOrderKind = SELFTEST_ORDER_CHAIN;

    for (int c = 0; c < SELFTEST_CHAIN_PASSES; c++) {
        int finalGlass = c + 1;

        int order[NUM_GLASSES];
        int n = 0;
        for (int i = 1; i <= NUM_GLASSES; i++) {
            if (i != finalGlass) order[n++] = i;
        }
        shuffle(order, n);
        order[n] = finalGlass;

        sPassNum  = SELFTEST_ISO_PASSES + c + 1;
        sVisitNum = 0;
        sGlass    = finalGlass;

        serviceBreak();
        if (sAbortReq) return;

        // motorHome() rather than runHomingSequence(): a failure here
        // costs one chain reading, and the screen build's retry prompt
        // would park the whole run waiting for a button press.
        if (!motorHome()) {
            telemetryPrintf("# SELFTEST chain pass %d: homing failed, skipped",
                            c + 1);
            continue;
        }

        bool bailed = false;
        for (int v = 0; v < NUM_GLASSES; v++) {
            sVisitNum = v + 1;
            sGlass    = order[v];

            serviceBreak();
            if (sAbortReq) { bailed = true; break; }

            motorMoveToGlass(order[v]);
            waitMs(SELFTEST_SETTLE_MS);
        }
        if (bailed) return;

        int err = 0;
        bool ok = measureHere(finalGlass, SELFTEST_ISO_PASSES + c,
                              SELFTEST_ORDER_CHAIN, NUM_GLASSES - 1, &err);
        sSummary.chainValid[finalGlass - 1] = ok;
        sSummary.chainErr[finalGlass - 1]   = err;
        sSummary.passesDone++;
    }
}

static void runSelfTest() {
    sRunning  = true;
    sAbortReq = false;

    memset(&sSummary, 0, sizeof(sSummary));
    memset(sIsoOk, 0, sizeof(sIsoOk));
    memset(sIsoErr, 0, sizeof(sIsoErr));

    sPassNum = 0; sVisitNum = 0; sGlass = 0;
    sOrderKind = SELFTEST_ORDER_SEQ;

    flushNotice();

    telemetryPrintf("# SELFTEST begin fw=%s iso=%d chain=%d pourOffset=%d "
                    "homeOffset=%d side=%d speed=%d",
                    FW_VERSION, SELFTEST_ISO_PASSES, SELFTEST_CHAIN_PASSES,
                    motorGetPourOffset(), (int)settingsGetHomeOffset(),
                    (int)settingsGetPourSide(), (int)settingsGetSpinSpeed());

    if (runHomingSequence()) {
        // The driver holds for the whole run, matching what a pour
        // sequence does. Disabling between positions would measure a
        // machine that does not exist.
        motorEnable();

        runIsolatedPhase();
        if (!sAbortReq) runChainPhase();

        if (sAbortReq) {
            sSummary.aborted = true;
            telemetryPrintf("# SELFTEST aborted by user after %d passes",
                            sSummary.passesDone);
        }

        computeSummary();
        sSummary.valid = true;
        logSummary();
    } else {
        telemetryPrintf("# SELFTEST abort: initial homing failed");
        sSummary.aborted = true;
    }

    motorDisable();
    sRunning = false;
    sPassNum = 0; sVisitNum = 0; sGlass = 0;

    if (sProgress) sProgress();
    audioPlayTone(sSummary.aborted ? TONE_ERROR : TONE_CONFIRM);
    wifiPortalBroadcastNow();
}

// ============================================================
// Public API
// ============================================================

void selfTestSetProgressHook(SelfTestProgressFn fn) {
    sProgress = fn;
}

bool selfTestRequest() {
    if (sPending || sRunning) return false;
    if (gameIsActive() || h2hIsActive()) return false;
    sPending = true;
    return true;
}

void selfTestAbort() {
    sAbortReq = true;
}

void selfTestUpdate() {
    if (!sPending) return;
    sPending = false;
    runSelfTest();
}

bool selfTestIsRunning()        { return sRunning; }
int  selfTestCurrentPass()      { return sPassNum; }
int  selfTestCurrentVisit()     { return sVisitNum; }
int  selfTestCurrentGlass()     { return sGlass; }
int  selfTestCurrentOrder()     { return sOrderKind; }

const SelfTestSummary* selfTestGetSummary() { return &sSummary; }
