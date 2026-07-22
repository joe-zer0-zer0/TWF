#include "ota.h"
#include "config.h"

#include <HTTPClient.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <mbedtls/md.h>

// ============================================================
// Blind Flight — OTA Firmware Update Module
// ============================================================

void otaMarkValid() {
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("[OTA] Current firmware marked valid");
}

// ============================================================
// Simple version comparison: "1.2.3" vs "1.2.4"
// Returns: -1 if a < b, 0 if equal, 1 if a > b
// ============================================================

static int compareVersions(const char* a, const char* b) {
    int aMajor = 0, aMinor = 0, aPatch = 0;
    int bMajor = 0, bMinor = 0, bPatch = 0;

    sscanf(a, "%d.%d.%d", &aMajor, &aMinor, &aPatch);
    sscanf(b, "%d.%d.%d", &bMajor, &bMinor, &bPatch);

    if (aMajor != bMajor) return (aMajor < bMajor) ? -1 : 1;
    if (aMinor != bMinor) return (aMinor < bMinor) ? -1 : 1;
    if (aPatch != bPatch) return (aPatch < bPatch) ? -1 : 1;
    return 0;
}

// ============================================================
// JSON string extraction helper (no external JSON library)
// ============================================================

static bool jsonExtractString(const char* json, const char* key,
                              char* out, int outLen) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char* pos = strstr(json, needle);
    if (!pos) return false;

    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':') pos++;
    if (*pos != '"') return false;
    pos++;

    int i = 0;
    while (*pos && *pos != '"' && i < outLen - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++;
            out[i++] = *pos;
        } else {
            out[i++] = *pos;
        }
        pos++;
    }
    out[i] = '\0';
    return true;
}

static bool jsonExtractUint32(const char* json, const char* key,
                              uint32_t& out) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char* pos = strstr(json, needle);
    if (!pos) return false;

    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':') pos++;

    out = (uint32_t)strtoul(pos, nullptr, 10);
    return true;
}

static bool jsonExtractBool(const char* json, const char* key, bool& out) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);

    const char* pos = strstr(json, needle);
    if (!pos) return false;

    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':') pos++;

    if (strncmp(pos, "true", 4) == 0)       { out = true;  return true; }
    if (strncmp(pos, "false", 5) == 0)       { out = false; return true; }
    return false;
}

// ============================================================
// Manifest fetch + version check
// ============================================================

bool otaCheckForUpdate(const char* manifestUrl, OtaUpdateInfo& info,
                       char* errMsg, int errMsgLen) {
    memset(&info, 0, sizeof(info));

    HTTPClient http;
    http.setTimeout(OTA_CHECK_TIMEOUT);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    http.begin(manifestUrl);
    int httpCode = http.GET();

    if (httpCode != 200) {
        http.end();
        if (httpCode < 0) {
            snprintf(errMsg, errMsgLen, "Network error");
        } else {
            snprintf(errMsg, errMsgLen, "Server error: %d", httpCode);
        }
        return false;
    }

    String body = http.getString();
    http.end();

    if (body.length() < 10 || body.length() > 2048) {
        snprintf(errMsg, errMsgLen, "Invalid manifest");
        return false;
    }

    const char* json = body.c_str();

    if (!jsonExtractString(json, "version", info.version, sizeof(info.version))) {
        snprintf(errMsg, errMsgLen, "Missing version");
        return false;
    }

    if (!jsonExtractString(json, "url", info.url, sizeof(info.url))) {
        snprintf(errMsg, errMsgLen, "Missing URL");
        return false;
    }

    jsonExtractString(json, "notes", info.notes, sizeof(info.notes));
    jsonExtractUint32(json, "size", info.size);
    jsonExtractString(json, "sha256", info.sha256, sizeof(info.sha256));

    // "force": true in the manifest bypasses version comparison —
    // used during dev iteration to avoid burning version numbers.
    bool forceUpdate = false;
    jsonExtractBool(json, "force", forceUpdate);

    if (forceUpdate) {
        info.available = true;
    } else {
        info.available = (compareVersions(FW_VERSION, info.version) < 0);
    }

    Serial.printf("[OTA] Current: v%s, Remote: v%s, Force: %s, Update: %s\n",
                  FW_VERSION, info.version,
                  forceUpdate ? "yes" : "no",
                  info.available ? "available" : "up to date");

    return true;
}

