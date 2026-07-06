# CLAUDE.md — Blind Flight

## What This Project Is

**Blind Flight** is a physical interactive blind-tasting device (whiskey/wine). A motorized rotating carousel positions four Glencairn glasses under a fixed pour spout for sequential automated pouring, with a gamified, suspenseful reveal experience. Built for use at tasting events.

Jeremy is the sole designer/developer across hardware, firmware, and enclosure. **Jeremy has no ESP32/Arduino/C++ background** — Claude writes all code; Jeremy flashes, tests on real hardware, and reports exact symptoms. Diagnose from symptom patterns. Project status: **active prototype, beta preparation.** First public test occurred; the platform stalled after two glasses due to weight/friction (see Current State).

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

* **Friction fix:** PTFE furniture pads confirmed working in initial testing. Carrier plate (attaches to top of motor) currently in physical prototyping. Ball transfer units at 120° spacing remain the fallback if needed.
* **Six bugs fixed** in the last bug-fix session (nine files): spin speed presets read correctly; transitions no longer override brightness; motor holding current released after moves; motor test screen reachable; `randomSeed()` moved off ADC2/strapping pin; homing retry converted from unbounded recursion to a loop.
* **Pour-side selection** — fully implemented. Settings menu item cycles Front/Right/Rear/Left, NVS-persistent, runtime offset applied to all motor glass positioning.
* **Ranked Flight mode (`GAME\_MODE\_RANK`)** — spec in `docs/specs/ranked_flight_spec.md`.

## Roadmap

1. OTA firmware updates via Wi-Fi (`/update` route) — needed for beta logistics; requires compatible partition table
2. Pour animation (procedural: glass outline drawn in code, filled with amber rectangle — no flash cost)
3. Low-battery warning on home screen
4. Home offset calibration as NVS-persistent per-unit setting (adjustment after Hall trigger)
5. Whiskey library expansion
6. Phone/WebSocket parity for Ranked Flight (deferred, same pattern as Best Guess parity)
7. Validate pour-side selection during beta
8. Deferred Session 14 polish items
9. Web animation of logo SVG (standalone; CSS `stroke-dashoffset` + `offset-path`; independent of firmware)

\---

## Known Gotchas \& Hard-Won Lessons

**Electrical**

* **ADC2/Wi-Fi conflict:** GPIO 0 and other ADC2 pins conflict with Wi-Fi. All ADC reads and `randomSeed()` sources must use ADC1 pins (e.g., GPIO 36).
* **Battery ADC:** use `analogReadMilliVolts()` for factory calibration; call `analogSetPinAttenuation(PIN\_BATT\_ADC, ADC\_11db)` in `settingsInit()`; optional 100nF filter cap at GPIO 36.
* **Grounding:** star ground topology; motor high-current ground path stays separate until the star point. TMC2209 has separate logic GND and VM GND — both must reach common ground. Shared common ground across all rails (7.4V/5V/3.3V) is mandatory for STEP/DIR signal reference.
* **Buzzer is inverted** (MH-FMD): LOW = sound on.

**Firmware**

* **Blocking motor loops freeze WebSocket/HTTP/DNS** — long spins disconnect phones. Known, unresolved; handle eventually.
* **`buildStateJSON` buffer bounds need guarding** — flagged, not yet fixed.
* **Screen state machine:** never nest `uiPushScreenT` calls during transitions (corrupts state). Established pattern: deferred flags (`pendingBrowse`, `awaitingBrowseReturn`, `phoneNameReady`) push actions out of `onEnter` into the next draw loop.
* **`config.h` truncation** was a recurring failure mode with file downloads — verify the file ends at the expected final `#define`. (Less relevant in Claude Code, but keep the habit of sanity-checking file integrity after large edits.)
* Microstepping: firmware constants assume **8×** (1600/rev). If motion lands wrong (e.g., alternating between two positions), suspect a wrong microstepping constant.

**Hardware-spec doc caveat:** the original `blind\_flight\_hardware\_spec.md` predates several discoveries. Where it conflicts with this file (microstepping mode, buzzer logic, battery ADC), **this file is authoritative.**

\---

## Working Conventions

These were shaped by the chat-based workflow; adapt as noted for Claude Code:

* **Spec-first for complex features:** new game modes get a markdown spec (design decisions, file-by-file change breakdown, testing checklist) before implementation. Keep specs in the repo (e.g., `docs/specs/`).
* **Session-scoped work:** each work unit is a discrete, testable deliverable that produces a flashable build. Don't sprawl across unrelated modules.
* **Bug batching:** issues from physical testing accumulate into a punch list, then get fixed together in a dedicated bug-fix pass.
* **Test loop:** Claude cannot run this hardware. Every change ends with a concrete physical testing checklist for Jeremy. Jeremy reports exact symptoms; diagnose from patterns.
* **Jeremy often arrives with root causes pre-analyzed** — trust his diagnosis as a starting hypothesis and verify against the code.
* **Explain non-obvious C++/embedded concepts briefly** when introducing them — Jeremy is learning the domain but shouldn't need to be an expert to review changes.
* **File carry-forward is obsolete in Claude Code** — the repo is the source of truth. Use git commits per session/feature instead.

