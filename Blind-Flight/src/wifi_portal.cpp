#include "wifi_portal.h"
#include "game.h"
#include "h2h.h"
#include "config.h"
#include "input.h"
#include "favorites.h"

#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <Preferences.h>

// ============================================================
// Blind Flight — Wi-Fi Portal Module
// ============================================================

// --- Network configuration ---
static const char* AP_SSID     = "BlindFlight";
static const char* AP_PASSWORD = nullptr;
static const byte  DNS_PORT    = 53;
static const char* MDNS_NAME   = "flight";

// --- Module instances ---
static DNSServer         dnsServer;
static WebServer         webServer(80);
static WebSocketsServer  wsServer(81);

// --- State ---
static bool portalRunning = false;
static bool staMode       = false;
static char savedSSID[33] = "";
static char savedPass[64] = "";

// --- Forward declarations ---
static void buildAndBroadcastH2H();

// --- State broadcast tracking ---
static unsigned long lastBroadcastMs = 0;
static bool     lastBcActive    = false;
static int      lastBcState     = -1;
static int      lastBcPour      = -1;
static int      lastBcReveal    = -1;
static int      lastBcGuess     = -1;

// ============================================================
// NVS credential storage
// ============================================================

static void loadCredentials() {
    Preferences p;
    p.begin(NVS_NS_WIFI, true);
    String s = p.getString("ssid", "");
    String pw = p.getString("pass", "");
    p.end();

    strncpy(savedSSID, s.c_str(), sizeof(savedSSID) - 1);
    savedSSID[sizeof(savedSSID) - 1] = '\0';
    strncpy(savedPass, pw.c_str(), sizeof(savedPass) - 1);
    savedPass[sizeof(savedPass) - 1] = '\0';
}

void wifiSaveCredentials(const char* ssid, const char* pass) {
    Preferences p;
    p.begin(NVS_NS_WIFI, false);
    p.putString("ssid", ssid);
    p.putString("pass", pass);
    p.end();

    strncpy(savedSSID, ssid, sizeof(savedSSID) - 1);
    savedSSID[sizeof(savedSSID) - 1] = '\0';
    strncpy(savedPass, pass, sizeof(savedPass) - 1);
    savedPass[sizeof(savedPass) - 1] = '\0';

    Serial.printf("[WiFi] Credentials saved for: %s\n", savedSSID);
}

void wifiForgetCredentials() {
    Preferences p;
    p.begin(NVS_NS_WIFI, false);
    p.remove("ssid");
    p.remove("pass");
    p.end();

    savedSSID[0] = '\0';
    savedPass[0] = '\0';

    Serial.println("[WiFi] Credentials forgotten");
}

bool wifiHasCredentials() {
    return savedSSID[0] != '\0';
}

// ============================================================
// STA mode accessors
// ============================================================

bool wifiIsSTAMode() {
    return staMode;
}

bool wifiIsConnected() {
    return staMode && WiFi.status() == WL_CONNECTED;
}

const char* wifiGetSSID() {
    return savedSSID;
}