// ============================================================
// Binary download + flash
// ============================================================

bool otaPerformUpdate(const char* binaryUrl, uint32_t expectedSize,
                      const char* expectedSha256,
                      OtaProgressCallback progressCb,
                      char* errMsg, int errMsgLen) {
    HTTPClient http;
    http.setTimeout(OTA_DOWNLOAD_TIMEOUT);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    http.begin(binaryUrl);
    int httpCode = http.GET();

    if (httpCode != 200) {
        http.end();
        if (httpCode < 0) {
            snprintf(errMsg, errMsgLen, "Download failed");
        } else {
            snprintf(errMsg, errMsgLen, "HTTP %d", httpCode);
        }
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        http.end();
        snprintf(errMsg, errMsgLen, "Unknown file size");
        return false;
    }

    if (!Update.begin(contentLength)) {
        http.end();
        snprintf(errMsg, errMsgLen, "Not enough space");
        return false;
    }

    // Initialize SHA-256 context if hash is provided
    bool doHash = (expectedSha256 && expectedSha256[0]);
    mbedtls_md_context_t mdCtx;
    if (doHash) {
        mbedtls_md_init(&mdCtx);
        mbedtls_md_setup(&mdCtx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
        mbedtls_md_starts(&mdCtx);
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    uint32_t written = 0;
    unsigned long lastProgress = 0;

    while (written < (uint32_t)contentLength) {
        int available = stream->available();
        if (available <= 0) {
            if (!stream->connected()) break;
            delay(1);
            continue;
        }

        int toRead = min(available, (int)sizeof(buf));
        int bytesRead = stream->readBytes(buf, toRead);
        if (bytesRead <= 0) break;

        if (doHash) {
            mbedtls_md_update(&mdCtx, buf, bytesRead);
        }

        int bytesWritten = Update.write(buf, bytesRead);
        if (bytesWritten != bytesRead) {
            if (doHash) mbedtls_md_free(&mdCtx);
            Update.abort();
            http.end();
            snprintf(errMsg, errMsgLen, "Flash write error");
            return false;
        }

        written += bytesWritten;

        if (progressCb && millis() - lastProgress >= 200) {
            lastProgress = millis();
            progressCb(written, contentLength);
        }
    }

    http.end();

    if (written != (uint32_t)contentLength) {
        if (doHash) mbedtls_md_free(&mdCtx);
        Update.abort();
        snprintf(errMsg, errMsgLen, "Incomplete download");
        return false;
    }

    // Verify SHA-256 hash
    if (doHash) {
        uint8_t hash[32];
        mbedtls_md_finish(&mdCtx, hash);
        mbedtls_md_free(&mdCtx);

        char hexHash[65];
        for (int i = 0; i < 32; i++) {
            snprintf(hexHash + i * 2, 3, "%02x", hash[i]);
        }

        if (strcasecmp(hexHash, expectedSha256) != 0) {
            Update.abort();
            snprintf(errMsg, errMsgLen, "Hash mismatch");
            Serial.printf("[OTA] SHA-256 mismatch!\n  Expected: %s\n  Got:      %s\n",
                          expectedSha256, hexHash);
            return false;
        }
        Serial.println("[OTA] SHA-256 verified OK");
    }

    if (!Update.end(true)) {
        snprintf(errMsg, errMsgLen, "Verify failed");
        return false;
    }

    Serial.printf("[OTA] Update complete: %u bytes\n", written);
    return true;
}
