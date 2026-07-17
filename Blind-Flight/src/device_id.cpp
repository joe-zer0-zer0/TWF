#include "device_id.h"
#include <esp_system.h>

static char serialBuf[16];  // "BF-XXXXXXXXXXXX" + null
static uint64_t macValue = 0;

void deviceIdInit() {
    macValue = ESP.getEfuseMac();

    uint8_t* m = (uint8_t*)&macValue;
    snprintf(serialBuf, sizeof(serialBuf),
             "BF-%02X%02X%02X%02X%02X%02X",
             m[0], m[1], m[2], m[3], m[4], m[5]);

    Serial.printf("[Device] Serial: %s\n", serialBuf);
}

const char* deviceGetSerial() {
    return serialBuf;
}

uint64_t deviceGetMAC() {
    return macValue;
}
