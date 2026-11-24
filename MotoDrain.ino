// MOTODRAIN_VERSION 1.1   (the build names the file with this number)
// Moto Drain Watch - ESP32-S3 N16R8 + OV7670 FIFO camera (22 pins) + 2 water wires + beeper
// 1.1: friendly water + smiley look, phone alerts anywhere (ntfy) with a picture,
//      good-morning "pan is dry" picture every day at 9 AM, Wi-Fi list to pick from,
//      Flip and Fix colors are saved on the board.
// (c) Moto Takeda 316-304-5398
//
// WIRING (camera pin -> board pin)
//   3V3 -> 3V3      GND -> GND      SIOC -> 8     SIOD -> 9     VSYNC -> 10
//   D0 -> 4   D1 -> 5   D2 -> 6   D3 -> 7   D4 -> 15  D5 -> 16  D6 -> 17  D7 -> 18
//   RCK -> 11   WR -> 12   WRST -> 13   RRST -> 14
//   OE -> GND   PWDN -> GND   RST -> 3V3   HREF and STR: nothing
// WATER: wire 1 -> pin 1     wire 2 -> GND
// BEEPER: + (long leg) -> pin 2     - -> GND

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <time.h>
#include "soc/gpio_reg.h"
#include "img_converters.h"

#define P_SIOC 8
#define P_SIOD 9
#define P_VSYNC 10
#define P_RCK 11
#define P_WR 12
#define P_WRST 13
#define P_RRST 14
#define P_WATER 1
#define P_BEEP 2
const int DPINS[8] = {4, 5, 6, 7, 15, 16, 17, 18};

#define W 320
#define H 240
#define FB_SIZE (W * H * 2)
#define CAM_ADDR 0x21

WebServer server(80);
Preferences prefs;
WiFiUDP udp;
uint8_t *fb = nullptr;
bool camOK = false, swapBytes = false, flipCam = false;
String camMsg = "starting";
String ssid, pass, topic, code = "04170417";
uint32_t touchBase = 0, touchNow = 0;
int sens = 15;
bool wet = false;
uint32_t silenceUntil = 0, lastBeep = 0, lastUdp = 0, lastWetAlert = 0;
int wetCount = 0, dryCount = 0, lastDay = -1;
bool sendNow = false, joinedMsg = false;

// ---------------------------------------------------------------- camera (SCCB)
bool wr(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(CAM_ADDR);
  Wire.write(reg);
  Wire.write(val);
  bool ok = Wire.endTransmission() == 0;
  delay(1);
  return ok;
}
int rd(uint8_t reg) {
  Wire.beginTransmission(CAM_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(CAM_ADDR, 1) != 1) return -1;
  return Wire.read();
}

