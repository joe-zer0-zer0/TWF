# Session Spec — Shareable Flight Results Card

## Overview

Give the phone a results view worth posting. Today, when a flight finishes, the phone's
`DO` (done) view renders a bare list of `glass. name` via `rs()` in
`web/phone_ui.html`. It carries no rank positions, no ratings, no bottle metadata, and no
branding — so after a Ranked Flight there is nothing on the phone that can usefully be
carried to Facebook or Instagram.

This spec adds, in phases:

1. A rank-aware results payload and a poster-styled results card in the phone UI, good
   enough that a plain phone screenshot is postable.
2. A `<canvas>` export that produces a clean 1080×1350 social image with no browser
   chrome, saved to the phone's photo library.
3. An optional composite of the user's own pre-tasting photo into that card.

Phase 4 (a cloud-hosted public results link) is scoped but explicitly deferred.

---

## Constraints

These four constraints determine the entire design. They are not negotiable at the
firmware level.

### C1 — No internet on the AP

At an event the phone is joined to the device's own access point, which has no upstream
connection. Facebook's share endpoints (`facebook.com/sharer/sharer.php`) accept a
**public URL** which Facebook's servers then scrape for `og:image` — they cannot ingest
an image payload, and they cannot reach `192.168.4.1`. **A direct "post to Facebook"
button is therefore impossible without a cloud backend** (Phase 4).

STA mode exists (`staMode`, `wifi_portal.cpp:53`) and does give the phone internet when
the device joins a house network, but that is not the event configuration and cannot be
relied on.

### C2 — `http://192.168.4.1` is not a secure context

Browsers gate `navigator.share` (the native iOS/Android share sheet) and
`navigator.clipboard` behind a secure context: HTTPS, or a loopback address. Private
IPv4 ranges do **not** qualify, and neither does the `flight.local` mDNS name over
plain HTTP. **Assume both APIs are unavailable.**

Serving HTTPS from the ESP32 would require a self-signed certificate and produce a
full-page security interstitial at every event. Rejected.

Consequences:
- The primary save path must be **long-press the image → Save to Photos**, which works
  on every phone over plain HTTP and fully offline.
- `navigator.share` is used only when feature detection says it exists (STA mode behind
  HTTPS, future hosting, or a browser that relaxes the rule). It is never the only path.
- Clipboard copy falls back to a hidden `<textarea>` + `document.execCommand('copy')`.

### C3 — The device has no clock

There is no RTC and no NTP sync anywhere in the firmware. The device cannot supply a
date for the card. **The date comes from the phone** (`new Date()` in the page), which
is correct and local by definition. Do not add a date field to the device payload.

### C4 — Flash budget

`web/phone_ui.html` is 34,253 bytes raw / 8,859 gzipped, compiled into PROGMEM by
`scripts/gzip_html.py` at build time. Firmware is 1,299,632 bytes against a ~1.9 MB
`min_spiffs` OTA partition. The additions here are expected to cost ~4–6 KB raw /
~1.5 KB gzipped — negligible. All canvas rendering runs on the phone's CPU; the ESP32
does no image work.

**Build note:** `gzip_html.py` regenerates `src/phone_ui_gz.h` only when the HTML's mtime
is newer than the header's. A git operation that rewrites the header can defeat this.
If phone UI changes do not appear on the device, touch the HTML or delete the header and
rebuild.

---

## Design Decisions

### The card is rendered twice, from one layout description

Phase 1 renders the card as DOM. Phase 2 renders the same card to a canvas. Rather than
maintain two layouts, define the card content once as a plain JS object built from the
WebSocket state:

```js
// buildCardModel() -> { title, mode, date, rows: [{rank, glass, name, stars, meta}], footer }
```

`renderCardDOM(model)` and `renderCardCanvas(model, ctx, scale)` both consume it. When a
field is added later, both paths get it.

### Rank position is computed on the device, not inferred on the phone

