# Blind Flight — Alignment Recovery Roadmap

**Created:** 2026-07-24
**Revised:** 2026-07-26 — measurement strategy, acceptance criteria, execution order
**Baseline:** v1.3.10 (`4fcae04`)
**Status:** Session 1 complete (v1.4.0, `54b8e30`). Session 2 complete (v1.4.1).
7b complete (v1.4.2/v1.4.3). 7j complete (already guarded in `ui.cpp`).
**Next: Session 5 (telemetry core).** See "Revised Execution Order" below — the
original 3-before-5 ordering assumed a serial capture path that does not exist.

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

**Corollary for this roadmap:** any diagnostic *sequence* must live in a module
compiled into both environments (e.g. `selftest.cpp`), with the screen as a thin
front-end. A self-test that only exists on the screen build is unavailable on
exactly the variant where remote troubleshooting matters most.

### 9. The deadband is in the shaft→disc joint, and it re-opens after every stop

Bench observation, 2026-07-26: at jog step:1 the **motor shaft visibly and
palpably rotates while the disc stays still**, for several steps, *regardless of
direction or of the direction of the previous move*.

This rules out rotor-side stiction and microstep torque quantisation — the rotor
is moving. The deadband is downstream, in the hub.

Two mechanisms explain the direction-independence, and both are probably active:

- **Momentum overrun.** During a CW move the shaft's driving flank pushes the
  disc, so the play sits behind. When the motor stops, the disc coasts forward
  past the shaft and comes to rest against the *trailing* flank. The play is now
  in front of the direction of travel and must be re-crossed from scratch.
- **Elastic relaxation.** The printed hub twists under drive torque. When motion
  stops, stored torque pushes the disc forward until it drops below static
  friction. The disc creeps ahead of the shaft; the next move must re-wind that
  twist before the disc breaks loose.

**Both scale with how abruptly the move ends.** Session 4's deceleration work
(sqrt ramp, separate `MOTOR_STOP_SPEED`) therefore reduces the deadband directly
— it is an alignment fix, not only an anti-toppling fix. Free experiment:
compare dead-step count at jog step:1 after a fast spin vs. after a slow
approach. Fewer dead steps after a gentle stop confirms the mechanism.

**Consequence for the mechanical fix:** a set screw removes the *gap* term and
does nothing to the *elastic* term. The elastic term is per-unit — it depends on
print density, hub fit, and applied torque. Firmware compensation is required
regardless of how well the hub is clamped, and it must be per-unit.

**The quantity that matters is the post-stop coast, not the full gap.** Let `D`
be the total play and `x` the shaft's position within it, where `x = D` means the
shaft is pressed against the flank that drives CW. An operational CW move ends at
`x = D`; the disc then coasts and unwinds forward, leaving `x_rest < D`. The dead
steps observed on the next CW move are `D − x_rest`, **not** `D`.

This matters for Session 10. Once the disc has actually moved in a direction, the
play is fully seated on that side — so a user's CCW alignment nudge does put the
play in a known state, and the residual error versus an operational approach is
only `x_rest`, not the whole `D`. But `x_rest` **depends on how abruptly the move
stopped**, so a lash-takeup maneuver executed as a short unramped burst at
`NUDGE_SPEED` leaves a *different* end state than a ramped operational approach.
The takeup's return leg must therefore reproduce the operational deceleration
profile, not merely the direction. See Session 10.

**One state this reasoning does not cover:** a user clicks CCW but the disc does
*not* visibly move — the shaft is somewhere mid-gap and `x` is indeterminate.
This is the case the takeup maneuver actually exists to resolve, and it is easy
to hit, because a `CAL_NUDGE_STEPS`-sized click (4 µsteps) is well under `D`.

### 10. Driver hold policy is intentional — and there is one gap in it

**The policy (confirmed by Jeremy, 2026-07-26, and it is correct):** the driver
holds position continuously for the duration of a pour sequence, and powers down
once the sequence completes or on idle timeout. The motor runs too hot to hold
indefinitely, and it costs battery. Position is re-verified by homing on recovery
from idle or power loss, which maintains a verified position across the sequence.

**The current code already implements this.** `motorDisable()` in `game.cpp`
appears only at [game.cpp:1204](../../Blind-Flight/src/game.cpp:1204) (sequence
end, one line before `state = GAME_TASTING`), `game.cpp:1098` (battery lockout)
and `game.cpp:1920` (`gameAbort`). There is **no per-pour disable** —
`motorEnable()` early-returns once `driverEnabled` is set, so the driver stays
energized from the first move to the end of the sequence. No change needed.

Detent snap on disable (up to 4 microsteps as the free rotor settles to the
nearest full step) is therefore **harmless** in the normal path: nothing needs
the position after the sequence ends, and idle-off is followed by a re-home.

**The gap — mid-flight idle timeout.** [ui.cpp:226](../../Blind-Flight/src/ui.cpp:226)
calls `motorDisable()` when the display times off. If that happens *during* an
active flight — between pour 2 and pour 3, say — the disc becomes free to rotate
by hand. On wake, [ui.cpp:242](../../Blind-Flight/src/ui.cpp:242) deliberately
skips re-homing while a game is active (correct — that was the 7j spill fix), and
its comment claims "the next `runPourCycle()` re-homes on its own via
`homedThisFlight`."

