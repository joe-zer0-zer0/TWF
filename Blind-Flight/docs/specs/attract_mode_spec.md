# Attract Mode — Script & Shot List

**Firmware location:** `src/screen_attract.cpp` / `src/screen_attract.h`
**Entry point:** Diagnostics menu → "Attract"
**Exit:** Long-press encoder at any time
**Loop behavior:** Repeats continuously until exited
**Total loop duration:** ~2:15 (varies slightly with motor homing time)

---

## Sequence Overview

| # | Section | Duration | Motor | Overlay Caption |
|---|---------|----------|-------|-----------------|
| 1 | Splash | ~4s | — | — |
| 2 | Mode Showcase | ~22s | 20s continuous spin | NINE WAYS TO TASTE BLIND |
| 3 | Choose Your Flight | ~5s | — | CHOOSE YOUR FLIGHT |
| 4 | Browse the Library | ~7.5s | — | BROWSE THE LIBRARY |
| 5 | Name Your Own | ~6s | — | NAME YOUR OWN |
| 6 | The Blind Pour | ~17s | Homing + 2 glass spins | THE BLIND POUR |
| 7 | Rate Your Pour | ~5.5s | — | RATE YOUR POUR |
| 8 | Guess the Glass | ~6.5s | — | GUESS THE GLASS |
| 9 | The Reveal | ~14s | Theatrical final spin | THE REVEAL |
| 10 | Head to Head | ~7.5s | — | HEAD TO HEAD |
| 11 | Flight Club | ~6.5s | — | FLIGHT CLUB |
| 12 | Connect Your Phone | ~6.5s | — | CONNECT YOUR PHONE |
| 13 | Your Settings | ~10s | — | YOUR SETTINGS |
| 14 | Always Current | ~6s | — | ALWAYS CURRENT |
| 15 | Built for the Session | ~8.5s | — | BUILT FOR THE SESSION |
| 16 | Closing | 3s | — | — |

---

## Detailed Script

Each section below shows what appears on screen, what the motor does, and
suggested video overlay/caption text. Title cards (accent-colored text on black,
with a selection tone) appear for 1.5s at the start of each section.

---

### 1. Splash (~4s)

**Screen:** The real splash animation plays — paper plane Bézier flight path
with logo fade-in.

**Motor:** None.

**Audio:** Splash animation tones.

**Shot notes:** Clean cold-open. Fills the screen edge-to-edge. Good for
establishing shot of the device.

---

### 2. Mode Showcase — "NINE WAYS TO TASTE BLIND" (~22s)

**Title card:** "NINE WAYS TO / TASTE BLIND"

**Screen:** Full-screen mode cards cycle every ~2.2s, each showing:
- Mode number ("1 of 9" ... "9 of 9") in dim text
- Mode name in accent color
- One-line description in white
- Decorative divider

**Modes shown (in order):**
1. QUICK FLIGHT — Numbered blind tasting
2. FULL FLIGHT — Named whiskey selection
3. BEST GUESS — Match names to glasses
4. RANKED FLIGHT — Order your favorites
5. GUESS + RANKED — The ultimate challenge
6. TWIN POUR — Find the matching pair
7. FIND THE RINGER — Spot the odd one out
8. HEAD TO HEAD — Competitive multiplayer
9. FLIGHT CLUB — Group tasting events

**Motor:** 20-second continuous clockwise rotation at 800 steps/sec (theatrical
speed, ~0.5 rev/s). Soft acceleration ramp over 400ms at start, soft deceleration
over 300ms at end.

**Audio:** Click tone on each mode transition.

**Shot notes:** This is the hero shot — disc spinning smoothly while modes cycle
on screen. Good for wide shots showing both device and display, or tight shots
alternating between screen and spinning disc. Motor runs long enough for multiple
full rotations and B-roll coverage.

---

### 3. Choose Your Flight (~5s)

**Title card:** "CHOOSE YOUR / FLIGHT"