The `rv` array in `buildStateJSON` (`wifi_portal.cpp:916`) is emitted in **reveal**
order. For rank modes the device's own `drawDone()` (`game.cpp:964`) walks
`revealMapCount - 1` down to `0` so that `rank = revealMapCount - i`. That inversion is
a device-side convention and the phone should not have to know it. Emit an explicit
`"r"` field and let the phone render what it is told.

### Extended `rv` schema

Current:

```json
"rv":[{"g":1,"n":"Eagle Rare"}]
```

Proposed (all new fields optional; omit when zero/unknown to save bytes):

| Key | Type | Meaning |
|-----|------|---------|
| `g` | int | Glass number (1-based) — existing |
| `n` | string | Bottle name — existing |
| `r` | int | Rank position, 1 = favorite. **Rank modes only** (`m` is `R` or `Q`) |
| `s` | int | Star rating 0–5, from `gameGetGlassRating(pourIdx)`. Omit when 0 |
| `p` | int | Proof, from `gameGetGlassEntry(pourIdx)->proof`. Omit when 0 |
| `t` | int | Price tier 1–4. Omit when 0 |
| `y` | int | Age in years. Omit when 0 |

**Buffer math.** `STATE_JSON_BUF` is 1280 bytes (`wifi_portal.cpp:761`). Worst-case
entry is `{"g":1,"n":"<21 chars escaped>","r":1,"s":5,"p":125,"t":4,"y":12},` ≈ 70 bytes.
Four glasses ≈ 280 bytes, against a base state of roughly 250–300 bytes at `GAME_DONE`.
Comfortable, but the truncation behaviour should be verified with four 21-character
names (see testing checklist).

**Safety.** `buildStateJSON` uses the `JSON_PUT` / `JSON_REM` macros, which are already
underflow-safe — `JSON_REM` clamps to 0 rather than letting `bufLen - pos` wrap. Keep
using them. Do **not** introduce raw `snprintf(buf + pos, sizeof(buf) - pos, ...)` here;
that is the exact pattern that caused the v1.5.2 stack overflow (see CLAUDE.md). Bottle
names must keep going through the existing per-character escape loop.

### Card dimensions and typography

- **1080 × 1350** (4:5). Facebook and Instagram both accept it as portrait without
  cropping, and it is the tallest common aspect — more room for four rows plus branding.
- Canvas area is 1.46 M px, far under iOS Safari's ~16.7 M px canvas cap. No tiling
  needed.
- Fonts: canvas cannot use the TFT's embedded fonts. Use a system stack
  (`-apple-system, "Segoe UI", Roboto, sans-serif`) so the card looks native on both
  platforms. Do not embed a webfont — it would cost more flash than the entire feature.
- Colours reuse the device palette (`COL_ACCENT`, `COL_BG`, `COL_DIM` equivalents) so
  the card reads as the same product as the screen.

### Logo on the card

Preferred: embed the wordmark as an SVG path string and stroke/fill it with `Path2D` on
canvas (and as an inline `<svg>` in the DOM card). The logo already exists in SVG form in
the asset pipeline (`convert_assets.py` extracts path data from Inkscape output), so this
is a few hundred bytes of path string, not an image.

Fallback if the path proves fiddly: set the wordmark as letter-spaced styled text. Decide
during Phase 2; do not block Phase 1 on it.

### Output format

- **Phase 2: PNG** via `canvas.toDataURL('image/png')`. The card is flat colour and
  compresses well.
- **Phase 3: JPEG** at quality 0.9 once a photograph is composited in, where PNG would
  balloon to several megabytes.

---

## Phased Roadmap

### Phase 1 — Rank-aware payload + poster-styled results view

**Goal:** the flight-complete view on the phone is a designed card showing rank
positions, ratings and bottle metadata. A plain screenshot of it is postable.
**Estimated effort:** 1 session.

#### Changes to `src/wifi_portal.cpp`

