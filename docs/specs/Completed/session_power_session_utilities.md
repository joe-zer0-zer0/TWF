# Session Spec — Power & Session Utilities

## Overview

Four production-hardening features batched into one session (bug-batch style — they're small individually and touch overlapping files):

1. **Session resume** — survive a power loss mid-flight without losing the glass mapping.
2. **Battery indicator + low-battery lockout** — battery level on the home screen; refuse to start a spin below a safe threshold.
3. **Variable glass count** — 2–4 glass flights, selected via encoder at the start of each mode.
4. **Display timeout / power saving** — tiered backlight dim → off, with optional deep sleep from idle.

## Feature 1 — Session Resume

### Rationale
A power bump mid-flight currently destroys the glass mapping — with real whiskey in unmarked glasses, the flight is unrecoverable. This is arguably a beta-blocker for a commercial product.

### Design
- Persist a compact session snapshot to **NVS** at each milestone: after mode/count selection, after each bottle assignment, after each confirmed pour, at guess completion. Clear it on flight completion or explicit discard.
- Snapshot contents: magic/version byte, mode, glass count, `glassName[4]` (or library entry indices + manual-name strings), glass↔pour mapping, `pourCount`, coarse state (SETUP / POURING / TASTING / GUESSING).
- **Boot flow:** after splash + homing, if a valid snapshot exists → "Resume flight?" screen (RESUME / DISCARD). Resume restores the session and lands on the appropriate state; homing already re-establishes absolute carousel position, so positions remain valid.
- **NVS wear:** ≤ ~10 writes per flight to one namespace key — negligible against 100k-cycle endurance even at heavy hobbyist use.

### Decisions to confirm
- Resume granularity: proposal is *pour-level* (mid-pour interruption resumes at "re-pour glass N" prompt for the interrupted pour, since we can't know if liquid landed). Confirm.
- Multiplayer (H2H) sessions: **not resumable** v1 — phones have disconnected anyway. Snapshot skipped when multiplayer flag set.

## Feature 2 — Battery Indicator + Low-Battery Lockout

### Design
- Existing hardware: voltage divider on **GPIO 36 (ADC1 — Wi-Fi safe)**. 2S Li-ion window ≈ 6.0–8.4 V.
- Read with 16-sample averaging + light exponential smoothing; convert via small voltage→percent lookup table (Li-ion curve is nonlinear — a linear map reads misleadingly high mid-range).
- **Home screen indicator:** battery glyph + percent in the title bar corner. Refresh every ~10 s while on home screen only (partial redraw of the glyph region, not a full redraw).
- **Warning tier** (~25%): amber glyph; toast/hint line "Battery low" on home screen.
- **Lockout tier** (~15%, and/or hard voltage floor ~6.6 V): starting a new spin is refused with a "Battery too low — charge before pouring" screen. In-progress pours are allowed to finish; session resume (Feature 1) covers a mid-flight death anyway.
- Calibrate the divider ratio against a multimeter reading and store the correction factor as a `config.h` constant (or NVS calibration value — decide; `config.h` is simpler for v1).

### Decisions to confirm
- Thresholds above are placeholders — validate against real sag under motor load (measure voltage during a spin; lockout should key off *loaded* voltage or add margin for sag).

## Feature 3 — Variable Glass Count (2–4)

### Answer to the design question
Yes — count selection at the start of each mode is viable and clean. Implement it as a **state inside game.cpp** (`GAME_COUNT_SELECT`) entered on game start, rather than a separate pushed screen — this sidesteps the nested push/pop hazards the deferred-browse pattern exists to avoid.

### Design
- Screen: "How many glasses?" with a large number (2/3/4) driven by encoder, glass icons filling in to match, click to confirm. Default = last used (persisted in settings NVS).
- Session gains `glassCount`; all `NUM_GLASSES` loops in game logic become `session.glassCount` (compile-time `NUM_GLASSES` stays as the physical max / array size).
- **Physical convention:** glasses load into positions **G1..GN** (document in quick-start: "for a 2-glass flight, use positions 1 and 2"). Randomization draws only from the first N positions.
- Mode interactions: Basic/Named/Best Guess/Ranked support 2–4. Duplicate/Decoy stay locked to 4 (per their spec) until validated.
- Reveal/tasting glass-indicator rows already iterate the glass array — parameterize count and center the row for N < 4.

## Feature 4 — Display Timeout / Power Saving

### Is it worth it? Yes — tiered, and conservative about sleep:
- **Tier 1 — Dim** (default 2 min idle): backlight PWM to ~25% (LEDC channel already drives the backlight for fade transitions). Any input restores full brightness *and consumes that input event* (a wake-press shouldn't also click a menu item).
- **Tier 2 — Display off** (default 10 min idle): backlight 0%. Wake on any input, event consumed.
- **Tier 3 — Deep sleep** (optional, default OFF; only ever from Home screen with no active session): shuts down Wi-Fi/CPU for multi-day shelf life. Constraints to respect:
  - Wake source must be an **RTC-capable GPIO** (0, 2, 4, 12–15, 25–27, 32–39). Verify the encoder button's pin; if it isn't RTC-capable, deep sleep needs a wiring change — in that case ship dim/off only and note the pin move for the PCB revision.
  - Wake = full reboot (splash + homing). Wi-Fi AP drops. Session resume makes this safe, but the reboot UX is why Tier 3 stays off by default.
- Idle = no encoder/button input AND no active WebSocket traffic AND motor idle. Timer values in settings (NVS): dim delay, off delay, deep sleep on/off.
- Interaction with existing brightness setting: dim tier scales *relative to* the user's chosen brightness, and restore returns to it (don't repeat the brightness-override bug class from the batch-fix session).

## Changes by File

| File | Change |
|------|--------|
| `game.h/.cpp` | `GAME_COUNT_SELECT` state + screen; `session.glassCount`; loops parameterized; snapshot save/clear calls at milestones |
| new `persist.h/.cpp` (or extend `settings.cpp`) | Session snapshot serialize/restore to NVS (separate namespace from settings) — proposed: **new module**, keeps settings clean |
| `screens.cpp` | Resume prompt screen (post-splash); home screen battery glyph + warning line |
| new `battery.h/.cpp` | ADC sampling, smoothing, LUT percent conversion, tier query API |
| `motor.cpp` or `game.cpp` | Lockout check at spin start (proposed: in game, before `motorSpinToGlass`, so motor module stays policy-free) |
| `ui.cpp/.h` | Idle timer hooks (reset on input), dim/off control, wake-consumes-event handling |
| `main.cpp` | Battery init, idle timer service in loop, deep sleep entry (if enabled) |
| `settings.cpp/.h` | Last glass count, dim/off delays, deep-sleep toggle |
| `screen_settings.cpp` | New settings rows |
| `config.h` | Divider ratio + calibration constant, battery LUT, thresholds, timeout defaults |

## Testing Checklist

- [ ] Kill power mid-flight after pour 2 → reboot → resume prompt → mapping and pour count intact → flight completes with correct reveal
- [ ] DISCARD clears NVS snapshot; next boot shows no prompt
- [ ] Completing a flight normally clears the snapshot
- [ ] Battery percent tracks multimeter within ~5% across 8.4 → 6.6 V; no visible ADC jitter on the home glyph
- [ ] Warning tier renders; lockout tier blocks spin start with message; existing flight can still finish
- [ ] Battery reads clean while Wi-Fi AP active with a client connected (ADC1 sanity check)
- [ ] 2-glass and 3-glass flights: randomization never targets unused positions; reveal rows centered for N < 4
- [ ] Glass count default persists across power cycles
- [ ] Dim at T1, off at T2; wake press restores brightness and does NOT trigger a UI action
- [ ] Idle timer suppressed during motor motion and active WS session
- [ ] Brightness setting respected after wake (no override regression)
- [ ] (If enabled) deep sleep from Home only; wake pin verified RTC-capable; wake → splash → home

## Deferred

- Battery calibration UI (multimeter-assisted, NVS-stored) — pairs with home offset calibration on the existing punch list
- Charge-state detection (requires hardware change)
- H2H session resume
