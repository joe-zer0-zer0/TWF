#pragma once

// ============================================================
// Blind Flight — OTA Firmware Update Module
// ============================================================
// Pull-model OTA: device fetches a JSON manifest from a URL,
// compares versions, and downloads+flashes the binary.
//
// Usage:
//   otaValidateTick()    — call every loop(); commits the running
//                           image once it has proven healthy
//   otaCheckForUpdate()  — fetch manifest, compare versions
//   otaPerformUpdate()   — download and flash the binary
// ============================================================

#include <Arduino.h>

struct OtaUpdateInfo {
    bool available;
    char version[16];
    char url[256];
    char notes[128];
    uint32_t size;
    char sha256[65];
};

// Progress callback: called periodically during download.
// progress = bytes downloaded, total = expected file size.
typedef void (*OtaProgressCallback)(uint32_t progress, uint32_t total);

// ============================================================
// Rollback safety (v1.4.2)
// ============================================================
// The bootloader ships with CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y,
// so a freshly-installed image boots in PENDING_VERIFY state. If the
// device reboots while still pending — including the automatic reboot
// after a panic or a watchdog timeout — the bootloader marks the image
// invalid and reverts to the previous partition.
//
// That protection only exists between boot and the moment the app
// confirms itself. Confirming on the first line of setup() (as this
// module used to) throws it away: a build that crashes during display
// init, homing, or Wi-Fi bring-up has already cancelled its own
// rollback. With the USB port inaccessible on the assembled device,
// that would leave no recovery path at all.
//
// So confirmation is now earned: the image must run for
// OTA_VALIDATE_UPTIME_MS without panicking.
//
// NOTE: this catches crashes, not hangs. Arduino's loopTask runs on
// core 1, whose idle task is not watched by the task WDT, so a loop
// that blocks forever will not reboot and will not roll back.

enum OtaValidState {
    OTA_VALID_CONFIRMED,  // committed — rollback no longer possible
    OTA_VALID_PENDING,    // first boot of a new image; a reboot now reverts it
    OTA_VALID_UNKNOWN     // no OTA state (factory / USB-flashed image)
};

// Call once per loop(). Reads the running partition's state on first
// call, then commits the image after OTA_VALIDATE_UPTIME_MS of healthy
// uptime. Cheap no-op once confirmed.
void otaValidateTick();

// Validation state of the running image, for UI and diagnostics.
OtaValidState otaGetValidState();

// Seconds until a pending image self-confirms. 0 if not pending.
uint32_t otaValidateSecondsRemaining();

// Commit the running image immediately, cancelling rollback. Called by
// otaValidateTick() when the uptime gate passes; exposed for an explicit
// "confirm now" path. Prefer the tick — confirming early is what this
// whole mechanism exists to avoid.
void otaMarkValid();

// Fetch the manifest and compare against FW_VERSION.
// Returns true on success (check info.available for update status).
// Returns false on network/parse error (errMsg filled).
bool otaCheckForUpdate(const char* manifestUrl, OtaUpdateInfo& info,
                       char* errMsg, int errMsgLen);

// Download the binary and flash it. Blocks until complete.
// Returns true on success (caller should reboot).
// Returns false on failure (errMsg filled).
// progressCb is called periodically for UI updates.
bool otaPerformUpdate(const char* binaryUrl, uint32_t expectedSize,
                      const char* expectedSha256,
                      OtaProgressCallback progressCb,
                      char* errMsg, int errMsgLen);
