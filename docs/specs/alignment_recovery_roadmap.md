# Blind Flight — Alignment Recovery Roadmap

**Created:** 2026-07-24
**Baseline:** v1.3.10 (`4fcae04`)
**Status:** Planned — no sessions executed yet

---

## Purpose

This roadmap addresses the glass-alignment problem blocking beta, plus a set of
independent correctness bugs found during a full-codebase review. It is written
so that a session picking up any single item has everything it needs without
re-deriving the analysis.

**Read the "Corrected Facts" section before touching `motor.cpp`.** Several
beliefs carried in earlier sessions are wrong, and they are wrong in ways that
produce plausible-looking but incorrect code.

---

## Corrected Facts (supersedes prior session assumptions)

### 1. The disc does NOT always travel clockwise

Prior sessions assumed all disc motion is CW. It is not.

`motorMoveToPosition()` ([motor.cpp:316](../../Blind-Flight/src/motor.cpp:316))
picks the **shortest direction**:

```cpp
if (cwDist <= ccwDist) { clockwise = true;  stepsToMove = cwDist;  }
else                   { clockwise = false; stepsToMove = ccwDist; }
```

`motorMoveToGlass()` calls it, so glass positioning can go either way. Only
`motorSpinToGlass()` and `motorSpinSteps()` are unconditionally CW.

`motorHome()` Phase 4 ends with a **CCW** move ([motor.cpp:293](../../Blind-Flight/src/motor.cpp:293)).

### 2. `homeOffset` silently changes the approach direction to glass 1

`effectivePourOffset = POUR_OFFSET(1000) + pourSide*400 + homeOffset`

The move from home (position 0) to glass 1 targets `effectivePourOffset`. It
goes **CCW when that value is > 800, CW when < 800**. With `pourSide = Front`,
that boundary sits at `homeOffset = -200`.

So the calibration trim does not merely shift the result — crossing that
threshold relocates where backlash is absorbed and reshapes the whole error
profile discontinuously. This is a likely cause of the "inconsistent results"
reported from bench testing.

### 3. Measured backlash is ~60 microsteps (13.5°)

Recorded run, `homeOffset = -60`, `effectivePourOffset = 940`:

| Move | From | To | CW | CCW | Chosen | Reversal | Offset | Delta |
|---|---|---|---|---|---|---|---|---|
| Home Ph4 | — | 0 | — | — | **CCW** | — | — | — |
| → G1 | 0 | 940 | 940 | 660 | **CCW** | no | +0 | — |
| → G2 | 940 | 1340 | 400 | 1200 | **CW** | **YES** | +60 | **+60** |
| → G3 | 1340 | 140 | 400 | 1200 | CW | no | +90 | +30 |
| → G4 | 140 | 540 | 400 | 1200 | CW | no | +90 | **0** |

One direction reversal in the sequence; it carries two-thirds of the error.
The +30 residual at G3 is consistent with *compliant* lash (a 3D-printed hub
winds up elastically and needs a second move to fully seat) rather than a
clean mechanical gap.

**The G3→G4 delta is zero.** Cumulative motor step loss would keep growing.
It does not. Motor step loss is NOT the dominant error term in this dataset —
mechanical lash is.

### 4. The glass-diag tool contaminates its own measurement

`undoNudgeRaw()` ([screen_glass_diag.cpp:197](../../Blind-Flight/src/screen_glass_diag.cpp:197))
reverses direction to undo each nudge, then the next `motorMoveToGlass()`
reverses back. With ~60 microsteps of lash, the instrument injects a lash
event between every reading.

### 5. `random()` must NOT be seeded

This ESP32 core defaults `random()` to the **hardware RNG**. Calling
`randomSeed()` switches it to pseudo-random — the opposite of what's wanted.
See the comment at `Arduino.h:153` in the installed core. The absence of a
`randomSeed()` call is correct. Do not "fix" it.

### 6. The magnet is in the disc, not on the motor shaft

Per `CLAUDE.md`: "Hall effect sensor + neodymium magnet embedded in disc."

This is the single most important fact for the closed-loop design. The Hall
sensor measures **actual disc position, backlash included**. It is the one
sensor in the system that is not fooled by the hub play. Closed-loop
correction built on it is lash-tolerant and does not depend on the set screw.