**It does not.** [game.cpp:1113](../../Blind-Flight/src/game.cpp:1113) homes only
`if (!homedThisFlight)`, and the flag is already true after pour 1. Same shape in
[h2h.cpp:292](../../Blind-Flight/src/h2h.cpp:292). So the remaining pours run on
a position the firmware believes but has not verified — which is exactly the
invariant the hold policy exists to protect.

Fix is small and is filed as **7k**: when the driver is disabled while a flight
is active, clear `homedThisFlight` (or set an explicit `positionVerified = false`)
so the next `runPourCycle()` re-homes. Costs one extra homing per idle event,
holds no current, and matches the stated recovery model.

### 10b. Re-home is NOT needed after the pour sequence

Also confirmed 2026-07-26: **the disc positions are physically numbered**, and
the tasting proceeds from the glasses themselves — Jeremy typically removes all
four, keeps them in numeric order, and finishes away from the device. Nothing
downstream of the final pour depends on the firmware knowing where the disc is.

So disc movement during TASTING is harmless, and an operator-facing re-home
control is unnecessary. `Re-Home` belongs under Diagnostics as an engineering
tool, which is where Session 9 puts it. *(This supersedes an earlier
recommendation in this document to keep Re-Home top-level or bind it to an
encoder long-press; that was based on a TASTING-phase risk that does not exist.)*

### 11. Battery voltage is an uncontrolled variable

Available motor torque falls as the 2S pack drains, which changes both the
deadband and any step loss. A unit that aligns at 8.4 V may drift at 6.8 V — and
a tasting event is a long discharge. **Every telemetry record must carry the
battery reading** so alignment can be plotted against state of charge. If the
two correlate, that is a beta-blocking finding that would otherwise surface for
the first time in front of guests.

---

## Acceptance Criteria

Established 2026-07-26. **These supersede any implicit "get the error to zero"
goal, and they change what the work is optimising for.**

The failure mode is not "the pour misses the glass." The top opening is the same
diameter as the glass rim, so modest misalignment still pours. The failure mode
is that **the user can identify which glass is under the spout**, which breaks
the blind tasting. Confirmed in testing: misalignment has been most pronounced
at glass 4, pronounced enough to be identifiable during a pour sequence.

That makes the criterion **differential, not absolute**:

- A constant offset shared by all four glasses is nearly harmless and is
  trivially calibrated out. **Common-mode bias is unconstrained.**
- **Per-glass variation is the enemy.** Target `max(offset) − min(offset)`
  across the four glasses.
- **Run-to-run scatter is equally the enemy**, because it makes a glass differ
  from itself between flights.

Scale: at r = 70 mm, one microstep = 0.225° = **0.275 mm** at the glass.

| Metric | Goal | Acceptable | Notes |
|---|---|---|---|
| Inter-glass spread (max−min of 4) | ≤ 8 µsteps (2.2 mm) | ≤ 15 µsteps (4.1 mm) | The primary number |
| Run-to-run scatter, single glass, peak-to-peak over 5 passes | ≤ 8 µsteps | ≤ 12 µsteps | Sets the floor; **not** removable by calibration |
| Common-mode offset | unconstrained | unconstrained | Calibrated out |

The numeric thresholds are **hypotheses**. The definitive acceptance test is
free: run a full flight without watching, and try to call the positions. If you
cannot identify them, the device passes regardless of what the numbers say. Run
it before and after each mechanical change.

**Diagnostic question — physical glass 4, or the fourth pour? Still open, and
the existing evidence is confounded.**

Clarified 2026-07-26: "glass 4" means the **fourth physical position on the
disc**. But Jeremy is explicit that this is a cumulative impression across all
alignment work rather than a controlled result, and — critically — **most of it
came from Glass Diag and Motor Test, where glasses are addressed directly and
traversed in order.** In those modes position 4 is simply the last one reached,
so accumulated error and a position-4 geometry defect are indistinguishable.

The two hypotheses:

- *Physical position 4 genuinely worst* → geometry / mount / holder-spacing
  error. Fixed by the per-glass calibration table (Session 10).
- *Last-visited worst, whichever position* → error accumulating across moves.
  Attacked by Sessions 3 and 4.

Note the apparent tension with Corrected Fact #3, whose recorded run shows the
G3→G4 delta at **zero** — error accruing early then plateauing. Both hypotheses
can be partly true.

**Requirement on Session 9:** the auto-diag must visit positions in **both
sequential (1→2→3→4) and randomised order**, and report per-*position* and
per-*visit-index* separately. Sequential-only would reproduce the exact confound
in the existing data and settle nothing.

**Calibration removes bias. It cannot remove scatter.** Measure the scatter
before deciding any mechanical question, including whether to epoxy the disc to
the shaft. A clamping hub or a flatted shaft with retaining compound is the
reversible middle option and should be tried first — epoxy couples disc, motor,
and skid plate into one non-serviceable assembly.

---

## Pre-work: free experiments (no code, no parts)

### A. Does a gentle stop shrink the deadband?

Tests Corrected Fact #9, and tells us whether Session 4 buys real accuracy.

