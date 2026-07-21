# Blind Flight — Phone-Only & Dual-Track Architecture

> Working spec for removing the hardware interface as a product variant, while preserving full phone functionality on the premium (screen) build.

---

## Background & Motivation

The current Blind Flight device uses an ESP32 as a WiFi AP, serving a phone UI over WebSocket while also driving a local TFT screen with rotary encoder and buttons. This spec defines how to split the firmware into two build targets: a **premium build** (screen + phone) and a **headless build** (phone-only), maintained from a single codebase.

### Why Consider This

- Removes the angled control panel and housing complexity from manufacturing
- Reduces BOM cost (TFT, encoder, buttons, connectors, SPI wiring)
- Simplifies the enclosure design significantly
- Phone UI can be richer than a 240×280 TFT allows

### The iPhone Problem (Solved)

iPhones drop AP connections that lack internet access. The existing STA-mode feature solves this: the device connects to the user's home WiFi and is reachable at `http://flight.local` via mDNS. Both phone and ESP32 are on a real network — no disconnection.

**Flow:** Power on (AP mode) → Phone connects to AP → Enter home WiFi creds → Device joins home network (STA) → Phone reconnects via flight.local

---

## System Architecture: Phone-Only Variant

### What Gets Removed (Hardware)

| Component | GPIOs Freed | Notes |
|-----------|-------------|-------|
| ST7789 TFT display | 4, 5, 16, 17, 18, 23 | Entire SPI bus freed |
| Rotary encoder | 32, 33, 34 | Plus external pull-up resistors |
| Soft buttons (L/R) | 12, 14 | |

### What Gets Removed (Firmware)

- `ui.cpp`, `input.cpp`, `transitions.cpp`, `splash.cpp`
- `screens.cpp`, `palate_training.cpp` (TFT menu dispatch)
- All `screen_*.cpp` files (settings, calibration, motor test, OTA, diagnostics, favorites, wifi setup/confirm/QR)
- Image asset headers (`img_logo_full.h`, `img_logo_text.h`, `img_plane_25.h`) — ~265 KB source
- TFT_eSPI library dependency (~1.6 MB in object form)

### What Stays

- Motor, Hall sensor, buzzer, battery ADC — unchanged
- WiFi AP/STA, WebSocket server, HTTP server, mDNS, captive portal
- All game logic (`game.cpp`), all modes, all state machines
- NVS settings persistence, whiskey library, favorites
- OTA update capability
- Diagnostics counters (`diagnostics.cpp` — NVS-based, no display dependency)
- Device ID / serial (`device_id.cpp`)

### New Hardware Needed

**One addressable RGB LED (WS2812B)** — single GPIO, provides boot/ready/error/pouring feedback before the phone connects. Without any visual indicator, the device is a silent black box. The buzzer provides audio feedback but isn't sufficient alone.

Optional: a single tactile button for power-on or emergency stop. Not strictly required if power switch = on.

---

## Split Architecture: Two Builds, One Codebase

### Build Configuration

A single `platformio.ini` with two environments, distinguished by a compile-time flag:

- `[env:esp32-screen]` — existing build, unchanged (premium product)
- `[env:esp32-headless]` — adds `-DHEADLESS_BUILD=1`, excludes screen files

### Code Sharing Breakdown

| Module | Build | Notes |
|--------|-------|-------|
| `game.cpp` / `game.h` | **Shared** | All game logic, all modes, state machine |
| `motor.cpp` | **Shared** | Motor control, homing, pour cycle |
| `wifi_portal.cpp` | **Shared** | WebSocket, HTTP, embedded HTML phone app |
| `audio.cpp` | **Shared** | Buzzer patterns |
| `settings.cpp` | **Shared** | NVS persistence |
| `persist.cpp` | **Shared** | Additional NVS persistence |
| `categories.cpp`, `browse.cpp` | **Shared** | Whiskey library |
| `h2h.cpp` | **Shared** | Head-to-Head multiplayer |
| `favorites.cpp` | **Shared** | Favorites list |
| `ota.cpp` | **Shared** | OTA updates |
| `battery.cpp` | **Shared** | Battery monitoring |
| `diagnostics.cpp` | **Shared** | Usage counters (NVS-based, no TFT) |
| `device_id.cpp` | **Shared** | MAC-based device serial |
| `main.cpp` | **Shared** | Entry point — `#ifdef` guards for screen init |
| `ui.cpp`, `input.cpp`, `transitions.cpp` | **Screen only** | Display rendering, physical input handling |
| `screens.cpp`, `palate_training.cpp` | **Screen only** | TFT menu dispatch |
| All `screen_*.cpp` | **Screen only** | Per-screen draw/input handlers |
| `splash.cpp`, `img_*.h` | **Screen only** | Image assets, boot animation |
| LED status driver (new) | **Headless only** | WS2812B state indicator |

