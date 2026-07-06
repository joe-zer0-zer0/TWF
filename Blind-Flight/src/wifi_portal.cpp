#include "wifi_portal.h"
#include "game.h"
#include "config.h"
#include "input.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>

// ============================================================
// Blind Flight — Wi-Fi Portal Module (Session 15, updated 17)
// ============================================================
// Soft AP "BlindFlight" with captive portal redirect, embedded
// web server (port 80), and WebSocket server (port 81) for
// real-time phone control.
//
// HTTP Routes:
//   /              — full interactive phone UI
//   /generate_204  — Android captive-portal probe
//   /hotspot-detect.html — iOS captive-portal probe
//   /connecttest.txt     — Windows captive-portal probe
//   /*             — fallback redirect to /
//
// WebSocket Protocol (port 81):
//   Server → Client: JSON state snapshot on every state change
//   Client → Server: JSON action commands
//
// mDNS: http://flight.local
// ============================================================

// --- Network configuration ---
static const char* AP_SSID     = "BlindFlight";
static const char* AP_PASSWORD = nullptr;   // Open network (no password)
static const byte  DNS_PORT    = 53;
static const char* MDNS_NAME   = "flight";  // flight.local

// --- Module instances ---
static DNSServer         dnsServer;
static WebServer         webServer(80);
static WebSocketsServer  wsServer(81);

// --- Portal running flag (Session 17) ---
static bool portalRunning = false;

// --- State broadcast tracking ---
static unsigned long lastBroadcastMs = 0;
static bool     lastBcActive    = false;
static int      lastBcState     = -1;
static int      lastBcPour      = -1;
static int      lastBcReveal    = -1;
static int      lastBcGuess     = -1;

// ============================================================
// Embedded HTML page — interactive phone UI (PROGMEM)
// ============================================================
// Dark-themed, mobile-responsive, context-sensitive layout
// that updates in real time via WebSocket. Matches the device
// aesthetic (dark background, amber accents).