1. Run a fast spin, let it stop, then jog at **step:1** and count the dead steps
   before the disc moves.
2. Repeat after a slow, gentle approach to the same position.

**Expected if the analysis is right:** fewer dead steps after the gentle stop.
That confirms momentum overrun / elastic relaxation and promotes Session 4 from
an anti-toppling fix to an alignment fix.

### B. Blind identification baseline

The definitive acceptance test, and it costs nothing. Run a full flight without
watching and try to call the positions. Record how many of four you get right.
Repeat after every mechanical or firmware change. **If you can't identify them,
the device passes** — whatever the microstep numbers say.

### C. `homeOffset` direction-flip check (original pre-work)

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

Sessions are ordered so each produces a **released, independently testable**
build. Sessions 1–2 exist to make the *instrument* trustworthy before using it
to evaluate anything else.

### Revised Execution Order (2026-07-26)

Session numbers are retained so cross-references stay valid; the *order* has
changed. The original 3-before-5 sequence assumed a serial capture path existed.
It does not — the USB port is inaccessible with the device assembled, and nothing
has ever pulled data off the device. Session 5 therefore becomes the enabler
rather than a follow-up.

| Order | Session | Deliverable | Version | Model |
|---|---|---|---|---|
| 1 | **5** | Telemetry core: ring buffer + `/log` route | 1.5.0 | Sonnet 5 |
| 2 | **9** *(new)* | Diagnostics consolidation + auto self-test → **baseline data** | 1.5.1 | Sonnet 5 |
| 3 | **3** | CW-only motion + homing hardening → re-measure | 1.6.0 | **Opus 5** |
| 4 | **4** | Ramp / stop-speed → re-measure | 1.6.1 | **Opus 5** (4a/4b) |
| 5 | **10** *(new)* | Teach-in per-glass calibration table | 1.7.0 | **Opus 5** |
| 6 | **6** | Closed-loop correction | 1.7.1 | **Opus 5** |
| — | **7** | Independent correctness fixes — **runs in parallel any time** | — | Sonnet 5 |
| — | **8** | Motor task migration — deferred, elevated for headless | — | **Opus 5** |

**Why measure before Session 3, not after.** A baseline taken on today's
shortest-path behaviour is what makes the CW-only change *provable* rather than
merely plausible. Every subsequent session re-runs the identical auto-diag and
compares. Without a baseline, a session that improves nothing looks the same as
one that fixes everything.

**Exception — pull 7k forward.** Item 7k (mid-flight idle timeout leaves the
disc unverified for the rest of the flight) is a live-event correctness bug, not
an alignment refinement. It is a few lines in `game.cpp` / `h2h.cpp` and should
ride along with Session 9, which already touches homing behaviour.

**Coding happens in separate sessions with clean context.** Each row above is a
session boundary. Do not batch rows — OTA-only delivery makes small diffs a
safety property (see `CLAUDE.md`, "Prefer small, frequent releases").

---

### Session 1 — Encoder quadrature decode — **DONE (v1.4.0, `54b8e30`)**

Bench-confirmed: nudge commands reach the motor from the Calibrate screen.
Also confirmed on the same run — after homing, the disc moves **CCW** to
glass 1 as Corrected Fact #2 predicts, and misses alignment by the backlash
that direction change introduces. Measured lash: **2–3 clicks at 10
microsteps/click ≈ 20–30 microsteps**, somewhat below the ~60 in Corrected
Fact #3 but the same order and the same sign. Session 3 is unblocked.

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

### Session 2 — Nudge determinism — **DONE (v1.4.1)**

**Priority:** P0. Second half of making the instrument trustworthy.
**Model:** Sonnet 5 — mechanical changes, tightly specified.
**Target version:** 1.4.1

**Deviations from the spec as written, and why:**

- **`NUDGE_STEPS` was NOT reduced to 4 globally.** It was split in two.
  `NUDGE_STEPS` stays at **10** for the pour-time nudge (`game.cpp`,
  `h2h.cpp`), and a new `CAL_NUDGE_STEPS = 4` serves the calibrate and
  glass-diag screens. Rationale: bench testing after Session 1 measured
  roughly 2–3 clicks of backlash absorption after a direction change at 10
  microsteps/click, i.e. **20–30 microsteps of lash**. At a uniform 4
  steps/click that same reversal costs 5–8 dead clicks. That is an acceptable
  price on a measurement screen, where resolution is the whole point, but not
  during a live pour where the operator wants a glass moved *now*. The two
  call sites have genuinely different requirements.
- **`NUDGE_SPEED = 300` looks like it violates the `MOTOR_MIN_SPEED >= 400`
  rule in `CLAUDE.md`. It does not.** That rule governs *ramped* moves, where
  the disc dwells at the start speed long enough to excite low-speed
  resonance. A nudge is an unramped burst of 4–10 steps lasting 13–33 ms —
  shorter than the several oscillation cycles resonance needs to build. The
  constant carries a comment saying so, so a future session doesn't "fix" it.
- **`ensureMotorOn()` in `screen_hw_diag.cpp` lost its local `delay(5)`** —
  `motorEnable()` owns the settle now. The boolean flag remains because it
  still drives the page indicator and `hwCleanup()`.