const uint8_t INIT[][2] = {
  {0x12, 0x14}, {0x40, 0xD0}, {0x8C, 0x00}, {0x11, 0x01}, {0x3A, 0x04}, {0x3D, 0xC0},
  {0x0C, 0x04}, {0x3E, 0x19}, {0x72, 0x11}, {0x73, 0xF1}, {0xA2, 0x02},
  {0x17, 0x16}, {0x18, 0x04}, {0x32, 0x24}, {0x19, 0x02}, {0x1A, 0x7A}, {0x03, 0x0A},
  {0x70, 0x3A}, {0x71, 0x35},
  {0x7A, 0x20}, {0x7B, 0x10}, {0x7C, 0x1E}, {0x7D, 0x35}, {0x7E, 0x5A}, {0x7F, 0x69},
  {0x80, 0x76}, {0x81, 0x80}, {0x82, 0x88}, {0x83, 0x8F}, {0x84, 0x96}, {0x85, 0xA3},
  {0x86, 0xAF}, {0x87, 0xC4}, {0x88, 0xD7}, {0x89, 0xE8},
  {0x13, 0xE0}, {0x00, 0x00}, {0x10, 0x00}, {0x0D, 0x40}, {0x14, 0x18},
  {0xA5, 0x05}, {0xAB, 0x07}, {0x24, 0x95}, {0x25, 0x33}, {0x26, 0xE3},
  {0x9F, 0x78}, {0xA0, 0x68}, {0xA1, 0x03}, {0xA6, 0xD8}, {0xA7, 0xD8}, {0xA8, 0xF0},
  {0xA9, 0x90}, {0xAA, 0x94}, {0x13, 0xE5},
  {0x0E, 0x61}, {0x0F, 0x4B}, {0x16, 0x02}, {0x1E, 0x07}, {0x21, 0x02}, {0x22, 0x91},
  {0x29, 0x07}, {0x33, 0x0B}, {0x35, 0x0B}, {0x37, 0x1D}, {0x38, 0x71}, {0x39, 0x2A},
  {0x3C, 0x78}, {0x4D, 0x40}, {0x4E, 0x20}, {0x69, 0x00}, {0x6B, 0x4A}, {0x74, 0x10},
  {0x8D, 0x4F}, {0x8E, 0x00}, {0x8F, 0x00}, {0x90, 0x00}, {0x91, 0x00}, {0x96, 0x00},
  {0x9A, 0x00}, {0xB0, 0x84}, {0xB1, 0x0C}, {0xB2, 0x0E}, {0xB3, 0x82}, {0xB8, 0x0A},
  {0x43, 0x0A}, {0x44, 0xF0}, {0x45, 0x34}, {0x46, 0x58}, {0x47, 0x28}, {0x48, 0x3A},
  {0x59, 0x88}, {0x5A, 0x88}, {0x5B, 0x44}, {0x5C, 0x67}, {0x5D, 0x49}, {0x5E, 0x0E},
  {0x6C, 0x0A}, {0x6D, 0x55}, {0x6E, 0x11}, {0x6F, 0x9F}, {0x6A, 0x40}, {0x01, 0x40},
  {0x02, 0x60}, {0x13, 0xE7},
  {0x4F, 0xB3}, {0x50, 0xB3}, {0x51, 0x00}, {0x52, 0x3D}, {0x53, 0xA7}, {0x54, 0xE4},
  {0x58, 0x9E}, {0x41, 0x08}, {0x3F, 0x00}, {0x75, 0x05}, {0x76, 0xE1}, {0x4C, 0x00},
  {0x77, 0x01}, {0x4B, 0x09}, {0xC9, 0x60}, {0x56, 0x40}, {0x34, 0x11}, {0x3B, 0x12},
  {0xA4, 0x88}, {0x96, 0x00}, {0x97, 0x30}, {0x98, 0x20}, {0x99, 0x30}, {0x9A, 0x84},
  {0x9B, 0x29}, {0x9C, 0x03}, {0x9D, 0x4C}, {0x9E, 0x3F}, {0x78, 0x04},
  {0x79, 0x01}, {0xC8, 0xF0}, {0x79, 0x0F}, {0xC8, 0x00}, {0x79, 0x10}, {0xC8, 0x7E},
  {0x79, 0x0A}, {0xC8, 0x80}, {0x79, 0x0B}, {0xC8, 0x01}, {0x79, 0x0C}, {0xC8, 0x0F},
  {0x79, 0x0D}, {0xC8, 0x20}, {0x79, 0x09}, {0xC8, 0x80}, {0x79, 0x02}, {0xC8, 0xC0},
  {0x79, 0x03}, {0xC8, 0x40}, {0x79, 0x05}, {0xC8, 0x30}, {0x79, 0x26},
  {0x12, 0x14}, {0x40, 0xD0}
};

void applyFlip() {
  wr(0x1E, flipCam ? 0x37 : 0x07);   // mirror + upside down
}

