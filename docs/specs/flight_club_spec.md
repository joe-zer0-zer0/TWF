# Session Spec — Flight Club (multi-participant sequential flights)

Module `party.cpp`, enum prefix `PARTY_`, mode `GAME_MODE_PARTY` — internal identifiers
stay neutral; "Flight Club" is the display name only.

**Flight Club must be fully operable from the encoder and screen with no phone
connected.** Phone support is an enhancement, never a dependency. See *Device-only
operation* below — this constraint has teeth in two places that are easy to miss.

## Overview

Every existing mode runs one flight for whoever is standing at the device. Party Flight
pours **the same bottles for each of N participants, in an independently randomized glass
order per participant**, holds every result back, and reveals a cumulative group result
only once everyone has submitted.

Glass count is selectable 2–4 as in the other modes, and is fixed for the whole event —
every participant's flight uses the same bottles and the same count. `settingsGetGlassCount()`
already exists (2–4, default 4, NVS-persisted), so this reuses existing machinery rather
than adding storage.

The blind property is structural, not enforced by secrecy: participants know which four
bottles are in play. Nobody — **including the host who is pouring** — knows which bottle
landed in which glass, and because each participant's flight is randomized separately,
comparing notes across the room leaks nothing. This is what lets the host play.

Flow, per participant:

1. Host picks glass count and enters that many bottles once, at the start of the event.
2. Participant's name is entered.
3. Device randomizes, names a bottle to pour, host pours; repeat per glass.
4. Device dictates the order to lift glasses into the participant's numbered tray.
5. Fresh glasses loaded, next participant's name entered, repeat.

After the last flight, participants submit either a **ranking** (best→worst) or a
**guess** (which bottle is in which glass), or both. The reveal is aggregate.

---

## Constraints

These come from the current source tree and determine the design.

### C1 — The pour view must never name a glass

The blind property depends entirely on glass identity being assigned at *collection*,
not at *pour*. The existing UI already has this: `drawPouring()`
([`game.cpp:598`](../../Blind-Flight/src/game.cpp:598)) renders the pour number and bottle
name only; `session.currentGlass` is tracked but never drawn, and the phone's `pouring`
view ([`phone_ui.html:271`](../../Blind-Flight/web/phone_ui.html:271)) says only "Ready To
Pour".

**This is now a requirement, not an accident.** `gameGetCurrentGlass()`
([`game.cpp:2083`](../../Blind-Flight/src/game.cpp:2083)) is exposed to the state JSON for
the Duplicate/Decoy challenge modes. Party Flight must not emit it — or any per-pour
glass number — during `PARTY_POURING`. A regression here silently destroys the mode
while everything still appears to work.

### C2 — Client cap is 5, and it is the AP that hurts

Three independent limits:

| Limit | Value | Where | Raisable |
|---|---|---|---|
| softAP stations | 4 (Arduino default) | [`wifi_portal.cpp:1578`](../../Blind-Flight/src/wifi_portal.cpp:1578) | Yes, `max_connection` arg (ESP-IDF max 10) |
| `WEBSOCKETS_SERVER_CLIENT_MAX` | 5 | `WebSocketsServer.h:31` | Yes, `build_flags` |
| `clientAuthed[5]` + three `>= 5` guards | 5 | [`wifi_portal.cpp:40`](../../Blind-Flight/src/wifi_portal.cpp:40), `:1621`, `:1634`, `:2045` | Yes |
| `CONFIG_LWIP_MAX_SOCKETS` / `MAX_ACTIVE_TCP` | 16 / 16 | prebuilt SDK | **No** |

In **AP mode** the binding limit is 4 stations → 3 participants plus host. In **STA
mode** the station cap disappears (the router handles association) and the ceiling
becomes lwIP: 16 sockets minus three listeners (HTTP 80, WS 81, DNS 53) minus transient
HTTP page fetches. **Practical safe ceiling ≈ 8–10 WebSocket clients.**

Party Flight is therefore designed for **STA mode with up to 8 participants**, with AP
mode supported but capped. `MAX_PARTY_PLAYERS = 8`.

> Heap per WS client has not been measured. Raising the define to 10 requires an actual
> free-heap reading with 8 clients attached before it ships — arithmetic is not enough.

