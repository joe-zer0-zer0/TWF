# TMC2209 — Current Setting & Mode Configuration

**Bench reference for Blind Flight.** Print or keep open on a second screen
while working on the device.

**Last updated:** 2026-09-07

---

## UART mode (v1.9.0+ — recommended)

As of firmware v1.9.0, the TMC2209 is configured **digitally via UART**.
When the three UART wires are connected, the firmware sets current,
microstepping, and chopper mode at boot — the Vref potentiometer is
ignored and no manual adjustment is needed.

### Wiring (3 wires)

| ESP32 GPIO | TMC2209 board pad | Notes |
|---|---|---|
| GPIO 21 | TX | Board's 1kΩ series resistor handles level translation |
| GPIO 22 | RX | Direct connection to PDN_UART |
| — | CLK → GND | Tells the TMC2209 to use its internal clock |

### What the firmware configures

| Parameter | Value | Why |
|---|---|---|
| RMS current | `TMC_RUN_CURRENT_MA` (config.h, default 1000 mA) | Matched to motor rating |
| Hold current | IHOLD=16 (~50% of run) | Keeps disc locked between moves |
| Chopper mode | **SpreadCycle** | Max torque under load (louder than StealthChop) |
| Microstepping | 8× (1600 steps/rev) | Matches all firmware motion constants |
| Interpolation | 256× (on by default) | Smooths motion without changing step count |

### Motor current reference

| Motor | Rated current | Set `TMC_RUN_CURRENT_MA` to |
|---|---|---|
| Small (current unit): 0.13 N·m, 3.5Ω, 5.2mH | 1.0 A | 1000 |
| Large (17HE15-1504S): 0.42 N·m, 2.3Ω, 4.0mH | 1.5 A | 1500 |

### Verifying UART communication

At boot, the Serial monitor prints one of:
- `[TMC] UART connected (TMC2209 detected)` — working, followed by config readback
- `[TMC] UART FAILED — version register returned 0x00` — wiring issue; driver falls back to standalone mode

### Sense resistor

The BigTreeTech TMC2209 v1.3 uses **0.11Ω** sense resistors. This is set
in config.h as `TMC_R_SENSE`. If you swap to a different manufacturer's
board, verify and update this value — it scales all current calculations.

---

## Standalone mode (fallback / legacy)

If UART is **not wired**, the driver operates in standalone mode and the
sections below apply. The firmware detects the missing UART at boot and
continues normally — all motor behavior works, but current is controlled
by the Vref pot and the driver defaults to StealthChop.

---

## Safety first

- **Never adjust VREF with the motor running.** Power down, adjust, power up.
- **Never hot-plug the stepper motor.** Disconnecting a stepper while the driver
  is powered generates a voltage spike that destroys the driver instantly.
  Motor connections get made and broken with VM fully off.
- **Use a ceramic or plastic trimmer tool**, not a metal screwdriver. A metal
  blade bridging the pot to an adjacent pad shorts VREF.
- The trimmer is fragile and has limited travel. **Quarter-turn increments
  maximum.** Do not force it past its stops.
- The driver gets hot. Assume the chip and the exposed pad are hot after any
  extended run.

---

## Step 1 — Identify your board

**This matters more than anything else on this page.** The VREF → current
formula depends on the sense resistor, which varies by manufacturer. Getting it
wrong is roughly a 2.5× error in either direction.

Look for a maker name and revision on the silkscreen — BigTreeTech, FYSETC,
Watterott SilentStepStick, or an unbranded clone.

| Board family | Typical R_sense | Commonly published formula |
|---|---|---|
| Watterott SilentStepStick | 0.11 Ω | `I_RMS = V_REF × 0.5` |
| BigTreeTech TMC2209 v1.2/v1.3 | 0.11 Ω | `I_RMS = V_REF / 1.44` |
| Some clones | 0.15 Ω | differs again |

**Do not pick one of these from the table and trust it.** Find your board's own
documentation and use the formula it publishes. If you cannot identify the
board, skip to the empirical method in Step 5 — it works without knowing the
formula at all.

**Record here once identified:**

```
Board:      ______________________
R_sense:    ______________________
Formula:    ______________________
Doc source: ______________________
```

---

## Step 2 — What current to target

The **driver**, not the motor, is the binding constraint.

- Current small motor (0.13 N·m): rated **1.0 A/phase** — target **1.0 A RMS**
- Upgrade motor 17HE15-1504S (0.42 N·m): rated **1.5 A/phase** — target **1.5 A RMS**
- Bare TMC2209, no heatsink, no airflow: realistically **~1.2 A RMS** sustained
- With a heatsink: **~1.7 A RMS** (needed for the upgrade motor)

**Note:** with UART (v1.9.0+), current is set in config.h and this section
is informational only. The Vref pot is overridden.

If you need more torque than 1.2 A provides, add a heatsink to the driver
before raising current further.

---

## Step 3 — Measure VREF

1. **Disconnect the stepper motor** from the driver.
2. Power the board's **logic** rail only (3.3 V or 5 V). Leave motor VM (7.4 V)
   disconnected if your wiring allows it.