### 7. Glass toppling is a centripetal-force problem

Lateral acceleration on a glass is `a = ω²r` at r = 70 mm:

| Speed (sps) | Rev/s | Lateral accel | In g |
|---|---|---|---|
| 2400 | 1.50 | 6.22 m/s² | **0.63 g** |
| 1600 | 1.00 | 2.76 m/s² | 0.28 g |
| 1200 | 0.75 | 1.55 m/s² | 0.16 g |
| 1000 | 0.63 | 1.08 m/s² | 0.11 g |
| 800 | 0.50 | 0.69 m/s² | 0.07 g |
| 600 | 0.38 | 0.39 m/s² | 0.04 g |

A Glencairn tips above roughly `base_radius / CoM_height` ≈ 24/45 ≈ **0.53 g**,
falling toward **0.44 g** as fill height raises the centre of mass. The current
Fast preset (2400 sps, 0.63 g) is over the threshold — matching the observed
toppling, including its dependence on fill level.

Because force scales with ω², **halving speed quarters the lateral force.**

### 8. Build targets

Two PlatformIO environments share `src/`:

- `esp32` — full build (screen + phone)
- `esp32-headless` — phone-only; `src_filter` excludes `ui.cpp`,
  `transitions.cpp`, `splash.cpp`, `screens.cpp`, `palate_training.cpp`,
  `screen_*.cpp`, `browse.cpp`

`motor.cpp` and `input.cpp` are in **both**. `input.cpp` guards its body with
`#ifndef HEADLESS_BUILD`. **Every session touching those files must compile
both environments before declaring done.**

---

## Pre-work: one free experiment (do this first)

Before any code changes, validate the backlash hypothesis on the current build:

1. Set `homeOffset` to **-200** via the Calibrate screen (pushes
   `effectivePourOffset` to 740, flipping the move to G1 from CCW to CW).
2. Run Glass Diag. Record all four offsets and the magnet width.
3. Repeat 3 times to gauge run-to-run scatter.

**Expected if the analysis is right:** the +60 jump relocates away from G1→G2
or disappears, because the reversal has moved.

**If the numbers come back unchanged**, the interpretation in Corrected Fact #3
is wrong and Sessions 2–3 should be re-scoped before building. Report the
numbers back before proceeding.

Note: with the current encoder bug (Session 1) these readings will be noisy.
Turn the encoder **slowly and deliberately** to minimise dropped detents.

---

## Session Plan

Sessions are ordered so each produces a flashable, independently testable
build. Sessions 1–2 exist to make the *instrument* trustworthy before using it
to evaluate anything else.

---

### Session 1 — Encoder quadrature decode

**Priority:** P0. Blocks all measurement work.
**Model:** Sonnet 5 — well-specified, self-contained, standard algorithm.
**Target version:** 1.4.0

**Problem.** Commit `7fb2025` added a 2 ms debounce to the encoder ISR
([input.cpp:14](../../Blind-Flight/src/input.cpp:14)). A KY-040 detent produces
two CLK edges; on a fast turn they land closer than 2 ms apart, so the second
is swallowed. `inputUpdate()` consumes exactly 2 counts per event:

```cpp
while (delta >= 2) { enqueueEvent(INPUT_ENC_CW); lastEncoderSnapshot += 2; delta -= 2; }
```

One dropped edge leaves a permanent parity residue that never clears — from
that point every other detent produces no movement. This is the reported
symptom: "not all rotary inputs translating to movement," intermittent, worse
on fast turns.

**Changes — `input.cpp` only:**

1. Attach `CHANGE` interrupts to **both** `PIN_ENC_CLK` and `PIN_ENC_DT`.
2. Replace the ISR with a Gray-code state machine:

