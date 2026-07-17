#pragma once

// ============================================================
// Blind Flight — Wi-Fi Portal Module
// ============================================================
// Dual-mode Wi-Fi: STA (station) primary, AP fallback.
//
// STA mode: device joins a local network. Phone connects to
//   the same network and browses to http://flight.local.
//   Solves iOS captive-portal timeouts and session loss.
//
// AP mode: fallback when no credentials are saved or the saved
//   network is unreachable. Phone connects to "BlindFlight" AP.
//
// HTTP server (port 80), WebSocket server (port 81), mDNS
// (flight.local) work identically in both modes.
// ============================================================

#include <Arduino.h>

// --- Core API ---

void wifiPortalInit();
void wifiPortalStop();
void wifiPortalUpdate();
int  wifiPortalGetClientCount();
void wifiPortalBroadcastNow();
bool wifiPortalIsRunning();

// H2H broadcast
void wifiPortalH2HBroadcast();

// --- STA mode API ---

bool wifiIsSTAMode();
bool wifiIsConnected();
const char* wifiGetSSID();
String wifiGetIP();
int8_t wifiGetRSSI();

// --- Credential management ---

bool wifiHasCredentials();
void wifiSaveCredentials(const char* ssid, const char* pass);
void wifiForgetCredentials();
void wifiReconnect();

// --- Security ---

const char* wifiGetAPPassword();
const char* wifiGetPIN();

// --- Pending wifi_connect confirmation ---

bool wifiGetPendingConnect();
const char* wifiGetPendingSSID();
void wifiConfirmConnect(bool confirm);