- **Clamp behaviour changed on both nudge screens.** Previously the counter
  clamped at ±200 / ±400 while the disc kept moving, so display and disc
  silently diverged at the limit. Now the click is rejected outright and
  neither moves.
- **`HW_DIAG_SPEED` (800 sps) in `screen_hw_diag.cpp` was left alone.** The
  1- and 10-step jog sizes have the same dead-stop pull-in problem, but that
  screen is a measurement instrument for characterising the driver — changing
  its speed changes what it measures. Revisit only if jog readings look off.

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
**Target version:** 1.6.0
**Prerequisite:** Sessions 5 and 9 released, and a **baseline auto-diag capture
in hand**. Do not start without it — the point of this session is a measurable
before/after, and there is no second chance to capture "before."
**Downstream:** item 5 (homing hardening) is a hard prerequisite for Session 10.
Teach-in calibration is only as repeatable as the zero it is measured against.

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
- [ ] Telemetry `magnet width` is consistent across all 8 runs (this is the
      item-5 fix working — inconsistent widths mean the guard band is still too
      small). Pull via `/log`.
- [ ] **Re-run the Session 9 auto-diag and compare against the archived
      baseline.** This is the acceptance test for the session.
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
**Target version:** 1.6.1

**This session is also an alignment fix — that was not understood when the
roadmap was written.** Per Corrected Fact #9, both deadband mechanisms (momentum
overrun and elastic relaxation) scale with how abruptly a move ends. A gentler
arrival leaves less momentum to carry the disc past the shaft flank and less
stored wind-up to release. Change 4b (`MOTOR_STOP_SPEED`) and the sqrt
deceleration ramp should therefore show up as *reduced deadband*, measurable by
re-running the Session 9 auto-diag. Add that to the acceptance criteria below.

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

### Session 5 — Telemetry core (**RUN FIRST**)

**Priority:** P0. Nothing downstream is measurable without it.
**Model:** Sonnet 5 — self-contained, no motion-geometry reasoning.
**Target version:** 1.5.0

Measurement only — no corrective action. Goal is hard data on whether lash,
elastic wind-up, or step loss dominates on any given mechanical build.

**Why this is not a serial feature.** Every value of interest already lives in
RAM on a device that runs a `WebServer` ([wifi_portal.cpp:44](../../Blind-Flight/src/wifi_portal.cpp:44))
with four routes already registered. Serial was the convenient transport when
this roadmap was written; it is not a requirement. The workflow is *ship OTA →
Jeremy runs a procedure → numbers come back → analyse* — a pull-based endpoint
fits that better than a console, because the capture happens after the run
instead of during it.

What serial genuinely provides that Wi-Fi cannot: panic backtraces, boot output
before the network is up, and output during a hang. None of that is alignment
work. It matters for Session 8 and for crash debugging, and the v1.4.2 rollback
already covers the failure mode it would guard. **A USB-accessible prototype is
therefore not on the critical path for this roadmap.** If a bench mule is built
for mechanical iteration, leaving its USB reachable costs nothing — but gate
nothing on it.

**Changes:**

1. **New `telemetry.cpp` / `telemetry.h`, compiled into BOTH environments.**
   Fixed-size RAM ring buffer (~16 KB, no NVS — write endurance is not worth
   spending, and the log is grabbed before power-down). `telemetryLog()` writes
   to Serial *and* the ring, so nothing available today is lost.

2. **Structured records**, one line each, CSV-shaped so they are machine-
   parseable when pasted into a session:

   ```
   H,<run>,<ms>,<mV>,<magnetWidth>,<retries>,<failPhase>          // homing
   X,<run>,<ms>,<mV>,<glass>,<crossIdx>,<expected>,<actual>,<drift>  // Hall crossing
   M,<run>,<ms>,<mV>,<from>,<to>,<dir>,<steps>                    // move (dir exposes reversals)
   ```

   `<mV>` is the battery reading — see Corrected Fact #11. `<run>` is a
   boot-session ID so captures correlate across power cycles.

3. **Log every Hall crossing during every spin.** `moveStepsVerified` currently
   latches only the **first** crossing. Record every crossing: expected step
   count vs. actual. With 1–3 extra revolutions this yields 2–4 data points per
   spin and reveals whether error accumulates during the spin or arrives at once.

4. **Log every homing run:** magnet width, retry count, which phase failed.

5. **Reset `lastDrift = 0`** at the top of every `moveStepsVerified` call, and
   return a `triggered` flag. (Fixes the stale-drift bug in Session 6's
   dependency chain — see below.)

6. **`GET /log` route** in `wifi_portal.cpp`, served as `text/plain`. Jeremy
   opens `http://flight.local/log` (or the AP IP) on a phone, selects all,
   pastes. Prepend a self-describing header: firmware version, device ID
   (`device_id.cpp`), uptime, battery mV, boot run ID. This is also the
   mechanism for remotely triaging a beta user's unit — a pasted log should
   stand alone with no other context.

   Optionally `GET /log.csv` serving only the structured records.

**Design constraints:**

