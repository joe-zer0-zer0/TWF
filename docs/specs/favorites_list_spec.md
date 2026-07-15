# Favorites List — Development Roadmap

## Overview

Add a user-managed "Favorites" list of up to 30 whiskey names that persist in NVS across power cycles. Favorites appear in the browse screen alongside the compiled library, giving users quick access to frequently-used names without manual re-entry. Entries can be added from manual entry or from the phone interface, and managed (viewed/deleted) from a settings sub-screen.

---

## Design Decisions

### Storage
- **NVS (Preferences library)**, same mechanism used by settings and session persistence
- Separate namespace `"bffav"` to avoid collision with `"bf"` (settings) and `"bfsess"` (session)
- Each entry stored as a key-value pair: key = `"f00"` through `"f29"`, value = name string
- Entry count stored as `"cnt"` (uint8_t)
- **Budget:** 30 entries × 22 bytes (`MAX_GLASS_NAME`) = 660 bytes — well within NVS limits
- Names stored in a RAM shadow array on boot (same pattern as settings module)

### Browse screen integration
- Favorites appear as a new top-level entry in the type list: `"-- Favorites --"` at index 1 (between `"-- Manual Entry --"` and the first whiskey type)
- Selecting it shows a flat list of all saved favorites — one level deep, no sub-hierarchy
- Selecting a favorite from the list uses it as the glass name (same as selecting a product)
- If the favorites list is empty, the entry still appears but shows "(empty)" when opened
- `TYPE_OFFSET` increases from 1 to 2 to account for the new entry

### Adding favorites
Two paths to add:
1. **After manual entry** — when the user confirms a manually typed name, prompt: "Save to Favorites? [LEFT: No] [RIGHT: Yes]". Brief confirmation screen, then continue the pour flow. Skip the prompt if the name is already in favorites or the list is full.
2. **From phone** — new WebSocket action `{"a":"fav_add","v":"name"}` adds a name. Phone UI gets a small "Manage Favorites" section (details in Phase 3).

### Deleting favorites
- **On-device:** Settings sub-screen "Favorites" shows the list in a ScrollList. Encoder click on an entry prompts "Delete? [LEFT: No] [RIGHT: Yes]". Confirmed deletion removes it and compacts the array.
- **From phone:** WebSocket action `{"a":"fav_del","v":0}` (by index) removes an entry. Phone UI shows the list with delete buttons.

### No editing
Renaming an entry is not supported — delete and re-add. Keeps the implementation simple.

---

## Phased Roadmap

### Phase 1: Storage Module + Browse Integration
**Goal:** Favorites persist and appear in the browse list. Add via manual entry.
**Estimated effort:** 1 session

#### New file: `favorites.h` / `favorites.cpp`
- `favoritesInit()` — load from NVS into RAM array on boot
- `favoritesGetCount()` — return current count (0–30)
- `favoritesGetName(int index)` — return name string at index
- `favoritesAdd(const char* name)` — add to list, write to NVS, return true/false
- `favoritesRemove(int index)` — remove by index, compact array, rewrite NVS
- `favoritesContains(const char* name)` — check for duplicates (case-insensitive)
- RAM shadow: `static char favNames[30][MAX_GLASS_NAME]` + `static uint8_t favCount`

#### Changes to `browse.cpp`
- `buildLevel()` level 0: add `"-- Favorites --"` at index 1
- `TYPE_OFFSET` changes from 1 to 2
- New level case: when user selects Favorites, show flat list of `favoritesGetName(i)` entries
- Selection from favorites list sets `resultName` and pops (same as product selection)
- Handle empty list: show a single "(No favorites saved)" disabled entry

#### Changes to `main.cpp`
- Call `favoritesInit()` during startup, after `settingsInit()`

#### "Save to Favorites?" prompt after manual entry
- In `browse.cpp`: after manual entry confirmation (`result == 1`), if the name isn't already in favorites and the list isn't full, show a brief confirmation dialog before popping
- Two soft buttons: LEFT = "No" (just use the name), RIGHT = "Save" (add + use)
- This can be a simple inline state in browse rather than a new screen

