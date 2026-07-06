#pragma once

// ============================================================
// Blind Flight — Wi-Fi Portal Module (Session 15, updated 17)
// ============================================================
// ESP32 soft AP with captive portal, embedded web server,
// and WebSocket server for real-time phone control.
//
//   - Soft AP: SSID "BlindFlight", no password
//   - DNS redirect: all DNS queries → ESP32 IP (captive portal)
//   - mDNS: http://flight.local
//   - HTTP server (port 80): serves interactive phone UI
//   - WebSocket server (port 81): bidirectional state sync
//
// Usage:
//   Call wifiPortalInit() once in setup() (if Wi-Fi enabled).
//   Call wifiPortalUpdate() every loop iteration.
//   Call wifiPortalGetClientCount() to check connections.
//
// Session 17 additions:
//   - wifiPortalStop() — tear down AP and all servers
//   - wifiPortalIsRunning() — check if portal is active
//   - Conditional init: main.cpp only calls init if settings say Wi-Fi on
// ============================================================

#include <Arduino.h>

// --- API ---

// Start the soft AP, DNS server, web server, WebSocket server,
// and mDNS. Call once during setup(), after display and other
// modules. No-op if already running.
void wifiPortalInit();

// Stop the soft AP, DNS, web server, WebSocket server, and
// mDNS. Disconnects all clients. No-op if not running.
void wifiPortalStop();

// Poll DNS, web server, and WebSocket server. Call every loop
// iteration. Also handles state broadcast to connected phones.
// Returns immediately if portal is not running.
void wifiPortalUpdate();

// Number of Wi-Fi stations currently connected to the AP.
// Returns 0 if portal is not running.
int wifiPortalGetClientCount();

// Immediately push a state snapshot to all connected phones,
// bypassing the periodic broadcast throttle in wifiPortalUpdate().
// Intended for callers (e.g. the game module) about to enter a
// blocking operation — such as a motor spin — during which the
// main loop won't reach wifiPortalUpdate() for several seconds.
// No-op if the portal is not running.
void wifiPortalBroadcastNow();

// True if the portal has been initialized and not stopped.
bool wifiPortalIsRunning();