### C3 — There is no role separation in the portal today

`buildStateJSON()` is built once and `broadcastTXT()`'d to every authenticated client
([`wifi_portal.cpp:1073`](../../Blind-Flight/src/wifi_portal.cpp:1073)). Every authed
client is equivalent. If a participant's phone is connected during pouring it receives
the pour instruction and the full bottle list — violating C1 through the side door.

H2H has the *shape* of a fix (per-client `h2h_you` / `h2h_result` sends at
[`wifi_portal.cpp:1391`](../../Blind-Flight/src/wifi_portal.cpp:1391)) but those supplement
a broadcast document rather than replacing it. Party Flight needs the state document
itself role-filtered. **This is the largest piece of new architecture in the spec** and
is why participant phones are deferred to Phase 3.

### C4 — The PIN identifies nobody

`generatePIN()` ([`wifi_portal.cpp:195`](../../Blind-Flight/src/wifi_portal.cpp:195))
derives the PIN deterministically from the eFuse MAC with no randomness and no rotation:
**it is the same four digits for the life of the device.** The phone caches it in
`localStorage` under `bf_pin` and silently re-auths
([`phone_ui.html:121`](../../Blind-Flight/web/phone_ui.html:121)).

So "knows the PIN" cannot mean "is the host" — anyone who has ever connected has
permanent auto-auth. Host identity needs its own token (see *Roles & ownership*).

### C5 — Participant identity must survive a reconnect

`H2HPlayer.clientNum` *is* the identity in H2H. That is fine for a five-minute game. A
party runs 30–60 minutes on a network the phone may leave and rejoin (screen lock, AP
with no internet, walking out of range). A returning phone gets a new client number and
the device no longer knows who it is. Identity must be a token the phone presents, not
the socket number.

### C6 — Headless has no browse and no screen

`browse.cpp` is excluded from `esp32-headless`
([`platformio.ini`](../../Blind-Flight/platformio.ini)), so on-device bottle entry does not
exist there. Bottle and name entry must be phone-driven, as `h2hPhoneAddBottle()` already
is.

More importantly: **H2H is broken headless precisely because no phone action can confirm
a pour** (see `CLAUDE.md`). Party Flight must have a phone action for *every* state
transition from day one — pour confirmed, collection confirmed, next participant, submit,
advance reveal. Designing screen-first and retrofitting is the known failure mode.

### C7 — Persistence is a single fixed struct at version 1

`persistSaveGame()` writes one `GameSession` blob with `PERSIST_VERSION 1`
([`persist.cpp:5`](../../Blind-Flight/src/persist.cpp:5)). A party session is a different
shape and, uniquely, is *long*: a brownout 40 minutes in currently loses the entire
event's data. Party state goes in its **own NVS key**, not by widening the existing
snapshot.

---

## Device-only operation

A full party — setup, every flight, every input round, the reveal — must be completable
on the encoder and screen alone. Auditing the existing screen path against that
requirement:

### Disc ownership — fix the class, not the instance

Two `ui.cpp` sites currently name every disc-owning module by hand. Both are the item-7k
class of bug: **"who owns the disc" is a hand-maintained list, and a mode not added to it
fails silently.**

1. **Idle-off invalidates homing per module, by name**
   ([`ui.cpp:233`](../../Blind-Flight/src/ui.cpp:233)) — `gameInvalidateHoming();
   h2hInvalidateHoming();`
2. **The bare wake-press homing guard**
   ([`ui.cpp:253`](../../Blind-Flight/src/ui.cpp:253)) — `if (!gameIsActive() &&
   !h2hIsActive()) runHomingSequence();`

**How much does per-participant re-homing help?** Flight Club homes at the start of every
participant's flight, so idle-off *between* participants is harmless — the next flight
re-homes regardless, and releasing the motor in that gap is desirable (see *Motor duty*
below). But the risk survives *within* a flight. `uiResetIdleTimer()` fires at pour-cycle
start ([`game.cpp:1127`](../../Blind-Flight/src/game.cpp:1127)) and the device then waits in
`GAME_POURING` for a "Done — Poured" press with no time bound. A host pulled away
mid-flight for ten minutes gets idle-off, `motorDisable()`, a hand-turnable disc, and the
remaining pours of that flight running on an unverified position. Smaller blast radius,
same bug.