- In `buildStateJSON`, `GAME_DONE` branch (currently line 916): extend each `rv` entry
  with `r`, `s`, `p`, `t`, `y` per the schema above.
  - `r` emitted only when mode is `GAME_MODE_RANK` or `GAME_MODE_GUESS_RANK`, computed
    as `revealCount - i` to match `drawDone()`.
  - `s` from `gameGetGlassRating(p)`; `p`/`t`/`y` from `gameGetGlassEntry(p)`, guarding
    the null return.
  - Omit any field whose value is 0.
- No new accessors needed — `gameGetGlassRating` and `gameGetGlassEntry` are already
  declared in `game.h:157-158`.
- No changes to routes, the WebSocket protocol version, or `handleRoot`.

#### Changes to `web/phone_ui.html`

- New `buildCardModel()` — reads `S.m`, `S.rv`, `S.p`, and the phone's `new Date()`,
  returns the layout model described above. Rank modes label rows `#1`–`#4`; all other
  modes label them `Glass N`.
- New `renderCardDOM(model)` replacing `rs()` (line 355). Emits a single
  `<div class="card">` containing: wordmark, mode name, date, the rows (rank/glass badge,
  bottle name, star row, metadata line), and a small footer.
- New CSS for `.card` and children. Card must be a self-contained rectangle with its own
  background so it reads as a graphic, not as page content.
- `DO` branch of the main render (line ~322) uses the card; the existing **Exit** and
  **New Flight** buttons move *outside* the card element so a screenshot of the card
  isn't cluttered with controls.
- `rs()` is removed; confirm no other caller.

#### Testing checklist

- [ ] Run a 4-glass **Ranked Flight** with named bottles from the library. Phone shows
      #1–#4 in the same order the device screen shows them.
- [ ] Ranked order on the phone matches the device's FLIGHT COMPLETE screen exactly
      (this is the regression that motivated the feature — check it first).
- [ ] Star ratings entered during tasting appear on the card; unrated glasses show no
      star row rather than zero stars.
- [ ] Proof / price tier / age appear for library bottles, and are silently absent for
      manually-typed names.
- [ ] Run a **Basic** flight — rows read `Glass N`, no rank badges, no layout breakage.
- [ ] Run a **Best Guess** flight — card renders, correct/incorrect state unaffected.
- [ ] Run a 2-glass and a 3-glass flight — card layout holds with fewer rows.
- [ ] Four bottles with 21-character names — no JSON truncation, card doesn't overflow
      (check the browser console for a JSON parse error, which is how truncation shows).