```cpp
// index = (prev << 2) | cur, where state = (CLK << 1) | DT
static const int8_t QTAB[16] = { 0,-1, 1, 0,  1, 0, 0,-1,
                                -1, 0, 0, 1,  0, 1,-1, 0 };
static volatile uint8_t encPrev  = 0;
static volatile int8_t  encAccum = 0;
static volatile int     encoderPos = 0;

void IRAM_ATTR encoderISR() {
    uint8_t cur = (digitalRead(PIN_ENC_CLK) << 1) | digitalRead(PIN_ENC_DT);
    encAccum += QTAB[(encPrev << 2) | cur];
    encPrev = cur;
    if      (encAccum >=  ENC_COUNTS_PER_DETENT) { encoderPos++; encAccum = 0; }
    else if (encAccum <= -ENC_COUNTS_PER_DETENT) { encoderPos--; encAccum = 0; }
}
```

3. Change `inputUpdate()` to consume **1** count per event (not 2).
4. Remove the `micros()` debounce entirely — the state machine rejects bounce
   by construction (a bounce returns to the previous state and nets zero).
5. Mark `encPrev`, `encAccum`, `encoderPos` as `volatile`. The existing
   `lastCLK` was **not** volatile despite being written from the ISR — that
   bug goes away with the rewrite, but do not reintroduce it.

**Unverified assumption — must be checked on hardware.** KY-040 modules vary:
some produce 4 state transitions per detent, some 2. Define
`ENC_COUNTS_PER_DETENT` in `config.h` and add a temporary serial print of raw
`encAccum` transitions so Jeremy can count them empirically on the first flash.
Do not hardcode 4 without confirming.

**Sign convention:** `QTAB` may produce inverted direction depending on how
CLK/DT are wired. If CW rotation decrements, negate the table. Confirm on
hardware, don't guess.

**Test checklist:**
- [ ] Both build environments compile
- [ ] Serial shows a consistent count-per-detent; note the number
- [ ] Slow turn: every click moves the menu selection exactly one item
- [ ] Fast flick of ~10 clicks: selection moves exactly 10 items, no drops
- [ ] Reverse direction repeatedly — no stuck/skipped clicks
- [ ] Menu direction matches physical rotation (CW = down the list)
- [ ] Encoder click (SW) still works; long-press still works
- [ ] Nudge on Calibrate screen responds to every click

---

### Session 2 — Nudge determinism

**Priority:** P0. Second half of making the instrument trustworthy.
**Model:** Sonnet 5 — mechanical changes, tightly specified.
**Target version:** 1.4.1

**Problem.** Three nudge implementations start instantly at 800 sps from a dead
stop with no post-enable settling delay. This is the same stiction issue
already diagnosed and fixed in HW Diag ([screen_hw_diag.cpp:62](../../Blind-Flight/src/screen_hw_diag.cpp:62)
uses `delay(5)`), but the fix never propagated.

**Changes:**

1. **`motor.cpp`** — add enable-state tracking so the settling delay is applied
   exactly once per disabled→enabled transition:

```cpp
static bool driverEnabled = false;

void motorEnable() {
    if (!driverEnabled) {
        digitalWrite(PIN_MOTOR_EN, LOW);
        delay(5);              // TMC2209 charge pump + current regulation
        driverEnabled = true;
    }
}
void motorDisable() { digitalWrite(PIN_MOTOR_EN, HIGH); driverEnabled = false; }
```

   Then replace every bare `digitalWrite(PIN_MOTOR_EN, LOW)` inside
   `moveSteps`, `moveStepsVerified`, `motorHome`, `motorSpinToGlass`,
   `motorSpinSteps` with a call to `motorEnable()`.

2. **`config.h`** — add `#define NUDGE_SPEED 300` (below pull-in rate; 10 steps
   takes 33 ms). Reduce `NUDGE_STEPS` from 10 to **4** (0.9°, ~1.1 mm at the
   glass) for finer calibration resolution.

3. **`screen_calibrate.cpp` / `screen_glass_diag.cpp`** — replace the local
   `CAL_NUDGE_SPEED` / `NUDGE_SPEED` (both currently 800) with the shared
   constant. Remove the duplicate local `#define NUDGE_STEPS` in
   `screen_calibrate.cpp:28`, which currently shadows the `config.h` value with
   an identical literal.

4. **Partial redraw.** Both nudge screens call `fillScreen()` + full redraw on
   every detent — roughly 50–100 ms of SPI at 40 MHz. Redraw only the numeric
   field (erase its bounding rect, draw new value). This is why the encoder
   feels unresponsive independent of the decode bug.