**Suppressing the idle timeout during pours is not the fix.** It costs the whole party's
worth of motor holding current (see *Motor duty*), and it does not address site 2 at all —
the dangerous wake-press moment is precisely the between-participant gap where idle *should*
be allowed: fresh glasses loaded, cover replaced, host taps a button to wake the screen.

**Recommended: move position trust into `motor.cpp` and let `motorDisable()` clear it.**
Disabling the driver *is* the moment the disc becomes hand-turnable, so that is where
invalidation belongs. Today each module carries its own `static bool homedThisFlight`
([`game.cpp:68`](../../Blind-Flight/src/game.cpp:68),
[`h2h.cpp:55`](../../Blind-Flight/src/h2h.cpp:55)) and its own `xInvalidateHoming()`; that
duplication is the hazard. With a single `motorPositionIsVerified()` flag, site 1 collapses
to nothing and no future mode can be forgotten.

> Not free: ~12 `motorDisable()` call sites need auditing (`screen_calibrate`, `selftest`,
> `screen_glass_diag`, `screen_hw_diag`, `screen_diagnostics`, `gameAbort`). All of them
> already home themselves before moving, so this is expected to be clean — but it is an
> audit, not a rename.

**Site 2 must still be fixed explicitly** — `&& !partyIsActive()`. Flight Club is the mode
most likely to sit at idle-off with a full set of glasses loaded, waiting for the host to
come back. Missing it doesn't produce a glitch, it produces a spill.

### Motor duty

After `motorSpinToGlass()` there is **no `motorDisable()`**
([`game.cpp:1184`](../../Blind-Flight/src/game.cpp:1184)) — the driver holds current from the
moment a glass reaches the spout until the flight's final spin. That is correct during a
pour; it is waste between participants.

Flight Club should therefore **call `motorDisable()` explicitly at the collection step**,
releasing the disc at the known-safe moment rather than waiting ~10 minutes for `offDelay`
to do it by accident. The idle timeout otherwise stays at its normal settings.

### Already handled — verified, no work needed

- **Wake presses are consumed.** The idle handler `continue`s past the waking event
  ([`ui.cpp:242`](../../Blind-Flight/src/ui.cpp:242)), so the press that lights the screen
  can't also select a menu item. A long party leans on this constantly.
- **Screen depth is fine.** `MAX_SCREEN_DEPTH` is 8; the deepest Party path is
  splash → menu → party → browse = 4.
- **Text entry is a widget, not a screen.** `TextEntry` is driven by a mode flag inside a
  host screen, exactly as `browse.cpp` does with `textEntryMode`
  ([`browse.cpp:24`](../../Blind-Flight/src/browse.cpp:24)). Name entry costs no stack depth
  and no new screen.
- **Scrolling lists, ranking, and guessing screens** all exist and are reusable as-is.

### Where device-only is genuinely worse

- **The reveal.** Eight participants × a glass number per bottle does not fit 240×280.
  Because the phone is optional, **paging is mandatory**, not a fallback: the bottle and
  its aggregate on one page, per-participant breakdown on subsequent pages.
- **Name entry.** `TEXT_ENTRY_MAX_LEN` is 20 and entry is character-by-character on the
  encoder — call it 20–30 s per name, so eight participants is several minutes of setup
  before any whiskey is poured. **Add a saved participant roster** modelled on
  `favorites.cpp`, so repeat guests are a single click and only new names are typed. This
  is the difference between "works without a phone" and "is pleasant without a phone".

---

## Data model

```c
#define MAX_PARTY_PLAYERS   8
#define PARTY_NAME_LEN      13   // matches H2H_NAME_LEN: 12 chars + null

struct PartyPlayer {
    char    name[PARTY_NAME_LEN];
    uint8_t bottleForGlass[NUM_GLASSES]; // glass_0based -> bottle index. The answer key.
    uint8_t rankOrder[NUM_GLASSES];      // rankOrder[slot] = glass# (1-based), 0 = unset
    int8_t  guessForGlass[NUM_GLASSES];  // glass_0based -> guessed bottle idx, -1 = unset
    uint32_t token;                      // reconnect identity (C5), 0 = never connected
    uint8_t clientNum;                   // current socket, 0xFF = not connected
    bool    poured;                      // flight has been poured
    bool    rankDone;
    bool    guessDone;
};
```