static const char PAGE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>Blind Flight</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#111;color:#eee;min-height:100vh;display:flex;flex-direction:column;
align-items:center;padding:16px}
.hdr{text-align:center;margin-bottom:12px}
.logo{font-size:22px;font-weight:700;color:#D4A547;letter-spacing:2px}
.sub{font-size:12px;color:#888}
.conn{margin-top:6px;font-size:13px}
.cdot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}
.con{background:#4CAF50}.coff{background:#F44336}
.card{background:#1a1a1a;border:1px solid #333;border-radius:16px;padding:24px;
max-width:380px;width:100%;margin-bottom:12px}
.st{text-align:center;font-size:18px;font-weight:600;margin-bottom:12px;color:#D4A547}
.big{text-align:center;font-size:56px;font-weight:700;color:#D4A547;margin:4px 0}
.nm{text-align:center;font-size:17px;color:#D4A547;margin:6px 0}
.msg{text-align:center;font-size:15px;color:#aaa;margin:10px 0}
.hint{text-align:center;font-size:13px;color:#666;margin-top:6px}
input[type=text]{width:100%;padding:14px;background:#222;border:2px solid #444;
border-radius:10px;color:#eee;font-size:18px;margin-bottom:10px;outline:none}
input[type=text]:focus{border-color:#D4A547}
.btn{width:100%;padding:14px;border:none;border-radius:10px;font-size:16px;
font-weight:600;cursor:pointer;margin-bottom:8px;transition:transform .1s}
.btn:active{transform:scale(.97)}
.bp{background:#D4A547;color:#111}
.bs{background:#333;color:#ccc}
.bd{background:#444;color:#F44336}
.btns{display:flex;gap:8px}.btns .btn{flex:1}
.glasses{display:flex;justify-content:center;gap:8px;margin:12px 0}
.gb{width:48px;height:48px;border-radius:8px;display:flex;align-items:center;
justify-content:center;font-weight:700;font-size:18px;flex-direction:column}
.gu{background:#D4A547;color:#111}.ge{background:#333;color:#888}
.pi{padding:14px;background:#222;border:1px solid #444;border-radius:10px;
margin-bottom:8px;cursor:pointer;font-size:16px;text-align:center}
.pi:active{background:#D4A547;color:#111}
.dots{display:flex;justify-content:center;gap:10px;margin:12px 0}
.dt{width:12px;height:12px;border-radius:50%}
.don{background:#D4A547}.dof{background:#444}
.rr{display:flex;align-items:center;padding:8px 0;border-bottom:1px solid #222}
.rn{font-weight:700;color:#D4A547;min-width:28px}
.score{text-align:center;font-size:48px;font-weight:700;margin:8px 0}
.chk{font-size:12px}.cok{color:#4CAF50}.cnk{color:#F44336}
</style>
</head>
<body>
<div class="hdr">
<div class="logo">BLIND FLIGHT</div>
<div class="sub">The Whiskey Flight</div>
<div class="conn"><span class="cdot coff" id="cd"></span><span class="hint" id="cs">Connecting...</span></div>
</div>
<div class="card" id="v"></div>
<script>
var S=null,ws=null,lk=false;
function cn(){
ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onopen=function(){
$('cd').className='cdot con';$('cs').textContent='Connected';
};
ws.onclose=function(){
$('cd').className='cdot coff';$('cs').textContent='Reconnecting...';
S=null;lk=false;rn();setTimeout(cn,2000);
};
ws.onmessage=function(e){
try{S=JSON.parse(e.data);lk=false;rn();}catch(x){}
};
}
function tx(o){
if(lk)return;lk=true;
if(ws&&ws.readyState===1)ws.send(JSON.stringify(o));
}
function $(id){return document.getElementById(id)}
function esc(s){if(!s)return'';var d=document.createElement('div');d.textContent=s;return d.innerHTML;}

function rn(){
var v=$('v');
if(!S){v.innerHTML='<div class="msg">Connecting to device...</div>';return;}
if(S.a&&S.sp){
v.innerHTML='<div class="st">Spinning...</div>'+
'<div class="msg">Selecting a glass</div>'+
'<div class="hint">Please wait</div>';
return;
}
if(!S.a){
v.innerHTML='<div class="st">Ready</div>'+
'<div class="msg">Start a flight</div>'+
'<button class="btn bp" onclick="tx({a:\'start\',v:\'B\'})">Basic Flight</button>'+
'<button class="btn bp" onclick="tx({a:\'start\',v:\'N\'})">Named Flight</button>'+
'<button class="btn bp" onclick="tx({a:\'start\',v:\'G\'})">Best Guess</button>'+
'<div class="hint">Or start from the device</div>';
return;
}
var s=S.s,h='';
if(s==='NM'){
h+='<div class="st">Pour '+(S.p+1)+'</div>';
h+='<input type="text" id="ni" placeholder="Enter whiskey name" maxlength="21" autocomplete="off" autocapitalize="words">';
h+='<button class="btn bp" onclick="sn()">Submit Name</button>';
if(S.p>0)h+='<button class="btn bs" onclick="tx({a:\'left\'})">Skip — End Flight</button>';
h+='<div class="hint">Type a name and tap Submit</div>';
}else if(s==='PO'){
h+='<div class="st">Pour '+(S.p+1)+'</div>';
h+='<div class="big">'+(S.p+1)+'</div>';
if(S.cn)h+='<div class="nm">'+esc(S.cn)+'</div>';
h+='<div class="msg">Ready To Pour</div>';
h+='<button class="btn bp" onclick="tx({a:\'right\'})">Done — Poured</button>';
h+='<button class="btn bs" onclick="tx({a:\'left\'})">Skip — End Flight</button>';
}else if(s==='TA'){
h+='<div class="st">Tasting Time</div>';
h+='<div class="msg">'+S.p+' Glasses Poured</div>';
h+=gl();
h+='<div class="msg">Enjoy Your Flight</div>';
if(S.m==='G')h+='<button class="btn bp" onclick="tx({a:\'right\'})">Start Guessing</button>';
else h+='<button class="btn bp" onclick="tx({a:\'right\'})">Reveal</button>';
}else if(s==='GU'){
h+='<div class="st">Glass '+S.gg+'</div>';
h+='<div class="msg">Which whiskey is this?</div>';
for(var i=0;i<S.gp.length;i++)
h+='<div class="pi" onclick="tx({a:\'guess\',v:'+i+'})">'+esc(S.gp[i])+'</div>';
h+='<button class="btn bs" onclick="tx({a:\'left\'})">Back</button>';
}else if(s==='GS'){
h+='<div class="st">Results</div>';
h+='<div class="score">'+S.gc+' of '+S.rc+'</div>';
h+='<div class="msg">Correct</div>';
h+='<div class="btns"><button class="btn bs" onclick="tx({a:\'left\'})">Re-guess</button>';
h+='<button class="btn bp" onclick="tx({a:\'right\'})">Next</button></div>';
}else if(s==='GD'){
h+='<div class="st">Your Guesses</div>';
h+=gd();
h+='<div class="msg">'+S.gc+' of '+S.rc+' Correct</div>';
h+='<div class="btns"><button class="btn bs" onclick="tx({a:\'left\'})">Re-guess</button>';
h+='<button class="btn bp" onclick="tx({a:\'right\'})">Reveal</button></div>';
}else if(s==='RE'){
h+='<div class="st">Glass '+S.rg+'</div>';
h+='<div class="nm" style="font-size:22px">'+esc(S.rn)+'</div>';
h+='<div class="hint">Pour #'+(S.rp+1)+'</div>';
h+=dt();
var last=(S.ri>=S.rc-1);
h+='<button class="btn bp" onclick="tx({a:\'right\'})">'+(last?'Finish':'Next')+'</button>';
}else if(s==='DO'){
h+='<div class="st" style="color:#4CAF50">Flight Complete</div>';
h+=rs();
h+='<div class="btns"><button class="btn bs" onclick="tx({a:\'left\'})">Exit</button>';
h+='<button class="btn bp" onclick="tx({a:\'right\'})">New Flight</button></div>';
}
v.innerHTML=h;
if(s==='NM'){var ni=$('ni');if(ni)ni.focus();}
}
function sn(){var ni=$('ni');if(ni&&ni.value.trim())tx({a:'name',v:ni.value.trim()});}
function gl(){
var h='<div class="glasses">';
for(var i=0;i<4;i++)h+='<div class="gb '+(S.u[i]?'gu':'ge')+'">'+(i+1)+'</div>';
return h+'</div>';
}
function gd(){
var h='<div class="glasses">';
for(var i=0;i<4;i++){
if(S.u[i]){
var ok=S.gx&&S.gx[i]!==undefined?S.gx[i]:false;
h+='<div class="gb gu">'+(i+1)+'<span class="chk '+(ok?'cok':'cnk')+'">'+(ok?'✓':'✗')+'</span></div>';
}else h+='<div class="gb ge">'+(i+1)+'</div>';
}
return h+'</div>';
}
function dt(){
var h='<div class="dots">';
for(var i=0;i<S.rc;i++)h+='<div class="dt '+(i<=S.ri?'don':'dof')+'"></div>';
return h+'</div>';
}
function rs(){
var h='';
if(S.rv)for(var i=0;i<S.rv.length;i++){
var e=S.rv[i];
h+='<div class="rr"><span class="rn">'+e.g+'.</span><span>'+esc(e.n)+'</span></div>';
}
return h;
}
document.addEventListener('keydown',function(e){
if(e.key==='Enter'&&S&&S.s==='NM')sn();
});
cn();
</script>
</body>
</html>
)rawliteral";

// ============================================================
// JSON state builder
// ============================================================
// Builds a JSON snapshot of the current game state and writes
// it into the provided buffer. Returns the number of bytes
// written (excluding null terminator).
//
// Safety note (bug 10): snprintf() returns the length it *would*
// have written had the buffer been big enough, not the number of
// bytes actually written. So after a truncated snprintf() call,
// `pos` can legally exceed `bufLen`. The raw `buf[pos++] = c`
// writes below have no bounds of their own, so once `pos` runs
// past `bufLen` they become out-of-bounds writes. JSON_PUT guards
// every raw write, and JSON_REM clamps the size passed to the
// next snprintf() so `buf + pos` is never advanced, and never
// treated as having more room, past the end of the buffer.
#define JSON_PUT(ch) do { if (pos < bufLen - 1) buf[pos++] = (ch); } while (0)
#define JSON_REM     ((pos < bufLen) ? (bufLen - pos) : 0)

static int buildStateJSON(char* buf, int bufLen) {
    bool active = gameIsActive();

    if (!active) {
        return snprintf(buf, bufLen, "{\"a\":false}");
    }

    GameState gs = gameGetState();
    GameMode  gm = gameGetMode();

    // State code
    const char* sc;
    switch (gs) {
        case GAME_NAMING:       sc = "NM"; break;
        case GAME_POURING:      sc = "PO"; break;
        case GAME_TASTING:      sc = "TA"; break;
        case GAME_GUESSING:     sc = "GU"; break;
        case GAME_GUESS_SCORE:  sc = "GS"; break;
        case GAME_GUESS_DETAIL: sc = "GD"; break;
        case GAME_RANKING:      sc = "RK"; break;
        case GAME_REVEAL:       sc = "RE"; break;
        case GAME_DONE:         sc = "DO"; break;
        default:                sc = "??"; break;
    }

    // Mode code
    const char* mc;
    switch (gm) {
        case GAME_MODE_BASIC: mc = "B"; break;
        case GAME_MODE_NAMED: mc = "N"; break;
        case GAME_MODE_GUESS: mc = "G"; break;
        case GAME_MODE_RANK:  mc = "R"; break;
        default:              mc = "?"; break;
    }

    int pc = gameGetPourCount();
    int pos = 0;

    // --- Base fields ---
    pos += snprintf(buf + pos, JSON_REM,
        "{\"a\":true,\"s\":\"%s\",\"m\":\"%s\",\"p\":%d",
        sc, mc, pc);

    // Spinning flag (bug 14) — set just before a blocking motor
    // spin starts so phones can show a spinner instead of going
    // "Reconnecting..." while the main loop is unserviced.
    if (gameIsSpinning()) {
        pos += snprintf(buf + pos, JSON_REM, ",\"sp\":true");
    }

    // --- Glass used array ---
    pos += snprintf(buf + pos, JSON_REM, ",\"u\":[%s,%s,%s,%s]",
        gameGetGlassUsed(0) ? "true" : "false",
        gameGetGlassUsed(1) ? "true" : "false",
        gameGetGlassUsed(2) ? "true" : "false",
        gameGetGlassUsed(3) ? "true" : "false");

    // --- Current name (for POURING state) ---
    if (gs == GAME_POURING) {
        // During POURING, pourCount hasn't incremented yet for the
        // current pour, so the name is at index pourCount.
        const char* cn = gameGetGlassName(pc);
        if (cn && cn[0]) {
            pos += snprintf(buf + pos, JSON_REM, ",\"cn\":\"");
            // Escape name for JSON
            for (int i = 0; cn[i]; i++) {
                char c = cn[i];
                if (c == '"' || c == '\\') JSON_PUT('\\');
                JSON_PUT(c);
            }
            pos += snprintf(buf + pos, JSON_REM, "\"");
        }
    }

    // --- Reveal data ---
    int rc = gameGetRevealCount();
    int ri = gameGetRevealIndex();
    pos += snprintf(buf + pos, JSON_REM, ",\"ri\":%d,\"rc\":%d", ri, rc);

    // Current reveal entry (for REVEAL state)
    if (gs == GAME_REVEAL && rc > 0 && ri < rc) {
        int rg = gameGetRevealGlass(ri);
        int rp = gameGetRevealPourIdx(ri);
        const char* rn = gameGetGlassName(rp);
        pos += snprintf(buf + pos, JSON_REM, ",\"rg\":%d,\"rp\":%d,\"rn\":\"", rg, rp);
        if (rn && rn[0]) {
            for (int i = 0; rn[i]; i++) {
                char c = rn[i];
                if (c == '"' || c == '\\') JSON_PUT('\\');
                JSON_PUT(c);
            }
        } else {
            // Basic mode fallback — show pour number (matches the
            // DONE-state fallback below so Basic-mode reveal isn't
            // shown with a blank title; bug 13).
            pos += snprintf(buf + pos, JSON_REM, "Pour #%d", rp + 1);
        }
        pos += snprintf(buf + pos, JSON_REM, "\"");
    }

    // --- Results (for DONE state) ---
    if (gs == GAME_DONE && rc > 0) {
        pos += snprintf(buf + pos, JSON_REM, ",\"rv\":[");
        for (int i = 0; i < rc; i++) {
            if (i > 0) JSON_PUT(',');
            int g = gameGetRevealGlass(i);
            int p = gameGetRevealPourIdx(i);
            const char* n = gameGetGlassName(p);
            pos += snprintf(buf + pos, JSON_REM, "{\"g\":%d,\"n\":\"", g);
            if (n && n[0]) {
                for (int j = 0; n[j]; j++) {
                    char c = n[j];
                    if (c == '"' || c == '\\') JSON_PUT('\\');
                    JSON_PUT(c);
                }
            } else {
                // Basic mode fallback — show pour number
                pos += snprintf(buf + pos, JSON_REM, "Pour #%d", p + 1);
            }
            pos += snprintf(buf + pos, JSON_REM, "\"}");
        }
        JSON_PUT(']');
    }

    // --- Guess data ---
    if (gs == GAME_GUESSING) {
        int gg = gameGetGuessGlass();
        int gpc = gameGetGuessPoolCount();
        pos += snprintf(buf + pos, JSON_REM, ",\"gg\":%d,\"gp\":[", gg);
        for (int i = 0; i < gpc; i++) {
            if (i > 0) JSON_PUT(',');
            JSON_PUT('"');
            const char* pn = gameGetGuessPoolName(i);
            if (pn) {
                for (int j = 0; pn[j]; j++) {
                    char c = pn[j];
                    if (c == '"' || c == '\\') JSON_PUT('\\');
                    JSON_PUT(c);
                }
            }
            JSON_PUT('"');
        }
        JSON_PUT(']');
    }

    // Guess correct count (for GUESS_SCORE, GUESS_DETAIL)
    if (gs == GAME_GUESS_SCORE || gs == GAME_GUESS_DETAIL) {
        int gc = gameGetGuessCorrect();
        pos += snprintf(buf + pos, JSON_REM, ",\"gc\":%d", gc);
    }

    // Guess detail per glass (for GUESS_DETAIL)
    if (gs == GAME_GUESS_DETAIL && rc > 0) {
        // gx: object mapping glass index (0-based) to correct bool
        pos += snprintf(buf + pos, JSON_REM, ",\"gx\":{");
        bool first = true;
        for (int i = 0; i < NUM_GLASSES; i++) {
            if (gameGetGlassUsed(i)) {
                // Find this glass in reveal map
                for (int r = 0; r < rc; r++) {
                    if (gameGetRevealGlass(r) == i + 1) {
                        if (!first) JSON_PUT(',');
                        first = false;
                        bool ok = gameGetGuessCorrectAt(r);
                        pos += snprintf(buf + pos, JSON_REM,
                            "\"%d\":%s", i, ok ? "true" : "false");
                        break;
                    }
                }
            }
        }
        JSON_PUT('}');
    }

    JSON_PUT('}');
    if (pos >= bufLen) pos = bufLen - 1;
    buf[pos] = '\0';
    return pos;
}

#undef JSON_PUT
#undef JSON_REM

// ============================================================
// State broadcast
// ============================================================

static void broadcastState() {
    char json[1024];
    buildStateJSON(json, sizeof(json));
    wsServer.broadcastTXT(json);
}

void wifiPortalBroadcastNow() {
    if (!portalRunning) return;
    broadcastState();
    // Also reset the throttle window so wifiPortalUpdate() doesn't
    // immediately re-send an identical snapshot on the next loop.
    lastBroadcastMs = millis();
}

static void sendStateToClient(uint8_t num) {
    char json[1024];
    buildStateJSON(json, sizeof(json));
    wsServer.sendTXT(num, json);
}

// ============================================================
// WebSocket action parser
// ============================================================
// Parses simple JSON commands from the phone:
//   {"a":"right"}             — right button
//   {"a":"left"}              — left button
//   {"a":"click"}             — encoder click
//   {"a":"name","v":"text"}   — submit whiskey name
//   {"a":"guess","v":2}       — select guess pool index
//   {"a":"start","v":"B"}     — start basic flight
//   {"a":"start","v":"N"}     — start named flight
//   {"a":"start","v":"G"}     — start best guess flight
//
// Uses simple string search instead of a JSON library to
// minimize RAM usage.

// Unescape a JSON string value in place (bug 12). handleWSAction
// uses plain string search rather than a JSON library, so escape
// sequences the browser puts in (e.g. a `"` in a whiskey name
// arrives as `\"`) were never decoded — the backslash was being
// stored literally in glassName. This handles the two sequences
// JS's JSON.stringify actually produces for typed text (\" and
// \\), passes through the character after any other backslash
// as a best effort, and drops stray control characters. Good
// enough for a name field without pulling in a full JSON parser.
static void jsonUnescapeInPlace(char* s) {
    char* out = s;
    for (char* in = s; *in; in++) {
        if (*in == '\\' && *(in + 1)) {
            in++;
            switch (*in) {
                case '"':  *out++ = '"';  break;
                case '\\': *out++ = '\\'; break;
                default:   *out++ = *in;  break; // best effort for \n, \t, etc.
            }
        } else if ((unsigned char)*in >= 0x20) {
            // Drop raw control characters (e.g. stray newlines)
            *out++ = *in;
        }
    }
    *out = '\0';
}

static void handleWSAction(uint8_t* payload, size_t length) {
    // Null-terminate
    char msg[256];
    int len = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
    memcpy(msg, payload, len);
    msg[len] = '\0';

    // Extract action: find "a":"<value>"
    char* aPtr = strstr(msg, "\"a\":\"");
    if (!aPtr) return;
    aPtr += 5; // skip past "a":"
    char* aEnd = strchr(aPtr, '"');
    if (!aEnd) return;
    *aEnd = '\0';
    String action(aPtr);
    *aEnd = '"'; // restore for further parsing

    // --- Button actions ---
    if (action == "right") {
        Serial.println("[WiFi] Phone: right");
        inputInjectEvent(INPUT_BTN_RIGHT);
        return;
    }
    if (action == "left") {
        Serial.println("[WiFi] Phone: left");
        inputInjectEvent(INPUT_BTN_LEFT);
        return;
    }
    if (action == "click") {
        Serial.println("[WiFi] Phone: click");
        inputInjectEvent(INPUT_ENC_CLICK);
        return;
    }

    // --- Name submission ---
    if (action == "name") {
        // Extract value: "v":"<text>"
        // NOTE: this grabs the *last* '"' in the message as the
        // closing delimiter, which only works because "v" is
        // currently the last field in the {"a":"name","v":"..."}
        // payload. If a field is ever added after "v", this will
        // need to switch to an escape-aware forward scan instead.
        char* vPtr = strstr(msg, "\"v\":\"");
        if (!vPtr) return;
        vPtr += 5;
        char* vEnd = strrchr(vPtr, '"');
        if (!vEnd || vEnd == vPtr) return;
        *vEnd = '\0';
        jsonUnescapeInPlace(vPtr);
        Serial.printf("[WiFi] Phone name: %s\n", vPtr);
        gamePhoneSubmitName(vPtr);
        return;
    }

    // --- Guess selection ---
    if (action == "guess") {
        // Extract value: "v":<number>
        char* vPtr = strstr(msg, "\"v\":");
        if (!vPtr) return;
        vPtr += 4;
        int idx = atoi(vPtr);
        Serial.printf("[WiFi] Phone guess: %d\n", idx);
        gamePhoneGuessSelect(idx);
        return;
    }

    // --- Game start ---
    if (action == "start") {
        char* vPtr = strstr(msg, "\"v\":\"");
        if (!vPtr) return;
        vPtr += 5;
        GameMode mode = GAME_MODE_BASIC;
        if (*vPtr == 'N') mode = GAME_MODE_NAMED;
        else if (*vPtr == 'G') mode = GAME_MODE_GUESS;
        Serial.printf("[WiFi] Phone start: mode %d\n", (int)mode);
        gameStartFromPhone(mode);
        return;
    }
}

// ============================================================
// WebSocket event handler
// ============================================================

static void webSocketEvent(uint8_t num, WStype_t type,
                           uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("[WiFi] WS client %u connected\n", num);
            // Send current state immediately
            sendStateToClient(num);
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WiFi] WS client %u disconnected\n", num);
            break;

        case WStype_TEXT:
            handleWSAction(payload, length);
            break;

        default:
            break;
    }
}

// ============================================================
// HTTP route handlers
// ============================================================

static void handleRoot() {
    webServer.send(200, "text/html", PAGE_HTML);
}

// Android captive-portal probe — redirect to landing page
static void handleGenerate204() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// iOS captive-portal probe
static void handleHotspotDetect() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// Windows captive-portal probe
static void handleConnectTest() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// Catch-all: redirect unknown paths to root
static void handleNotFound() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

// ============================================================
// Public API
// ============================================================

void wifiPortalInit() {
    if (portalRunning) return;  // Already running

    Serial.println("[WiFi] Starting soft AP...");

    // --- Soft AP ---
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[WiFi] AP SSID: %s\n", AP_SSID);
    Serial.printf("[WiFi] AP IP:   %s\n", apIP.toString().c_str());

    // --- DNS server (captive portal) ---
    dnsServer.start(DNS_PORT, "*", apIP);
    Serial.println("[WiFi] DNS server started (captive portal)");

    // --- mDNS ---
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("ws", "tcp", 81);
        Serial.printf("[WiFi] mDNS: http://%s.local\n", MDNS_NAME);
    } else {
        Serial.println("[WiFi] mDNS failed to start");
    }

    // --- Web server routes ---
    webServer.on("/", handleRoot);
    webServer.on("/generate_204", handleGenerate204);
    webServer.on("/hotspot-detect.html", handleHotspotDetect);
    webServer.on("/connecttest.txt", handleConnectTest);
    webServer.onNotFound(handleNotFound);

    webServer.begin();
    Serial.println("[WiFi] Web server started on port 80");

    // --- WebSocket server ---
    wsServer.begin();
    wsServer.onEvent(webSocketEvent);
    Serial.println("[WiFi] WebSocket server started on port 81");

    portalRunning = true;
}

void wifiPortalUpdate() {
    if (!portalRunning) return;

    dnsServer.processNextRequest();
    webServer.handleClient();
    wsServer.loop();

    // --- State broadcast (poll for changes every 150ms) ---
    unsigned long now = millis();
    if (now - lastBroadcastMs >= 150) {
        lastBroadcastMs = now;

        bool active = gameIsActive();
        int  st     = active ? (int)gameGetState() : -1;
        int  pour   = gameGetPourCount();
        int  reveal = gameGetRevealIndex();
        int  guess  = active ? (int)gameGetState() : -1;
        // Include guess pool count in change detection for GUESSING state
        int  extra  = (st == (int)GAME_GUESSING) ? gameGetGuessPoolCount() : 0;

        if (active != lastBcActive || st != lastBcState ||
            pour != lastBcPour || reveal != lastBcReveal ||
            extra != lastBcGuess) {

            lastBcActive = active;
            lastBcState  = st;
            lastBcPour   = pour;
            lastBcReveal = reveal;
            lastBcGuess  = extra;

            broadcastState();
        }
    }
}

int wifiPortalGetClientCount() {
    if (!portalRunning) return 0;
    return WiFi.softAPgetStationNum();
}

void wifiPortalStop() {
    if (!portalRunning) return;

    Serial.println("[WiFi] Stopping portal...");

    wsServer.close();
    webServer.stop();
    dnsServer.stop();
    MDNS.end();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    portalRunning = false;
    Serial.println("[WiFi] Portal stopped");
}

bool wifiPortalIsRunning() {
    return portalRunning;
}