3. Set the multimeter to DC volts, 2 V range if it isn't autoranging.
4. **Black probe to a GND pin** on the driver or the common ground point.
5. **Red probe to VREF:**
   - **Preferred:** the dedicated VREF test pad or via, if your board has one.
     Many do — look for a small pad labelled `VREF` or `VM`/`SET`.
   - **Fallback:** the **metal top of the trimmer pot**. That's the wiper. Move
     slowly. A probe slipping onto an adjacent pin can kill the driver.
6. Read and record.

```
Baseline VREF (before any change):  __________ V
Date/time:                          __________
```

**Record the baseline before touching anything.** If an adjustment makes things
worse, this is how you get back.

---

## Step 4 — Adjust

- **Clockwise on the trimmer usually raises** VREF, but not universally — check
  which way yours moves before assuming.
- Quarter-turn maximum per adjustment.
- Power down → adjust → power up → measure. Do not measure while turning.
- Re-measure after every change. The pots are cheap and non-linear; the
  relationship between rotation and voltage is not proportional.

---

## Step 5 — Empirical method (recommended)

**Trust this over the arithmetic.** You already have the right tool built into
the firmware: the **HW Diag → Step Test** page sends precise 400-step quarter
revolutions, so four presses should return the disc to exactly its starting
position.

1. Mark the disc and the enclosure so you can see the start position precisely.
2. **Load four glasses filled to the highest level you'd actually pour** — you
   want worst-case inertia and worst-case friction.
3. Record the baseline VREF and run the step test. Note where the disc lands
   after four presses.
4. Raise VREF by **0.05 V**. Re-run. Note the landing position.
5. Repeat until step accuracy stops improving. That's the torque knee — beyond
   it you're adding heat, not capability.
6. Back off slightly from the knee.
7. **Thermal check:** run the motor continuously for ~5 minutes, then check
   temperatures. If the driver is too hot to hold a finger on (above roughly
   60–70 °C), or the motor is too hot to touch comfortably, back the current
   down.

**Log your runs:**

| VREF (V) | Landing error after 4× 400 steps | Driver temp after 5 min | Notes |
|---|---|---|---|
| | | | baseline |
| | | | |
| | | | |
| | | | |

---

## Step 6 — StealthChop vs SpreadCycle

**With UART (v1.9.0+), SpreadCycle is enabled automatically** via
`en_spreadCycle(true)` at boot. This section only applies to standalone mode.

With `PDN_UART` floating (standalone mode), the TMC2209 defaults to
**StealthChop2**: quiet, but noticeably weaker at speed and sluggish responding
to load transients. That is close to the worst-case match for a heavy carousel
doing multi-revolution spins.

**SpreadCycle** gives more torque and better step fidelity under load, at the
cost of audible motor noise. On this device a mechanical spin noise is
thematically fine — arguably a feature.

**To switch in standalone mode:** tie the `SPREAD` pin **HIGH** (to 3.3 V).
Implementation varies: some boards expose it on the header, some use a solder
jumper, some label it `SPRD`. Check your board's pinout.

---

## Step 7 — Standstill current reduction

In standalone mode the TMC2209 typically drops to roughly **50% current about a
second after the last step pulse**.

That means holding torque fades right when the disc is still settling from a
spin — a real contributor to final-position scatter on a heavy carousel with
mechanical play.

Check whether your board exposes a **PDN / standstill jumper**. If the disc
tends to creep or settle after arriving, disabling standstill reduction is worth
testing — at the cost of continuous heat in both driver and motor while idle.

---

## Blind Flight — current configuration

Fill in as you go. This is the record for this specific unit.

```
Board:                    ______________________
R_sense:                  ______________________
VREF as shipped:          ______________________
VREF as set:              ______________________
Calculated I_RMS:         ______________________
Chopper mode:             StealthChop  /  SpreadCycle
Standstill reduction:     enabled  /  disabled
Heatsink fitted:          yes  /  no
Date configured:          ______________________
```

---

## Quick reference — microstepping

Blind Flight firmware assumes **8× microstepping = 1600 microsteps/rev**.

With UART (v1.9.0+), this is set via the `microsteps(8)` register write.
In standalone mode, it is the TMC2209 default with **MS1 and MS2
unconnected**. Either way, do not add pull-ups or jumpers to MS1/MS2 —
the firmware constants (`MICROSTEPS_PER_REV`, `MICROSTEPS_PER_GLASS`,
`POUR_OFFSET`) all depend on 8×.

**Symptom of a wrong microstepping setting:** the disc alternates between two
positions instead of visiting four, or lands at half/double the expected angles.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Motor whines, doesn't turn | Current far too low, or a phase pair miswired |
| Skips steps under load | Current too low, or StealthChop at speed |
| Driver too hot to touch | Current too high, or no heatsink |
| Motor very hot, driver cool | Current above the motor's rating |
| First few steps after idle are lost | Enable settling delay — firmware issue, see roadmap Session 2 |
| Disc alternates between two positions | Wrong microstepping (MS1/MS2 jumpered) |
| Works cold, skips when warm | Driver thermal throttling — heatsink or lower current |