Arrays are sized `NUM_GLASSES` (4) but only `glassCount` entries are used — the same
pattern `GameSession` already follows for variable-count flights.

Event-level state sits alongside: `glassCount` (2–4), `bottleName[NUM_GLASSES]`,
`scoring`, `playerCount`, `currentPlayer`, `phase`.

~40 bytes per player; 8 players ≈ 320 bytes, plus bottle names at `MAX_GLASS_NAME` (22)
= up to 88. Total well under 1 KB — a non-issue for RAM and a comfortable NVS blob.

`bottleForGlass` is the whole point: it is written during that participant's pour loop
and is the per-flight answer key that both scoring variants resolve against.

---

## Roles & ownership

**Two separate concepts; do not merge them.**

*Device owner* — a persistent 64-bit token minted on first claim, hash stored in NVS,
token handed to the phone which keeps it in `localStorage` under `bf_owner` alongside the
existing `bf_pin`. Presented on every connect. Needs:

- **"Forget owner phone"** in Settings — phones get replaced and browsers get cleared.
- **Physical access wins.** Re-claim is always possible from the device's own encoder.
  On headless that needs a claim window (first 60 s after boot) since there is no screen
  to authorize from.

*Session host* — whoever started the current party flight. Usually the owner; the owner
token is the default and the tiebreak. Keeping these separate means lending the device to
a friend needs no re-pairing.

Every client carries `role: "host" | "guest"` in its state document. During
`PARTY_POURING`, the guest document omits bottle names, the pour instruction, and any
glass number (C1, C3).

---

## Offline participation

Can the full reveal be pushed to the phone at end-of-pours but withheld until unlock?

**Not securely — anything sent to the phone is readable by a determined user.** The page
JS is view-source, and WebSocket frames are inspectable. Treat "hidden in JS" as
obfuscation, not secrecy. But the question conflates two capabilities that should be
separated, and separating them makes the mode substantially better:

**Completing a flight needs no secrets at all.** To rank or guess, the phone needs only
the participant's identity, the glass count, and the bottle list — all of which
participants already know. Nothing withheld, nothing to leak. **This should be delivered
unconditionally**, and it is the part that actually matters: tasting is the long,
away-from-the-device phase.

**Seeing the reveal needs the answer key**, which is the only genuinely secret payload.
Two workable options:

- **Reconnect to reveal.** The phone submits, the device computes the aggregate, and the
  reveal arrives over the connection. Simplest; no secrets ever sit unlocked on a phone.
- **Out-of-band host code.** Ship the reveal payload encrypted at end-of-pours; the host
  reads out a short code when everyone has submitted. Fully offline, host-controlled
  timing, and brute-forcing four digits is an unmistakably deliberate act rather than an
  accident. Honest about the threat model, which at a tasting party is casual peeking.

A device-generated timestamp lock is **not** an option — the phone's clock is
user-controlled.

### This materially relaxes C2

If phones need the device only to *receive* a payload and *submit* a result, connections
become **brief and serialized rather than sustained and simultaneous**. The 5-client cap
stops being a participant cap: eight people can connect one at a time, take their payload,
disconnect, taste, and reconnect to submit. That removes the strongest argument for
raising `WEBSOCKETS_SERVER_CLIENT_MAX`, and makes AP mode viable for group sizes that
otherwise required STA.

**Design Phase 3 around brief connections rather than a persistent lobby**, which is the
opposite of how H2H works and is the better fit here.

---

## Scoring variants

The pour engine is identical across variants; only the input round and the aggregate
differ. **This is one mode with three scoring settings, not three modes** — chosen at
setup, stored as `PartyScoring`:

| Variant | Per-participant input | Aggregate |
|---|---|---|
| `PARTY_SCORE_RANK` | Rank glasses best→worst | Mean rank per bottle |
| `PARTY_SCORE_GUESS` | Match bottle to each glass | Participants banded by count correct |
| `PARTY_SCORE_BOTH` | Rank, then guess | Both boards |

