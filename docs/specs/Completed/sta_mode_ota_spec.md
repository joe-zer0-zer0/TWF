# STA Mode Wi-Fi + OTA Updates — Development Spec

## Overview

Switch the ESP32 from access-point-only mode to a **dual-mode architecture**: station mode (STA) as the primary connection method, with the existing AP mode as fallback. This solves iOS/iPadOS captive-portal timeouts, network-switching behavior, and session loss on phone lock. With STA mode providing internet access, OTA firmware updates become a direct download from a hosted server — no phone-as-middleman required.

---

## Problems Solved

1. **Captive portal timeout** — iOS dismisses the captive portal sheet after ~30 seconds of inactivity, losing the player's session.
2. **Network switching** — iOS/iPadOS deprioritizes Wi-Fi networks without internet, silently reconnecting to a network that has it.
3. **Session loss on lock** — phone disconnects from AP on lock; reopening requires full reconnect and re-login.
4. **OTA complexity** — without internet, firmware updates require the phone to download a binary and push it over WebSocket. With STA mode, the device fetches updates directly.

---

## Design Decisions

### Dual-mode: STA primary, AP fallback

- **STA mode** is used whenever the device has saved Wi-Fi credentials and can connect.
- **AP mode** activates automatically when: no credentials are saved, or the saved network is unreachable after a timeout.
- The user configures Wi-Fi credentials from a new settings sub-screen on the device, or from the phone UI while in AP mode.
- Only one mode is active at a time — the ESP32 supports `WIFI_AP_STA` but running both simultaneously complicates DNS, mDNS, and phone behavior. Keep it simple: one or the other.

### Credential storage

- New NVS namespace `"bfwifi"` stores SSID (max 32 chars) and password (max 63 chars).
- Only one network is stored. Supporting multiple saved networks adds complexity with minimal benefit for the use case (tasting events at a fixed venue).
- A "Forget Network" option clears stored credentials and forces AP mode on next boot.

### Device discovery in STA mode

- **mDNS** — `flight.local` continues to work. This is the primary discovery method.
- **IP display** — the device's IP address is shown on the About screen and briefly on the display after successful STA connection, so users can fall back to direct IP if mDNS fails.
- **Same WebSocket/HTTP ports** — port 80 (HTTP) and port 81 (WebSocket) remain unchanged. The phone UI code doesn't need to change its connection logic beyond using the new address.

### Phone UI changes for STA mode

- In STA mode, there's no captive portal — users open a browser and navigate to `http://flight.local` (or the IP).
- The existing HTML/JS in PROGMEM works as-is. The WebSocket reconnect logic already uses `window.location.hostname`, so it adapts to whatever address the page was loaded from.
- No changes needed to the phone UI HTML for basic STA support.

### Security considerations

- The WebSocket server has no authentication. On a local network, any device can connect and send game commands. For a tasting game at a home/venue, this is acceptable risk.
- **No action needed now.** If desired later, a simple shared PIN (entered on phone, checked on handshake) could be added without architectural changes.
- The device should not be exposed to the internet (no port forwarding). This is a usage guideline, not something enforced in firmware.
- OTA downloads should use HTTPS where possible, but HTTP is acceptable for a local/trusted update server. The ESP32's TLS stack has limited memory; HTTPS to GitHub Releases or S3 works but requires careful memory management.

### OTA architecture

- **Pull model** — the device downloads a firmware binary from a URL. No phone involvement.
- **Trigger:** manual only, from a new "Update" settings menu item. No automatic background checks (saves memory and avoids surprise updates mid-event).
- **Update server:** a simple HTTPS endpoint that serves a version manifest (JSON) and the binary file. GitHub Releases is the simplest option — a public repo's release assets are directly downloadable via URL.
- **Version check:** the device compares its compiled version string against the manifest's latest version. If newer, it offers to update.
- **Partition table:** requires two OTA app partitions. The default `esp32dev` partition table already includes `app0` and `app1` (1.25 MB each), which is sufficient. We'll explicitly set `board_build.partitions = default.csv` in `platformio.ini` to make this intentional rather than implicit.
- **Rollback:** ESP32 IDF supports automatic rollback if the new firmware fails to boot. We'll mark the new partition as valid after successful startup (call `esp_ota_mark_app_valid_cancel_rollback()` in `setup()`).
- **Flash size budget:** current firmware compiles well under 1.25 MB. The 14 KB PROGMEM HTML and font data are the largest contributors. Two 1.25 MB app slots leave room for growth.

