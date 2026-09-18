# Session Spec — On-Screen Wi-Fi Setup

## Overview

Let the device join a Wi-Fi network using only its own screen, encoder and soft
buttons, with no phone involved. Today the only setup path is: phone joins the
`BlindFlight` access point → enters the PIN → phone scans → phone sends
`wifi_connect` → device asks for confirmation on screen. With no phone at hand, or
with a phone that won't cooperate, a fresh unit cannot get online, and since OTA
is the only firmware path, a unit that can't get online can't be updated.

This came up on the large-motor prototype (2026-09-17). The phone scan returned
nothing; the unit only got online through manual network-name entry, added to the
phone page in v1.9.2.

Two phases, each ending in its own release:

- **Phase 0 — Scan diagnosis (v1.9.3).** The scan still returns 0 networks in setup
  mode. The picker is pointless until the scan works, so this comes first.
- **Phase 1 — On-screen picker (v1.10.0).** Scan list, manual network name, a
  password keyboard with lowercase and symbols, and connecting with a result screen.

Screen build only. The headless build has no screen, so it keeps the phone path
(it only needs to keep compiling against the changed `TextEntry` API).

---

## Current state (as of v1.9.2)

| Piece | Where | Notes |
|---|---|---|
| Scan | `wifi_portal.cpp` `handleWifiScan` / `pollWifiScan` / `publishWifiScanResults` | Async, retried up to 3×. Results are serialized straight to a WebSocket broadcast and then deleted, so nothing else can read them. |
| Connect | `handleWifiConnect()` | Blocking, up to `WIFI_STA_TIMEOUT` (10 s). **Saves credentials before trying**, so a typo overwrites a previously working network. |
| Confirm | `screen_wifi_confirm.cpp` | Exists only so a remote phone can't move the device to another network. Not needed when the request comes from the device itself. |
| Wi-Fi Setup screen | `screen_wifi_setup.cpp` | No saved network: static "use your phone" text, only Back works. Saved network: shows info + Forget / Reconnect. |
| Text entry | `ui.cpp` `TextEntry` | A–Z, 0–9, space, backspace, OK. **Max 20 chars, uppercase only.** Used by Browse (Manual mode) and Flight Club names. |

Wi-Fi passwords are case-sensitive, may contain symbols, and run up to 63
characters. SSIDs run up to 32 bytes and may also be mixed case. The existing
keyboard can enter neither, so extending it is the biggest part of Phase 1.

---

## Phase 0 — Scan diagnosis (v1.9.3)

### Symptom

Setup mode (AP+STA since v1.9.2), phone taps Scan: the result is 0 networks. The
device joined the same house network by typing its name manually, so the radio and
antenna work. The v1.9.1 log showed the scan being refused as it started
(`Scan failed` immediately); the v1.9.2 log wasn't captured.

### Hypotheses

1. **The start is still refused** (`scanNetworks` returns `-2` each attempt), so the
   AP+STA change didn't address the real cause.
2. **The library's own timeout fires.** `WiFiScanClass` sets
   `_scanTimeout = max_ms_per_chan × 20` = 6 s, and `scanComplete()` reports
   `WIFI_SCAN_FAILED` once that passes. With the AP active and a phone connected,
   the radio has to return to the AP's channel between scan channels, which can
   push a full sweep past 6 s. Every retry would then fail the same way.
3. **The scan really finds nothing in AP+STA mode** but works in STA-only mode.

### Changes