void camInit() {
  Wire.begin(P_SIOD, P_SIOC, 100000);
  pinMode(P_VSYNC, INPUT);
  for (int i = 0; i < 8; i++) pinMode(DPINS[i], INPUT);
  pinMode(P_RCK, OUTPUT);
  pinMode(P_WR, OUTPUT);
  pinMode(P_WRST, OUTPUT);
  pinMode(P_RRST, OUTPUT);
  digitalWrite(P_WR, LOW);
  digitalWrite(P_WRST, HIGH);
  digitalWrite(P_RRST, HIGH);
  digitalWrite(P_RCK, LOW);
  int pid = rd(0x0A);
  if (pid != 0x76) {
    camMsg = pid < 0 ? "Camera not answering - check SIOC (8), SIOD (9), 3V3 and GND"
                     : "Camera gave a strange answer - check the wires";
    camOK = false;
    return;
  }
  wr(0x12, 0x80);
  delay(200);
  for (size_t i = 0; i < sizeof(INIT) / 2; i++) wr(INIT[i][0], INIT[i][1]);
  applyFlip();
  if (!fb) fb = (uint8_t *)ps_malloc(FB_SIZE);
  if (!fb) { camMsg = "No PSRAM memory - use the N16R8 board"; camOK = false; return; }
  camOK = true;
  camMsg = "Camera ready";
}

bool waitV(int level, uint32_t ms) {
  uint32_t t = millis();
  while (digitalRead(P_VSYNC) != level)
    if (millis() - t > ms) return false;
  return true;
}

static inline uint8_t readByte() {
  REG_WRITE(GPIO_OUT_W1TS_REG, 1UL << P_RCK);
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
  uint32_t v = REG_READ(GPIO_IN_REG);
  REG_WRITE(GPIO_OUT_W1TC_REG, 1UL << P_RCK);
  asm volatile("nop; nop; nop; nop; nop; nop; nop; nop;");
  return ((v >> 4) & 0x0F) | (((v >> 15) & 0x0F) << 4);
}

bool capture() {
  if (!camOK) camInit();
  if (!camOK) return false;
  if (!waitV(HIGH, 600)) { camMsg = "No picture signal - check VSYNC (pin 10)"; return false; }
  digitalWrite(P_WRST, LOW);
  delayMicroseconds(5);
  digitalWrite(P_WRST, HIGH);
  digitalWrite(P_WR, HIGH);
  if (!waitV(LOW, 600) || !waitV(HIGH, 600)) { digitalWrite(P_WR, LOW); camMsg = "Picture took too long"; return false; }
  digitalWrite(P_WR, LOW);
  noInterrupts();
  digitalWrite(P_RRST, LOW);
  readByte();
  readByte();
  digitalWrite(P_RRST, HIGH);
  for (uint32_t i = 0; i < FB_SIZE; i++) {
    fb[i] = readByte();
    if ((i & 0x3FFF) == 0) { interrupts(); noInterrupts(); }
  }
  interrupts();
  if (swapBytes)
    for (uint32_t i = 0; i < FB_SIZE; i += 2) { uint8_t t = fb[i]; fb[i] = fb[i + 1]; fb[i + 1] = t; }
  camMsg = "Camera ready";
  return true;
}

// picture as JPEG (caller frees). returns false if no picture
bool takeJpeg(uint8_t **out, size_t *len) {
  if (!capture()) return false;
  return fmt2jpg(fb, FB_SIZE, W, H, PIXFORMAT_RGB565, 80, out, len);
}

// ---------------------------------------------------------------- phone alerts (ntfy - works anywhere)
String myLink() {
  return WiFi.status() == WL_CONNECTED ? "http://" + WiFi.localIP().toString() : String("http://192.168.4.1");
}