**Screen sequence:**
1. Home screen — 4-glass grid (all empty), "Ready" hint, SETTINGS / START buttons (1.5s)
2. Mode selection menu — Quick Flight, Full Flight (highlighted), Ranked Flight,
   Palate Training, Head to Head, Flight Club (2s)

**Motor:** None.

**Audio:** Selection tone on menu.

**Shot notes:** Shows the main navigation flow. Quick cut between home and menu.

---

### 4. Browse the Library — "BROWSE THE LIBRARY" (~7.5s)

**Title card:** "BROWSE THE / LIBRARY"

**Screen sequence:**
1. Type selection — Bourbon (highlighted), Rye, Scotch, Irish, Japanese (2s)
2. Distillery list — Buffalo Trace (highlighted), Heaven Hill, Maker's Mark,
   Wild Turkey, Woodford (2s)
3. Product list — Eagle Rare (highlighted), Blanton's, E.H. Taylor, Stagg Jr (2s)

**Motor:** None.

**Audio:** Click/select tones on each drill-down.

**Shot notes:** Shows the three-level library hierarchy. Each screen is a menu
with highlight advancing through the drill-down.

---

### 5. Name Your Own — "NAME YOUR OWN" (~6s)

**Title card:** "NAME YOUR OWN"

**Screen:** Simulated text entry typing "WOODFORD" one character at a time on the
real TextEntry grid layout. Shows cursor advancing, character grid with current
letter highlighted, and CANCEL / DONE soft buttons. Each character appears at
~350ms intervals, with the final "DONE" state held for 600ms.

**Motor:** None.

**Audio:** Click tone per character, confirm tone at completion.

**Shot notes:** Demonstrates manual text entry. Good close-up shot of the
character grid and encoder interaction concept. 8 characters typed = "WOODFORD".

---

### 6. The Blind Pour — "THE BLIND POUR" (~17s)

**Title card:** "THE BLIND POUR"

**Screen sequence:**
1. "Homing" / "Aligning disc..." — brief while motor finds home position
2. "SPINNING" / "Finding glass..." — during motor travel to glass 1
3. "READY TO POUR" — Glass 1 of 2, "Eagle Rare", "Pour and press DONE" (2.5s)
4. "SPINNING" / "Finding glass..." — during motor travel to glass 2
5. "READY TO POUR" — Glass 2 of 2, "Woodford", "Pour and press DONE" (2.5s)

**Motor:** Real motor movement throughout.
- Homing sequence (disc finds Hall sensor reference)
- Spin to glass 1 (Normal speed, 1600 sps)
- Spin to glass 2 (Normal speed, 1600 sps)

Phone state broadcasts occur before/after spins (phone shows spinning state
if connected).

**Audio:** Home-found tone after homing, arrive tone after each spin, confirm
tone after each "pour".

**Shot notes:** The money shot for demonstrating the core product function.
Multiple camera angles recommended: overhead showing disc rotation and glass
positioning, front showing display, side profile showing the full device.
Two glasses is enough to demonstrate the concept without dragging.

---

### 7. Rate Your Pour — "RATE YOUR POUR" (~5.5s)

**Title card:** "RATE YOUR POUR"

**Screen sequence:**
1. Glass 1: "Eagle Rare" — stars fill in one at a time (1★ → 2★ → 3★ → 4★),
   with click tone per star (1.2s + 0.4s hold)
2. Glass 2: "Woodford" — stars fill in (1★ → 2★ → 3★ → 4★ → 5★),
   with click tone per star (1.5s + 0.4s hold)

**Motor:** None.

**Audio:** Click tone per star, confirm tone after each glass.

**Shot notes:** Quick section showing the rating mechanic. Stars animate in
sequence for visual interest.

---

### 8. Guess the Glass — "GUESS THE GLASS" (~6.5s)

**Title card:** "GUESS THE GLASS"

**Screen sequence:**
1. Glass 1 — "WHICH WHISKEY?" with pool of two names; "Eagle Rare" highlighted
   as the guess (1.5s)
