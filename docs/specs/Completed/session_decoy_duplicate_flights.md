# Session Spec — Duplicate & Decoy Flight Modes

## Overview

Two closely related game modes that reuse the existing pour/guess machinery. They ship together because they share setup and reveal patterns:

1. **GAME_MODE_DUPLICATE ("Twin Pour")** — user selects **3 bottles**; the device secretly assigns one of them to **two glasses**. After tasting, the player identifies the matching pair.
2. **GAME_MODE_DECOY ("Find the Ringer")** — user selects **2 bottles**: a main bottle (poured into 3 glasses) and a ringer (1 glass). After tasting, the player identifies the ringer glass.

Both are pure firmware — zero new hardware — and both exploit the device's core trick: the user pours from bottles they can see into glasses whose identity they can't.

## Core Mechanic (shared)

The pour prompt shows the **bottle name to pour**, not the glass number. The carousel position hides which glass is receiving it. Internally the session tracks the true bottle→glass mapping.

- Duplicate: pour prompts appear 4 times; the duplicated bottle is prompted twice (in randomized pour order, so its two pours are not necessarily consecutive).
- Decoy: main bottle prompted 3 times, ringer once, order randomized.

## Design Decisions (confirm before implementation)

| # | Decision | Proposed default |
|---|----------|------------------|
| 1 | Bottle selection method | Reuse browse flow (library) with manual/phone entry as fallback — same as Named Flight. Duplicate needs 3 selections, Decoy needs 2 (main first, then ringer). |
| 2 | Menu structure | Two separate menu entries vs. one "Challenge Flights" submenu. Proposed: **two top-level entries** for now; menu list already scrolls. |
| 3 | Guess UI | Reuse Best Guess round patterns. Duplicate: player selects two glass numbers (encoder cycles 1–4, click selects, second click on a different glass locks the pair). Decoy: single glass selection. |
| 4 | Scoring/feedback | Simple correct/incorrect result screen with celebratory vs. consolation tone, then full reveal (all glass→bottle mappings). |
| 5 | Duplicate randomization | Randomly choose which of the 3 bottles duplicates AND random glass assignment. Both hidden from user. |
| 6 | Glass count interaction | These modes assume 4 glasses. If the variable glass-count feature (separate session) lands first, **lock these modes to 4** initially; note as deferred. |
| 7 | Pour prompt naming | Show bottle name large + "Pour X of 4" counter. Do NOT show glass numbers anywhere during setup or pours. |

## State Machine

Both modes reuse the existing flow with a mode-specific setup and guess phase:

```
SELECT MODE → BOTTLE SELECT (browse ×3 or ×2)
  → per pour: SPINNING → POURING (bottle name shown) → repeat
  → FINAL SPIN → TASTING
  → GUESS (pair-select or single-select)
  → RESULT (correct/incorrect) → REVEAL (full mapping) → DONE
```

Reuse the deferred-browse pattern (`pendingBrowse` / `awaitingBrowseReturn`) for bottle selection — same non-transition push rules as Named Flight to avoid nested push/pop corruption.

## Changes by File

| File | Change |
|------|--------|
| `game.h` | `GAME_MODE_DUPLICATE`, `GAME_MODE_DECOY`; session fields: `bottleName[3]` (or entry ptrs), `bottleToGlass[]` mapping, `guessPair[2]` / `guessRinger`, `duplicateIdx` |
| `game.cpp` | Setup flow (N bottle selections), randomized pour-order builder, pour prompt by bottle name, guess screens (pair + single), result screen, reveal mapping |
| `screens.cpp` | Two new menu entries wired to `gameSetMode(...)` + non-T push (same as Named/Best Guess) |
| `browse.cpp/.h` | Support sequential multi-select ("Select bottle 2 of 3" title context) — likely just a title string parameter |
| `audio.h/.cpp` | Optional: distinct correct/incorrect jingles (reuse existing tones if preferred) |
| `config.h` | Any new tunables (result screen dwell time) |
| `wifi_portal.cpp` | **Deferred** — phone parity follow-up session |

## Testing Checklist

- [ ] Duplicate: select 3 bottles → 4 pour prompts, one bottle prompted exactly twice, order randomized across runs
- [ ] Duplicate guess: encoder pair selection works; cannot select same glass twice; correct pair detection verified against internal mapping
- [ ] Decoy: main bottle prompted 3×, ringer 1×; single-glass guess resolves correctly
- [ ] Result screen: correct and incorrect paths both reachable and render properly
- [ ] Reveal shows full true mapping for all 4 glasses in both modes
- [ ] Back/skip mid-setup doesn't corrupt screen stack (exercise browse-cancel at each selection step)
- [ ] Session restart ("New Flight") fully clears duplicate/decoy state
- [ ] Regression: Basic, Named, Best Guess, Ranked flows unchanged

## Deferred

- Phone WebSocket parity (guess via phone)
- Variable glass counts for these modes (e.g., 3-glass decoy)
- Head-to-head scoring integration (multiplayer session builds on the guess phases defined here)
