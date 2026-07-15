# Session Plan: Pour Position Selection

## Goal
Let the user choose which side of the box the pour hole aligns with — **Front, Right, Rear, Left** — via the Settings menu. Since the box is square and the lid rotates freely, this maps directly to a 0/90°/180°/270° offset added to the base `POUR_OFFSET`.

## Concept

The existing `POUR_OFFSET` (1000 microsteps) compensates for the fixed Hall-sensor-to-pour-spout geometry. That stays. The new **pour position** setting adds 0, 1, 2, or 3 glass positions (× 400 microsteps each) on top of it, rotating the effective pour point by 90° increments.

```
Total offset = POUR_OFFSET + (pourPosition × MICROSTEPS_PER_GLASS)
             = 1000 + (0|1|2|3 × 400)   mod 1600
```

The user sees: `Front | Right | Rear | Left`
Internally:     `0       1       2       3`

## Files to Modify

### 1. `settings.h` / `settings.cpp` — New NVS-backed setting
- Add RAM shadow: `static uint8_t sPourPos = 0;` (default: Front)
- NVS key: `"pourPos"` (UChar, 0–3)
- Add getter/setter: `settingsGetPourPos()` / `settingsSetPourPos()`
- Clamp to 0–3 on load
- Add to `settingsSave()` flush

### 2. `motor.h` / `motor.cpp` — Runtime pour offset
- Add `motorSetPourPosition(uint8_t pos)` — stores position, recomputes effective offset
- Internal variable: `static int effectivePourOffset`
- Compute: `effectivePourOffset = (POUR_OFFSET + pos * MICROSTEPS_PER_GLASS) % MICROSTEPS_PER_REV`
- Replace `POUR_OFFSET` references in `motorMoveToGlass()` and `motorSpinToGlass()` with `effectivePourOffset`
- Call `motorSetPourPosition()` from `main.cpp setup()` after `settingsInit()` loads the stored value
- `motorInit()` initializes `effectivePourOffset` to `POUR_OFFSET` (default/Front)

### 3. `screen_settings.cpp` — New menu item
- Add `SET_POUR` to the `SettingsItem` enum (insert before `SET_REHOME` to keep action items at bottom)
- Bump `SET_COUNT` to 7
- Label: `"Pour Pos"`
- Value display: `pourPosLabels[] = { "Front", "Right", "Rear", "Left" }`
- Edit behavior: encoder CW/CCW cycles through 0–3 (wrapping)
- On apply: `settingsSetPourPos(editValue)` + `motorSetPourPosition(editValue)`

### 4. `config.h` — No changes needed
- `POUR_OFFSET` stays as the hardware-geometry constant (compile-time default)
- The runtime variable in `motor.cpp` layers the user's selection on top

### 5. `main.cpp` — Init call
- After `settingsInit()` and `motorInit()`, add:
  `motorSetPourPosition(settingsGetPourPos());`

## Files NOT Modified
- `game.cpp` — already calls `motorMoveToGlass()` / `motorSpinToGlass()`, no offset awareness needed
- `wifi_portal.cpp` — phone UI calls the same motor functions, transparent
- `screen_motor_test.cpp` — uses `motorMoveToGlass()`, will automatically pick up the new offset
- `config.h` — `POUR_OFFSET` constant preserved as default

## Estimated Scope
- ~15 new/changed lines in `settings.h` + `settings.cpp`
- ~15 new/changed lines in `motor.h` + `motor.cpp`
- ~20 new/changed lines in `screen_settings.cpp`
- ~2 lines in `main.cpp`
- **Total: ~50 lines of code, single session, low risk**

## Testing Checklist
- [ ] Default behavior unchanged (fresh NVS = Front = same as current)
- [ ] Each of the 4 positions moves the glass to the correct pour point
- [ ] Setting persists across power cycle
- [ ] Motor Test screen reflects the new offset (glass lands at correct position)
- [ ] Phone UI pours work correctly with non-default pour position
- [ ] Spin-to-glass theatrical spin lands correctly with non-default position

## Session Prerequisites
Upload these files at session start:
- `config.h`
- `settings.h` + `settings.cpp`
- `motor.h` + `motor.cpp`
- `screen_settings.cpp`
- `main.cpp`

## Notes
- The "Front" label should match whatever orientation the user considers the front of the box during initial setup. This is intuitive — if they put the lid on with the hole facing them, that's "Front."
- If the physical label orientation matters for beta units, Jeremy can relabel or instruct testers to treat a specific side as "Front" relative to the display.