### Maintenance Cost Per Feature

When adding a new game mode or feature:

1. Game logic (state machine, motor, scoring) — **written once**
2. WebSocket state broadcast (`buildStateJSON`) — **written once**
3. Phone HTML UI — **written once, served by both builds**
4. TFT screen rendering — **screen build only (~20% of work)**

**Assessment:** ~80% of feature work is shared code. The phone UI is identical across both builds. Only TFT screen rendering is exclusive to the premium build.

---

## Phone + Screen Simultaneously (Premium Build)

### Already Working

The phone and hardware interface already operate simultaneously. Phone actions inject into the same input queue via `inputInjectEvent()`. A button tap on the phone is indistinguishable from a physical button press. The screen redraws in response; the phone gets the updated state broadcast. No conflicts exist.

### The Premium Build Gets Everything

- Full expanded phone UI (same as headless version)
- Hardware interface as bonus/fallback (works without phone connection)
- Both inputs drive the same state machine — user can switch freely

### Known Limitation

Blocking motor spins freeze WebSocket (phones temporarily disconnect during a spin). This affects both builds equally and is a known, documented issue. Solvable in future with async motor control.

---

## Flash & OTA Recovery

### Current State

| Metric | Value |
|--------|-------|
| ESP32 total flash | 4 MB |
| Current partition (`default.csv`) | 1.31 MB × 2 OTA slots + 1.38 MB SPIFFS |
| Current firmware.bin | 1.23 MB (1,290,640 bytes) |
| Headroom per OTA slot | ~80 KB |
| Current embedded HTML | ~22 KB (minified, uncompressed in PROGMEM) |
| Gzip serving | Not implemented yet |

### The Problem

The current partition layout supports dual-OTA (automatic rollback on failed update) but has limited headroom. An expanded phone UI won't fit without action.

### OTA Rollback (Why It Matters)

Dual-OTA means: new firmware is written to the inactive slot, marked pending, device reboots. If it doesn't confirm healthy, the bootloader automatically reverts to the previous working firmware. **The device cannot be bricked by a failed OTA update.** This is critical for non-technical users who can't USB-reflash.

> **Losing dual-OTA is not acceptable** for the premium (screen) build targeting less technical users. Any solution must preserve automatic rollback.

### Recommended Solution: Gzip + min_spiffs Partition

**Step 1:** Gzip the phone HTML at build time, serve with `Content-Encoding: gzip`.

| Phone UI Size | Raw | Gzipped in Flash |
|--------------|-----|------------------|
| Current | ~22 KB | ~5 KB |
| Expanded (settings, calibration, library browser) | ~150 KB | ~30–40 KB |
| Rich (animations, SVG, full feature parity) | ~250 KB | ~50–60 KB |

**Step 2:** Switch to `min_spiffs.csv` partition table.

| Partition | default.csv (current) | min_spiffs.csv (recommended) |
|-----------|----------------------|------------------------------|
| app0 (OTA slot 1) | 1.31 MB | **1.88 MB** |
| app1 (OTA slot 2) | 1.31 MB | **1.88 MB** |
| SPIFFS | 1.38 MB | 128 KB |
| Dual-OTA rollback | Yes | **Yes** |

**Result:** ~580 KB headroom per OTA slot. Room for TFT_eSPI + gzipped rich phone UI + all game modes + future growth, with full automatic rollback preserved. One-time reflash required to repartition.

### Flash Module Strategy

Staying with 4 MB flash for now (current stock). 8 MB ESP32-WROOM-32E modules (same footprint, drop-in replacement, ~$0.50–$1.00 delta) will be ordered with the next material run, likely for the premium build. Gives 3.14 MB per OTA slot. No PCB change needed.

---

## Functions That Need Rethinking (Headless Only)