`PARTY_SCORE_RANK` reuses the `GAME_RANKING` machinery (`rankOrder[]`, `rankIndex`,
`gamePhoneRankSelect()`); `PARTY_SCORE_GUESS` reuses `GAME_GUESSING`'s pool management
(`guessForGlass[]`, `gameGetGuessPoolName()`, `gamePhoneGuessSelect()`). Both run
per-participant instead of per-session.

### Rank aggregate

Each participant's glass ranking is resolved through their own `bottleForGlass` into a
bottle ranking, then averaged across participants. Lower mean = better.

**Ties are common** with 3–4 participants (two bottles at 2.5 is routine). Tiebreak, in
order: most 1st-place votes → most 2nd-place votes → … → bottle entry order. Deterministic
and explainable out loud, which matters when someone contests the result at a party.

A bottle ranked identically by everyone is flagged **"Unanimous"** in the reveal.

### Guess aggregate — banded, not ranked

A guess is a permutation; score = number of fixed points. **With N glasses you can never
score exactly N−1** — if N−1 are right the last one must be too. Reachable scores and
their distribution over all N! permutations:

| Glasses | 0 | 1 | 2 | 3 | 4 | Reachable scores |
|---|---|---|---|---|---|---|
| 2 | 1 | **0** | 1 | — | — | 0, 2 |
| 3 | 2 | 3 | **0** | 1 | — | 0, 1, 3 |
| 4 | 9 | 8 | 6 | **0** | 1 | 0, 1, 2, 4 |

Expected score from pure guessing is exactly 1 at every N.

**The reveal groups participants into bands by number correct, ascending from 0** — all
who got none, then all who got one, and so on. It is not a ranked list and there is no
placement, so **the guess variant needs no tiebreak at all.** (The rank variant's
tiebreak above still stands; the two aggregates are independent.)

Rendering rules that follow from the table:

- **Skip empty bands**, including the unreachable N−1 band — never render a "3 correct"
  heading with nobody under it.
- **Always close on the perfect band, even when empty.** "Nobody swept it" is the
  punchline; silently ending on the 2-correct group loses the beat.
- Surface "N−1 correct is impossible" in the UI. It preempts the "but I got three right!"
  argument, which will otherwise happen at every event.

**Two-glass guessing is a coin flip** — the only outcomes are 0 and 2, and half of all
guesses are perfect. Worth a warning at setup when the host picks 2 glasses with a guess
variant selected. Not blocked; it is a legitimate quick round, just not a test of much.

---

## Flow

```
PARTY_SETUP        scoring variant, participant count
PARTY_COUNT        glass count 2–4                            [once per event]
PARTY_BOTTLES      enter that many bottles (browse/phone/manual)
PARTY_NAME         enter participant name                     ┐
PARTY_POURING      randomize → name bottle → pour → confirm ×N │ per participant
PARTY_COLLECT      "Lift glass 1 … glass N" into the tray      ┘
PARTY_COLLECTING   all flights poured; awaiting submissions
PARTY_ENTRY        host-proxy entry, or per-participant phones
PARTY_REVEAL       aggregate reveal, worst → best
PARTY_DONE
```

`PARTY_COLLECTING` offers the host two actions:

- **Enter rankings** — proxy entry on the host's behalf for a named participant. The
  host picks a participant, walks their input round, returns to the list.
- **Show results** — a participant's own answer key (name + every pour by glass number),
  for someone who finishes early and wants to leave.

  > **Confirm-gate this.** Handing Dave his answer key while Susan is still tasting puts
  > the bottles in the room. Gate it — "This reveals the answers for Dave. Continue?" —
  > rather than blocking it.

- **Skip to reveal** — proceed with incomplete data. **Participants without complete
  input are excluded from the aggregate entirely**, not partially counted. Name them on
  the reveal screen so their absence is visible and not mistaken for a bug.

### Reveal

Worst first, building to the winner. Per bottle:

1. Mean rank (or correct-count board, per variant).
2. Which glass number it was **for each participant** — this is why `bottleForGlass` is
   stored per player rather than per session.
3. "Unanimous" flag where applicable.

