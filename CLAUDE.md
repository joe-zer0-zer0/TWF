# CLAUDE.md — Blind Flight

## What This Project Is

**Blind Flight** is a physical interactive blind-tasting device (whiskey/wine). A motorized rotating carousel positions four Glencairn glasses under a fixed pour spout for sequential automated pouring, with a gamified, suspenseful reveal experience. Built for use at tasting events.

Jeremy is the sole designer/developer across hardware, firmware, and enclosure. **Jeremy has no ESP32/Arduino/C++ background** — Claude writes all code **and ships it OTA** (see "Firmware Delivery — OTA Only" — USB flashing is no longer possible); Jeremy accepts the update on-device, tests on real hardware, and reports exact symptoms. Diagnose from symptom patterns. Project status: **active prototype, beta preparation.** First public test occurred; the platform stalled after two glasses due to weight/friction (see Current State).

\---

## Hardware

### Stack

* **MCU:** ESP32 DevKit V1 (Wi-Fi AP for phone interface)
* **Motor:** NEMA 17 stepper (1.8°/step, \~40 N·cm), **direct drive** — no gear reduction or belt drive allowed (design constraint)
* **Driver:** TMC2209 in standalone STEP/DIR mode (no UART)
* **Display:** ST7789 IPS TFT, 240×280, SPI (TFT\_eSPI)
* **Input:** KY-040 rotary encoder + two soft buttons (context-labeled by display)
* **Homing:** Hall effect sensor + neodymium magnet embedded in disc
* **Audio:** MH-FMD passive buzzer module — **inverted logic: LOW = on**
* **Power:** 2S Li-ion pack (7.4V nominal), TP4056 2S BMS w/ USB-C, buck converter → 5V to ESP32 VIN, ESP32 onboard regulator → 3.3V rail. Voltage divider on **GPIO 36** for battery monitoring.
* **Assembly:** Breakout boards on perfboard with **star ground topology**. Custom PCB deliberately deferred until design stabilizes.

### Pin Assignments

|Function|GPIO|Notes|
|-|-|-|
|Display MOSI / SCLK / CS / DC / RST / BLK|23 / 18 / 5 / 16 / 17 / 4|BLK is PWM-dimmable|
|Motor STEP / DIR / EN|25 / 26 / 27|EN active LOW|
|Encoder CLK / DT / SW|32 / 33 / 34|SW is input-only, external 10kΩ pull-up|
|Left / Right button|14 / 12|Internal pull-ups, active LOW|
|Hall sensor|35|Input-only, external 10kΩ pull-up|
|Buzzer|13|PWM tones; MH-FMD inverted (LOW = on)|
|Battery ADC|36|ADC1 — must use `analogReadMilliVolts()`|

### Key Geometry \& Motion Constants

* **1600 microsteps/rev** — TMC2209 with MS1/MS2 unconnected = **8× microstepping** (not 16×). 256-step interpolation keeps motion smooth.
* **400 microsteps/glass** (90° between the four positions)
* **`POUR\_OFFSET` = 1000 microsteps** (135° CCW from Hall trigger to pour position)
* `MOTOR\_CW\_DIR` / `MOTOR\_CCW\_DIR` defined in `config.h`
* **`MOTOR\_MIN\_SPEED` ≥ 400 steps/sec** — starting slower hits the low-speed resonance zone and causes stall/vibration
* Glasses sit at 70mm radius on a rotating disc; pour spout fixed front-center; square lid rotates in 90° increments
* Enclosure: wooden box, angled trapezoidal front control panel (\~15° tilt)

\---

## Firmware

* **Environment:** PlatformIO + VS Code, Arduino framework, C++
* **Libraries:** TFT\_eSPI (ST7789), links2004/WebSockets
* **Architecture:** Modular multi-file. Key modules: `motor`, `audio`, `input`, `ui`, `game`, `settings`, `wifi\_portal`, `browse`/`categories`, `transitions`, `splash`, plus per-screen files (`screen\_settings.cpp`, `screen\_motor\_test.cpp`, etc.)
* **Phone interface:** WebSocket server on port 81; UI embedded in PROGMEM. Wi-Fi AP with captive portal; mDNS fallback at `http://flight.local`
* **Asset pipeline:** `convert\_assets.py` converts PNG artwork → RGB565 C header arrays and extracts Bézier waypoints from SVG path data. Logo/sprites designed in Inkscape.
* **Core game flow:** STARTUP (home disc) → IDLE → SELECTING (random unvisited glass) → SPINNING → POURING → loop until 4 glasses → TASTING → REVEAL
* **Modes:** Basic (numbered), Browse (category → product list), Manual (encoder text entry), Best Guess, Ranked Flight