### Firmware version scheme

- Semantic versioning: `MAJOR.MINOR.PATCH` (e.g., `1.0.0`).
- Stored as a `#define FW_VERSION "1.0.0"` in `config.h`.
- Displayed on the About screen.
- The update manifest includes the version string and binary URL.

### Update manifest format

Hosted as a small JSON file (e.g., `version.json` alongside the binary):

```json
{
  "version": "1.1.0",
  "url": "https://github.com/<user>/<repo>/releases/download/v1.1.0/firmware.bin",
  "notes": "Bug fixes and new feature X"
}
```

The device fetches this file, compares versions, and if newer, downloads the binary from `url`.

---

## Session Breakdown

### Session 1: STA Mode + Credential Management

**Goal:** Device can join a Wi-Fi network, persist credentials, and serve the phone UI over the local network. AP fallback works when no network is available.

**Estimated effort:** 1 session

#### New NVS namespace: `"bfwifi"`

| Key | Type | Description |
|-----|------|-------------|
| `ssid` | string (32) | Saved network SSID |
| `pass` | string (63) | Saved network password |

#### Changes to `wifi_portal.h` / `wifi_portal.cpp`

- **New state:** track whether running in STA or AP mode (`wifiIsSTAMode()` accessor).
- **`wifiPortalInit()` rewrite:**
  1. Check NVS for saved credentials.
  2. If credentials exist, attempt STA connection with a 10-second timeout.
  3. On success: start HTTP server, WebSocket server, mDNS — same as today but on the STA interface. Show IP on display briefly.
  4. On failure: fall back to AP mode (current behavior). Optionally show "Network not found" briefly.
  5. If no credentials: start AP mode directly (current behavior).
- **No DNS server in STA mode** — the DNS wildcard redirect is only needed for captive portal in AP mode.
- **`wifiPortalStop()` changes:** handle both STA and AP teardown.
- **New functions:**
  - `wifiSaveCredentials(const char* ssid, const char* pass)` — write to NVS.
  - `wifiForgetCredentials()` — clear NVS, flag for AP mode on next init.
  - `wifiGetSSID()` — return saved or connected SSID for display.
  - `wifiGetIP()` — return current IP as string (for display on About screen).
  - `wifiIsConnected()` — check STA connection status.

#### New settings sub-screen: `screen_wifi_setup.cpp`

Accessible from the settings menu (new item between "Wi-Fi" toggle and "Spin Speed"):

- **If no credentials saved:** show "No network configured" with a prompt. Since the device has no full keyboard, Wi-Fi setup uses the phone UI while in AP mode:
  - Display shows: "Connect to BlindFlight Wi-Fi on your phone, then open flight.local to configure"
  - This is the bootstrap flow — you need AP mode to configure STA mode.
- **If credentials saved:** show connected SSID, IP address, signal strength (RSSI as bars), and options:
  - **"Forget Network"** — clears credentials, confirms, notes AP mode will activate on restart.
  - **"Reconnect"** — attempts to reconnect to saved network.
- **Back button** returns to settings.

#### Phone UI addition (AP mode only): Wi-Fi configuration section

- New collapsible section in the phone HTML (like Favorites): "Wi-Fi Setup".
- Shows a scan button that triggers a new WebSocket action to scan for networks.
- Displays found networks as a selectable list (SSID + signal strength).
- Password field for the selected network.
- "Connect" button sends credentials to device via WebSocket.
- Device saves credentials, attempts connection, reports success/failure back.
- On success, the phone UI shows the new IP/mDNS address to reconnect to.

**New WebSocket messages:**

| Direction | Action | Payload | Description |
|-----------|--------|---------|-------------|
| Phone → Device | `wifi_scan` | — | Trigger network scan |
| Device → Phone | `wifi_scan_result` | `{"t":"wifi_nets","nets":[{"s":"SSID","r":-67,"e":true},...]}` | Scan results (SSID, RSSI, encrypted) |
| Phone → Device | `wifi_connect` | `{"a":"wifi_connect","s":"SSID","p":"password"}` | Save creds and attempt connection |
| Device → Phone | `wifi_status` | `{"t":"wifi_status","ok":true,"ip":"192.168.1.42"}` | Connection result |
| Phone → Device | `wifi_forget` | — | Clear saved credentials |

#### Changes to `config.h`