- **Pull, not push.** Blocking motor loops freeze HTTP, so a live stream would
  stutter exactly when the interesting data is being produced. Reading a buffer
  after the fact sidesteps the problem entirely — and this is why Session 8 is
  *not* a prerequisite.
- **Never log from inside the step loop and never from an ISR.** Buffer values
  in a small array during a move; flush after it completes.
- Ring writes happen only from `loop()` context; the HTTP handler reads. Guard
  the index update if that assumption is ever broken.

**Deliberately NOT in this session:** the corrective move, and any UI change.

**Test checklist:**
- [ ] Both build environments compile
- [ ] `/log` reachable in AP mode and in STA mode
- [ ] Header identifies version, device ID, uptime, battery
- [ ] Log from a full flight is complete and parses cleanly
- [ ] Magnet width logged on every home
- [ ] Multiple crossings logged per spin
- [ ] Battery mV present on every record
- [ ] Ring wraps cleanly under a long session — oldest lines drop, no corruption
- [ ] No timing regression: spin duration unchanged vs. v1.4.3

---

### Session 6 — Closed-loop position correction

**Priority:** P1, but **gated on the set screw / hub fix**.
**Model:** **Opus 5.** Control-loop design with sign conventions, wraparound
arithmetic, and hardware-timing interaction.
**Target version:** 1.7.1

**Do not start this session until:**
- Sessions 5, 9, 3, 4 and 10 are released and validated
- The hub/set-screw manufacturing issue is resolved (thresholds cannot be tuned
  against a moving mechanical baseline)
- Session 5 telemetry from a mechanically stable build is available
- **The Session 9 scatter number says a control loop can help.** If run-to-run
  scatter is already at the acceptance threshold, closed-loop correction adds
  risk and complexity for nothing — the residual is noise, not bias, and a loop
  cannot remove noise. Check before building.

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

**7b — `otaMarkValid()` defeats rollback — DONE, pulled forward (v1.4.2).**
Escalated to P0 on 2026-07-25 when USB access was lost, and shipped ahead of
Session 3.