String wifiGetIP() {
    if (staMode) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

int8_t wifiGetRSSI() {
    if (staMode && WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

// ============================================================
// Embedded HTML page — interactive phone UI (PROGMEM)
// ============================================================

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
input[type=text],input[type=password]{width:100%;padding:14px;background:#222;border:2px solid #444;
border-radius:10px;color:#eee;font-size:18px;margin-bottom:10px;outline:none}
input[type=text]:focus,input[type=password]:focus{border-color:#D4A547}
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
.fhdr{display:flex;justify-content:space-between;align-items:center;cursor:pointer;padding:4px 0}
.fhdr span:last-child{color:#888;font-size:14px}
.fi{display:flex;justify-content:space-between;align-items:center;padding:10px;
background:#222;border:1px solid #333;border-radius:8px;margin-bottom:6px}
.fi span{color:#eee;font-size:15px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;flex:1}
.fx{background:none;border:none;color:#F44336;font-size:20px;cursor:pointer;padding:0 0 0 12px;
font-weight:700;line-height:1}
.fadd{display:flex;gap:8px;margin-bottom:8px}
.fadd input{flex:1;margin:0}
.fadd button{flex-shrink:0;width:60px;margin:0;padding:14px 0}
.fcnt{text-align:center;font-size:13px;color:#666;margin-top:4px}
.ferr{text-align:center;font-size:13px;color:#F44336;margin:4px 0}
.plr{padding:10px;background:#222;border:1px solid #333;border-radius:8px;margin-bottom:6px;
display:flex;align-items:center;gap:8px}
.plr .pdot{width:10px;height:10px;border-radius:50%;flex-shrink:0}
.pcon{background:#4CAF50}.pdis{background:#F44336}
.gsel{display:flex;gap:8px;margin:8px 0}
.gsel .gbtn{flex:1;padding:16px 8px;background:#222;border:2px solid #444;border-radius:10px;
text-align:center;cursor:pointer;font-size:15px;color:#eee}
.gsel .gbtn.active{border-color:#D4A547;background:#2a2210}
.rok{color:#4CAF50;font-weight:700}.rnk{color:#F44336;font-weight:700}
.wnet{padding:12px;background:#222;border:1px solid #444;border-radius:10px;
margin-bottom:8px;cursor:pointer;display:flex;justify-content:space-between;align-items:center}
.wnet:active{background:#2a2210;border-color:#D4A547}
.wnet.sel{border-color:#D4A547;background:#2a2210}
.wrssi{color:#888;font-size:13px}
.wlock{color:#888;font-size:14px;margin-left:4px}
</style>
</head>
<body>
<div class="hdr">
<div class="logo">BLIND FLIGHT</div>
<div class="sub">The Whiskey Flight</div>
<div class="conn"><span class="cdot coff" id="cd"></span><span class="hint" id="cs">Connecting...</span></div>
</div>
<div class="card" id="v"></div>
<div class="card" id="wc" style="display:none"></div>
<div class="card" id="fc" style="display:none"></div>
<script>
var S=null,ws=null,lk=false,F=null,fOpen=false;
var H=null,HY=null,HR=null,HJ=false,HN='';
var hg0=-1,hg1=-1,hClaim=0;
var wOpen=false,wNets=null,wSel='',wSta=false,wMsg='';
function cn(){
ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onopen=function(){
$('cd').className='cdot con';$('cs').textContent='Connected';
tx({a:'fav_list'});
};
ws.onclose=function(){
$('cd').className='cdot coff';$('cs').textContent='Reconnecting...';
S=null;F=null;H=null;HY=null;HR=null;HJ=false;lk=false;hg0=-1;hg1=-1;hClaim=0;rn();rfav();setTimeout(cn,2000);
};
ws.onmessage=function(e){
try{var d=JSON.parse(e.data);
if(d.t==='favs'){F=d;lk=false;rfav();return;}
if(d.t==='h2h'){H=d;lk=false;rh();return;}
if(d.t==='h2h_you'){HY=d;lk=false;rh();return;}
if(d.t==='h2h_result'){HR=d;lk=false;rh();return;}
if(d.t==='h2h_err'){lk=false;return;}
if(d.t==='wifi_nets'){wNets=d.nets;lk=false;rwifi();return;}
if(d.t==='wifi_status'){lk=false;
if(d.ok){wMsg='Connected! IP: '+d.ip;wSta=true;}
else{wMsg='Connection failed';}
rwifi();return;}
S=d;lk=false;rn();showFavCard();showWifiCard();
}catch(x){}
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
function showFavCard(){
var fc=$('fc');
if(!S||S.a){fc.style.display='none';return;}
fc.style.display='';
}
function showWifiCard(){
var wc=$('wc');
if(!wc)return;
if(!S||S.a||S.sta){wc.style.display='none';return;}
wc.style.display='';
rwifi();
}
function rfav(){
var fc=$('fc');
if(!F){fc.innerHTML='';return;}
var h='<div class="fhdr" onclick="fOpen=!fOpen;rfav()"><div class="st" style="margin:0">Favorites</div><span>'+(fOpen?'▲':'▼')+'</span></div>';
if(fOpen){
h+='<div class="fadd"><input type="text" id="fi" placeholder="Add favorite" maxlength="21" autocomplete="off" autocapitalize="words">';
h+='<button class="btn bp fadd" onclick="favAdd()">Add</button></div>';
h+='<div id="fe"></div>';
if(F.list.length===0){h+='<div class="msg">No favorites yet</div>';}
else{for(var i=0;i<F.list.length;i++){
h+='<div class="fi"><span>'+esc(F.list[i])+'</span><button class="fx" onclick="favDel('+i+')">&times;</button></div>';
}}
h+='<div class="fcnt">'+F.cnt+' / '+F.max+'</div>';
}
fc.innerHTML=h;
}
function favAdd(){
var fi=$('fi');if(!fi)return;
var v=fi.value.trim();if(!v)return;
if(v.length>21){$('fe').innerHTML='<div class="ferr">Max 21 characters</div>';return;}
if(F&&F.cnt>=F.max){$('fe').innerHTML='<div class="ferr">Favorites list full</div>';return;}
if(F){for(var i=0;i<F.list.length;i++){if(F.list[i].toLowerCase()===v.toLowerCase()){
$('fe').innerHTML='<div class="ferr">Already in favorites</div>';return;}}}
tx({a:'fav_add',v:v});fi.value='';
}
function favDel(i){tx({a:'fav_del',v:i});}
function rwifi(){
var wc=$('wc');
if(!wc)return;
var h='<div class="fhdr" onclick="wOpen=!wOpen;rwifi()"><div class="st" style="margin:0">Wi-Fi Setup</div><span>'+(wOpen?'▲':'▼')+'</span></div>';
if(wOpen){
if(wMsg){h+='<div class="msg" style="color:'+(wSta?'#4CAF50':'#F44336')+'">'+esc(wMsg)+'</div>';}
if(wSta){
h+='<div class="hint">Reconnect to your network, then visit the IP above or http://flight.local</div>';
}else{
h+='<button class="btn bs" onclick="wNets=null;wSel=\'\';wMsg=\'\';tx({a:\'wifi_scan\'})">Scan for Networks</button>';
if(wNets){
for(var i=0;i<wNets.length;i++){
var n=wNets[i];
var sel=wSel===n.s;
h+='<div class="wnet'+(sel?' sel':'')+'" onclick="wSel=\''+esc(n.s).replace(/'/g,"\\'")+'\';rwifi()">';
h+='<span>'+esc(n.s)+(n.e?'<span class="wlock"> 🔒</span>':'')+'</span>';
h+='<span class="wrssi">'+rssiBar(n.r)+'</span></div>';
}
if(wSel){
h+='<input type="password" id="wp" placeholder="Password for '+esc(wSel)+'" autocomplete="off">';
h+='<button class="btn bp" onclick="wconn()">Connect</button>';
}
}
}
}
wc.innerHTML=h;
}
function rssiBar(r){
var b=r>-50?4:r>-60?3:r>-70?2:1;
var s='';for(var i=0;i<4;i++)s+=i<b?'▰':'▱';
return s;
}
function wconn(){
var wp=$('wp');
var p=wp?wp.value:'';
wMsg='Connecting...';rwifi();
tx({a:'wifi_connect',s:wSel,p:p});
}
function rh(){
var v=$('v');
if(!H){return;}
$('fc').style.display='none';
var wc=$('wc');if(wc)wc.style.display='none';
var ph=H.phase,md=H.mode||'2x2',h='';
if(!HJ&&(ph==='lobby'||ph==='bottles'||ph==='select'||ph==='count')){
h+='<div class="st">HEAD TO HEAD</div>';
h+='<div class="msg">Enter your name to join</div>';
h+='<input type="text" id="hn" placeholder="Your name" maxlength="12" autocomplete="off" autocapitalize="words">';
h+='<button class="btn bp" onclick="hjoin()">Join Game</button>';
if(H.players&&H.players.length>0){
h+='<div class="msg" style="margin-top:12px">Players:</div>';
for(var i=0;i<H.players.length;i++){
var p=H.players[i];
h+='<div class="plr"><span class="pdot '+(p.c?'pcon':'pdis')+'"></span>'+esc(p.n)+'</div>';
}}
h+='<div class="hint">Waiting for host to start</div>';
v.innerHTML=h;
var hn=$('hn');if(hn)hn.focus();
return;
}
if(ph==='lobby'){
h+='<div class="st">LOBBY</div>';
h+='<div class="msg">Waiting for game to start...</div>';
for(var i=0;i<H.players.length;i++){
var p=H.players[i];
h+='<div class="plr"><span class="pdot '+(p.c?'pcon':'pdis')+'"></span>'+esc(p.n)+'</div>';
}
h+='<div class="hint">'+H.players.length+' player(s) joined</div>';
}else if(ph==='pouring'){
h+='<div class="st">Pouring...</div>';
h+='<div class="msg">Please wait</div>';
h+='<div class="hint">The host is pouring glasses</div>';
}else if(ph==='tasting'){
h+='<div class="st">TASTING TIME</div>';
h+='<div class="msg">Take one glass each</div>';
h+='<div class="hint">Waiting for host to continue</div>';
}else if(md==='2x2'&&(ph==='assign'||ph==='guessing')){
if(HY&&HY.glasses&&!HR){
if(ph==='assign'){
h+='<div class="st">YOUR GLASSES</div>';
h+='<div class="big">'+HY.glasses[0]+' & '+HY.glasses[1]+'</div>';
h+='<div class="msg">Take glasses '+HY.glasses[0]+' and '+HY.glasses[1]+'</div>';
h+='<div class="hint">Taste both and guess which is which</div>';
}else{
h+='<div class="st">GUESS</div>';
h+='<div class="msg">Which bottle is in each glass?</div>';
if(H.bottles){
for(var g=0;g<2;g++){
h+='<div style="margin:12px 0"><div style="font-size:14px;color:#888;margin-bottom:4px">Glass '+HY.glasses[g]+':</div>';
h+='<div class="gsel">';
for(var b=0;b<H.bottles.length;b++){
var sel=(g===0?hg0:hg1)===b;
h+='<div class="gbtn'+(sel?' active':'')+'" onclick="hgs('+g+','+b+')">'+esc(H.bottles[b])+'</div>';
}
h+='</div></div>';
}}
var ready=(hg0>=0&&hg1>=0);
h+='<button class="btn bp'+(ready?'':' bs')+'" onclick="hsub()"'+(ready?'':' disabled')+'>Submit Guess</button>';
if(H.submitted!==undefined)h+='<div class="hint">'+H.submitted+' of '+H.total+' submitted</div>';
}}else{
h+='<div class="st">WAITING</div>';
h+='<div class="msg">Waiting for assignments...</div>';
}
}else if(md==='random'&&ph==='guessing'){
if(!HR){
var gc=H.gc||4;
var claimed=[];
if(H.players)for(var i=0;i<H.players.length;i++){if(H.players[i].gl>0)claimed.push(H.players[i].gl);}
if(hClaim===0){
h+='<div class="st">WHICH GLASS?</div>';
h+='<div class="msg">Which glass did you take?</div>';
h+='<div class="gsel">';
for(var g=1;g<=gc;g++){
var taken=false;
for(var c=0;c<claimed.length;c++){if(claimed[c]===g)taken=true;}
if(taken){h+='<div class="gbtn" style="opacity:0.3;cursor:default">'+g+'</div>';}
else{h+='<div class="gbtn" onclick="hclaim('+g+')">'+g+'</div>';}
}
h+='</div>';
h+='<div class="hint">Select your glass number</div>';
}else{
h+='<div class="st">GUESS</div>';
h+='<div class="msg">Glass '+hClaim+' — What\'s in your glass?</div>';
if(H.bottles){
for(var b=0;b<H.bottles.length;b++){
h+='<div class="pi" onclick="hsubr('+b+')">'+esc(H.bottles[b])+'</div>';
}}
h+='<button class="btn bs" onclick="hClaim=0;lk=false;rh()">Change Glass</button>';
if(H.submitted!==undefined)h+='<div class="hint">'+H.submitted+' of '+H.total+' submitted</div>';
}
}else{
h+='<div class="st">SUBMITTED</div>';
h+='<div class="msg">Waiting for other players...</div>';
}
}else if(md==='premium'&&ph==='guessing'){
if(!HR){
var gc=H.gc||4;
var claimed=[];
if(H.players)for(var i=0;i<H.players.length;i++){if(H.players[i].gl>0)claimed.push(H.players[i].gl);}
if(hClaim===0){
h+='<div class="st">WHICH GLASS?</div>';
h+='<div class="msg">Which glass did you take?</div>';
h+='<div class="gsel">';
for(var g=1;g<=gc;g++){
var taken=false;
for(var c=0;c<claimed.length;c++){if(claimed[c]===g)taken=true;}
if(taken){h+='<div class="gbtn" style="opacity:0.3;cursor:default">'+g+'</div>';}
else{h+='<div class="gbtn" onclick="hclaim('+g+')">'+g+'</div>';}
}
h+='</div>';
h+='<div class="hint">Select your glass number</div>';
}else{
h+='<div class="st">PREMIUM POUR</div>';
h+='<div class="msg">Did you get the premium?</div>';
if(H.premium)h+='<div class="nm">'+esc(H.premium)+'</div>';
h+='<div class="btns" style="margin-top:16px">';
h+='<button class="btn bp" onclick="hsubp(1)">YES</button>';
h+='<button class="btn bd" onclick="hsubp(0)">NO</button>';
h+='</div>';
h+='<button class="btn bs" style="margin-top:8px" onclick="hClaim=0;lk=false;rh()">Change Glass</button>';
if(H.submitted!==undefined)h+='<div class="hint">'+H.submitted+' of '+H.total+' submitted</div>';
}
}else{
h+='<div class="st">SUBMITTED</div>';
h+='<div class="msg">Waiting for other players...</div>';
}
}else if(ph==='results'){
h+='<div class="st">RESULTS</div>';
if(HR){
if(HR.g){
for(var i=0;i<HR.g.length;i++){
var r=HR.g[i];
h+='<div class="plr"><span class="'+(r.ok?'rok':'rnk')+'">'+(r.ok?'✓':'✗')+'</span> Glass '+r.glass+': '+(r.ok?'Correct!':'Wrong')+'</div>';
}
}else if(HR.had!==undefined){
h+='<div class="plr"><span class="'+(HR.ok?'rok':'rnk')+'">'+(HR.ok?'✓':'✗')+'</span> Glass '+HR.glass+'</div>';
if(HR.ok){h+='<div class="msg" style="color:#4CAF50">Correct!</div>';}
else{h+='<div class="msg" style="color:#F44336">Wrong!</div>';}
h+='<div class="hint">'+(HR.had?'You had the premium!':'You did not have the premium.')+'</div>';
h+='<div class="hint">Glass '+HR.premGlass+' was '+esc(HR.premium)+'</div>';
}else{
h+='<div class="plr"><span class="'+(HR.ok?'rok':'rnk')+'">'+(HR.ok?'✓':'✗')+'</span> Glass '+HR.glass+'</div>';
if(HR.ok){h+='<div class="msg" style="color:#4CAF50">Correct!</div>';}
else{h+='<div class="msg" style="color:#F44336">Wrong!</div>';
h+='<div class="hint">It was actually: '+esc(HR.actual)+'</div>';}
}
}else{
h+='<div class="msg">Waiting for results...</div>';
}
}else if(ph==='reveal'||ph==='done'){
h+='<div class="st">'+(ph==='done'?'COMPLETE':'REVEAL')+'</div>';
h+='<div class="msg">See the device for details</div>';
}else{
h+='<div class="st">HEAD TO HEAD</div>';
h+='<div class="msg">Game in progress...</div>';
}
v.innerHTML=h;
}
function hjoin(){
var hn=$('hn');if(!hn)return;
var v=hn.value.trim();if(!v)return;
HN=v;HJ=true;
tx({a:'h2h_join',v:v});
}
function hgs(g,b){
if(g===0)hg0=b;else hg1=b;
lk=false;rh();
}
function hsub(){
if(hg0<0||hg1<0)return;
tx({a:'h2h_guess',g0:hg0,g1:hg1});
}
function hclaim(g){
hClaim=g;
tx({a:'h2h_claim',v:g});
lk=false;rh();
}
function hsubr(b){
tx({a:'h2h_guess_random',v:b});
}
function hsubp(v){
tx({a:'h2h_guess_premium',v:v});
}
document.addEventListener('keydown',function(e){
if(e.key==='Enter'&&S&&S.s==='NM')sn();
if(e.key==='Enter'&&fOpen&&document.activeElement===$('fi'))favAdd();
if(e.key==='Enter'&&!HJ&&$('hn')&&document.activeElement===$('hn'))hjoin();
if(e.key==='Enter'&&wOpen&&wSel&&document.activeElement===$('wp'))wconn();
});
cn();
</script>
</body>
</html>
)rawliteral";

// ============================================================
// JSON state builder
// ============================================================

#define JSON_PUT(ch) do { if (pos < bufLen - 1) buf[pos++] = (ch); } while (0)
#define JSON_REM     ((pos < bufLen) ? (bufLen - pos) : 0)

static int buildStateJSON(char* buf, int bufLen) {
    bool active = gameIsActive();

    if (!active) {
        return snprintf(buf, bufLen, "{\"a\":false,\"sta\":%s}",
                        staMode ? "true" : "false");
    }

    GameState gs = gameGetState();
    GameMode  gm = gameGetMode();

    const char* sc;
    switch (gs) {
        case GAME_NAMING:          sc = "NM"; break;
        case GAME_BOTTLE_SELECT:   sc = "BS"; break;
        case GAME_POURING:         sc = "PO"; break;
        case GAME_TASTING:         sc = "TA"; break;
        case GAME_GUESSING:        sc = "GU"; break;
        case GAME_GUESS_SCORE:     sc = "GS"; break;
        case GAME_GUESS_DETAIL:    sc = "GD"; break;
        case GAME_RANKING:         sc = "RK"; break;
        case GAME_CHALLENGE_GUESS: sc = "CG"; break;
        case GAME_CHALLENGE_RESULT:sc = "CR"; break;
        case GAME_REVEAL:          sc = "RE"; break;
        case GAME_DONE:            sc = "DO"; break;
        default:                   sc = "??"; break;
    }

    const char* mc;
    switch (gm) {
        case GAME_MODE_BASIC:     mc = "B"; break;
        case GAME_MODE_NAMED:     mc = "N"; break;
        case GAME_MODE_GUESS:     mc = "G"; break;
        case GAME_MODE_RANK:      mc = "R"; break;
        case GAME_MODE_GUESS_RANK: mc = "Q"; break;
        case GAME_MODE_DUPLICATE: mc = "D"; break;
        case GAME_MODE_DECOY:     mc = "Y"; break;
        default:                  mc = "?"; break;
    }

    int pc = gameGetPourCount();
    int pos = 0;

    pos += snprintf(buf + pos, JSON_REM,
        "{\"a\":true,\"s\":\"%s\",\"m\":\"%s\",\"p\":%d,\"sta\":%s",
        sc, mc, pc, staMode ? "true" : "false");

    if (gameIsSpinning()) {
        pos += snprintf(buf + pos, JSON_REM, ",\"sp\":true");
    }

    pos += snprintf(buf + pos, JSON_REM, ",\"u\":[%s,%s,%s,%s]",
        gameGetGlassUsed(0) ? "true" : "false",
        gameGetGlassUsed(1) ? "true" : "false",
        gameGetGlassUsed(2) ? "true" : "false",
        gameGetGlassUsed(3) ? "true" : "false");

    if (gs == GAME_POURING) {
        const char* cn = gameGetGlassName(pc);
        if (cn && cn[0]) {
            pos += snprintf(buf + pos, JSON_REM, ",\"cn\":\"");
            for (int i = 0; cn[i]; i++) {
                char c = cn[i];
                if (c == '"' || c == '\\') JSON_PUT('\\');
                JSON_PUT(c);
            }
            pos += snprintf(buf + pos, JSON_REM, "\"");
        }
    }

    int rc = gameGetRevealCount();
    int ri = gameGetRevealIndex();
    pos += snprintf(buf + pos, JSON_REM, ",\"ri\":%d,\"rc\":%d", ri, rc);

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
            pos += snprintf(buf + pos, JSON_REM, "Pour #%d", rp + 1);
        }
        pos += snprintf(buf + pos, JSON_REM, "\"");
    }

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
                pos += snprintf(buf + pos, JSON_REM, "Pour #%d", p + 1);
            }
            pos += snprintf(buf + pos, JSON_REM, "\"}");
        }
        JSON_PUT(']');
    }

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

    if (gs == GAME_GUESS_SCORE || gs == GAME_GUESS_DETAIL) {
        int gc = gameGetGuessCorrect();
        pos += snprintf(buf + pos, JSON_REM, ",\"gc\":%d", gc);
    }

    if (gs == GAME_GUESS_DETAIL && rc > 0) {
        pos += snprintf(buf + pos, JSON_REM, ",\"gx\":{");
        bool first = true;
        for (int i = 0; i < NUM_GLASSES; i++) {
            if (gameGetGlassUsed(i)) {
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
    if (h2hIsActive()) buildAndBroadcastH2H();
    lastBroadcastMs = millis();
}

static void sendStateToClient(uint8_t num) {
    char json[1024];
    buildStateJSON(json, sizeof(json));
    wsServer.sendTXT(num, json);
}

// ============================================================
// Favorites JSON helpers
// ============================================================

static void sendFavoritesJSON(uint8_t clientNum) {
    char json[1024];
    int pos = 0;
    int cnt = favoritesGetCount();
    pos += snprintf(json + pos, sizeof(json) - pos,
        "{\"t\":\"favs\",\"cnt\":%d,\"max\":%d,\"list\":[", cnt, FAV_MAX_COUNT);
    for (int i = 0; i < cnt; i++) {
        if (i > 0 && pos < (int)sizeof(json) - 1) json[pos++] = ',';
        pos += snprintf(json + pos, sizeof(json) - pos, "\"");
        const char* n = favoritesGetName(i);
        for (int j = 0; n[j] && pos < (int)sizeof(json) - 2; j++) {
            if (n[j] == '"' || n[j] == '\\') json[pos++] = '\\';
            json[pos++] = n[j];
        }
        pos += snprintf(json + pos, sizeof(json) - pos, "\"");
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");
    json[pos] = '\0';
    wsServer.sendTXT(clientNum, json);
}

static void broadcastFavoritesJSON() {
    char json[1024];
    int pos = 0;
    int cnt = favoritesGetCount();
    pos += snprintf(json + pos, sizeof(json) - pos,
        "{\"t\":\"favs\",\"cnt\":%d,\"max\":%d,\"list\":[", cnt, FAV_MAX_COUNT);
    for (int i = 0; i < cnt; i++) {
        if (i > 0 && pos < (int)sizeof(json) - 1) json[pos++] = ',';
        pos += snprintf(json + pos, sizeof(json) - pos, "\"");
        const char* n = favoritesGetName(i);
        for (int j = 0; n[j] && pos < (int)sizeof(json) - 2; j++) {
            if (n[j] == '"' || n[j] == '\\') json[pos++] = '\\';
            json[pos++] = n[j];
        }
        pos += snprintf(json + pos, sizeof(json) - pos, "\"");
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");
    json[pos] = '\0';
    wsServer.broadcastTXT(json);
}

// ============================================================
// H2H state broadcast
// ============================================================

static void buildAndBroadcastH2H() {
    if (!h2hIsActive()) return;

    H2HPhase ph = h2hGetPhase();
    int pc = h2hGetPlayerCount();
    H2HSubMode sm = h2hGetSubMode();

    const char* phaseStr;
    switch (ph) {
        case H2H_MODE_SELECT:   phaseStr = "select";   break;
        case H2H_COUNT_SELECT:  phaseStr = "count";    break;
        case H2H_BOTTLE_SELECT: phaseStr = "bottles";  break;
        case H2H_LOBBY:         phaseStr = "lobby";    break;
        case H2H_POURING:       phaseStr = "pouring";  break;
        case H2H_TASTING:       phaseStr = "tasting";  break;
        case H2H_ASSIGN:        phaseStr = "assign";   break;
        case H2H_WAITING:       phaseStr = "guessing"; break;
        case H2H_RESULTS:       phaseStr = "results";  break;
        case H2H_REVEAL:        phaseStr = "reveal";   break;
        case H2H_DONE:          phaseStr = "done";     break;
        default:                phaseStr = "unknown";  break;
    }

    const char* modeStr;
    switch (sm) {
        case H2H_SUB_2X2:     modeStr = "2x2";     break;
        case H2H_SUB_RANDOM:  modeStr = "random";   break;
        case H2H_SUB_PREMIUM: modeStr = "premium";  break;
        default:              modeStr = "unknown";   break;
    }

    char json[768];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos,
        "{\"t\":\"h2h\",\"phase\":\"%s\",\"mode\":\"%s\",\"gc\":%d,\"players\":[",
        phaseStr, modeStr, h2hGetGlassCount());

    for (int i = 0; i < pc; i++) {
        const H2HPlayer* p = h2hGetPlayer(i);
        if (!p) continue;
        if (i > 0 && pos < (int)sizeof(json) - 1) json[pos++] = ',';
        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"n\":\"%s\",\"c\":%s,\"s\":%s,\"gl\":%d}",
            p->name,
            p->connected ? "true" : "false",
            p->submitted ? "true" : "false",
            p->claimedGlass);
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]");

    if (ph == H2H_WAITING) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            ",\"submitted\":%d,\"total\":%d",
            h2hGetSubmittedCount(), h2hGetRequiredPlayers());
    }

    if (ph >= H2H_ASSIGN) {
        int bc = h2hGetBottleCount();
        pos += snprintf(json + pos, sizeof(json) - pos, ",\"bottles\":[");
        for (int i = 0; i < bc; i++) {
            if (i > 0 && pos < (int)sizeof(json) - 1) json[pos++] = ',';
            pos += snprintf(json + pos, sizeof(json) - pos,
                "\"%s\"", h2hGetBottleName(i));
        }
        pos += snprintf(json + pos, sizeof(json) - pos, "]");
    }

    if (sm == H2H_SUB_PREMIUM && ph >= H2H_TASTING) {
        pos += snprintf(json + pos, sizeof(json) - pos,
            ",\"premium\":\"%s\",\"premGlass\":%d",
            h2hGetBottleName(0), h2hGetPremiumGlassNum());
    }

    pos += snprintf(json + pos, sizeof(json) - pos, "}");
    json[pos] = '\0';

    wsServer.broadcastTXT(json);

    if (sm == H2H_SUB_2X2 && (ph == H2H_ASSIGN || ph == H2H_WAITING)) {
        for (int i = 0; i < pc; i++) {
            const H2HPlayer* p = h2hGetPlayer(i);
            if (!p || !p->connected) continue;
            char pjson[256];
            snprintf(pjson, sizeof(pjson),
                "{\"t\":\"h2h_you\",\"glasses\":[%d,%d]}",
                p->assignedGlasses[0], p->assignedGlasses[1]);
            wsServer.sendTXT(p->clientNum, pjson);
        }
    }

    if (ph == H2H_RESULTS) {
        for (int i = 0; i < pc; i++) {
            const H2HPlayer* p = h2hGetPlayer(i);
            if (!p || !p->connected) continue;
            char pjson[256];
            if (sm == H2H_SUB_2X2) {
                snprintf(pjson, sizeof(pjson),
                    "{\"t\":\"h2h_result\",\"g\":[{\"glass\":%d,\"ok\":%s},{\"glass\":%d,\"ok\":%s}]}",
                    p->assignedGlasses[0], p->correctPerGlass[0] ? "true" : "false",
                    p->assignedGlasses[1], p->correctPerGlass[1] ? "true" : "false");
            } else if (sm == H2H_SUB_PREMIUM) {
                int premGlass = h2hGetPremiumGlassNum();
                bool hadPremium = (p->claimedGlass == premGlass);
                snprintf(pjson, sizeof(pjson),
                    "{\"t\":\"h2h_result\",\"ok\":%s,\"glass\":%d,\"had\":%s,\"premGlass\":%d,\"premium\":\"%s\"}",
                    p->correct ? "true" : "false",
                    p->claimedGlass,
                    hadPremium ? "true" : "false",
                    premGlass,
                    h2hGetBottleName(0));
            } else {
                int actualIdx = h2hGetBottleForGlass(p->claimedGlass);
                const char* actualName = (actualIdx >= 0) ? h2hGetBottleName(actualIdx) : "";
                const char* guessedName = (p->guessBottleIdx >= 0) ? h2hGetBottleName(p->guessBottleIdx) : "";
                snprintf(pjson, sizeof(pjson),
                    "{\"t\":\"h2h_result\",\"ok\":%s,\"glass\":%d,\"actual\":\"%s\",\"guessed\":\"%s\"}",
                    p->correct ? "true" : "false",
                    p->claimedGlass,
                    actualName, guessedName);
            }
            wsServer.sendTXT(p->clientNum, pjson);
        }
    }
}

void wifiPortalH2HBroadcast() {
    if (!portalRunning) return;
    buildAndBroadcastH2H();
}

// ============================================================
// JSON unescape helper
// ============================================================

static void jsonUnescapeInPlace(char* s) {
    char* out = s;
    for (char* in = s; *in; in++) {
        if (*in == '\\' && *(in + 1)) {
            in++;
            switch (*in) {
                case '"':  *out++ = '"';  break;
                case '\\': *out++ = '\\'; break;
                default:   *out++ = *in;  break;
            }
        } else if ((unsigned char)*in >= 0x20) {
            *out++ = *in;
        }
    }
    *out = '\0';
}

// ============================================================
// Wi-Fi scan/connect WebSocket handlers
// ============================================================

static void handleWifiScan() {
    Serial.println("[WiFi] Starting network scan...");
    int n = WiFi.scanNetworks();
    if (n < 0) n = 0;
    if (n > WIFI_MAX_SCAN_RESULTS) n = WIFI_MAX_SCAN_RESULTS;

    char json[1024];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"t\":\"wifi_nets\",\"nets\":[");
    for (int i = 0; i < n; i++) {
        if (i > 0 && pos < (int)sizeof(json) - 1) json[pos++] = ',';
        pos += snprintf(json + pos, sizeof(json) - pos,
            "{\"s\":\"");
        String ssid = WiFi.SSID(i);
        for (int j = 0; j < (int)ssid.length() && pos < (int)sizeof(json) - 4; j++) {
            char c = ssid[j];
            if (c == '"' || c == '\\') json[pos++] = '\\';
            json[pos++] = c;
        }
        pos += snprintf(json + pos, sizeof(json) - pos,
            "\",\"r\":%d,\"e\":%s}",
            WiFi.RSSI(i),
            WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    }
    pos += snprintf(json + pos, sizeof(json) - pos, "]}");
    json[pos] = '\0';

    WiFi.scanDelete();
    wsServer.broadcastTXT(json);
    Serial.printf("[WiFi] Scan complete: %d networks\n", n);
}

static void handleWifiConnect(const char* ssid, const char* pass) {
    Serial.printf("[WiFi] Connecting to: %s\n", ssid);

    wifiSaveCredentials(ssid, pass);

    // We need to tear down AP and try STA. If it fails, we come
    // back to AP. The phone will lose connection during this, but
    // if STA succeeds the phone UI will tell the user the new IP.

    // Stop DNS (captive portal) — not needed for STA attempt
    dnsServer.stop();

    // Disconnect AP, try STA
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);

    Serial.println("[WiFi] Waiting for STA connection...");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_STA_TIMEOUT) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        staMode = true;
        IPAddress ip = WiFi.localIP();
        Serial.printf("[WiFi] STA connected! IP: %s\n", ip.toString().c_str());

        // Restart mDNS on STA interface
        MDNS.end();
        if (MDNS.begin(MDNS_NAME)) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("ws", "tcp", 81);
        }

        // Broadcast success (phone may already be disconnected
        // from AP, but if on same network it'll reconnect)
        char json[128];
        snprintf(json, sizeof(json),
            "{\"t\":\"wifi_status\",\"ok\":true,\"ip\":\"%s\"}",
            ip.toString().c_str());
        wsServer.broadcastTXT(json);
    } else {
        Serial.println("[WiFi] STA connection failed, reverting to AP");
        staMode = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);

        IPAddress apIP = WiFi.softAPIP();
        dnsServer.start(DNS_PORT, "*", apIP);

        MDNS.end();
        if (MDNS.begin(MDNS_NAME)) {
            MDNS.addService("http", "tcp", 80);
            MDNS.addService("ws", "tcp", 81);
        }

        wsServer.broadcastTXT("{\"t\":\"wifi_status\",\"ok\":false}");
    }
}

// ============================================================
// WebSocket action parser
// ============================================================

static void handleWSAction(uint8_t clientNum, uint8_t* payload, size_t length) {
    char msg[256];
    int len = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
    memcpy(msg, payload, len);
    msg[len] = '\0';

    char* aPtr = strstr(msg, "\"a\":\"");
    if (!aPtr) return;
    aPtr += 5;
    char* aEnd = strchr(aPtr, '"');
    if (!aEnd) return;
    *aEnd = '\0';
    String action(aPtr);
    *aEnd = '"';

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

    // --- Favorites ---
    if (action == "fav_list") {
        sendFavoritesJSON(clientNum);
        return;
    }
    if (action == "fav_add") {
        char* vPtr = strstr(msg, "\"v\":\"");
        if (!vPtr) return;
        vPtr += 5;
        char* vEnd = strrchr(vPtr, '"');
        if (!vEnd || vEnd == vPtr) return;
        *vEnd = '\0';
        jsonUnescapeInPlace(vPtr);
        Serial.printf("[WiFi] Phone fav_add: %s\n", vPtr);
        favoritesAdd(vPtr);
        broadcastFavoritesJSON();
        return;
    }
    if (action == "fav_del") {
        char* vPtr = strstr(msg, "\"v\":");
        if (!vPtr) return;
        vPtr += 4;
        int idx = atoi(vPtr);
        Serial.printf("[WiFi] Phone fav_del: %d\n", idx);
        favoritesRemove(idx);
        broadcastFavoritesJSON();
        return;
    }

    // --- Wi-Fi scan ---
    if (action == "wifi_scan") {
        handleWifiScan();
        return;
    }

    // --- Wi-Fi connect ---
    if (action == "wifi_connect") {
        char* sPtr = strstr(msg, "\"s\":\"");
        if (!sPtr) return;
        sPtr += 5;
        char* sEnd = strchr(sPtr, '"');
        if (!sEnd) return;
        *sEnd = '\0';
        char ssidBuf[33];
        strncpy(ssidBuf, sPtr, sizeof(ssidBuf) - 1);
        ssidBuf[sizeof(ssidBuf) - 1] = '\0';
        jsonUnescapeInPlace(ssidBuf);

        char* pPtr = strstr(sEnd + 1, "\"p\":\"");
        char passBuf[64] = "";
        if (pPtr) {
            pPtr += 5;
            char* pEnd = strrchr(pPtr, '"');
            if (pEnd && pEnd > pPtr) {
                *pEnd = '\0';
                strncpy(passBuf, pPtr, sizeof(passBuf) - 1);
                passBuf[sizeof(passBuf) - 1] = '\0';
                jsonUnescapeInPlace(passBuf);
            }
        }

        handleWifiConnect(ssidBuf, passBuf);
        return;
    }

    // --- Wi-Fi forget ---
    if (action == "wifi_forget") {
        wifiForgetCredentials();
        return;
    }

    // --- H2H: join lobby ---
    if (action == "h2h_join") {
        char* vPtr = strstr(msg, "\"v\":\"");
        if (!vPtr) return;
        vPtr += 5;
        char* vEnd = strrchr(vPtr, '"');
        if (!vEnd || vEnd == vPtr) return;
        *vEnd = '\0';
        jsonUnescapeInPlace(vPtr);
        Serial.printf("[WiFi] H2H join: %s (client %u)\n", vPtr, clientNum);
        int result = h2hPhoneJoin(clientNum, vPtr);
        if (result < 0) {
            wsServer.sendTXT(clientNum, "{\"t\":\"h2h_err\",\"msg\":\"Lobby full\"}");
        }
        return;
    }

    // --- H2H: submit 2x2 guess ---
    if (action == "h2h_guess") {
        char* g0Ptr = strstr(msg, "\"g0\":");
        char* g1Ptr = strstr(msg, "\"g1\":");
        if (!g0Ptr || !g1Ptr) return;
        int g0 = atoi(g0Ptr + 5);
        int g1 = atoi(g1Ptr + 5);
        Serial.printf("[WiFi] H2H guess from client %u: [%d, %d]\n",
                      clientNum, g0, g1);
        h2hPhoneGuess2x2(clientNum, g0, g1);
        return;
    }

    // --- H2H: claim a glass ---
    if (action == "h2h_claim") {
        char* vPtr = strstr(msg, "\"v\":");
        if (!vPtr) return;
        int glassNum = atoi(vPtr + 4);
        Serial.printf("[WiFi] H2H claim glass %d from client %u\n",
                      glassNum, clientNum);
        h2hPhoneClaimGlass(clientNum, glassNum);
        return;
    }

    // --- H2H: submit Random guess ---
    if (action == "h2h_guess_random") {
        char* vPtr = strstr(msg, "\"v\":");
        if (!vPtr) return;
        int bottleIdx = atoi(vPtr + 4);
        Serial.printf("[WiFi] H2H random guess bottle %d from client %u\n",
                      bottleIdx, clientNum);
        h2hPhoneGuessRandom(clientNum, bottleIdx);
        return;
    }

    // --- H2H: submit Premium guess ---
    if (action == "h2h_guess_premium") {
        char* vPtr = strstr(msg, "\"v\":");
        if (!vPtr) return;
        bool guess = (atoi(vPtr + 4) != 0);
        Serial.printf("[WiFi] H2H premium guess %s from client %u\n",
                      guess ? "YES" : "NO", clientNum);
        h2hPhoneGuessPremium(clientNum, guess);
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
            sendStateToClient(num);
            if (h2hIsActive()) buildAndBroadcastH2H();
            break;

        case WStype_DISCONNECTED:
            Serial.printf("[WiFi] WS client %u disconnected\n", num);
            h2hPhoneDisconnect(num);
            break;

        case WStype_TEXT:
            handleWSAction(num, payload, length);
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

static void handleGenerate204() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

static void handleHotspotDetect() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

static void handleConnectTest() {
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

static void handleNotFound() {
    if (staMode) {
        webServer.send(404, "text/plain", "Not Found");
    } else {
        webServer.sendHeader("Location", "/");
        webServer.send(302, "text/plain", "");
    }
}

// ============================================================
// Public API
// ============================================================

void wifiPortalInit() {
    if (portalRunning) return;

    loadCredentials();

    // --- Try STA mode if credentials are saved ---
    if (savedSSID[0] != '\0') {
        Serial.printf("[WiFi] Trying STA: %s\n", savedSSID);
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID, savedPass);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - start < WIFI_STA_TIMEOUT) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            staMode = true;
            IPAddress ip = WiFi.localIP();
            Serial.printf("[WiFi] STA connected! IP: %s\n", ip.toString().c_str());
            Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());
        } else {
            Serial.println("[WiFi] STA connection failed, falling back to AP");
            WiFi.disconnect(true);
            staMode = false;
        }
    }

    // --- Fall back to AP mode ---
    if (!staMode) {
        Serial.println("[WiFi] Starting soft AP...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);

        IPAddress apIP = WiFi.softAPIP();
        Serial.printf("[WiFi] AP SSID: %s\n", AP_SSID);
        Serial.printf("[WiFi] AP IP:   %s\n", apIP.toString().c_str());

        // DNS server (captive portal) — AP mode only
        dnsServer.start(DNS_PORT, "*", apIP);
        Serial.println("[WiFi] DNS server started (captive portal)");
    }

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

    if (!staMode) {
        dnsServer.processNextRequest();
    }
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
    if (staMode) {
        return wsServer.connectedClients();
    }
    return WiFi.softAPgetStationNum();
}

void wifiPortalStop() {
    if (!portalRunning) return;

    Serial.println("[WiFi] Stopping portal...");

    wsServer.close();
    webServer.stop();
    if (!staMode) {
        dnsServer.stop();
    }
    MDNS.end();

    if (staMode) {
        WiFi.disconnect(true);
    } else {
        WiFi.softAPdisconnect(true);
    }
    WiFi.mode(WIFI_OFF);

    portalRunning = false;
    staMode = false;
    Serial.println("[WiFi] Portal stopped");
}

bool wifiPortalIsRunning() {
    return portalRunning;
}

void wifiReconnect() {
    if (!portalRunning) return;
    if (!wifiHasCredentials()) return;

    Serial.printf("[WiFi] Reconnecting to: %s\n", savedSSID);

    // Tear down current mode
    if (!staMode) {
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
    } else {
        WiFi.disconnect(true);
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID, savedPass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - start < WIFI_STA_TIMEOUT) {
        delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
        staMode = true;
        Serial.printf("[WiFi] Reconnected! IP: %s\n",
                      WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] Reconnect failed, reverting to AP");
        staMode = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        IPAddress apIP = WiFi.softAPIP();
        dnsServer.start(DNS_PORT, "*", apIP);
    }

    MDNS.end();
    if (MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("ws", "tcp", 81);
    }
}