- `#define FW_VERSION "1.0.0"` — initial version string.
- `#define WIFI_STA_TIMEOUT 10000` — STA connection timeout in ms.
- `#define WIFI_MAX_SCAN_RESULTS 15` — cap scan list size.
- `#define NVS_NS_WIFI "bfwifi"` — namespace constant.

#### Changes to `screens.h` / `screens.cpp`

- Add `screenWifiSetup` to the screen table.
- Register its `onEnter`, `draw`, `input`, and `onExit` functions.

#### Changes to `screen_settings.cpp`

- Add `SET_WIFISETUP` menu item (label: "Wi-Fi Setup") after the Wi-Fi toggle.
- Pushes to `screenWifiSetup` on selection.
- Update `SET_COUNT`.

#### Changes to `main.cpp`

- Call `wifiPortalInit()` as today — the init function itself now decides STA vs AP.
- Display firmware version in boot splash or About screen.

#### Changes to About screen

- Show firmware version (`FW_VERSION`).
- Show current Wi-Fi mode (STA/AP), SSID, and IP.

#### Testing checklist

- [ ] Fresh boot with no saved credentials → AP mode starts, phone connects as before
- [ ] Phone UI shows Wi-Fi Setup section in AP mode
- [ ] Scan finds local networks with signal strength
- [ ] Enter password and connect → device joins network, phone gets new IP
- [ ] Power cycle → device auto-connects to saved network (STA mode)
- [ ] Phone on same network navigates to `flight.local` → phone UI loads
- [ ] WebSocket connects and game works normally over local network
- [ ] Phone lock/unlock → WebSocket reconnects without losing session (key improvement)
- [ ] Settings screen shows connected SSID and IP
- [ ] "Forget Network" clears credentials, next boot returns to AP mode
- [ ] Saved network unavailable → device falls back to AP after timeout
- [ ] About screen shows firmware version, Wi-Fi mode, SSID, IP

---

### Session 2: OTA Firmware Updates

**Goal:** Device can check for and download firmware updates over Wi-Fi. Manual trigger from settings menu.

**Estimated effort:** 1 session

**Prerequisite:** Session 1 (STA mode with internet access)

#### New module: `ota.h` / `ota.cpp`

- `otaInit()` — no-op for now (no background tasks). Called from `setup()` for future use.
- `otaCheckForUpdate(const char* manifestUrl)` — fetches the version manifest JSON, parses it, compares against `FW_VERSION`. Returns a struct: `{bool available, char version[16], char url[256], char notes[128]}`.
- `otaPerformUpdate(const char* binaryUrl)` — downloads the binary and flashes it using `esp_http_client` + `esp_ota_ops` (or the Arduino `HTTPUpdate` wrapper). Returns success/failure. Calls a progress callback for UI updates.
- `otaMarkValid()` — called from `setup()` after successful boot to confirm the running partition is good (`esp_ota_mark_app_valid_cancel_rollback()`).

#### New settings sub-screen: OTA update screen

Accessible from settings menu as "Update" item (after "About"):

**Flow:**

1. **Entry:** "Check for Updates?" [LEFT: Back] [RIGHT: Check]
2. **Checking:** "Checking..." with spinner/animation. Device fetches manifest.
3. **Result — no update:** "Up to date (v1.0.0)" [LEFT: Back]
4. **Result — update available:** "Update available: v1.1.0" + truncated release notes. [LEFT: Cancel] [RIGHT: Install]
5. **Downloading:** Progress bar showing download percentage. "Downloading v1.1.0... 45%". Non-cancellable once started (partial flash is dangerous).
6. **Success:** "Update complete! Restarting..." Auto-reboot after 2 seconds.
7. **Failure:** "Update failed: <reason>" [LEFT: Back]. Reasons: network error, download interrupted, verification failed, not enough space.

**Important:** OTA download is a blocking HTTP operation that will freeze the WebSocket server. This is acceptable because:
- Updates are manual and intentional (not during a game).
- The display shows progress directly.
- We can add a warning: "Wi-Fi clients will disconnect during update."

#### Changes to `platformio.ini`

```ini
board_build.partitions = default.csv
```

Explicitly set to make the dual-OTA partition table intentional. Verify the default table provides two app partitions of sufficient size.

#### Changes to `config.h`

- `#define OTA_MANIFEST_URL "https://..."` — default manifest URL (can be overridden later via NVS if needed).
- `#define OTA_CHECK_TIMEOUT 10000` — HTTP timeout for manifest fetch.
- `#define OTA_DOWNLOAD_TIMEOUT 120000` — HTTP timeout for binary download (large file).

