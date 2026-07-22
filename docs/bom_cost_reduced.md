# Blind Flight — Reduced-Cost BOM (Per-Unit)

Generated 2026-07-16. Baseline: `bom_cost_per_unit.md` ($70.40/unit, Amazon-only).

Strategy: Amazon for fast-turnaround and quality-critical items; AliExpress/specialty suppliers for commodity modules where 2-3 week lead time is acceptable. No design changes except the optional driver swap (noted).

## Summary

| Category | Amazon Baseline | Reduced | Savings |
|----------|----------------|---------|---------|
| Core electronics | $34.07 | $17.86 | $16.21 |
| Power | $23.37 | $14.08 | $9.29 |
| Interface | $1.42 | $1.33 | $0.09 |
| Wiring | $8.98 | $3.50 | $5.48 |
| Board & passives | $2.56 | $1.37 | $1.19 |
| **Total** | **$70.40** | **$38.14** | **$32.26** |

## Core Electronics — $17.86

| Component | Source | Pack Price | Pack Qty | Per Unit Qty | Per Unit Cost | Notes |
|-----------|--------|-----------|----------|-------------|---------------|-------|
| ESP32 DevKit V1 | Amazon | $16.79 | 3 | 1 | $5.60 | Keep Amazon — fast replacement if one dies during dev |
| NEMA 17 stepper (23mm, 1.8°) | AliExpress | $12–15 | 3 | 1 | $4.00 | Search "NEMA 17 23mm stepper"; same spec as MAKERELE |
| TMC2209 stepper driver | AliExpress | $7–9 | 3 | 1 | $2.50 | Same board, ~1/3 the Amazon price |
| ST7789 TFT display (240x280) | AliExpress | $6–8 | 2 | 1 | $3.00 | Search "ST7789 1.69 inch 240x280 SPI"; verify pin-compatible |
| KY-040 rotary encoder | AliExpress | $3.50 | 5 | 1 | $0.70 | Or bare EC11 encoders for ~$0.30 each |
| Hall effect sensor (A3144) | Amazon | $5.99 | 5 | 1 | $1.20 | Already cheap; keep Amazon for reliability |
| Passive buzzer (bare, no module) | AliExpress | $2.50 | 10 | 1 | $0.25 | Bare piezo disc; wire directly, no breakout needed |
| MH-FMD buzzer module | Amazon | $8.07 | 5 | 1 | $1.61 | Alternative: keep module if you prefer plug-and-play |

**Buzzer note:** The MH-FMD module is just a passive buzzer on a breakout with a header. Since you drive it with PWM from GPIO 13, a bare passive buzzer soldered inline works identically. The $0.25 line assumes bare; use the $1.61 module line if you prefer keeping the breakout.

**Driver swap option (not included above):** Replacing the TMC2209 with an A4988 ($1.00–1.50/unit from AliExpress in 5-packs) saves another $1.00–1.50 but loses StealthChop silent operation. Requires a small firmware change (microstepping config). The TMC2209 at AliExpress pricing is already a big improvement, so this is optional.

## Power — $14.08

| Component | Source | Pack Price | Pack Qty | Per Unit Qty | Per Unit Cost | Notes |
|-----------|--------|-----------|----------|-------------|---------------|-------|
| 18650 battery cells | Specialty | $16–20 | 4 | 2 | $8.00 | 18650BatteryStore or LiionWholesale; name-brand cells (Samsung 25R, Sony VTC6, etc.) |
| 2S BMS charging board (USB-C) | AliExpress | $4.00 | 3 | 1 | $1.33 | Search "2S BMS USB-C charge board"; same HW-568 style |
| Buck converter (5V output) | AliExpress | $4.00 | 5 | 1 | $0.80 | Mini MP1584 or similar; verify 2A+ output |
| 18650 battery holder (2S) | Amazon | $6.99 | 5 | 1 | $1.40 | Keep Amazon — holder quality matters for contact reliability |
| Rocker switch (on/off) | AliExpress | $3.00 | 10 | 1 | $0.30 | Standard KCD1 mini rocker |

**Battery note:** This is the single biggest cost driver. The specialty battery suppliers sell tested, authentic cells with datasheets. Avoid no-name AliExpress 18650s — capacity claims are often fraudulent. Budget $4–5/cell for quality; $8/pair is conservative.