**Test checklist:**
- [ ] Both build environments compile
- [ ] First nudge after entering Calibrate moves the disc (previously lost)
- [ ] 10 consecutive clicks CW move the disc 40 microsteps — verify against
      the HW Diag Step Test page or a physical mark
- [ ] Same test CCW
- [ ] Nudge display updates without full-screen flicker
- [ ] HW Diag jog still works at all four step sizes

---

### Session 3 — CW-only motion

**Priority:** P0. The core lash-tolerance change.
**Model:** **Opus 5.** This is where prior sessions went wrong — it requires
holding the interaction between shortest-path selection, `homeOffset`, pour
side, and the homing sequence simultaneously. Do not delegate.
**Target version:** 1.5.0

**Rationale.** "Always approach from the same side" is the standard
anti-backlash technique. It works *because* the lash exists — it parks the play
permanently on one side where it becomes a constant that `homeOffset` absorbs,
rather than a variable that flips sign with every direction change. **It does
not depend on the set screw being fixed.**

**Changes:**

1. **New API in `motor.h` / `motor.cpp`:**

```cpp
// Move to absolute position, always clockwise. Never reverses.
void motorMoveToPositionCW(int targetPos);

// Move the disc by a small signed delta at NUDGE_SPEED, updating tracked
// position. May go either direction — for manual nudges only, never for
// automatic positioning.
void motorNudge(int delta);

// Tell the motor module the disc actually moved `delta` from where it
// thought it was, without moving anything. For diagnostics folding a
// manual correction into tracked position.
void motorAdjustPosition(int delta);
```

2. **`motorMoveToPosition` becomes CW-only.** Delete the shortest-path branch.
   Worst case is 1600 steps instead of 400 — about 1.6 s at the new Normal
   speed. Acceptable.

3. **CRITICAL — migrate the pour nudge first.** The pour-sequence nudge calls
   `motorMoveToPosition(motorGetPosition() ± NUDGE_STEPS)` in
   [game.cpp:1549](../../Blind-Flight/src/game.cpp:1549) and
   [h2h.cpp:972](../../Blind-Flight/src/h2h.cpp:972). Once the function is
   CW-only, a CCW click becomes a **1590-step full revolution**. Change both
   call sites to `motorNudge(±NUDGE_STEPS)` **in the same commit**. Missing
   this ships a build that spins a full revolution on a single encoder click
   mid-pour, with glasses loaded.

4. **Homing Phase 4 — approach centre from the CW side.** Replace the CCW
   backup at [motor.cpp:293](../../Blind-Flight/src/motor.cpp:293):

```cpp
// Was: reverse CCW by magnetWidth/2 (direction reversal, eats lash)
// Now: continue CW the long way to arrive at centre without reversing.
int forwardSteps = MICROSTEPS_PER_REV - (magnetWidth / 2);
motorEnable();
digitalWrite(PIN_MOTOR_DIR, MOTOR_CW_DIR);
delayMicroseconds(10);
// Use a ramped move at ~800 sps — this is a known-distance move, not a search
```

   Costs one extra revolution (~2 s ramped). Also delete the now-unneeded
   `delay(20)` reversal-settling hack.

5. **Homing hardening** (bundle here, same file, same test cycle):
   - **Phase 1 guard band.** Commit `c8dc2e3` added an early `break` when the
     Hall releases, parking the disc exactly on the trailing edge. Phase 2 then
     starts polling for a LOW immediately, so one noisy sample declares a false
     leading edge → `magnetWidth` reads ~1–5 instead of ~30–60 → position 0
     lands on the trailing edge, off by `magnetWidth/2`. Fix: after the Hall
     releases, continue CW another **30 microsteps** before entering Phase 2.
   - **Edge debounce.** Require Hall LOW for 3 consecutive steps before
     declaring the leading edge; same for the Phase 3 release.
   - **Width sanity check.** Reject and retry any run where `magnetWidth < 8`
     or differs from the previous successful run by more than 50%.

6. **Glass-diag procedure.** Delete `undoNudgeRaw()`. Instead call
   `motorAdjustPosition(diagNudge)` to fold the correction into tracked
   position, then move CW to the next glass. Keep a running software total so
   the summary still reports **cumulative** offsets (unchanged display
   semantics), but the disc never reverses.