**The device screen cannot hold this.** Eight participants × a glass number per bottle
does not fit 240×280. On-screen reveal shows the bottle, its mean rank, and the unanimous
flag; the per-participant breakdown is phone-only, or paged behind a "Details" press.

---

## File-by-file changes

| File | Change |
|---|---|
| `src/party.cpp` / `.h` | **New.** Entire mode, modelled on `h2h.cpp`. Do not extend `game.cpp` — it is 2311 lines carrying eight modes already. |
| `src/game.h` | Add `GAME_MODE_PARTY` to the enum only. |
| `src/persist.cpp` / `.h` | New NVS key for party state (C7); `PERSIST_VERSION` → 2. Existing snapshot struct untouched. |
| `src/wifi_portal.cpp` | Role-filtered state doc (C3); owner token claim/verify (C4); participant token reconnect (C5); `clientAuthed[]` resize (C2); new `party_*` actions. |
| `src/motor.cpp` / `.h` | Position-trust flag cleared by `motorDisable()`; `motorPositionIsVerified()`. Replaces the per-module `homedThisFlight` bools. |
| `src/ui.cpp` | Drop the by-name invalidation list; add `!partyIsActive()` to the wake-press homing guard. See *Disc ownership*. |
| `src/screens.cpp` | Mode menu entry (guarded `#ifndef HEADLESS_BUILD`). |
| `src/roster.cpp` / `.h` | **New, Phase 1.** Saved participant names, modelled on `favorites.cpp`. |
| `src/settings.cpp` / `screen_settings.cpp` | "Forget owner phone". |
| `web/phone_ui.html` | Host setup, proxy entry, participant name-claim, ranking, guessing, waiting, aggregate reveal. Est. +8–10 KB raw / +2–3 KB gzipped. |
| `platformio.ini` | `-DWEBSOCKETS_SERVER_CLIENT_MAX=10` on **both** environments. |

Flash budget is comfortable: 1,302,816 bytes today against a ~1.92 MB `min_spiffs` OTA
slot.

---

## Phasing

Networking risk is deliberately last; each phase ends in a released, testable build.

**Phase 1 — engine + host-proxy, rank scoring, fully device-operable.** Multi-flight pour
loop, collection prompts, host-proxy rank entry, paged cumulative reveal with tiebreak and
unanimous flag, saved roster, NVS persistence, the disc-ownership refactor, and explicit
motor release at collection. No phone required at any step; no role separation needed.
**This alone covers the home use case.**

> The disc-ownership refactor touches `motor.cpp` and every `motorDisable()` caller. Per
> `CLAUDE.md`'s preference for small releases, it is worth tagging as its own build ahead
> of the Flight Club engine — it is a self-contained fix that benefits every existing mode,
> and it keeps a motor-behaviour change out of a diff that also introduces a game mode.

**Phase 2 — guess variant.** `PARTY_SCORE_GUESS` and `PARTY_SCORE_BOTH`, participant
leaderboard with shared placements. Pure addition to Phase 1's engine.

**Phase 3 — participant phones.** Role separation (C3), owner token (C4), reconnect tokens
(C5), offline payload + submit flow (*Offline participation*). Built around brief
serialized connections, so the client-cap raise (C2) becomes optional rather than a
prerequisite — measure heap before deciding to raise it at all.

**Prerequisite, any phase — resting battery measurement.** See *Battery*. Independent of
Flight Club and worth doing regardless; every mode currently samples voltage under load.

---

## Battery — the threshold cannot be set from current data

**There is no empirical basis for a Flight Club battery threshold, and raising the
existing one would make things worse.** The only capture we have is
`docs/baselines/selftest_baseline_2026-07-28_fw1.5.2.log` — 66 reads over 128.3 s, one
pack, unloaded disc, no ground truth on state of charge.

What that log actually shows, read as a trajectory rather than as endpoints:

| Time | mV | Note |
|---|---|---|
| 11.4 s | 8003 | run start; header reports boot at 8119 mV / 84 % |
| 54.8 s | 7339 | −664 mV in 43 s |
| 71.3 s | 7189 | **first read at/below the 15 % lockout voltage (7.20 V)** |
| 111.1 s | 6986 | |
| 120.6 s | 7040 | **recovers +54 mV** |
| 135.1 s | 6993 | |