2. Glass 2 — same layout; "Woodford" highlighted (1.5s)
3. Results — "2 of 2" in large text, "Correct!" — perfect score (2s)

**Motor:** None.

**Audio:** Select tones on guesses, confirm tone on results.

**Shot notes:** Demonstrates the guessing mechanic. Results screen is satisfying
and shows the payoff.

---

### 9. The Reveal — "THE REVEAL" (~14s)

**Title card:** "THE REVEAL"

**Screen sequence:**
1. "FINAL SPIN" / "Revealing..." — during theatrical motor spin
2. Glass 1 reveal — slot-machine text effect: "Blanton's" → "Stagg Jr" →
   "E.H. Taylor" → **"Eagle Rare"** (final, in accent color), with metadata
   ("90 proof | $$") and 4-star rating (2.5s)
3. Glass 2 reveal — same effect → **"Woodford"** with metadata
   ("90.4 proof | $$") and 5-star rating (2.5s)
4. Flight Complete — final summary card showing both whiskeys with their
   ratings, EXIT / NEW buttons (2.5s)

**Motor:** Theatrical spin — 3+ full revolutions with random extra (blocking
`motorSpinSteps`). Creates suspense before the name reveal.

**Audio:** Arrive tone after spin, home-found tone on each glass reveal,
confirm tone on flight complete.

**Shot notes:** High-energy section. The slot-machine text scramble is a visual
highlight. Good for quick cuts synced to the name landing. The final summary
card is a natural screenshot/thumbnail candidate.

---

### 10. Head to Head — "HEAD TO HEAD" (~7.5s)

**Title card:** "HEAD TO HEAD"

**Screen sequence:**
1. Sub-mode menu — "2 x 2", "Random" (highlighted), "Premium" (2s)
2. Lobby — players join one at a time:
   - Jeremy joins (1.2s)
   - Jeremy + Sarah (1.2s)
   - Jeremy + Sarah + Mike — START button appears (1.2s)

**Motor:** None.

**Audio:** Select tone on mode, click tone per player joining, confirm tone
when lobby is full.

**Shot notes:** Shows multiplayer capability. Players appearing one-by-one
gives a sense of real-time interaction. Green dots indicate connected status.

---

### 11. Flight Club — "FLIGHT CLUB" (~6.5s)

**Title card:** "FLIGHT CLUB"

**Screen sequence:**
1. Roster — four names listed (Jeremy, Sarah, Mike, Alex) with numbered
   positions (2.5s)
2. Round indicator — "Round 1 of 4", "Jeremy" in large accent text,
   "Step up to the device" hint, POUR button (2.5s)

**Motor:** None.

**Audio:** Select tone on roster, confirm tone on round start.

**Shot notes:** Shows group tasting event mode. The round indicator with
the player callout demonstrates the turn-based flow.

---

### 12. Connect Your Phone — "CONNECT YOUR PHONE" (~6.5s)

**Title card:** "CONNECT YOUR / PHONE"

**Screen sequence:**
1. QR code screen — simulated QR pattern with "BlindFlight" network name
   and "PIN: 4829" (2.5s)
2. Wi-Fi connected — "Local Network" / "MyHomeWiFi" / "http://flight.local",
   "All phones on same network" hint (2.5s)

**Motor:** None.

**Audio:** Confirm tone on connected screen.

**Shot notes:** Demonstrates both connectivity modes: direct AP (QR scan) and
local network (mDNS). Good place to cut to a phone screen showing the web
interface if filming B-roll.

---

### 13. Your Settings — "YOUR SETTINGS" (~10s)

**Title card:** "YOUR SETTINGS"

**Screen:** Settings menu with four items, each cycling through its values:
1. **Pour Side** — Front → Right → Rear → Left (4 values × 600ms)
2. **Spin Speed** — Fast → Normal → Slow (3 values × 600ms)
3. **Sound** — three volume levels (3 × 600ms)
4. **Brightness** — three brightness levels (3 × 600ms)

Each setting is highlighted in turn, with its value changing on the right
side of the row while the other settings show their defaults.