\---

## Current State

* **Released firmware: v1.6.1.** Master is clean and both manifests match their published assets.
* **The headless build is now actually playable (v1.6.0).** Through v1.5.4 it compiled and booted but no flight could start or advance: `ui.cpp` is excluded by `build_src_filter` and the stub `uiUpdate()` drained nothing, so `gameDraw()` (the deferred phone-action pump) and `gameInput()` (which the phone's Done/Reveal/Back buttons reach via `inputInjectEvent`) never ran. `headless_stubs.cpp` now keeps a real screen stack and dispatches input and draw. **Head-to-Head is still broken headless** — `h2hInput()` and `screenH2H` are inside the screen-only block and there is no phone action to confirm an H2H pour, so a game reaches the first pour and stops. Solo modes are fine.
* **Mechanical (in progress, Jeremy's bench):** PTFE furniture pads confirmed working in initial testing. Carrier plate (attaches to top of motor) and set-screw placement/materials currently in physical prototyping. Ball transfer units at 120° spacing remain the fallback if needed. Final battery/charging part selection also open.
* **Alignment baseline — read before planning motor work.** The Session 9 auto-diag capture archived at `docs/baselines/selftest_baseline_2026-07-28_fw1.5.2.log` came back **inside the roadmap's goal thresholds**: interGlassSpread=3, worstScatter=3, accumMax=1, 24 reads / 0 failed. Per-position means were `+1, +1, -2, 0` — **glass 4 was not the worst**, contradicting the impression that drove the alignment roadmap. The run is **unloaded** (the auto-diag requires glasses off — many revolutions at speed), so the residual error users could identify at the event is most likely load-dependent, pointing at the mechanical work above rather than at firmware.
* **Pour-side selection** — fully implemented. Settings menu item cycles Front/Right/Rear/Left, NVS-persistent, runtime offset applied to all motor glass positioning.
* **Shipped game modes:** Basic, Named, Best Guess, Ranked, Guess+Rank, Twin Pour (`GAME\_MODE\_DUPLICATE`), Find the Ringer (`GAME\_MODE\_DECOY`), Head-to-Head (all three sub-modes). Also shipped: favorites list, library metadata (proof / price tier / age) + star-rating round, STA mode + mDNS, OTA, telemetry `/log`, auto self-test, battery indicator with low-battery warning and lockout, NVS-persistent home-offset calibration.

## Roadmap

**Open work — none of it blocked by the mechanical prototyping except where noted.**

1. **Alignment Session 3** — CW-only motion + homing hardening (target v1.6.0). Spec: `docs/specs/alignment_recovery_roadmap.md`. **Timing-sensitive:** its acceptance test compares against the archived baseline, which was captured on the *current* shaft coupling. Changing the set screw first invalidates that comparison — either run Session 3 before the mechanical change lands, or budget a re-baseline after it.
2. **Shareable results card** — spec in `docs/specs/share_card_spec.md`, Phase 1 (rank-aware payload + poster-styled phone results view) not started. Phone UI + `wifi\_portal.cpp` only; zero hardware dependency.
3. **Pour animation** (procedural: glass outline drawn in code, filled with amber rectangle — no flash cost)
4. **Session 8 — motor task migration.** Deferred, but elevated for the headless build: a blocking motor loop is indistinguishable from a hung device with no screen, and it is the one failure mode the OTA rollback net cannot catch (it catches crashes, not hangs).
5. **Headless status LED** (WS2812B) — the one unbuilt piece of `docs/specs/phone_only_architecture.md`; the two-environment build split itself is done.
6. Whiskey library expansion
7. Validate pour-side selection during beta
8. Deferred Session 14 polish items
9. Web animation of logo SVG (standalone; CSS `stroke-dashoffset` + `offset-path`; independent of firmware)

**Battery/charging note:** nothing firmware-side is blocked by the pack decision, but do not finalize the warn/lockout percentages until the pack is chosen — the current pack sagged 8003 → 6958 mV across a single 2-minute diag run. The 2S pack still charges on series total with no per-cell balancing; that remains an open safety item before beta events.

\---

## Known Gotchas \& Hard-Won Lessons

**Electrical**

* **ADC2/Wi-Fi conflict:** GPIO 0 and other ADC2 pins conflict with Wi-Fi. All ADC reads and `randomSeed()` sources must use ADC1 pins (e.g., GPIO 36).
* **Battery ADC:** use `analogReadMilliVolts()` for factory calibration; call `analogSetPinAttenuation(PIN\_BATT\_ADC, ADC\_11db)` in `settingsInit()`; optional 100nF filter cap at GPIO 36.
* **Grounding:** star ground topology; motor high-current ground path stays separate until the star point. TMC2209 has separate logic GND and VM GND — both must reach common ground. Shared common ground across all rails (7.4V/5V/3.3V) is mandatory for STEP/DIR signal reference.
* **Buzzer is inverted** (MH-FMD): LOW = sound on.

**Firmware**

* **Blocking motor loops freeze WebSocket/HTTP/DNS** — the step loops themselves are still unserviced (Session 8 owns that). As of v1.5.2 the *surrounding* blocking waits — `delayWithAudio`, the homing wait loops, the battery-lockout modal — call `wifiPortalService()`, so a phone survives the pauses between spins. **Call `wifiPortalService()`, never `wifiPortalUpdate()`, from inside a blocking loop:** `wifiPortalUpdate()` pushes and pops screens for the pending-`wifi_connect` flow, and doing that mid-operation corrupts the screen stack (see next bullet). Never call either from inside a step loop — it takes milliseconds and would drop steps.
* **JSON builders in `wifi_portal.cpp`:** use the `jsonAppend` / `jsonAppendChar` / `jsonAppendEscaped` helpers, not raw `snprintf(buf + pos, sizeof(buf) - pos, ...)`. `sizeof()` is `size_t`, so that subtraction underflows to ~4 billion once `pos` passes the buffer length and writes off the end of a stack buffer (fixed in v1.5.2). Anything phone-supplied — player names, SSIDs, favorites — must go through `jsonAppendEscaped`. (`buildStateJSON` uses its own equivalent `JSON_PUT`/`JSON_REM` macros and is already safe.)
* **Screen state machine:** never nest `uiPushScreenT`/`uiPopScreenT` calls during transitions (corrupts state) — this includes calling them from `onEnter`. Established pattern: deferred flags (`pendingBrowse`, `awaitingBrowseReturn`, `phoneNameReady`, `pendingStart`/`pendingExit`) push actions out of `onEnter` into the next draw loop. Blocking work that draws its own full-screen UI (homing) belongs there too, not in `onEnter`.
* **`config.h` truncation** was a recurring failure mode with file downloads — verify the file ends at the expected final `#define`. (Less relevant in Claude Code, but keep the habit of sanity-checking file integrity after large edits.)
* Microstepping: firmware constants assume **8×** (1600/rev). If motion lands wrong (e.g., alternating between two positions), suspect a wrong microstepping constant.

**Hardware-spec doc caveat:** the original `blind\_flight\_hardware\_spec.md` predates several discoveries. Where it conflicts with this file (microstepping mode, buzzer logic, battery ADC), **this file is authoritative.**

\---

## Firmware Delivery — OTA Only (MANDATORY)

**As of 2026-07-25, USB flashing is no longer possible.** The ESP32's USB port is
inaccessible with the device assembled. **OTA is the only way firmware reaches the
hardware.** A build that compiles locally but is never released is a build Jeremy
cannot test.

Therefore: **a session is not finished when the code compiles. It is finished when
the release is live and verified.** Any time the old workflow would have said "flash
and test," run the full release procedure below without being asked.

### Release procedure

1. **Bump `FW_VERSION` in `src/config.h`.** Patch for bug fixes and session work,
   minor for behavioural changes. This must match the tag exactly — the OTA manifest
   is generated from the tag, and a device already running that version will not
   offer the update.
2. **Compile both environments** — `esp32` and `esp32-headless`. As of v1.6.1 CI
   builds both too, so a headless-only break now fails the release rather than
   shipping silently — but compile locally first anyway, so you find it in seconds
   instead of after a tag push.

   ```bash
   cd Blind-Flight && pio run -e esp32 && pio run -e esp32-headless
   ```
   (PlatformIO is not on PATH in the Bash tool; use `~/.platformio/penv/Scripts/pio.exe`.)
3. **Commit, then rebase onto origin/master before pushing.** CI pushes a
   `version.json` commit back to master after every release, so local master is
   almost always one behind. `git pull --rebase origin master` first.
4. **Push master, then push the tag:**

   ```bash
   git push origin master && git tag -a vX.Y.Z -m "vX.Y.Z — <summary>" && git push origin vX.Y.Z
   ```
5. **Watch the workflow to completion** — `gh run watch <id> --exit-status`. A failed
   run means no release exists and the device will not see the update.
6. **Verify both published manifests** — pull, then confirm `release/version.json`
   *and* `release/version-headless.json` show the new version and that each one's
   `size`/`sha256` match its own release asset. The device validates both fields; a
   mismatch aborts the update on-device. Downloading the asset and hashing it is the
   only check that actually proves the channel works.
7. **Then** give Jeremy the testing checklist, noting the version to look for.

`.github/workflows/release.yml` does the rest: builds **both** environments,
publishes the GitHub Release with `firmware.bin` (screen) and
`firmware-headless.bin`, regenerates `release/version.json` and
`release/version-headless.json`, and commits both back to master.

**Two OTA channels, one per build variant.** `OTA_MANIFEST_URL` in `config.h` is
`#ifdef`'d on `HEADLESS_BUILD`, so each build can only ever be offered its own
binary. This is not cosmetic: installing the screen build on headless hardware
would not crash, so bootloader rollback would not catch it — the unit would boot
TFT firmware on a device with no TFT and go unreachable inside a sealed enclosure.
Never collapse the two manifests back into one without also removing the split
build.

**If a tag push produces no workflow run at all** (this happened on 2026-08-06 with
v1.6.0 — tag on the remote, workflow file present on the tagged commit, Actions
enabled, no GitHub incident, and re-pushing the tag changed nothing), the workflow
also accepts a manual trigger:

```bash
gh workflow run release.yml --ref vX.Y.Z
```

`GITHUB_REF_NAME` is the tag name when dispatched that way, so every step behaves
exactly as it does on a tag push. Root cause was never identified; later tag pushes
worked normally.

### Consequences of losing USB

* **Bootloader rollback is the only safety net, and it is armed (as of v1.4.2).**
  The stock Arduino-ESP32 bootloader ships with `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`,
  `ESP_SYSTEM_PANIC_PRINT_REBOOT=y`, and the task/interrupt/bootloader watchdogs on —
  verified in `~/.platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/sdkconfig`.
  A new image therefore boots in `PENDING_VERIFY`, and any reboot before it confirms
  itself — including the automatic reboot after a panic — reverts to the previous
  partition. Confirmation is gated on `OTA_VALIDATE_UPTIME_MS` (30 s) of healthy
  running, in `otaValidateTick()` from `loop()`. **Never move that confirmation back
  into `setup()`.** Treat any change to `setup()`, `main.cpp`, `ota.cpp`, or the
  partition table as high-risk and review it twice before tagging.
* **This catches crashes, not hangs.** Arduino's `loopTask` runs on core 1, whose
  idle task the task WDT does not watch, so an infinite loop in `loop()` neither
  reboots nor rolls back. Blocking motor code is the realistic source of such a hang
  — one more reason roadmap Session 8 (motor task migration) matters.
* **After an OTA update, leave the device powered for at least 30 seconds.** Power-
  cycling inside the confirmation window is exactly what triggers a revert, so an
  impatient switch-off silently downgrades the unit. The post-update screen says so.
* **Prefer small, frequent releases** over batching many changes into one tag. If
  something bricks, the smaller the diff, the faster the diagnosis.
* Disassembly to reach the USB port is the fallback, and it is expensive. Assume it
  is unavailable.

\---

## Working Conventions

These were shaped by the chat-based workflow; adapt as noted for Claude Code:

* **Spec-first for complex features:** new game modes get a markdown spec (design decisions, file-by-file change breakdown, testing checklist) before implementation. Keep specs in the repo (e.g., `docs/specs/`).
* **Session-scoped work:** each work unit is a discrete, testable deliverable that ends in a **released** build. Don't sprawl across unrelated modules. See "Firmware Delivery — OTA Only" above: compiling is not the finish line, tagging and verifying the release is.
* **Bug batching:** issues from physical testing accumulate into a punch list, then get fixed together in a dedicated bug-fix pass.
* **Test loop:** Claude cannot run this hardware. Every change ends with the release pushed OTA (see above) **and** a concrete physical testing checklist for Jeremy, stating which version he should see the device offer. Jeremy reports exact symptoms; diagnose from patterns.
* **Jeremy often arrives with root causes pre-analyzed** — trust his diagnosis as a starting hypothesis and verify against the code.
* **Explain non-obvious C++/embedded concepts briefly** when introducing them — Jeremy is learning the domain but shouldn't need to be an expert to review changes.
* **File carry-forward is obsolete in Claude Code** — the repo is the source of truth. Use git commits per session/feature instead.