**Do NOT change in this session:** `motorSpinToGlass` / `motorSpinSteps` are
already CW-only. Leave them alone.

**Test checklist:**
- [ ] Both build environments compile
- [ ] **Pour nudge:** during POURING, one CCW encoder click moves the disc a
      few microsteps CCW — NOT a full revolution. Test this first, before
      loading glasses.
- [ ] Homing succeeds from 8 starting positions including parked on the magnet
- [ ] Serial `magnet width` is consistent across all 8 runs (this is the A1 fix
      working — inconsistent widths mean the guard band is still too small)
- [ ] Glass Diag: run 3× with `homeOffset = 0`. Record all offsets.
- [ ] Glass Diag: repeat at `homeOffset = -100` and `-200`. Offsets should now
      shift **linearly** with the trim — no discontinuity at -200. This is the
      proof that Corrected Fact #2 is resolved.
- [ ] Full 4-glass flight, glasses empty, then loaded

---

### Session 4 — Speed presets and motion profile

**Priority:** P1. Fixes glass toppling; also a safety issue at live events.
**Model:** Opus 5 for the ramp maths, Sonnet 5 acceptable for the preset values
if split into two sessions.
**Target version:** 1.5.1

**Change 4a — square-root acceleration ramp.** `moveSteps` and
`moveStepsVerified` compute `accelSteps` with the correct constant-acceleration
formula but then ramp speed **linearly against step index**. Constant
acceleration requires `v(n) = sqrt(v₀² + 2·a·n)`. The linear ramp yields
`a(n) = v · dv/dn`:

| Point | Speed | Actual accel | vs. configured 1600 |
|---|---|---|---|
| Start | 500 sps | 763 sps² | 48% |
| End of ramp | 1600 sps | **2441 sps²** | **153%** |

So torque demand spikes 1.5× exactly where the motor is fastest and weakest,
and the mirror-image spike at the start of deceleration causes overrun. Replace
both branches in **both** functions (they contain duplicated ramp logic —
consider extracting a shared helper):

```cpp
// accel phase
currentSpeed = sqrtf((float)startSpeed*startSpeed + 2.0f*MOTOR_ACCEL*(i + 1));
// decel phase
int stepsLeft = steps - i;
currentSpeed = sqrtf((float)stopSpeed*stopSpeed + 2.0f*MOTOR_ACCEL*stepsLeft);
```

**Change 4b — split start and stop speed.** `MOTOR_MIN_SPEED` currently governs
both. They have opposite requirements: the start needs torque margin, the stop
needs low kinetic energy so the disc doesn't ring past target.

```c
#define MOTOR_START_SPEED   500   // torque margin off the line
#define MOTOR_STOP_SPEED    250   // 4× less KE to absorb at arrival
```

**Change 4c — reduce speed presets** (`motorSetSpinSpeed`, [motor.cpp:419](../../Blind-Flight/src/motor.cpp:419)):

```cpp
case 0:  runtimeMaxSpeed = 1600; runtimeExtraMin = 1; runtimeExtraMax = 2; break; // Fast,   0.28 g
case 2:  runtimeMaxSpeed =  600; runtimeExtraMin = 2; runtimeExtraMax = 4; break; // Gentle, 0.04 g
default: runtimeMaxSpeed = 1000; runtimeExtraMin = 1; runtimeExtraMax = 3; break; // Normal, 0.11 g
```

Extra-rev counts are raised on slower presets so total spin *duration* stays
theatrical rather than merely slow.

**Change 4d — unify the labels.** Firmware says `{"Fast","Normal","Slow"}`
([screen_settings.cpp:57](../../Blind-Flight/src/screen_settings.cpp:57)); the
phone UI says `['Fast','Normal','Theatr.']`
([web/phone_ui.html:526](../../Blind-Flight/web/phone_ui.html:526)). Pick one
set and apply to both. Recommend `Fast / Normal / Gentle`.

**Test checklist:**
- [ ] Both build environments compile
- [ ] All three presets: 4-glass flight with glasses filled to the **highest**
      level you'd ever pour