| Function | Issue | Solution |
|----------|-------|----------|
| PIN display for WebSocket auth | Currently shown on TFT — no screen to show it | Derive from MAC (like AP password), print on device label, or drop PIN since AP proximity *is* the auth |
| Boot/ready feedback | No screen to show "Homing..." or "Ready" | WS2812B LED color states + buzzer patterns |
| Mid-flight phone disconnect | No physical controls to advance game state | Device holds state; phone reconnects and picks up. Auto-timeout could abort after extended disconnect |
| Settings/calibration | Currently screen-based UI | All moves to phone UI (settings page, motor test, calibration wizard) |
| Offline operation | Currently works without phone | Gone by definition — phone is required. Acceptable for this product tier |

---

## Phone UI: Current Game Mode Parity

The phone can currently *launch* only three of eight game modes. The remaining five can be broadcast/displayed on phone during play but cannot be started from the phone.

| Mode | Code | Phone can launch? | Phone can play? |
|------|------|-------------------|-----------------|
| Basic (Quick Flight) | `GAME_MODE_BASIC` | Yes | Yes |
| Named (Browse Library) | `GAME_MODE_NAMED` | Yes | Yes |
| Best Guess | `GAME_MODE_GUESS` | Yes | Yes |
| Ranked Flight | `GAME_MODE_RANK` | No | Partial (state broadcast exists) |
| Guess + Ranked | `GAME_MODE_GUESS_RANK` | No | Partial |
| Twin Pour (Duplicate) | `GAME_MODE_DUPLICATE` | No | Partial |
| Find the Ringer (Decoy) | `GAME_MODE_DECOY` | No | Partial |
| Head-to-Head | `GAME_MODE_H2H` | No (has own join flow) | Yes (dedicated phone protocol) |

Phase 2 must close all of these gaps for the headless build to be viable.

---

## Development Sessions

Work is broken into 5 discrete sessions, each producing a flashable, testable build. Sessions are sequential — each depends on the prior session being tested and stable.

### Session 1: Foundation (Phase 1)

1. Switch to `min_spiffs.csv` partition table (both builds)
2. Implement gzip-compressed HTML serving in `wifi_portal.cpp`
3. Add `HEADLESS_BUILD` flag and `#ifdef` guards in `game.cpp`, `main.cpp`, and any shared modules that currently `#include` screen headers
4. Create second PlatformIO environment (`[env:esp32-headless]`) with `src_filter` to exclude screen-only files
5. Verify both builds compile and existing functionality is preserved

**Test checkpoint:** Flash screen build — confirm all existing functionality works (game modes, phone connection, OTA). Flash headless build — confirm it boots, connects phone, runs a basic game without crashing.

### Session 2: Phone Game Mode Parity (Phase 2a)

1. Add phone-launchable support for Ranked Flight
2. Add phone-launchable support for Guess + Ranked
3. Add phone-launchable support for Twin Pour (Duplicate)
4. Add phone-launchable support for Find the Ringer (Decoy)
5. Phone UI for H2H game creation (currently requires screen to set up host)

**Test checkpoint:** Every game mode must be startable and playable entirely from the phone. Test on both screen and headless builds. Fix any issues before proceeding.

### Session 3: Phone Settings & Tools (Phase 2b)

1. Expand HTML app with full settings panel (pour side, brightness, glass count defaults)
2. Add calibration wizard to phone UI
3. Add motor test / diagnostics to phone UI
4. Add OTA trigger from phone UI (partially exists already)

**Test checkpoint:** All settings changes from phone persist across reboot. Calibration wizard produces correct homing offset. Motor test and diagnostics pages functional.

### Session 4: Headless Hardware & Boot (Phase 3)

1. WS2812B LED driver with state-based color patterns
2. PIN auth rework (MAC-derived or proximity-only)
3. Phone disconnect handling (state hold + reconnect pickup)
4. Boot sequence without screen (LED + buzzer for homing feedback)

**Test checkpoint:** Headless build boots with LED color feedback, homes with buzzer/LED cues, handles phone disconnect gracefully. LED wiring required for this session.

### Session 5: Polish & Parity (Phase 4)

1. Phone UI visual improvements (animations, SVG logo, transitions)
2. Confirm all game modes work identically on both builds via testing
3. Battery status on phone UI
4. Whiskey library browser on phone (currently screen-only for selection)

**Test checkpoint:** Full cross-build testing matrix — every mode, every setting, both builds.

---

## Open Decisions

- **PIN auth strategy for headless (Session 4):** MAC-derived PIN printed on label? Drop PIN entirely (AP password = auth)? Or display PIN via LED blink pattern?
- **Phone disconnect policy (Session 4):** Hold state indefinitely? Timeout and abort after N minutes? Buzzer alert?
