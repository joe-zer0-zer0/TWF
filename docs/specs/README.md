# Specs

Working specs live in this folder. Once the work they describe is **released and
verified on hardware**, move the spec into `Completed/` — the folder is the status,
there is no status header convention inside the files.

Audited 2026-08-02 against the source tree at v1.5.4.

## Open

| Spec | Status |
|---|---|
| `alignment_recovery_roadmap.md` | **In progress.** Sessions 1, 2, 5, 7, 9 released. Baseline archived (`docs/baselines/`). Session 3 is next and unblocked — but see the timing note in `CLAUDE.md`, its acceptance test is scored against a baseline captured on the current shaft coupling. |
| `share_card_spec.md` | **Not started.** Phases 1–3 scoped, Phase 4 (cloud link) deferred. Phone UI + `wifi_portal.cpp` only. |
| `phone_only_architecture.md` | **Partially done.** The two-environment build split (`esp32` / `esp32-headless`) is live and both must compile before any tag. The headless WS2812B status LED is the remaining piece — no driver exists in `src/` yet. |

## Completed

| Spec | Shipped as |
|---|---|
| `ranked_flight_spec.md` | `GAME_MODE_RANK`, plus `GAME_MODE_GUESS_RANK`. Phone/WebSocket parity is done (`wifi_portal.cpp` mode code `R`). |
| `session_plan_pour_position.md` | Pour-side setting, NVS-persistent, applied in `motorGetPourOffset()`. |
| `session_power_session_utilities.md` | Session resume, battery indicator + low-battery lockout, variable glass count, display timeout. |
| `favorites_list_spec.md` | `favorites.cpp`, `screen_favorites.cpp`, browse integration (`TYPE_OFFSET 2`), phone `fav_add`. |
| `session_decoy_duplicate_flights.md` | `GAME_MODE_DUPLICATE`, `GAME_MODE_DECOY`. |
| `session_head_to_head.md` | `h2h.cpp`; all three sub-modes (`H2H_SUB_2X2`, `H2H_SUB_RANDOM`, `H2H_SUB_PREMIUM`), lobby, phone `h2h_join`. |
| `session_library_metadata_tasting_notes.md` | `LibraryEntry` proof / priceTier / ageYears; `GAME_RATING` round and `glassRating[]`. |
| `sta_mode_ota_spec.md` | STA + AP fallback, mDNS `flight.local`, `ota.cpp` / `screen_ota.cpp`. OTA is now the only firmware delivery path — see `CLAUDE.md`. |

`Completed/revert_library_metadata.patch` is a rollback escape hatch for the library
metadata change. That change has been stable for many releases; the patch is kept only
because it is cheap to keep, and it is safe to delete.
