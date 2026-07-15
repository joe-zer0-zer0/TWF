#pragma once

// ============================================================
// Blind Flight — OTA Firmware Update Module
// ============================================================
// Pull-model OTA: device fetches a JSON manifest from a URL,
// compares versions, and downloads+flashes the binary.
//
// Usage:
//   otaMarkValid()       — call early in setup() to confirm
//                           the running partition (prevents
//                           automatic rollback)
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

// Call early in setup() to mark the current firmware as valid,
// preventing automatic rollback to the previous partition.
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
                      OtaProgressCallback progressCb,
                      char* errMsg, int errMsgLen);