- [ ] Manually-entered names containing `"` and `\` render correctly (escape path).
- [ ] Screenshot the card on the phone — it looks like a deliberate graphic.
- [ ] Headless build still compiles (`-e esp32-headless`).

---

### Phase 2 — Canvas export and save to Photos

**Goal:** one tap produces a clean 1080×1350 image with no browser chrome, saved to the
phone's photo library.
**Estimated effort:** 1 session.

#### Changes to `web/phone_ui.html` only

No firmware changes in this phase.

- `renderCardCanvas(model, ctx, scale)` — draws the same model at 1080×1350.
- **Save Image** button under the card:
  1. Render to an offscreen `<canvas>`.
  2. `canvas.toDataURL('image/png')`.
  3. Swap the DOM card for an `<img>` at that data URL, with the instruction *"Press and
     hold the image, then Save to Photos"* (iOS) / *"…then Download image"* (Android) —
     select the wording from a coarse UA check, or show both.
  4. A **Back** control returns to the interactive view.
- **Native share, when available:** feature-detect
  `navigator.share && navigator.canShare?.({files:[f]})`. When present, offer a
  **Share** button that goes through `canvas.toBlob()` → `new File([blob], 'flight.png')`
  → `navigator.share`. Per C2 this is expected to be absent on the AP; it must never be
  the only route to the image.
- **Copy as text** button: builds a plain-text summary (mode, date, ranked list) and
  copies it. Try `navigator.clipboard.writeText`, fall back to a hidden `<textarea>` +
  `select()` + `document.execCommand('copy')`, and if both fail, reveal the textarea for
  manual selection.
- Guard `toDataURL` in a `try/catch`; on failure fall back to the Phase 1 DOM card with
  a "screenshot this" hint rather than showing a broken image.

#### Testing checklist

- [ ] iPhone Safari on the AP: **Save Image** → long-press → **Add to Photos** → image
      appears in Photos at 1080×1350.
- [ ] Android Chrome on the AP: same flow via **Download image**.
- [ ] Saved image is legible at Facebook feed size — check on the actual phone, not a
      desktop browser.
- [ ] Card text does not clip with the longest bottle names.
- [ ] Confirm whether `navigator.share` is present on the AP. If it *is*, the Share
      button appears and works; if absent, no dead button is shown. **Record which,
      because it settles C2 empirically for this hardware.**
- [ ] **Copy as text** puts a usable summary on the clipboard; paste into the Facebook
      composer to confirm.
- [ ] Back control returns to the live view; **New Flight** still works afterward.
- [ ] Repeat a flight and export twice in one session — no memory growth or blank canvas
      on the second export.
- [ ] Reload the page mid-flight and export — card still renders from rebroadcast state.

---

### Phase 3 — Composite the user's own photo

**Goal:** the pre-tasting bottle-lineup photo becomes the background of the results card.
**Estimated effort:** 1 session. Do this only after seeing Phase 2 on real hardware.

- `<input type="file" accept="image/*">` — works over plain HTTP, no permissions prompt
  beyond the normal photo picker.
- Prefer `createImageBitmap(file, { imageOrientation: 'from-image' })` so EXIF rotation
  is applied; fall back to `URL.createObjectURL` + `<img>` where unsupported.
- Cover-crop into the upper region of the card, with a dark gradient scrim behind the
  results so text stays legible over any photo.
- Two layouts, user-togglable: **photo top / results below**, and **full-bleed photo with
  scrim**.
- Switch export to `toDataURL('image/jpeg', 0.9)`.
- Revoke object URLs after draw.

#### Testing checklist

- [ ] Pick a portrait photo — correct orientation, no sideways image.
- [ ] Pick a landscape photo — cover-crop is sensible, no letterboxing.
- [ ] Very large photo (12 MP+) — no crash or blank canvas on the phone.
- [ ] Results text legible over both a bright and a dark photo.
- [ ] Exported JPEG is under ~1 MB.
- [ ] Skipping the photo still produces the Phase 2 card unchanged.

---

### Phase 4 — Cloud share link (deferred, not scheduled)

The only route to genuine one-tap posting. Device or phone uploads the results to a
hosted endpoint; the phone receives a short URL to a public page carrying `og:image`
tags, which Facebook can then scrape.

Blocked on decisions outside this spec:
- Where it is hosted (overlaps the pre-launch item about moving OTA hosting off the
  personal GitHub account).
- Whether tasting data leaving the device is acceptable, and what the retention policy
  is.
- Requires internet at post time, i.e. STA mode or a phone hotspot — not the event
  configuration.

Not started until Phases 1–3 have been used at a real event.

---

## Non-goals

- Posting to Facebook, Instagram, or any network directly from the device.
- HTTPS on the ESP32.
- Server-side image generation on the ESP32.
- A date/time source (RTC or NTP) on the device — C3 stands; the phone supplies the date.
- Video or animated export.
- Editing the card (fonts, colours, captions) from the phone.

---

## Release

Per CLAUDE.md, each phase ends in a released build, not a compiling one:

1. Bump `FW_VERSION` in `src/config.h` (patch level for each phase).
2. Compile **both** environments — `pio run -e esp32 && pio run -e esp32-headless`.
   Phase 1 touches `wifi_portal.cpp`, which is in the headless build.
3. `git pull --rebase origin master`, push master, then push the tag.
4. Watch the workflow to completion, verify `release/version.json` size and sha256.
5. Hand Jeremy the phase's testing checklist with the version to look for.

Phases 2 and 3 are phone-UI-only, but still ship as firmware releases because the page is
compiled into PROGMEM — there is no way to update the phone UI without an OTA.