#### Changes to `main.cpp`

- Call `otaMarkValid()` early in `setup()` — confirms current firmware is good, prevents automatic rollback.

#### Changes to `screen_settings.cpp`

- Add `SET_UPDATE` menu item (label: "Update"). Only visible/enabled when in STA mode with internet access.
- Pushes to the OTA update screen.

#### Changes to `screens.h` / `screens.cpp`

- Add `screenOtaUpdate` to the screen table.

#### Update server setup (not firmware — done on GitHub/hosting)

- Create a GitHub repo (or use existing) with Releases.
- Each release includes:
  - `firmware.bin` — the compiled binary (PlatformIO `.pio/build/esp32/firmware.bin`).
  - `version.json` — the manifest file.
- Document the release process: bump `FW_VERSION` in `config.h`, build, create GitHub Release, upload both files.

#### HTTPS considerations

- ESP32 Arduino's `HTTPClient` supports HTTPS but requires either:
  - A root CA certificate embedded in firmware (works but must be updated when cert rotates), or
  - `setInsecure()` to skip verification (acceptable for firmware from a known URL — the binary itself can be checksum-verified).
- **Recommendation:** use `setInsecure()` for the HTTPS connection, and add a SHA-256 hash to the manifest for binary integrity verification. This avoids cert management while still ensuring the downloaded binary matches what was published.
- Alternative: serve the manifest and binary from an HTTP (not HTTPS) endpoint. Simpler but no transport security. Fine for a tasting device on a home network.

#### Manifest with integrity check

```json
{
  "version": "1.1.0",
  "url": "https://github.com/<user>/<repo>/releases/download/v1.1.0/firmware.bin",
  "sha256": "a1b2c3d4...",
  "size": 823456,
  "notes": "Bug fixes and new feature X"
}
```

The device verifies the SHA-256 of the downloaded binary before committing the flash. If mismatch, abort and report error.

#### Testing checklist

- [ ] "Update" menu item appears in settings when in STA mode
- [ ] "Update" menu item hidden or shows "Requires Wi-Fi" when in AP mode
- [ ] Check for updates with no update available → shows "Up to date"
- [ ] Check for updates with update available → shows version and notes
- [ ] Cancel on update-available screen → returns to settings
- [ ] Install update → progress bar shows download progress
- [ ] Successful update → device reboots, runs new firmware
- [ ] About screen shows new version after update
- [ ] Failed download (kill network mid-transfer) → error message, device continues on current firmware
- [ ] Manifest unreachable → appropriate error message
- [ ] SHA-256 mismatch → update aborted with error

---

## Files Changed/Created Summary

### Session 1
| File | Change |
|------|--------|
| `wifi_portal.h` | New function declarations, STA mode accessors |
| `wifi_portal.cpp` | STA/AP dual-mode init, credential storage, scan/connect WebSocket handlers, phone UI HTML additions |
| `config.h` | `FW_VERSION`, STA timeout, scan limit, NVS namespace |
| `screen_wifi_setup.cpp` | **New file** — Wi-Fi setup settings sub-screen |
| `screen_settings.cpp` | New "Wi-Fi Setup" menu item |
| `screens.h` | New screen declaration |
| `screens.cpp` | New screen registration |
| `main.cpp` | Minor — init flow unchanged, version display |

### Session 2
| File | Change |
|------|--------|
| `ota.h` | **New file** — OTA function declarations |
| `ota.cpp` | **New file** — manifest fetch, download, flash, rollback |
| `screen_settings.cpp` | New "Update" menu item |
| `screens.h` | New screen declaration |
| `screens.cpp` | New screen registration |
| `config.h` | OTA manifest URL, timeouts |
| `platformio.ini` | Explicit partition table |
| `main.cpp` | `otaMarkValid()` call in setup |

---

## Future Enhancements (Not in Scope)

- **Automatic update check on boot** — show a notification badge on settings if update available. Deferred to avoid complexity and surprise behavior.
- **Multiple saved networks** — store a list of SSIDs/passwords. Overkill for current use case.
- **WebSocket authentication** — PIN-based access control. Add if needed after beta testing reveals issues.
- **Phone-pushed OTA** — upload binary via phone WebSocket while in AP mode (for venues without Wi-Fi). Complex; defer unless STA mode proves insufficient for beta.
- **OTA via AP mode** — if a phone has the binary cached, it could serve it to the ESP32 over the AP network. Very niche; not planned.