- **Log what the scan does.** `pollWifiScan()` logs the elapsed time since start
  and the raw `scanComplete()` value each time it completes or fails. Also log to
  telemetry (so it's readable at `/log` with no USB), not only to Serial.
- **Pass an explicit per-channel time.** Call
  `WiFi.scanNetworks(true, false, false, 500)`. That raises the library's timeout
  to 10 s and gives each channel longer to answer. This tests hypothesis 2 directly.
- **Add a diagnostic scan.** Add a **Wi-Fi Scan Test** item to the Diagnostics
  menu. It runs the same scan and shows on screen: attempts, elapsed ms, raw
  result, network count, and the top 5 SSIDs. It works in either mode, so it can be
  run first in STA mode (the unit's current state, reachable by OTA), then in setup
  mode after Forget Network + restart. That separates hypothesis 3 from 1 and 2.

### Acceptance

- In STA mode, the scan test lists nearby networks.
- In setup mode, the phone scan and the scan test both list nearby networks.
- If setup mode still returns 0 networks, the scan test output says which
  hypothesis is right. Fix that before starting Phase 1.

---

## Phase 1 — On-screen picker (v1.10.0)

### Design decisions

**D1 — One screen with internal states, not a stack of screens.** A new
`screen_wifi_join.cpp` holds a small state machine:

```
SCANNING → LIST → (PASSWORD | SSID_ENTRY → PASSWORD) → CONNECTING → RESULT
```

The whole flow is one push from Wi-Fi Setup and one pop at the end. That avoids
pushing and popping several screens mid-flow, which is the screen-stack corruption
CLAUDE.md warns about, and a successful join doesn't have to pop three screens at
once. Back steps to the previous internal state; Back from LIST (or from SCANNING)
pops the screen.

**D2 — One shared scan cache.** Scan results are stored in a small array in
`wifi_portal.cpp`, filled once when a scan completes:

```cpp
struct WifiNet { char ssid[33]; int8_t rssi; bool secured; };
```

The results are de-duplicated by SSID (mesh systems show one SSID per access
point; keep the strongest), empty/hidden SSIDs are dropped, the list is sorted by
signal, and capped at `WIFI_MAX_SCAN_RESULTS` (15). The phone broadcast and the new
screen both read from this cache, so there is one scan path to debug, not two. The
phone list gets de-duplicated for free.

**D3 — No confirmation screen for a join started on the device.** The Allow/Deny
screen exists because a phone on the AP is remote. Someone turning the encoder is
physically present, and choosing Connect already counts as confirmation. The phone
path keeps its confirmation screen unchanged.

**D4 — The password is shown in plain text while typing.** Entering a case-
sensitive password one character at a time with an encoder, without seeing it, is
unusable, and the person reading the screen is the person typing. It is never
written to Serial or telemetry.

**D5 — Save credentials only after a successful join.** Change
`handleWifiConnect()` to call `wifiSaveCredentials()` only once
`WL_CONNECTED` is reached. That fixes the existing phone path too: today a failed
attempt overwrites a working saved network.

**D6 — The keyboard gets pages; existing callers don't change.** See next section.

### TextEntry: full keyboard mode

A new keyboard option, `TE_KB_FULL`. The existing `uiTextEntryInit()` keeps the
current layout (`TE_KB_UPPER`) exactly, so Browse and Flight Club look and
behave the same.

The layout still uses 40 cells (10 × 4); the current keyboard uses 39. Full mode
makes space single-width and uses the freed cell for a page key:

| Row 3, columns 6–9 | Legacy (`TE_KB_UPPER`) | Full (`TE_KB_FULL`) |
|---|---|---|
| 6 | Space, spanning 2 columns | Space |
| 7 | (space) | Page key: `ab` / `#+` / `AB` |
| 8 | ← (backspace) | ← (backspace) |
| 9 | OK | OK |

Full mode has three pages, each with 36 character cells:

| Page | Contents |
|---|---|
| `ABC` | A–Z, 0–9 (same as today) |
| `abc` | a–z, 0–9 |
| `#+=` | The 32 printable ASCII symbols (listed below), plus 4 blank cells that can't be selected |

Symbols page, in order: ``! " # $ % & ' ( ) * + , - . / : ; < = > ? @ [ \ ] ^ _ ` { | } ~``

- The page key cycles `ABC → abc → #+= → ABC`. The cursor stays at the same grid
  position, so switching pages doesn't move the highlight.
  If that position is a blank cell on the symbols page, the highlight moves to the
  nearest symbol before it.
- The page key is labelled with the page it switches to.
- The right button long-press also cycles pages, as a shortcut.
- `TEXT_ENTRY_MAX_LEN` goes from 20 to 63. `maxLen` is still clamped per call, so
  existing callers keep their 20. The cost is 43 bytes of RAM per `TextEntry`
  instance (two static instances today: Browse and Flight Club).
- The text field already scrolls to show the end of long text. Keep the `n/63`
  counter.
- New entry point:
  `uiTextEntryInitEx(TextEntry*, int maxLen, const char* initial, TextEntryKeyboard kb)`.
  `uiTextEntryInit()` becomes a wrapper that passes `TE_KB_UPPER`.

### Screen flow

**Wi-Fi Setup (`screen_wifi_setup.cpp`)**

- **No saved network:** replace the static text with a menu:
  - **Find Networks**: pushes the join screen in SCANNING.
  - **Enter Name Manually**: pushes the join screen in SSID_ENTRY (for hidden
    networks, or when the scan finds nothing).
  - **Use Phone**: pushes the existing QR screen.
- **Saved network:** add **Change Network** above Forget / Reconnect. It pushes
  the join screen in SCANNING.

**Join screen states (`screen_wifi_join.cpp`)**

| State | Display | Encoder | Left | Right |
|---|---|---|---|---|
| SCANNING | "Scanning…" with an animated indicator | — | Cancel (pop) | — |
| LIST | Network list: SSID, lock icon if secured, 4-bar signal. Last two rows: **Other network…**, **Scan again** | Scroll | Back (pop) | Select |
| SSID_ENTRY | Full keyboard, title "NETWORK NAME", max 32 | Keyboard | Back (to LIST, or pop if entered from Setup) | Keyboard |
| PASSWORD | Full keyboard, title "PASSWORD", SSID shown as a hint, max 63. Skipped for open networks | Keyboard | Back | Keyboard |
| CONNECTING | "Joining <SSID>…" | — | — | — |
| RESULT | Success: SSID, IP, `flight.local`, **Done** (pop). Failure: "Couldn't join — check the password", **Retry** (to PASSWORD with the entered text kept) / **Back** (to LIST) | — | per state | per state |

- The list uses `ScrollList` for scrolling and draws its labels. The screen then
  draws the signal bars and lock icon over the visible rows (using `scrollOffset`),
  so `ScrollList` itself doesn't change.
- SSIDs longer than the row are truncated with `…`.
- An empty scan goes straight to a LIST containing only **Other network…** and
  **Scan again**, headed "No networks found".
- For "Other network…", ask for the password next. The name was typed, so the
  device doesn't know whether the network is secured. An empty password (OK
  pressed on an empty field) means an open network.

**Connecting.** The join itself is a blocking call of up to 10 s, like Reconnect
today. It must not run in `onEnter` or in the input handler. The input handler
sets `pendingJoin`, the CONNECTING frame is drawn, and the next draw pass runs
the join. This is the same deferred pattern CLAUDE.md prescribes for homing.
During the wait loop, call `ledTick()` so the status LED keeps animating. No
`wifiPortalService()` call is needed there, because the AP is being taken down.

**After success.** mDNS restarts on the new network (already done in
`handleWifiConnect`). The phone, if one was on the AP, drops off, as it does today.

### File-by-file changes

| File | Change |
|---|---|
| `wifi_portal.cpp` / `.h` | Scan cache (D2), exposed as `wifiScanStart()`, `wifiScanState()` (idle / running / done / failed), `wifiScanCount()`, `wifiScanGet(i)`. `publishWifiScanResults()` reads from the cache. Split the core of `handleWifiConnect()` into `bool wifiJoin(const char* ssid, const char* pass)`, used by both the phone and the screen. Save credentials on success only (D5). `ledTick()` in the wait loop. |
| `ui.h` / `ui.cpp` | Full keyboard mode (D6), `uiTextEntryInitEx`, `TEXT_ENTRY_MAX_LEN` 63, page key, long-press page cycle. Legacy layout unchanged. |
| `headless_stubs.cpp` | Match the new `TextEntry` API so the headless build compiles. |
| `screen_wifi_join.cpp` (new) | The join screen state machine. |
| `screen_wifi_setup.cpp` | No-saved-network menu; Change Network item. |
| `screens.h` | Declare `screenWifiJoin`. The headless build already excludes `screen_*.cpp`, so no `platformio.ini` change. |
| `screen_diagnostics.cpp` | (Phase 0) Wi-Fi Scan Test item. |

### Testing checklist (Phase 1)

Run with the unit in setup mode (Settings → Wi-Fi Setup → Forget Network, then
restart), except where noted.

1. Wi-Fi Setup shows Find Networks / Enter Name Manually / Use Phone.
2. **Find Networks:** "Scanning…" then a list within about 10 s. Your house network
   appears once (not once per mesh node), with a lock and plausible signal bars.
3. Scroll to the end: **Other network…** and **Scan again** are there. Scan again
   rescans.
4. Pick your network. The password keyboard opens. Check every page: uppercase,
   lowercase, symbols. The page key cycles, the highlight stays put, and right-
   button long-press also cycles.
5. Enter a **wrong** password → failure screen. Retry keeps the typed text.
   **Restart the device:** it must come back in setup mode, not stuck trying the
   bad password (D5).
6. Enter the right password → success screen with IP. Open `http://flight.local`
   from a phone on the house network. Settings → Update shows up to date.
7. **Change Network** (in STA mode): list appears. Back out without joining; the
   device stays on its current network.
8. **Enter Name Manually**, with a mixed-case name → joins.
9. Browse → Manual entry and Flight Club name entry look and behave exactly as
   before (uppercase, 20 characters, double-width space).
10. Phone path still works: phone scan lists the same networks (de-duplicated),
    and the device still asks Allow/Deny for a phone-initiated join.

### Out of scope

- The headless build (no screen). It keeps the phone path.
- WPA2-Enterprise / captive-portal networks (hotels, conference venues). They need
  a username or a browser login the device can't do.
- Storing more than one network.