**Motor:** None.

**Audio:** Click tone on each value change.

**Shot notes:** Quick-fire settings showcase. Values animate in place, giving
a sense of the configuration depth without dwelling. Pour Side cycling through
all four positions is a feature highlight.

---

### 14. Always Current — "ALWAYS CURRENT" (~6s)

**Title card:** "ALWAYS CURRENT"

**Screen sequence:**
1. "FIRMWARE UPDATE" / "Checking..." / "Fetching manifest" (2s)
2. "UP TO DATE" — current firmware version in large accent text,
   "No update available", "Updates delivered over Wi-Fi" hint (2.5s)

**Motor:** None.

**Audio:** Confirm tone on up-to-date screen.

**Shot notes:** Demonstrates OTA update capability. Shows the real firmware
version number. Brief and professional — communicates the feature without
over-explaining.

---

### 15. Built for the Session — "BUILT FOR THE SESSION" (~8.5s)

**Title card:** "BUILT FOR THE / SESSION"

**Screen sequence:**
1. Battery normal — simulated 72% / 7.82V reading with green indicator,
   "Rechargeable Li-ion pack" / "USB-C charging" hints (2.5s)
2. **Low battery warning** — simulated 18% / 6.95V with amber indicator and
   warning title bar, "Charge before pouring" hint (2s)
3. Session recovery — "RESUME FLIGHT?" / "Named Flight" / "2 of 4 poured" /
   "Power was lost mid-flight", DISCARD / RESUME buttons (2.5s)

**Battery readings are simulated** (hardcoded values) so they stay consistent
across video takes regardless of actual charge state.

**Motor:** None.

**Audio:** Error tone on low battery warning, select tone on recovery prompt.

**Shot notes:** Three distinct sub-stories in quick succession: healthy
operation, graceful degradation, and session resilience. The low battery
warning is a good visual beat (amber color shift).

---

### 16. Closing (3s)

**Screen:** "BLIND FLIGHT" centered in accent color on black. Simple logo hold.

**Audio:** Confirm tone.

**Shot notes:** Clean end card. Loop restarts from Splash after this.

---

## Implementation Notes

### Motor movement summary
- **Section 2:** 20s non-blocking continuous spin (direct GPIO stepping at 800 sps
  with soft ramp). Position tracking is lost — disc re-homes in section 6.
- **Section 6:** Blocking `motorHome()`, then two blocking `motorSpinToGlass()` calls
  at Normal speed (1600 sps).
- **Section 9:** Blocking `motorSpinSteps()` — 3+ revolutions with random extra for
  theatrical effect.

### Phone integration
- All phone input commands are silently dropped during attract mode
  (`attractIsRunning()` guard in `handleWSAction()`).
- WebSocket connection stays alive — `wifiPortalService()` is called throughout.
- State broadcasts (`wifiPortalBroadcastNow()`) fire before/after motor spins so a
  connected phone shows the spinning state.
- For live event display: a phone placed next to the device will show real-time
  state changes (spinning indicator, mode info) without accepting input.

### Build compatibility
- `screen_attract.cpp` is automatically excluded from the headless build by the
  existing `-<screen_*.cpp>` filter in `platformio.ini`.
- The `wifi_portal.cpp` include and guard are wrapped in `#ifndef HEADLESS_BUILD`.

### Exit behavior
- Long-press on the encoder exits immediately from any point in the sequence.
- Motor is disabled on exit.
- Screen pops back to the Diagnostics menu with a fade transition.

### Video production tips
- Run the device on wall power (USB-C) during filming to avoid actual battery drain.
- Battery readings are simulated, so multiple takes will look identical.
- Audio tones are included for live event use; mute the device or edit in post for
  the promotional video.
- The 20s continuous spin (section 2) is the longest single shot — plan camera
  movement around it.
- Title cards (1.5s each) provide natural edit points between sections.
- For phone B-roll: connect a phone to the device's AP during the pour sequence
  (section 6) to capture the spinning state on the phone screen.
