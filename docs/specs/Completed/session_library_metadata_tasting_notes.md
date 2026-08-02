# Session Spec — Library Metadata + Tasting Notes (Enriched Reveal)

## Overview

Two features that combine naturally because they both feed the reveal screen:

1. **Library metadata** — extend whiskey library entries with ABV/proof, price tier, and optional age statement.
2. **Tasting notes capture** — a per-glass rating round (encoder-driven, 1–5 stars) before the reveal.

Together they enable the "enriched reveal": *"Glass 3 — Eagle Rare 10yr, 90 proof, $$ — your rating: ★★★★★. You rated the cheapest bottle highest."*

This session is part data-entry, part firmware. Budget accordingly.

## Design Decisions (confirm before implementation)

| # | Decision | Proposed default |
|---|----------|------------------|
| 1 | Price representation | **Tier, not dollars** — `$` / `$$` / `$$$` / `$$$$` (e.g., <$30, $30–60, $60–120, $120+). Dollar amounts go stale; tiers don't. Stored as `uint8_t`. |
| 2 | ABV storage | `uint8_t` ABV × 10 impossible in one byte for >25.5%... use **proof as uint8_t** (0–255 covers 0–200 proof cleanly). Display as proof, derive ABV if needed. |
| 3 | Age statement | Optional `uint8_t` years, 0 = NAS (No Age Statement). |
| 4 | Metadata coverage | Metadata is **optional per entry**. Entries without it (0 values) simply omit those lines on reveal. Avoids blocking on full data entry. |
| 5 | Rating round placement | New game state `GAME_RATING` between `GAME_TASTING` and reveal. One screen per glass: "Glass 1 — rate it" with star row driven by encoder, click to confirm, auto-advance. |
| 6 | Rating applies to which modes | All modes. In Best Guess, rating happens **before** the guessing round (rate blind, then guess) so ratings aren't biased by guesses. Confirm ordering preference. |
| 7 | Skip rating | LEFT soft button = "SKIP" skips the entire rating round (not per-glass). Ratings default to 0 = unrated. |
| 8 | Manual-entry bottles | Phone/manual-entered names have no metadata; reveal shows name + rating only. |

## Data Structure Changes

### categories.h / categories.cpp

Current entries are name strings. Extend to a struct:

```c
struct LibraryEntry {
    const char* name;
    uint8_t proof;      // 0 = unknown
    uint8_t priceTier;  // 0 = unknown, 1–4 = $..$$$$
    uint8_t ageYears;   // 0 = NAS/unknown
};
```

- All tables remain `const` / flash-resident. Flash cost: +3 bytes per entry — negligible.
- Browse flow currently passes a `const char*` name into the game session. Change to pass an **entry index or pointer** so metadata travels with the selection. Session struct gains `const LibraryEntry* glassEntry[NUM_GLASSES]` (nullptr for manual/basic).

### game.h / game.cpp

- Add `uint8_t glassRating[NUM_GLASSES]` to session struct (0 = unrated).
- New state `GAME_RATING` + `drawRating()` + input handling (encoder CW/CCW adjusts stars, click confirms glass, advance until all rated).
- Reveal (`drawReveal`) gains metadata lines when `glassEntry[i]` is non-null, plus star row when rated.
- Done screen (`drawDone`): add one summary line if data allows, e.g., "Top rated: Glass 3" or the price-vs-rating callout when all glasses have tiers.

## Changes by File

| File | Change |
|------|--------|
| `categories.h` | `LibraryEntry` struct; table declarations updated |
| `categories.cpp` | Tables converted to struct initializers; metadata populated (data-entry pass) |
| `browse.h/.cpp` | Selection returns entry pointer/index instead of bare name |
| `game.h` | Session struct: `glassEntry[]`, `glassRating[]`; `GAME_RATING` state |
| `game.cpp` | Rating state machine, enriched `drawReveal`/`drawDone` |
| `config.h` | `RATING_MAX 5`, price tier boundary comments |
| `wifi_portal.cpp` | **Deferred** — phone parity (rating via phone, metadata on phone reveal) is a follow-up session per established pattern |

## Testing Checklist

- [ ] Named Flight: select 4 library bottles → complete flight → rating round appears → encoder adjusts stars, click advances
- [ ] Reveal shows name, proof, price tier, age, and stars for each glass
- [ ] Entry with 0-value metadata omits those lines cleanly (no "0 proof")
- [ ] SKIP during rating jumps straight to reveal; reveal shows no star rows
- [ ] Basic Flight: rating round works with numbered glasses; reveal shows numbers + stars only
- [ ] Best Guess: rating occurs before guess round; guesses unaffected
- [ ] Manual/phone-named glass mixed with library glasses renders correctly on reveal
- [ ] Regression: Ranked Flight revealMap ordering unaffected by new session fields

## Deferred

- Phone WebSocket parity for rating + enriched reveal
- Persisting ratings to flight history (pairs with history/NVS work)
- Barcode-based entry (see separate architecture discussion — secure-context and connectivity constraints)
