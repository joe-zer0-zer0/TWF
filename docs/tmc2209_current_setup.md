# TMC2209 — Current Setting & Mode Configuration

**Bench reference for Blind Flight.** Print or keep open on a second screen
while working on the device.

**Last updated:** 2026-07-24

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

- NEMA 17, ~40 N·cm: typically rated **1.5–1.7 A/phase**
- Bare TMC2209, no heatsink, no airflow: realistically **~1.2 A RMS** sustained
- With a heatsink: **~1.4 A RMS**

**Target 1.0–1.2 A RMS** as a starting point. This is well under the motor's
rating, which is fine — you have torque headroom in the motor and thermal
headroom is what you're short of.

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

**Try this before spending much time on VREF. It may matter more.**

With `PDN_UART` floating (standalone mode), the TMC2209 defaults to
**StealthChop2**: quiet, but noticeably weaker at speed and sluggish responding
to load transients. That is close to the worst-case match for a heavy carousel
doing multi-revolution spins.

**SpreadCycle** gives more torque and better step fidelity under load, at the
cost of audible motor noise. On this device a mechanical spin noise is
thematically fine — arguably a feature.

**To switch:** tie the `SPREAD` pin **HIGH** (to 3.3 V). Implementation varies:
some boards expose it on the header, some use a solder jumper, some label it
`SPRD`. Check your board's pinout.

**Test:** run a full 4-glass flight loaded, both ways, and compare Glass Diag
offsets. Expect SpreadCycle to be louder and more accurate.

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

That is the TMC2209 standalone default with **MS1 and MS2 unconnected**. Do not
add pull-ups or jumpers to those pins — the firmware constants
(`MICROSTEPS_PER_REV`, `MICROSTEPS_PER_GLASS`, `POUR_OFFSET`) all depend on it.

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