## Interface — $1.33

| Component | Source | Pack Price | Pack Qty | Per Unit Qty | Per Unit Cost | Notes |
|-----------|--------|-----------|----------|-------------|---------------|-------|
| Tactile buttons | Amazon | $7.99 | 180 | 2 | $0.09 | Already negligible; keep current source |
| PTFE furniture pads | Amazon | $7.99 | 24 | 4 | $1.33 | No cheaper source found; consumable anyway |

*Note: buttons reduced from $0.09 to $0.00 effective change — not worth re-sourcing.*

## Wiring — $3.50

| Component | Source | Pack Price | Pack Qty | Per Unit Qty | Per Unit Cost | Notes |
|-----------|--------|-----------|----------|-------------|---------------|-------|
| Pre-crimped JST-XH pigtails (assorted 2–6 pin) | AliExpress | $8–12 | 50 pcs | ~10 | $2.00 | Search "JST XH 2.54mm pigtail wire"; buy a 50-pc multi-pack |
| Silicone hookup wire (22AWG, multi-color) | AliExpress | $5–6 | 5×2m | shared | $1.00 | For power runs and any direct-solder connections |
| Heat shrink tubing assortment | AliExpress | $3.00 | kit | shared | $0.50 | One kit does many units |

**Wiring approach:** Use pre-crimped JST-XH connectors on the module side (display, encoder, driver, hall sensor) and solder the other end to the perfboard. No hand crimping. JST-XH is more secure than Dupont — the latch prevents accidental disconnection. Silicone wire for power runs (battery → BMS → buck → ESP32) since those carry more current and benefit from direct soldering.

**At higher volume (10+ units):** Custom cable assemblies from JLCPCB or a cable house would drop this further, but the MOQ makes it impractical below ~25 units.

## Board & Passives — $1.37

| Component | Source | Pack Price | Pack Qty | Per Unit Qty | Per Unit Cost | Notes |
|-----------|--------|-----------|----------|-------------|---------------|-------|
| Perfboard + headers kit | Amazon | $9.99 | kit | ~1/8 | $1.25 | Keep current; good variety kit |
| 10K resistors | Amazon | $3.99 | 100 | 3 | $0.12 | Same price, 10x the quantity — just buy the 100-pack |
| 47K resistors | Amazon | $6.51 | 100 | 1 | $0.07 | Already bulk-priced |
| 100K resistors | Amazon | $3.99 | 100 | 1 | $0.04 | Already bulk-priced |

*Note: the only real savings here is the 10K resistor fix — buying 100 instead of 10 for the same $3.99.*

## Sourcing Strategy

**Keep on Amazon** (fast shipping, quality matters, or already cheap):
- ESP32 DevKit — need fast replacements during development
- 18650 holder — contact quality matters
- Perfboard/headers kit — variety is the value
- Hall sensor — already $1.20, not worth the hassle
- Tactile buttons — already $0.09
- PTFE pads — consumable, need to re-order quickly

**Move to AliExpress** (commodity modules, 2-3 week lead time acceptable):
- NEMA 17 stepper
- TMC2209 driver
- ST7789 display
- KY-040 encoder
- 2S BMS board
- Buck converter
- Rocker switch
- Buzzer (bare)
- Wiring (JST-XH pigtails, silicone wire, heat shrink)

**Move to specialty supplier** (quality matters, better pricing than Amazon):
- 18650 battery cells

## Volume Scaling

At 10+ units, additional savings become available:

| Opportunity | Estimated per-unit at 10+ |
|-------------|--------------------------|
| JLCPCB custom PCB (replaces perfboard + most wiring) | $2–4 for PCB, but eliminates ~$3.50 wiring + $1.25 perfboard |
| Bulk battery cells (case of 20+) | $3.50–4.00/cell vs $4–5 |
| LCSC component sourcing (resistors, connectors, headers) | Pennies per component |

A custom PCB from JLCPCB ($2–8 for 5 boards depending on size) would be the single biggest quality-of-life and cost improvement at volume — it eliminates hand-soldering the perfboard, most of the wiring harness, and the associated assembly time.