*Verified first:* the stock Arduino-ESP32 bootloader has
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`, `ESP_SYSTEM_PANIC_PRINT_REBOOT=y`,
`ESP_TASK_WDT_PANIC=y` (5 s) and `BOOTLOADER_WDT_ENABLE=y` (9 s). Rollback is
genuinely armed, so the old call site was discarding working protection rather
than being cosmetic.

*Shipped:* confirmation moved out of `setup()` into `otaValidateTick()`, called
from `loop()`, gated on `OTA_VALIDATE_UPTIME_MS` (30 s) of healthy uptime.
Post-update screen tells the user to stay powered; About screen shows the version
in orange with a countdown while pending.

*Deliberately NOT gated on homing success*, contrary to the sketch above. Homing
fails for **mechanical** reasons — detached magnet, jammed disc, wedged glass. Tying
firmware validity to it would turn a mechanical fault into a firmware rollback, and
since the previous build would fail to home too, the result is a silent downgrade
that fixes nothing and hides the real cause. Uptime is the honest health signal.

*Known limit:* catches crashes, not hangs — `loopTask` is on core 1 and the task WDT
does not watch core 1's idle task.

Was: called as the first line of
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

**7k — mid-flight idle timeout silently invalidates position.** *(new,
2026-07-26. Arguably P0 for event correctness — consider pulling forward into
Session 9, which already touches homing-on-entry.)*

`motorDisable()` at [ui.cpp:226](../../Blind-Flight/src/ui.cpp:226) fires on
display idle-off **regardless of whether a flight is in progress**, freeing the
disc to be rotated by hand. On wake, [ui.cpp:242](../../Blind-Flight/src/ui.cpp:242)
correctly skips the re-home while a game is active (that is the 7j spill fix),
and its comment asserts "the next `runPourCycle()` re-homes on its own via
`homedThisFlight`."

**That assertion is false.** [game.cpp:1113](../../Blind-Flight/src/game.cpp:1113)
homes only `if (!homedThisFlight)`, and the flag is set true at
[game.cpp:1115](../../Blind-Flight/src/game.cpp:1115) during pour 1. After that,
no pour in the flight re-homes. [h2h.cpp:292](../../Blind-Flight/src/h2h.cpp:292)
has the same shape. So a flight that idles out between pours resumes on an
unverified position, and pours 3 and 4 can land anywhere.

This directly violates the design intent stated in Corrected Fact #10 — hold
through the sequence, re-home on any recovery, maintain a verified position
throughout.

**Fix:** when the driver is disabled while a flight is active, clear
`homedThisFlight` (or add an explicit `positionVerified` flag) so the next
`runPourCycle()` re-homes. One extra homing per idle event, no held current, no
change to the thermal or battery profile. Fix both `game.cpp` and `h2h.cpp`.
Also correct the misleading comment at `ui.cpp:244`.

**7j — `motorHome()` called bare on idle wake — DONE.**
Verified 2026-07-26: [ui.cpp:236](../../Blind-Flight/src/ui.cpp:236) now guards
the idle-wake re-home behind `!gameIsActive() && !h2hIsActive()` and calls
`runHomingSequence()`. The spill risk described here is closed. Left in place as
a record of the finding.

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

**Priority raised for the headless variant.** The Session 9 auto-diag runs
several revolutions of blocking motor code with `wifiPortalUpdate()` unserviced.
On the screen build that is tolerable — the TFT shows progress. On
`esp32-headless` the phone *is* the entire UI, so a self-test looks
indistinguishable from a hung device. Not a blocker for Session 9, but it moves
Session 8 ahead of Session 6 on the headless track.

---

### Session 9 — Diagnostics consolidation + auto self-test *(new, 2026-07-26)*

**Priority:** P0. Produces the baseline every later session is measured against.
**Model:** Sonnet 5 — UI restructuring plus a well-specified measurement routine.
No motion-geometry reasoning; the auto-diag uses existing move primitives.
**Target version:** 1.5.1
**Prerequisite:** Session 5 released (this session's output goes to telemetry).

#### 9a — Consolidate the diagnostic surface

The settings menu has accumulated five overlapping entries: `Re-Home`,
`Motor Test`, `Glass Diag`, `HW Diag`, plus the hidden `Diagnostics` screen
reached by long-pressing the encoder on `About`
([screen_settings.cpp:548](../../Blind-Flight/src/screen_settings.cpp:548)).
The container already exists — this is mostly relocating menu entries.

1. **Move `Motor Test`, `Glass Diag`, and `HW Diag` under `Diagnostics`.**
   Remove them from the top-level settings menu.
2. **Every diagnostic tool homes on entry.** This is the real bug hiding inside
   the refactor: none of these tools currently home first, so they inherit
   whatever `currentMotorPos` claims. After a hand-rotated disc — see Corrected
   Fact #10 — that value is fiction, and the tool silently measures nonsense.
   Use `runHomingSequence()`, not bare `motorHome()`.
3. **`Re-Home` moves under `Diagnostics` with the rest.** No operator-facing
   re-home control, and **no encoder long-press binding.**

   *Rationale (revised 2026-07-26 — see Corrected Fact #10b).* An earlier draft
   of this document argued for keeping Re-Home reachable mid-session because the
   disc is free to rotate during TASTING. That risk does not exist: the disc
   positions are physically numbered and the tasting runs off the glasses, which
   are usually removed from the device entirely. Nothing after the final pour
   depends on the firmware's position belief. Power-cycle and session-recovery
   both home already, and item 2 above makes every diagnostic tool home on entry.
   Re-Home is an engineering tool, so it lives with the engineering tools.

   *Do not bind `INPUT_ENC_LONG`.* It is already consumed at
   `screen_settings.cpp:548` (About → Diagnostics) and
   `screen_diagnostics.cpp:387`, and there is no longer a reason to add a third
   meaning.

   *The mid-flight case is real but belongs to 7k, not here.* Losing verified
   position between pours is handled by re-homing on the next pour cycle, not by
   an operator control — see Corrected Fact #10 and item 7k.

#### 9b — `selftest.cpp` — automatic characterisation

**Must be compiled into both environments** (Corrected Fact #8 corollary). The
screen is a thin front-end; the phone/WebSocket path triggers the same routine.

**The measurement.** For each glass position: move to it, stop, then continue
**CW until the Hall triggers**, counting actual steps against predicted.

This is the strongest measurement available on this hardware, for one reason:
**the magnet is in the disc, not on the shaft** (Corrected Fact #6). The Hall
reports true disc angle with all lash and wind-up included. Every other signal
reports where the firmware *believes* the shaft is. It also removes the human
from the loop — unlike Glass Diag, nobody has to judge alignment by eye.

Requirements:

1. **Measure both magnet edges and take the centre**, as `motorHome()` already
   does. Single-edge readings are contaminated by Hall hysteresis and magnet
   asymmetry. Log the width — width variation is itself the Session 3 item-5
   diagnostic.
2. **Multiple passes (default 5), and report scatter, not just mean.** This is
   the point of the whole session. Mean error is bias and calibration removes
   it; **scatter cannot be removed by anything** and sets the floor on
   achievable accuracy. It is the acceptance criterion for the carrier plate and
   the input to the epoxy decision.
3. **Visit positions in both sequential and randomised order, and report
   per-position AND per-visit-index separately.** See the diagnostic question in
   Acceptance Criteria — the existing "glass 4 is worst" evidence came from tools
   that traverse in order, so sequential-only measurement reproduces the confound
   and settles nothing.
4. **Hold the driver enabled for the whole of a pass**, matching pour-sequence
   behaviour (Corrected Fact #10 — the driver already stays energized from the
   first move to the end of a sequence, by design). Do not disable between
   positions; that would measure something the device never does.
5. **Step budget and graceful failure.** If the Hall does not trigger within
   1.5 revolutions, abort that pass, log it, and continue — never hang.
6. **Refuse to start with glasses loaded**, or at minimum require an explicit
   "glasses are removed" confirmation. Several revolutions of characterisation
   is not something to discover mid-pour.
7. **All output goes to telemetry**, retrievable via `/log`. Nothing that
   matters should exist only on the 240×280 screen.

#### 9c — Phone behaviour during the self-test

**The phone will not disconnect — it will stop responding.** Verified
2026-07-26: `wifi_portal.cpp` never calls `enableHeartbeat()`, so the
links2004 WebSocket server runs no ping/pong timeout. A blocked `loop()` leaves
the TCP connection open and the kernel buffers the traffic; there is no
server-side liveness check to fail. (`dnsServer.processNextRequest()` also
stalls, but an already-connected client does not need DNS.)

So the mitigation Jeremy proposed is sufficient, and is the requirement:

1. **Before starting, push a state to the phone** that renders a full-screen
   notice — *"Diagnostic test in progress. The device will not respond until the
   sequence completes."* — and **suspends all user inputs** in the phone UI.
   Push and flush this *before* the first move, or it will not arrive until
   after the test.
2. **Clear it when the run finishes or aborts**, including on the failure paths.

**Also chunk the sequence.** Call `wifiPortalUpdate()` between passes and between
position moves — never inside a step loop, which would drop steps. Individual
moves are 1–2 s, which is comfortably survivable; the full 5-pass sequence is
tens of seconds and would otherwise be one long silence. Chunking is nearly free
and lets the notice show live progress ("pass 2 of 5"), which is a much better
experience than a frozen panel. This does **not** require Session 8.

*This measurement is destructive* — reading glass N requires moving past it, so
the routine characterises but cannot correct in place. That is fine for
calibration and is why Session 6 is a separate exercise.

**Test checklist:**
- [ ] Both build environments compile
- [ ] All four tools (incl. Re-Home) reachable under Diagnostics; all removed
      from the top-level settings menu
- [ ] Each tool homes on entry — verify by hand-rotating the disc first
- [ ] Long-press still opens Diagnostics from About (no new long-press bindings)
- [ ] Auto-diag completes 5 passes and reports mean + spread per position
- [ ] Both sequential and randomised visit orders present in the output
- [ ] Phone shows the "test in progress" notice and rejects input for the
      duration; notice clears on completion **and** on abort
- [ ] Phone is still connected and responsive when the run finishes
- [ ] Progress updates during the run (chunking is working)
- [ ] Auto-diag runs from the phone on `esp32-headless`
- [ ] Aborts cleanly with the Hall sensor disconnected
- [ ] **Capture and archive the baseline log — this is the deliverable**

---

### Session 10 — Teach-in per-glass calibration *(new, 2026-07-26)*

**Priority:** P1. The per-unit answer to a per-unit problem.
**Model:** **Opus 5.** Offset arithmetic, wraparound, direction seating, and the
interaction with homing and pour side — the exact class of reasoning that
produced the original alignment bug.
**Target version:** 1.7.0
**Prerequisite:** Session 3 item 5 (homing hardening) released and validated.
Teach-in is only as repeatable as the zero it measures against; calibrating
against a jittering magnet-width origin stores noise.

**What exists today.** Glass N's target is computed as

```
target = (glass-1)*400 + POUR_OFFSET(1000) + pourSide*400 + homeOffset
```

([motor.cpp:367](../../Blind-Flight/src/motor.cpp:367)) — one compile-time
constant, a coarse 90° selector, and a **single global trim**.

**What replaces it.** Four *measured* values, one per glass: the CW step count
from that glass's true aligned position to the Hall centre. Operationally, from
home, glass N is reached by `MICROSTEPS_PER_REV - S[N]` clockwise steps.

Three things this fixes that no global offset can:

1. **Per-unit assembly variation** — magnet phase, disc mount clocking, spout
   position — absorbed automatically, with nobody measuring anything. This is
   the answer to "it will vary from device to device based on its individual
   assembly."
2. **Non-uniform glass spacing.** The current code hard-assumes glasses sit
   exactly `MICROSTEPS_PER_GLASS` (400) apart. If the holders are not at exactly
   90°, no single trim can ever correct it. Four independent values can.
3. **Elastic deadband**, which per Corrected Fact #9 survives any set screw and
   differs per unit.

**Procedure.**

1. Home. Move to glass N using the current best estimate.
2. User nudges **freely in both directions** to align the glass under the spout.
3. On Confirm, the firmware performs a **lash-takeup maneuver**: back off CCW by
   `TAKEUP_STEPS`, then return CW by `TAKEUP_STEPS`. If the play is `D`, the disc
   travels (`TAKEUP_STEPS` − `D`) in each direction — **net zero, so the user's
   alignment is preserved** — and the play ends seated on the CW flank.

   **The return leg must reproduce the operational arrival profile**, not merely
   the direction. Per Corrected Fact #9 the residual that matters is `x_rest`,
   the post-stop coast, and `x_rest` depends on how abruptly the move ended. A
   short unramped burst at `NUDGE_SPEED` leaves a different end state than a
   ramped approach decelerating to `MOTOR_STOP_SPEED`, and the difference lands
   straight in `S[N]`.

   Size `TAKEUP_STEPS` so the return leg reaches cruise and decelerates
   identically to a real move — roughly `2 × accelSteps`, which at
   `MOTOR_ACCEL 1600` and the Normal preset is ~470, so **600 microsteps (135°)
   is a safe choice.** Use the same ramped move primitive the pour sequence uses.

4. Traverse **CW to the Hall**, measuring both edges, and store the centre count
   as `S[N]`.
5. Repeat for all four glasses.

**The governing principle: calibrate the way you operate.** Operational moves
arrive CW after a home, ramped, decelerating to `MOTOR_STOP_SPEED`. If
calibration ends in that same mechanical state, the deadband is common-mode
between the two and cancels. Step 3 is what makes the scheme work despite the
lash — it is not optional.

**A refinement, per Jeremy 2026-07-26.** The play must be crossed completely
before the disc moves at all, so a user nudge that *visibly moved the glass* has
already seated the play on that side — the state is known, and the residual
versus an operational approach is only `x_rest`, not the whole `D`. That is a
smaller error than this spec originally assumed. Two reasons the maneuver is
still required:

- A click that does **not** visibly move the disc leaves the shaft mid-gap and
  `x` indeterminate. With `CAL_NUDGE_STEPS` at 4 µsteps and `D` in the tens, this
  is the *common* case on the final click, not an edge case.
- `x_rest` is profile-dependent, so even a correctly-seated CCW nudge does not
  reproduce an operational arrival.

**Rejected alternative, recorded so it is not re-proposed.** Making the nudge
itself CW-only — a CCW request served as a ~340° CW rotation — is correct in
principle and unusable in practice: a user who overshoots by three clicks waits
through three near-full revolutions. The takeup maneuver buys the same guarantee
in ~0.5 s.

**Storage.** `int16_t S[4]` in NVS plus a `calibrated` flag and a schema version.
**When uncalibrated, fall back to the existing arithmetic unchanged** so a fresh
or factory-reset unit still works.

**Effect on existing settings.** For a calibrated unit, `homeOffset` and
`pourSide` become redundant — the user aligns to the physical spout wherever it
is. Keep both as coarse pre-calibration defaults; do not delete them.

**Test checklist:**
- [ ] Both build environments compile
- [ ] Uncalibrated unit behaves exactly as v1.6.1 (fallback path verified first)
- [ ] Alignment set by the user survives the takeup maneuver — glass does not
      visibly shift on Confirm
- [ ] Calibration survives power cycle
- [ ] Deliberate CCW-final-nudge and CW-final-nudge produce the same `S[N]`
      within scatter — this is the proof step 3 works
- [ ] Session 9 auto-diag after calibration: **inter-glass spread within the
      acceptance criteria**
- [ ] Blind identification test: run a full flight without watching; positions
      not identifiable
- [ ] Factory reset clears calibration and restores fallback

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

**Use Opus 5 for:** Sessions 3, 4a/4b, 6, 8, **10**. Anything touching motion
geometry, direction selection, offset arithmetic, control loops, or concurrency.

**Sonnet 5 is appropriate for:** Sessions 1, 2, 5, 7, **9**, and 4c/4d. These are
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
   the Delta column already shows. *Partly superseded:* Session 9's auto-diag
   becomes the primary instrument, and it reports absolute per-position error
   against the Hall. Glass Diag survives as a manual spot-check.

4. ~~**Session 7j urgency.**~~ **RESOLVED** — already fixed in `ui.cpp`.

5. **Set screw timeline.** Session 6 is gated on it. If the 3D-print issue
   persists, consider an interim mechanical fix (thread-locker, a flatted shaft
   with adhesive, or a clamping hub) so the closed-loop work isn't blocked
   indefinitely. **Do not reach for epoxy until Session 9 reports a scatter
   number** — it permanently couples disc, motor, and skid plate into a single
   non-serviceable assembly, so a fault in any one means replacing all three.

6. **Physical position 4, or last-visited?** Clarified to mean physical position
   4, but the evidence is confounded — see Acceptance Criteria. Session 9
   resolves it by measuring sequential and randomised orders separately. Still
   the highest-value open question.

7. ~~**Hold current during a flight.**~~ **RESOLVED, 2026-07-26.** Policy
   confirmed and already implemented correctly: hold continuously through a pour
   sequence, power down after completion and on idle timeout. The motor runs too
   hot to hold indefinitely and it costs battery; verified position is restored
   by homing on recovery. No change wanted. The one defect found while checking
   this is filed as **7k**.

8. ~~**Does any of this need serial / a USB-accessible prototype?**~~
   **RESOLVED, 2026-07-26 — no.** Every value of interest is already in RAM on a
   device running an HTTP server; `/log` (Session 5) is a better transport for
   bulk numeric capture than a console. Serial retains a narrow advantage for
   panic backtraces, pre-network boot output, and hangs — relevant to Session 8
   and crash debugging, not to alignment. A USB-accessible bench mule is
   worthwhile if one is built anyway for mechanical iteration, but **nothing in
   this roadmap is gated on it.**

9. **Preset labels vs. the deadband finding.** Session 4 lowers all three speed
   presets. If the Gentle preset also measurably improves alignment (Corrected
   Fact #9), consider whether the *pour* approach move should always use Gentle
   regardless of the user's spin-speed preference — the theatrical spin and the
   final approach have different requirements.

---

## Related Documents

- `docs/tmc2209_current_setup.md` — bench procedure for driver current and mode
- `CLAUDE.md` — authoritative hardware facts (supersedes the original hardware spec)
- `docs/specs/phone_only_architecture.md` — headless build architecture