bool pushPhone(const String &title, const String &msg, const char *tags, const char *prio, bool withPic) {
  if (topic.length() < 4 || WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient h;
  if (!h.begin(cli, "https://ntfy.sh/" + topic)) return false;
  h.addHeader("Title", title);
  h.addHeader("Tags", tags);
  h.addHeader("Priority", prio);
  h.addHeader("Click", myLink());
  int rc;
  uint8_t *jpg = nullptr;
  size_t len = 0;
  if (withPic && takeJpeg(&jpg, &len)) {
    h.addHeader("Filename", "drain.jpg");
    h.addHeader("Message", msg);
    rc = h.sendRequest("PUT", jpg, len);
    free(jpg);
  } else {
    rc = h.POST(msg);
  }
  h.end();
  return rc == 200;
}

// ---------------------------------------------------------------- water + beeper
void beep(int ms) {
  digitalWrite(P_BEEP, HIGH);
  delay(ms);
  digitalWrite(P_BEEP, LOW);
}

uint32_t touchAvg() {
  uint32_t s = 0;
  for (int i = 0; i < 8; i++) s += touchRead(P_WATER);
  return s / 8;
}

void setDry() {
  touchBase = touchAvg();
  prefs.putUInt("base", touchBase);
  wet = false;
  wetCount = dryCount = 0;
}

void waterLoop() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();
  touchNow = touchAvg();
  if (!touchBase) return;
  uint32_t limit = touchBase + touchBase * sens / 100;
  if (touchNow > limit) { wetCount++; dryCount = 0; } else { dryCount++; wetCount = 0; }
  if (!wet && wetCount >= 3) { wet = true; lastWetAlert = 0; }
  if (wet && dryCount >= 6) {
    wet = false;
    pushPhone("Drain pan is dry again", "All good now. Thank you!", "smile,droplet", "default", false);
  }
  if (wet && (lastWetAlert == 0 || millis() - lastWetAlert > 2UL * 3600UL * 1000UL)) {
    lastWetAlert = millis();
    pushPhone("WATER in the drain pan!", "Please check the AC drain. Picture attached.", "warning,droplet", "high", true);
  }
  if (wet && millis() > silenceUntil && millis() - lastBeep > 1500) {
    lastBeep = millis();
    beep(150); delay(100); beep(150);
  }
}

// every morning at 9 a picture that says all is well
void dailyLoop() {
  static uint32_t last = 0;
  if (millis() - last < 20000) return;
  last = millis();
  struct tm t;
  if (!getLocalTime(&t, 10)) return;
  if (t.tm_hour == 9 && t.tm_yday != lastDay) {
    lastDay = t.tm_yday;
    if (wet) pushPhone("Good morning - WATER in the pan", "The drain pan has water. Please check it.", "warning,droplet", "high", true);
    else pushPhone("Good morning! Drain pan is DRY", "Everything looks good today. Have a blessed day!", "smile,sunny", "default", true);
  }
}