#### Testing checklist
- [ ] Boot with no favorites saved — verify clean startup, no NVS errors
- [ ] Open browse → Favorites → see "(No favorites saved)"
- [ ] Manual entry → type a name → confirm → "Save to Favorites?" prompt appears
- [ ] Press RIGHT (Save) → name appears in favorites list on next browse
- [ ] Press LEFT (No) → name used for glass but NOT saved to favorites
- [ ] Add 30 favorites → verify 31st add is rejected (no prompt shown)
- [ ] Add duplicate name → verify no prompt shown
- [ ] Power cycle → favorites survive
- [ ] Verify compiled library still works normally (TYPE_OFFSET change didn't break navigation)

---

### Phase 2: Settings Management Screen
**Goal:** View and delete favorites from the on-device settings menu.
**Estimated effort:** 0.5 session

#### New file: `screen_favorites.cpp`
- New screen pushed from settings menu
- Title bar: "FAVORITES (n/30)"
- ScrollList of all saved names
- Soft buttons: LEFT = "Back", RIGHT = "Delete"
- RIGHT triggers confirmation: "Delete [name]?" with LEFT = "No", RIGHT = "Yes"
- After deletion, list refreshes with compacted array
- Empty state: "(No favorites)" centered message, LEFT = "Back" only

#### Changes to `screen_settings.cpp`
- Add "Favorites" menu item (after existing items, before "About" or at end)
- Selecting it pushes `screenFavorites`

#### Changes to `screens.h` / `screens.cpp`
- Declare `extern const Screen screenFavorites`
- Register in screen table if needed

#### Testing checklist
- [ ] Settings → Favorites → see list of saved names with count
- [ ] Select a name → "Delete?" prompt → confirm → name removed, list updates
- [ ] Delete last remaining favorite → screen shows empty state
- [ ] Back button returns to settings menu
- [ ] Delete from middle of list → verify remaining entries compact correctly

---

### Phase 3: Phone Interface
**Goal:** Add and delete favorites from the phone UI.
**Estimated effort:** 0.5 session

#### Changes to `wifi_portal.cpp`

**New WebSocket actions:**
- `{"a":"fav_add","v":"Lagavulin 16"}` — add a favorite, respond with updated list
- `{"a":"fav_del","v":2}` — delete by index, respond with updated list
- `{"a":"fav_list"}` — request current favorites list

**New WebSocket broadcast:**
- `{"t":"favs","list":["name1","name2",...],"cnt":5,"max":30}`
- Sent in response to fav_add, fav_del, fav_list actions

**Phone UI additions (embedded HTML):**
- New collapsible section "Favorites" in the phone page (visible in IDLE / pre-game states)
- Text input + "Add" button for adding names
- List of current favorites with "×" delete button per entry
- Counter showing "n / 30"
- Validation: max 21 chars, no empty strings, duplicate check

#### Testing checklist
- [ ] Phone: open Favorites section, see current list
- [ ] Phone: type a name and tap Add → appears in list and on device browse
- [ ] Phone: tap × on a name → deleted from list and from device
- [ ] Phone: try to add when list is full → shows error message
- [ ] Phone: try to add duplicate → shows error message
- [ ] Device browse → Favorites shows names added from phone
- [ ] Phone reflects names added via device manual entry

---

## File Impact Summary

| File | Phase | Change |
|---|---|---|
| `favorites.h` (new) | 1 | Storage API declarations |
| `favorites.cpp` (new) | 1 | NVS storage, RAM shadow, CRUD operations |
| `browse.cpp` | 1 | Favorites entry in type list, flat list sublevel, save prompt |
| `main.cpp` | 1 | Call `favoritesInit()` at startup |
| `screen_favorites.cpp` (new) | 2 | Settings sub-screen for managing favorites |
| `screen_settings.cpp` | 2 | Add "Favorites" menu item |
| `screens.h` / `screens.cpp` | 2 | Register new screen |
| `wifi_portal.cpp` | 3 | WebSocket actions + phone UI HTML |

## NVS Namespace Layout

```
Namespace: "bffav"
  "cnt"  — uint8_t  — number of stored favorites (0–30)
  "f00"  — string   — favorite name at index 0
  "f01"  — string   — favorite name at index 1
  ...
  "f29"  — string   — favorite name at index 29
```

## Constraints & Notes

- `MAX_GLASS_NAME` (22 chars including null) is the name length limit — same as the compiled library and session persistence. `TEXT_ENTRY_MAX_LEN` (20 chars) is the manual entry limit, which fits within this.
- NVS wear: writes only happen on add/delete, not per-session. At typical use frequency (a few edits per event), NVS endurance (100K+ cycles) is not a concern.
- The favorites list is global — not per-mode. A favorite saved during a Named Flight is available in Best Guess, Ranked, etc.
- No import/export in this spec. A future OTA + web config page could allow bulk loading, but that's a separate feature.
