# Session Spec — Head-to-Head Multiplayer

## Overview

Head-to-Head is a top-level game mode (`GAME_MODE_H2H`) containing **three sub-modes**, each with a different competitive mechanic. All three share a common multi-player infrastructure (player registry, lobby, per-player WebSocket tracking) but differ in pour logic, guess format, and results display.

### Sub-modes

| # | Name | Players | Glasses | Mechanic |
|---|------|---------|---------|----------|
| 1 | **2x2** | 2 | 4 (fixed) | 2 bottles poured twice each. Each player gets one glass of each bottle and guesses which is which. |
| 2 | **Multiplayer Random** | 2–4 | 2–4 (user-selected) | Standard randomized pour. Each player takes one glass and guesses which bottle they received. |
| 3 | **Premium Pour** | 2–4 | 2–4 (matches player count) | One premium bottle poured first, remainder are standard. Players guess yes/no whether they got the premium. |

### Shared Infrastructure

All three sub-modes require:
- **Player registry** in `wifi_portal`: track connected WebSocket clients with display names, per-player guess state, submitted flag
- **Lobby screen** on device: shows joined players, host starts when ready
- **Phone join flow**: load portal → enter name → join game → wait in lobby
- **"Waiting for guesses" screen** on device with live counter ("2 of 3 submitted")
- **Per-player results** pushed to each phone after all guesses are in

### Protocol: Multi-player WebSocket Extensions

The existing single-client WebSocket protocol becomes a player registry. New message types:

```
phone → device:
  {"a":"h2h_join","v":"Alice"}         // join lobby with display name
  {"a":"h2h_guess","v":<mode-specific>} // submit guess (format varies by sub-mode)

device → phones (broadcast):
  {"t":"h2h","phase":"lobby","players":["Alice","Bob"]}
  {"t":"h2h","phase":"assign","you":{glasses:[1,3],bottles:["Eagle Rare","Weller"]}}  // 2x2 only
  {"t":"h2h","phase":"guessing","submitted":1,"total":2}
  {"t":"h2h","phase":"results","correct":true|false,...}  // per-player targeted, not broadcast
```

**Critical rule:** the true glass→bottle mapping must never be sent before results. Only bottle *names* (unordered) go out during guessing.

### Player Registry Data Structure

```cpp
#define H2H_MAX_PLAYERS  4
#define H2H_NAME_LEN     13  // 12 chars + null

struct H2HPlayer {
    uint8_t clientNum;           // WebSocket client ID
    char    name[H2H_NAME_LEN];
    bool    connected;
    bool    submitted;
    // Guess data (union-like, varies by sub-mode):
    int     guess2x2[2];         // 2x2: which glass has bottle 0, which has bottle 1
    int     guessBottleIdx;      // Multiplayer Random: which bottle index they think they got
    bool    guessPremium;        // Premium Pour: yes/no
    bool    correct;             // result after scoring
    int     assignedGlasses[2];  // 2x2: which glass positions this player has
};
```

---

## Sub-mode 1: 2x2

### Flow

1. **Setup:** Device operator enters 2 bottle names via browse library (reuses existing bottle-select pattern from Duplicate/Decoy)
2. **Lobby:** 2 players join via phone. Device shows lobby with names. Host presses START when both are connected.
3. **Pour sequence** (4 pours total):
   - Randomly select an unused glass position → pour Bottle A
   - Confirm pour → randomly select another unused glass → pour Bottle A again
   - Confirm pour → randomly select another unused glass → pour Bottle B
   - Confirm pour → pour Bottle B into the last remaining glass