**This is load sag, not discharge.** A 2S pack does not lose 79 percentage points of
charge in two minutes of light stepper duty, and the recovery at 120.6 s is the tell —
voltage rises when load drops. `readRawVoltage()` samples whenever `batteryUpdate()` runs,
including mid-spin, so `smoothedV` tracks the sag.

Two consequences:

1. **The device already reads below its own lockout under routine motor load.** 40 of 66
   reads (61 %) sat at or below 7.20 V — the `BATT_LOCKOUT_PCT 15` threshold — on a pack
   that booted at 84 %. It doesn't trip today only because `batteryIsLockout()` is checked
   at pour-cycle start ([`game.cpp:1100`](../../Blind-Flight/src/game.cpp:1100)) and
   `BATT_SMOOTHING 0.2f` at 2 s intervals (~10 s time constant) lags the transient. Flight
   Club's back-to-back spins are exactly the duty cycle that closes that gap.
2. **Raising the percentage would cause spurious mid-party lockouts**, not prevent real
   ones. The number isn't wrong; the measurement is.

Also note the LUT in `battery.cpp` is commented "Measured points along a typical discharge
profile" but is a generic 2S Li-ion curve — it was not characterised against this pack.

### Prerequisite measurement

Neither of these is a firmware project; telemetry already logs mV per event, so both are
bench procedures.

1. **Sample at rest.** Gate `batteryUpdate()` on the motor being disabled, or keep a
   separate resting-voltage estimate that only updates when `PIN_MOTOR_EN` is high. This
   alone likely removes most of the error and should land before any threshold is chosen.
2. **Run to stall.** Full charge, repeated flight cycles with a loaded disc, logging mV and
   cycle count until the carousel actually stalls. That yields the two numbers that matter:
   **flights per charge**, and **the voltage at which stall occurs** — the only defensible
   basis for a lockout threshold.

> **Sequencing:** stall voltage is a function of mechanical load, and the PTFE pad /
> carrier plate work changes that load. Run this after the mechanical prototyping settles,
> or budget a re-run. Same trap as the alignment baseline in `CLAUDE.md`.

Until then, Flight Club should **warn** the host at session start with the current
percentage and an explicit "this is the longest motor session the device runs" note, and
not invent a hard threshold.

---

## Open decisions

1. **Offline reveal key delivery** — see *Offline participation*: out-of-band host code, or
   require a reconnect for the reveal.

---

## Testing checklist

Filled in per phase at release time. Non-obvious items that must be on it:

- [ ] **Blind property:** with a participant phone connected during pouring, confirm the
      guest document contains no bottle name, no pour instruction, no glass number (C1/C3).
- [ ] Power-cycle mid-party; confirm the event resumes with all completed flights intact.
- [ ] Lock a participant's phone for 10 minutes, reopen; confirm they are recognized and
      not duplicated (C5).
- [ ] Skip to reveal with one participant incomplete; confirm they are excluded and named.
- [ ] Force a rank tie; confirm the tiebreak resolves and matches the stated rule.
- [ ] Run at 2, 3, and 4 glasses; confirm collection prompts and reveal both track the
      count.
- [ ] Guess variant: confirm no participant is ever shown a score of N−1, that empty
      bands are skipped, and that an all-imperfect round still closes on "nobody swept".
- [ ] **Complete an entire party start to finish with no phone connected.** Not a
      spot-check — the full path, including reveal paging.
- [ ] Let the device reach idle-off between participants with fresh glasses loaded, then
      press a button to wake: confirm the carousel does **not** spin, and that the next
      pour re-homes before moving.
- [ ] Stall *mid-flight* — leave the device in `GAME_POURING` past `offDelay` — then
      confirm the remaining pours of that flight re-home rather than trusting the old
      position.
- [ ] Confirm the motor is de-energised during the glass-swap gap, not holding current.
- [ ] Headless build: complete an entire party from a phone alone, with no screen (C6).
- [ ] Eight clients attached: record free heap before tagging (C2).
- [ ] Log battery mV across a full multi-participant party; confirm no spurious lockout
      and compare the sag profile against the 2026-07-28 baseline.
- [ ] Confirm a participant can receive their payload, disconnect entirely, rank offline,
      and reconnect to submit.