- [ ] No toppling on any preset (this is the acceptance criterion)
- [ ] Glass Diag before/after — arrival accuracy should improve from 4b
- [ ] Spin still feels suspenseful on Normal; adjust extra-revs if not
- [ ] Setting persists across power cycle; phone UI shows matching label

---

### Session 5 — Monitoring and telemetry

**Priority:** P1. Explicitly requested: "the current issues certainly
demonstrate the need for a robust monitoring and error correction system."
**Model:** Sonnet 5.
**Target version:** 1.5.2

Measurement only — no corrective action yet. Goal is hard data on whether lash
or step loss dominates on any given mechanical build.

**Changes:**

1. **Log every Hall crossing during every spin.** `moveStepsVerified` currently
   latches only the **first** crossing. Change it to record every crossing:
   expected step count vs. actual, for each. With 1–3 extra revolutions this
   yields 2–4 data points per spin, and reveals whether error accumulates
   during the spin or arrives all at once.
2. **Log every homing run:** magnet width, retry count, which phase failed.
3. **Session log with a run ID** so serial captures can be correlated across
   power cycles.
4. **Reset `lastDrift = 0`** at the top of every `moveStepsVerified` call, and
   return a `triggered` flag. (Fixes the stale-drift bug in Session 6's
   dependency chain — see below.)

**Deliberately NOT in this session:** the corrective move. Collect data first.

**Test checklist:**
- [ ] Both build environments compile
- [ ] Serial log from a full flight is parseable and complete
- [ ] Magnet width logged on every home
- [ ] Multiple crossings logged per spin
- [ ] No timing regression — logging must not happen inside the step loop
      (buffer values, print after the move completes)

---

### Session 6 — Closed-loop position correction

**Priority:** P1, but **gated on the set screw / hub fix**.
**Model:** **Opus 5.** Control-loop design with sign conventions, wraparound
arithmetic, and hardware-timing interaction.
**Target version:** 1.6.0

**Do not start this session until:**
- Sessions 3 and 5 are flashed and validated
- The hub/set-screw manufacturing issue is resolved (thresholds cannot be tuned
  against a moving mechanical baseline)
- Session 5 telemetry from a mechanically stable build is available

**Existing bug this fixes.** `motorSpinToGlass`
([motor.cpp:388](../../Blind-Flight/src/motor.cpp:388)) applies drift
correction on `if (lastDrift != 0)` with no check that *this* spin measured
anything. If a spin doesn't trigger, the previous spin's drift is silently
re-applied. Session 5 adds the `triggered` flag; this session consumes it.

**Design.** The magnet is in the **disc**, so the Hall sensor sees true disc
position including backlash. Combined with CW-only motion from Session 3:

1. Measure actual position at the **last** home crossing before the target
   (not the first).
2. Deliberately aim **short** of the target so residual error is always in the
   same direction.
3. Correct **forward, CW only** — never reverse. Backlash becomes a constant
   the correction absorbs rather than a variable it fights.
4. Apply the corrective move at `MOTOR_STOP_SPEED` with a threshold (~2
   microsteps) to avoid hunting.

**Test checklist:**
- [ ] Both build environments compile
- [ ] Glass Diag offsets converge toward zero across G1–G4
- [ ] Correction never causes a full-revolution move (CW-only + aim-short
      invariant holds — verify by inspection AND on hardware)