// ---------------------------------------------------------------- web page
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Moto Drain Watch</title>
<style>
body{margin:0;font-family:sans-serif;color:#063b5c;background:linear-gradient(#bfe9ff,#e9f8ff 45%,#ffffff);min-height:100vh;overflow-x:hidden}
.sea{position:fixed;left:0;right:0;bottom:0;height:120px;z-index:-1;opacity:.55}
.sea svg{position:absolute;bottom:0;width:200%;height:120px;animation:w 9s linear infinite}
.sea svg+svg{opacity:.6;animation-duration:14s}
@keyframes w{to{transform:translateX(-50%)}}
.w{max-width:520px;margin:auto;padding:12px 12px 130px}
h1{font-size:24px;text-align:center;margin:8px 0 2px;color:#0a6fb3}
.sub{text-align:center;font-size:14px;color:#3c7aa3;margin-bottom:10px}
.card{background:#ffffffe6;border-radius:22px;padding:14px;margin-bottom:12px;box-shadow:0 6px 20px #6ab7e655}
#st{text-align:center;padding:16px;border-radius:20px;transition:.4s}
#face{font-size:84px;line-height:1}
#word{font-size:26px;font-weight:bold;margin-top:6px}
.dry{background:linear-gradient(#d9fbe6,#b8f0cf);color:#11643a}
.wet{background:linear-gradient(#ffe0e0,#ffbcbc);color:#8a1414;animation:f 1s infinite}
@keyframes f{50%{transform:scale(1.03)}}
canvas{width:100%;border-radius:16px;background:#cfeaff;display:block}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
button,select{flex:1;min-width:130px;padding:14px;font-size:17px;font-weight:bold;border:0;border-radius:16px;cursor:pointer}
button{color:#fff;background:linear-gradient(#45b4ff,#1c7fd6);box-shadow:0 4px 0 #135a99}
button:active{transform:translateY(3px);box-shadow:0 1px 0 #135a99}
.g{background:linear-gradient(#52d98a,#22a85a);box-shadow:0 4px 0 #17703c}
.o{background:linear-gradient(#ffb357,#f08a1c);box-shadow:0 4px 0 #a85a0c}
select,input{width:100%;padding:12px;font-size:17px;border-radius:12px;border:2px solid #9fd3f5;box-sizing:border-box;margin:5px 0;background:#f4fbff;color:#063b5c}
small{color:#3c7aa3}a{color:#0a6fb3;font-weight:bold}
.ft{text-align:center;font-size:11px;color:#5d8fb3;margin:10px}
</style></head><body>
<div class=sea><svg viewBox="0 0 1200 120" preserveAspectRatio=none><path fill="#4fb3ff" d="M0 60 Q75 20 150 60 T300 60 T450 60 T600 60 T750 60 T900 60 T1050 60 T1200 60 V120 H0Z"/></svg>
<svg viewBox="0 0 1200 120" preserveAspectRatio=none><path fill="#1c8fe6" d="M0 80 Q75 50 150 80 T300 80 T450 80 T600 80 T750 80 T900 80 T1050 80 T1200 80 V120 H0Z"/></svg></div>
<div class=w>
<h1>&#128167; Moto Drain Watch &#128167;</h1><div class=sub>Keeping your AC drain happy &#128522;</div>
<div class=card><div id=st class=dry><div id=face>&#128522;</div><div id=word>All dry - all good!</div></div>
<div class=row><button class=o onclick="go('/silence')">&#128263; Quiet 30 min</button><button onclick="go('/beep')">&#128264; Test beep</button></div>
<div class=row><button class=g onclick="if(confirm('Is the pan dry right now?'))go('/dry')">&#128077; Pan is dry now</button></div>
<small id=info></small></div>
<div class=card><canvas id=cv width=320 height=240></canvas>
<div class=row><button onclick=snap()>&#128247; Take picture</button><button id=lb onclick=live()>&#127909; Live: OFF</button></div>
<div class=row><button onclick="go('/flip')">&#128260; Flip</button><button onclick="go('/swap')">&#127752; Fix colors</button></div>
<div class=row><button class=g onclick="go('/send')">&#128241; Send picture to phones</button></div>
<small id=cm></small></div>
<div class=card><b>&#128241; Phone alerts anywhere</b><br><small>Install the free <b>ntfy</b> app, then open this link on each phone (yours and Terry's):</small><br>
<a id=al href=#></a></div>
<div class=card><b>&#9881; Setup</b> <small>(code needed)</small>
<input id=k type=password placeholder="Code">
<select id=s><option value="">Pick Wi-Fi...</option></select><button onclick=scan()>&#128246; Find Wi-Fi</button>
<input id=p type=password placeholder="Wi-Fi password">
<small>Sensitivity: <span id=sv></span>%</small><input id=se type=range min=5 max=60 onchange="go('/sens?v='+this.value)">
<div class=row><button class=g onclick=save()>&#128190; Save</button></div></div>
<div class=ft>(c) Moto Takeda 316-304-5398</div></div>
<script>
var lv=false,busy=false;
function $(i){return document.getElementById(i)}
function go(u){fetch(u).then(r=>r.text()).then(t=>{if(t)$('info').textContent=t;st();if(u=='/flip'||u=='/swap')snap()})}
function st(){fetch('/status').then(r=>r.json()).then(j=>{var e=$('st');e.className=j.wet?'wet':'dry';
$('face').innerHTML=j.wet?'&#128552;':'&#128522;';$('word').textContent=j.wet?'WATER! Please check the drain':'All dry - all good!';
$('info').textContent='Sensor '+j.now+' / dry '+j.base+'  -  Wi-Fi: '+j.ip+(j.quiet?'  (quiet)':'');
$('cm').textContent=j.cam;$('sv').textContent=j.sens;$('se').value=j.sens;var a=$('al');a.href=j.alert;a.textContent=j.alert})}
function draw(raw){var c=$('cv'),x=c.getContext('2d'),im=x.createImageData(320,240),d=im.data;
for(var i=0;i<76800;i++){var v=raw[i*2]<<8|raw[i*2+1],o=i*4;d[o]=(v>>11&31)*255/31;d[o+1]=(v>>5&63)*255/63;d[o+2]=(v&31)*255/31;d[o+3]=255}x.putImageData(im,0,0)}
function snap(){if(busy)return;busy=true;fetch('/frame').then(r=>{if(!r.ok)throw 0;return r.arrayBuffer()}).then(b=>{draw(new Uint8Array(b));busy=false;if(lv)setTimeout(snap,300)})
.catch(()=>{busy=false;st();if(lv)setTimeout(snap,2000)})}
function live(){lv=!lv;$('lb').innerHTML=lv?'&#127909; Live: ON':'&#127909; Live: OFF';if(lv)snap()}
function scan(){$('s').innerHTML='<option>Looking...</option>';fetch('/scan').then(r=>r.json()).then(l=>{$('s').innerHTML='<option value="">Pick Wi-Fi...</option>'+l.map(n=>'<option>'+n.replace(/</g,'')+'</option>').join('')})}
function save(){var q='/save?k='+encodeURIComponent($('k').value)+'&s='+encodeURIComponent($('s').value)+'&p='+encodeURIComponent($('p').value);
fetch(q).then(r=>r.text()).then(t=>alert(t))}
st();setInterval(st,2000);snap();
</script></body></html>)HTML";

void handleStatus() {
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.SSID() + " " + WiFi.localIP().toString() : String("not joined yet");
  String j = "{\"wet\":" + String(wet ? "true" : "false") + ",\"now\":" + touchNow + ",\"base\":" + touchBase +
             ",\"sens\":" + sens + ",\"ip\":\"" + ip + "\",\"quiet\":" + String(millis() < silenceUntil ? "true" : "false") +
             ",\"cam\":\"" + camMsg + "\",\"alert\":\"https://ntfy.sh/" + topic + "\"}";
  server.send(200, "application/json", j);
}

void handleFrame() {
  if (!capture()) { server.send(503, "text/plain", camMsg); return; }
  server.setContentLength(FB_SIZE);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/octet-stream", "");
  WiFiClient c = server.client();
  size_t s = 0;
  uint32_t t = millis();
  while (s < FB_SIZE && c.connected() && millis() - t < 8000) {
    size_t n = c.write(fb + s, min((size_t)2048, (size_t)(FB_SIZE - s)));
    s += n;
    if (!n) delay(2);
  }
}

void handleJpg() {
  uint8_t *jpg = nullptr;
  size_t len = 0;
  if (!takeJpeg(&jpg, &len)) { server.send(503, "text/plain", camMsg); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char *)jpg, len);
  free(jpg);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    s.replace("\"", "");
    s.replace("\\", "");
    if (!s.length() || j.indexOf("\"" + s + "\"") >= 0) continue;
    if (j.length() > 1) j += ",";
    j += "\"" + s + "\"";
  }
  j += "]";
  server.send(200, "application/json", j);
}

void handleSave() {
  if (server.arg("k") != code) { server.send(403, "text/plain", "Wrong code"); return; }
  if (server.arg("s").length()) {
    prefs.putString("s", server.arg("s"));
    prefs.putString("p", server.arg("p"));
    server.send(200, "text/plain", "Saved! Restarting to join " + server.arg("s") + ". Your phones will get a hello message.");
    delay(800);
    ESP.restart();
  }
  server.send(200, "text/plain", "Pick a Wi-Fi first.");
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  pinMode(P_BEEP, OUTPUT);
  digitalWrite(P_BEEP, LOW);
  prefs.begin("drain", false);
  ssid = prefs.getString("s", "");
  pass = prefs.getString("p", "");
  sens = prefs.getInt("sens", 15);
  touchBase = prefs.getUInt("base", 0);
  swapBytes = prefs.getBool("sw", false);
  flipCam = prefs.getBool("fl", false);
  topic = prefs.getString("t", "");
  if (topic.length() < 4) {   // a private, hard-to-guess alert name for this board
    uint64_t mac = ESP.getEfuseMac();
    char b[40];
    snprintf(b, sizeof(b), "moto-drain-%04x%08x", (uint16_t)(mac >> 32), (uint32_t)mac ^ 0x5A17C0DE);
    topic = b;
    prefs.putString("t", topic);
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("MotoDrain", code.c_str());
  if (ssid.length()) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(200);
  }
  configTzTime("CST6CDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
  MDNS.begin("motodrain");
  udp.begin(4210);

  camInit();
  if (!touchBase) setDry();

  server.on("/", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/status", handleStatus);
  server.on("/frame", handleFrame);
  server.on("/pic.jpg", handleJpg);
  server.on("/scan", handleScan);
  server.on("/save", handleSave);
  server.on("/beep", [] { beep(300); server.send(200, "text/plain", "Beep!"); });
  server.on("/dry", [] { setDry(); server.send(200, "text/plain", "Thank you! This is dry now."); });
  server.on("/silence", [] { silenceUntil = millis() + 30UL * 60UL * 1000UL; server.send(200, "text/plain", "Quiet for 30 minutes."); });
  server.on("/sens", [] { sens = constrain(server.arg("v").toInt(), 5, 60); prefs.putInt("sens", sens); server.send(200, "text/plain", ""); });
  server.on("/flip", [] { flipCam = !flipCam; prefs.putBool("fl", flipCam); applyFlip(); server.send(200, "text/plain", "Flipped."); });
  server.on("/swap", [] { swapBytes = !swapBytes; prefs.putBool("sw", swapBytes); server.send(200, "text/plain", "Colors changed."); });
  server.on("/send", [] {
    if (WiFi.status() != WL_CONNECTED) { server.send(200, "text/plain", "Join a Wi-Fi with internet first (Setup)."); return; }
    sendNow = true;
    server.send(200, "text/plain", "Sending the picture to your phones...");
  });
  server.begin();
  beep(80);
}

void loop() {
  server.handleClient();
  waterLoop();
  dailyLoop();
  if (sendNow) {
    sendNow = false;
    pushPhone(wet ? "Drain pan has WATER" : "Drain pan picture - all dry", "Here is the drain pan right now.", wet ? "warning,droplet" : "smile,camera", "default", true);
  }
  if (!joinedMsg && WiFi.status() == WL_CONNECTED) {
    joinedMsg = true;
    pushPhone("Moto Drain Watch is online", "Hello! I am watching the AC drain. Tap to open me at home: " + myLink(), "smile,droplet", "default", false);
  }
  if (millis() - lastUdp > 5000) {
    lastUdp = millis();
    String m = "AMYBOX|Drain pan|" + String(wet ? "WET" : "DRY") + "|Water";
    udp.beginPacket(IPAddress(255, 255, 255, 255), 4210);
    udp.print(m);
    udp.endPacket();
  }
  if (WiFi.status() != WL_CONNECTED && ssid.length()) {
    static uint32_t retry = 0;
    if (millis() - retry > 30000) { retry = millis(); WiFi.begin(ssid.c_str(), pass.c_str()); }
  }
}