4. **Glass assignment screen:** Device displays:
   - "Player 1: Alice" → Glass X and Glass Y
   - "Player 2: Bob" → Glass Z and Glass W
   - Each player gets one glass of each bottle (assigned so that each player has one A and one B, but they don't know which is which)
5. **Phone guess:** Each player sees their two glass numbers and the two bottle names. They assign which glass they think is which bottle.
6. **Results:** Phone shows correct/incorrect per glass. Device shows both players' results.

### Glass Assignment Logic

After pouring, the four glasses contain: A, A, B, B. Assign so each player gets one A and one B:
- Find the two glass positions containing Bottle A → give one to Player 1, one to Player 2
- The remaining two glasses (Bottle B) are assigned accordingly
- Randomize which A-glass goes to which player

### Device Screens

- **Bottle select** (reuse existing `GAME_BOTTLE_SELECT` pattern, target = 2 bottles)
- **Lobby** ("HEAD TO HEAD — 2x2", player list, START button)
- **Pour** (reuse existing pour screen, show "Pour X of 4" with bottle name)
- **Assignment** ("Take Your Glasses" — Player 1: glasses X,Y / Player 2: glasses Z,W)
- **Waiting** ("Waiting for guesses… 1/2 submitted")
- **Results** (both players: name, correct/incorrect per glass)

### Phone Screens

- **Join** (name entry → join button)
- **Lobby** (waiting for game to start)
- **Assignment** ("Your glasses: 1 and 3")
- **Guess** (two dropdowns or tap-to-assign: Glass X = ?, Glass Y = ?)
- **Results** (your guesses with ✓/✗)

---

## Sub-mode 2: Multiplayer Random

### Flow

1. **Setup:** Device operator selects glass count (2–4) and enters bottle names via browse library (same as existing Named/Best Guess flow)
2. **Lobby:** 2–4 players join. Player count must match glass count. Host starts when all are connected.
3. **Pour:** Standard randomized pour sequence (reuses existing `selectRandomGlass()` + browse-per-pour flow)
4. **Tasting:** Device shows "Tasting Time" — players each take one glass. Device or host verbally assigns glass numbers. (No enforced assignment — honor system.)
5. **Phone guess:** Each player sees the full list of bottle names and selects which one they think they received. If bottle was entered by pour number only, the phone shows "Pour 1", "Pour 2", etc.
6. **Results:**
   - Phone: first shows "Correct!" or "Incorrect" with what they guessed vs. what their glass actually was
   - Device results screen:
     - **Green banner** at top: "CORRECT" — lists glass numbers + player names who guessed correctly
     - **Red banner** at bottom: "INCORRECT" — lists glass numbers + player names who guessed incorrectly
     - If the entry was just by pour number (no name), show only the player number (e.g., "Player 1") not the pour-number label

### Glass-to-Player Mapping

Unlike 2x2, there's no enforced glass assignment. Each player self-reports which glass they took:
- Option A: Player declares their glass number on the phone before guessing
- Option B: Host assigns verbally; phone just asks "which bottle?"

**Proposed: Option A** — phone asks "Which glass did you take?" (1–4) before showing the bottle list. This enables per-player scoring without the host needing to track assignments.

### Device Screens

- **Count select** (reuse existing `GAME_COUNT_SELECT`)
- **Browse/pour** (reuse existing named-flight flow)
- **Lobby** ("MULTIPLAYER RANDOM", player list, START when count matches glass count)
- **Tasting** (reuse existing, but hint changes to "Take one glass each")
- **Waiting** ("Waiting for guesses… 2/3 submitted")
- **Results** (green banner: correct guesses, red banner: incorrect guesses)

### Phone Screens

- **Join** (shared with 2x2)
- **Lobby** (shared)
- **Glass select** ("Which glass did you take?" — tap 1–4, only unselected glasses available)
- **Guess** ("What's in your glass?" — list of bottle names)
- **Results** (correct/incorrect with the actual answer)

---

## Sub-mode 3: Premium Pour

### Flow

1. **Setup:** Device operator enters the premium bottle name first (browse library), then enters remaining bottles (count = player count − 1) via standard flow
2. **Lobby:** 2–4 players join. Player count must equal total glass count.
3. **Pour sequence:**
   - Premium bottle poured **first** into a randomly selected glass
   - Remaining bottles poured in standard random order
4. **Tasting:** Same as Multiplayer Random — players each take a glass
5. **Phone guess:** Simple yes/no — "Did you get the premium pour?" with YES and NO buttons
6. **Glass select:** Same as Multiplayer Random — player declares which glass they took before guessing
7. **Results:**
   - Phone: shows "Correct!" or "Wrong!" and reveals which glass actually had the premium
   - Device: shows each player's guess and whether they were correct/incorrect

### Device Screens

- **Bottle select** (modified: first bottle labeled "PREMIUM BOTTLE", remaining labeled "Standard Bottle X of Y")
- **Lobby** ("PREMIUM POUR", player list)
- **Pour** (show bottle name; premium pour displays with special accent)
- **Tasting** (same pattern)
- **Waiting** (same pattern)
- **Results** (per-player: name, guess (yes/no), correct/incorrect)

### Phone Screens

- **Join** (shared)
- **Lobby** (shared)
- **Glass select** (shared with Multiplayer Random)
- **Guess** ("Did you get the premium pour?" — big YES / NO buttons, premium bottle name shown)
- **Results** (correct/incorrect, reveals which glass was premium)

---

## Session Breakdown

### Session A: Multi-player Infrastructure + 2x2 Mode

**Goal:** Land the player registry, lobby system, and the most complex sub-mode (2x2) so the framework is battle-tested.

**Changes by file:**

| File | Change |
|------|--------|
| `config.h` | `H2H_MAX_PLAYERS`, `H2H_NAME_LEN`, `H2H_LOBBY_TIMEOUT_S` |
| `game.h` | `GAME_MODE_H2H` enum value; `H2H_SUB_2X2` / `H2H_SUB_RANDOM` / `H2H_SUB_PREMIUM` sub-mode enum; new game states: `GAME_H2H_SELECT`, `GAME_H2H_LOBBY`, `GAME_H2H_ASSIGN`, `GAME_H2H_WAITING`, `GAME_H2H_RESULTS`; `H2HPlayer` struct; state getters for H2H |
| `game.cpp` | Sub-mode selection screen, 2x2 bottle select (reuse pattern), 2x2 pour logic, glass assignment logic, assignment display screen, waiting screen, results screen, H2H state getters |
| `wifi_portal.h/.cpp` | Player registry: join/leave tracking, `h2h_join`/`h2h_guess` action handlers, per-player state send, lobby broadcast, H2H phase tracking in `buildStateJSON`, player-targeted messages |
| Portal HTML/JS | Join screen (name entry), lobby waiting, glass assignment view, 2x2 guess UI (assign bottles to glasses), results view |
| `screens.cpp` | "Head to Head" menu entry → pushes H2H sub-mode selector |

**Testing checklist:**
- [ ] 2 phones join lobby with names; device shows both
- [ ] Start with 2 players → pour sequence follows correct order (A, A, B, B into random glasses)
- [ ] Assignment screen shows each player's 2 glasses (one of each bottle)
- [ ] Phone shows correct glass assignment per player
- [ ] Both players submit guesses; device counter increments
- [ ] Results show correct/incorrect per glass per player
- [ ] Disconnect mid-guess: player marked disconnected, partial data preserved
- [ ] Reconnect with same name reclaims slot
- [ ] Glass→bottle mapping never sent before results phase

**Estimated scope:** ~600–800 lines new code across game.cpp, wifi_portal.cpp, and HTML. Largest session of the three due to infrastructure buildout.

---

### Session B: Multiplayer Random

**Goal:** Add the second sub-mode, leveraging the player registry and lobby from Session A.

**Changes by file:**

| File | Change |
|------|--------|
| `game.cpp` | Multiplayer Random pour flow (reuses existing `selectRandomGlass()` + browse), glass-select-per-player tracking, waiting screen, green/red banner results screen |
| `game.h` | Glass-per-player tracking field in `H2HPlayer`, new state getters for random mode results |
| `wifi_portal.cpp` | `h2h_guess` handler for random mode (bottle index), glass-claim handler, results builder for random mode |
| Portal HTML/JS | Glass select UI ("Which glass did you take?"), bottle guess list, results with correct/incorrect |

**Testing checklist:**
- [ ] 3 players join; glass count set to 3
- [ ] Standard pour sequence works (browse per glass)
- [ ] Each player selects their glass (no duplicates allowed)
- [ ] Each player guesses a bottle from the full list
- [ ] Device results: green banner shows correct, red banner shows incorrect
- [ ] Pour-number-only entries show "Player X" not "Pour #Y" in results
- [ ] 2 players and 4 players also work correctly

**Estimated scope:** ~300–400 lines. Much of the infrastructure exists from Session A.

---

### Session C: Premium Pour

**Goal:** Add the third sub-mode — simplest guess mechanic (yes/no).

**Changes by file:**

| File | Change |
|------|--------|
| `game.cpp` | Premium pour flow (premium first, rest standard), premium bottle tracking, yes/no results scoring |
| `game.h` | Premium bottle index field |
| `wifi_portal.cpp` | `h2h_guess` handler for premium mode (boolean), results builder |
| Portal HTML/JS | "Did you get the premium pour?" yes/no UI, results reveal |

**Testing checklist:**
- [ ] Premium bottle entered first, labeled distinctly on device
- [ ] Premium bottle poured first into random glass
- [ ] Remaining bottles follow standard entry/pour
- [ ] Phone shows yes/no with premium bottle name
- [ ] Results correctly identify who had the premium and who guessed right
- [ ] Works with 2, 3, and 4 players

**Estimated scope:** ~200–300 lines. Simplest session — reuses all infrastructure.

---

## Design Decisions

| # | Decision | Resolution |
|---|----------|------------|
| 1 | Max players | **4** (matches glass count; comfortable for RAM and WebSocket library) |
| 2 | Player-to-glass mapping (Random/Premium) | Player self-selects glass on phone before guessing |
| 3 | 2x2 glass assignment | Automatic: system assigns one glass of each bottle to each player, randomized |
| 4 | Duplicate glass claims | Prevent: if Player 1 claims Glass 2, it's grayed out for Player 2 |
| 5 | Mid-game disconnect | Player marked "(disconnected)"; submitted guesses count; reconnect by same name reclaims slot |
| 6 | Lobby start requirement | 2x2: exactly 2 players. Random/Premium: player count must match glass count |
| 7 | Menu structure | "Head to Head" in main menu → sub-mode selector (2x2 / Multiplayer Random / Premium Pour) |
| 8 | Results on device vs. phone | Both: device shows all players' results; each phone shows that player's individual result |

## RAM & Robustness Notes

- Player registry: ~80 bytes/player × 4 = ~320 bytes. Negligible.
- WebSocket library (links2004) supports up to 5 concurrent connections by default (configurable). 4 players + possible reconnect overlap = safe.
- Per-player targeted messages use `wsServer.sendTXT(clientNum, ...)` — already in use for favorites.
- Broadcast changes happen on state transitions (event-driven), not polling. The 150ms poll in `wifiPortalUpdate()` catches any missed transitions.

## Deferred

- Timeout for guess submission (configurable timer — can add later as a settings option)
- Persistent player profiles / running score across multiple flights
- Spectator mode (phone watches without playing)
- Phone-initiated H2H game start (device-only for v1)