- [ ] No hunting/oscillation at the arrival point
- [ ] Behaviour is sane when the Hall sensor is disconnected (fail safe to
      open-loop, don't hang)
- [ ] 4-glass flight, loaded, repeated 5× — measure spout alignment each time

---

### Session 7 — Independent correctness fixes

**Priority:** P1 for beta. No dependency on the mechanical work — **can run in
parallel with Sessions 1–6.**
**Model:** Sonnet 5 for all items.
**Target version:** 1.6.1

These are unrelated to alignment and were found during the same review.

**7a — JSON buffer arithmetic** (`wifi_portal.cpp`). Four functions use
`pos += snprintf(json + pos, sizeof(json) - pos, ...)`. `sizeof()` is `size_t`
(unsigned), so once `pos > sizeof(json)` the size argument **underflows to
~4 billion** and `snprintf` writes past the end of a stack buffer.
Affected: `sendFavoritesJSON` (:1010), `broadcastFavoritesJSON` (:1031),
`buildAndBroadcastH2H` (:1113), `handleWifiScan` (:1243).

Reachability: favorites ≈ 796 B against a 1024 B buffer — under today, but one
constant change from overflowing. `handleWifiScan` ≈ 909 B with 15 max-length
SSIDs — genuinely marginal, and a crowded venue is where it would bite.
Additionally `json[pos] = '\0'` is unguarded in all four and can write one byte
past the end on exact-boundary truncation — that one is reachable now.

Fix with a single shared helper:

```cpp
static void jsonAppend(char* buf, int bufLen, int& pos, const char* fmt, ...) {
    if (pos >= bufLen - 1) return;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf + pos, bufLen - pos, fmt, ap);
    va_end(ap);
    pos = (n < 0 || pos + n >= bufLen) ? bufLen - 1 : pos + n;
}
```

Also unify the state-buffer sizes: `broadcastState` uses 1280 B while
`sendStateToClient` uses 1024 B, so a client's first snapshot truncates
differently from subsequent broadcasts.

**7b — `otaMarkValid()` defeats rollback.** Called as the first line of
`setup()` ([main.cpp:31](../../Blind-Flight/src/main.cpp:31)), before display,
Wi-Fi, or motor init. A new image that crash-loops has already been marked
valid and will never roll back. Move it to after the device reaches a known-good
state — after splash completes and first homing succeeds, or on a ~30 s healthy
uptime timer. **This matters for beta OTA logistics specifically.**

**7c — OTA download stall.** [ota.cpp:217](../../Blind-Flight/src/ota.cpp:217)
spins forever if the connection stays open but stops delivering. Track
`millis()` of the last successful read; abort after ~30 s of no progress.

**7d — `expectedSize` unused.** `otaPerformUpdate()` takes the parameter and
never references it; the manifest's advertised size is parsed then ignored.
Either enforce it against `Content-Length` or drop the parameter.

**7e — H2H battery lockout dead-end.**
[h2h.cpp:284](../../Blind-Flight/src/h2h.cpp:284) returns without advancing
`phase` or showing anything — the game silently stops responding. Mirror the
proper handling in `game.cpp:1095`.

**7f — H2H never sets `spinningNow`.** `game.cpp` brackets its spins with the
flag; `h2h.cpp` doesn't, so the phone never shows spinning state during
Head-to-Head.

**7g — Volume level 0 parks the buzzer pin LOW.**
`applyVolumeDuty()` ([audio.cpp:73](../../Blind-Flight/src/audio.cpp:73)) calls
`ledcWrite(ch, 0)` at level 0, which on the active-low MH-FMD means the driver
conducts DC through the coil for the duration of every "silent" note — the exact
condition `buzzerIdle()` exists to prevent. Add
`if (sVolLvl == 0) { buzzerIdle(); return; }`.

**7h — WebSocket starvation in blocking waits.** `delayWithAudio()`
([game.cpp:74](../../Blind-Flight/src/game.cpp:74)) and the three wait loops in
`runHomingSequence()` don't call `wifiPortalUpdate()`. One line each.
(The motor-spin half of this problem is Session 8.)

**7i — Small items:**
- `persistLoadGame` doesn't range-check `glassCount` / `pourCount` / `mode` from
  NVS; `pourCount` indexes a 4-element array in `runPourCycle`
- `settingsInit()` clamps out-of-range NVS values but leaves `sDirty = false`,
  so corrections are never written back and re-clamp every boot
- `uiPopScreenT()` called from inside `onEnter()` on the homing-failure path in
  `screen_calibrate.cpp:240` and `screen_glass_diag.cpp:323` — the nested
  push/pop-during-transition anti-pattern `CLAUDE.md` warns about; use the
  deferred-flag idiom
- `WiFi.scanNetworks()` runs synchronously, blocking the loop for seconds; use
  the async form plus a poll

**7j — `motorHome()` called bare on idle wake.**
[ui.cpp:242](../../Blind-Flight/src/ui.cpp:242) calls the raw motor function
with no retry, no error handling, and no UI — and it runs **regardless of game
state**. If the display timed out during TASTING with four full glasses, the
first button press spins the carousel up to 1.5 revolutions. At a live event
that is a spill. Skip the re-home when a flight is active (defer to the next
`runPourCycle`, which already homes via `homedThisFlight`), and use
`runHomingSequence()` when it does run.

*Note: 7j is arguably P0 for event safety even though it's unrelated to
alignment. Consider pulling it forward into Session 1.*

---

### Session 8 — Motor task migration (deferred)

**Priority:** P2. Architectural.
**Model:** Opus 5.

`moveSteps` calls `yield()` every 256 steps, but `yield()` hands off to the RTOS
scheduler and does **not** run `loop()`. A 3-revolution spin is ~3 s with the
WebSocket unserviced — the "blocking motor loops freeze WebSocket/HTTP/DNS"
gotcha in `CLAUDE.md`.

`wifiPortalUpdate()` cannot be called from inside the step loop (it can take
milliseconds and would drop steps). The correct fix is moving motor control to
a dedicated FreeRTOS task pinned to core 1 while `loop()` services WebSockets on
core 0.

**This becomes mandatory for the headless build**, where the phone is the only
interface and a 3-second freeze is the entire UI.

Defer until the alignment work is settled — it touches every blocking call site
and would make Sessions 1–6 harder to test in isolation.

---

## Model Allocation Rationale

This project was built almost entirely by Opus 4.6. The review that produced
this roadmap found several errors in that work, and they share a shape worth
naming.

**The characteristic failure was asserting control flow without reading it.**
The specific instance: prior sessions stated the disc only ever travels in one
direction, which is contradicted by the shortest-path branch sitting in
`motorMoveToPosition`. That single wrong belief made the direction-reversal
backlash invisible, and it is the root of the alignment problem.

That failure mode is not about code-writing ability — it is about holding
several interacting invariants at once (offset arithmetic → direction selection
→ mechanical lash → measured error) and *verifying* rather than assuming.

**Use Opus 5 for:** Sessions 3, 4a/4b, 6, 8. Anything touching motion geometry,
direction selection, control loops, or concurrency.

**Sonnet 5 is appropriate for:** Sessions 1, 2, 5, 7, and 4c/4d. These are
tightly specified, self-contained, and have clear acceptance criteria.

**Process guardrail for every session, regardless of model:**

> Before changing any motor behaviour, read `motorMoveToPosition`,
> `motorMoveToGlass`, `motorSpinToGlass`, and `motorHome` **in full** and state
> in the session notes which direction each move actually takes for the current
> `effectivePourOffset`. Do not assume. This is the specific error that produced
> the alignment problem.

---

## Open Questions

1. **Fast preset ceiling.** 1600 sps (0.28 g) gives roughly 2× margin against
   the estimated 0.53 g tipping threshold — but that threshold is calculated
   from nominal Glencairn dimensions, not measured. Worth an empirical check:
   fill a glass to the highest level you'd pour, and find the speed at which it
   actually topples. If the real threshold is lower than 0.53 g, drop Fast to
   1200 sps.

2. **Preset naming.** Recommend `Fast / Normal / Gentle`. Confirm before
   Session 4d touches both the firmware array and the phone UI.

3. **Glass-diag reporting semantics.** Session 3 keeps the display **cumulative**
   (running software total) even though the disc no longer reverses. Confirm
   that's the more useful read — the alternative is per-move incremental, which
   the Delta column already shows.

4. **Session 7j urgency.** The idle-wake re-home is a spill risk at live events
   and is independent of everything else. Recommend pulling it into Session 1
   rather than waiting for Session 7.

5. **Set screw timeline.** Session 6 is gated on it. If the 3D-print issue
   persists, consider an interim mechanical fix (thread-locker, a flatted shaft
   with adhesive, or a clamping hub) so the closed-loop work isn't blocked
   indefinitely.

---

## Related Documents

- `docs/tmc2209_current_setup.md` — bench procedure for driver current and mode
- `CLAUDE.md` — authoritative hardware facts (supersedes the original hardware spec)
- `docs/specs/phone_only_architecture.md` — headless build architecture
