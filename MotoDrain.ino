// MOTODRAIN_VERSION 1.0   (the build names the file with this number)
// Moto Drain Watch - ESP32-S3 N16R8 + OV7670 FIFO camera (22 pins) + 2 water wires + beeper
// (c) Moto Takeda 316-304-5398
//
// WIRING (camera pin -> board pin)
//   3V3 -> 3V3      GND -> GND      SIOC -> 8     SIOD -> 9     VSYNC -> 10
//   D0 -> 4   D1 -> 5   D2 -> 6   D3 -> 7   D4 -> 15  D5 -> 16  D6 -> 17  D7 -> 18
//   RCK -> 11   WR -> 12   WRST -> 13   RRST -> 14
//   OE -> GND   PWDN -> GND   RST -> 3V3   HREF and STR: nothing
// WATER: wire 1 -> pin 1     wire 2 -> GND      (tape both in the pan, a finger apart)
// BEEPER: + (long leg) -> pin 2     - -> GND

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include "soc/gpio_reg.h"

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
bool camOK = false;
String camMsg = "starting";
String ssid, pass, topic, code = "04170417";
uint32_t touchBase = 0, touchNow = 0;
int sens = 15;            // percent rise that means water
bool wet = false, pushed = false;
uint32_t silenceUntil = 0, lastBeep = 0, lastUdp = 0, wetSince = 0;
int wetCount = 0, dryCount = 0;

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
    camMsg = pid < 0 ? "camera not answering - check SIOC (8) and SIOD (9), 3V3 and GND"
                     : "camera answered with a strange id - check wires";
    camOK = false;
    return;
  }
  wr(0x12, 0x80);   // reset
  delay(200);
  for (size_t i = 0; i < sizeof(INIT) / 2; i++) wr(INIT[i][0], INIT[i][1]);
  if (!fb) fb = (uint8_t *)ps_malloc(FB_SIZE);
  if (!fb) {
    camMsg = "no PSRAM memory - pick the N16R8 board";
    camOK = false;
    return;
  }
  camOK = true;
  camMsg = "camera ready";
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
  if (!camOK) return false;
  // write one frame into the camera's own memory
  if (!waitV(HIGH, 600)) { camMsg = "no VSYNC - check pin 10"; return false; }
  digitalWrite(P_WRST, LOW);
  delayMicroseconds(5);
  digitalWrite(P_WRST, HIGH);
  digitalWrite(P_WR, HIGH);
  if (!waitV(LOW, 600) || !waitV(HIGH, 600)) { digitalWrite(P_WR, LOW); camMsg = "frame timeout"; return false; }
  digitalWrite(P_WR, LOW);
  // read it out
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
  camMsg = "camera ready";
  return true;
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
  pushed = false;
}

void pushAlert(const String &msg) {
  if (topic.length() < 4 || WiFi.status() != WL_CONNECTED) return;
  HTTPClient h;
  h.begin("http://ntfy.sh/" + topic);
  h.addHeader("Title", "Moto Drain Watch");
  h.addHeader("Priority", "high");
  h.POST(msg);
  h.end();
}

void waterLoop() {
  static uint32_t last = 0;
  if (millis() - last < 500) return;
  last = millis();
  touchNow = touchAvg();
  if (!touchBase) return;
  uint32_t limit = touchBase + touchBase * sens / 100;
  bool now = touchNow > limit;
  if (now) { wetCount++; dryCount = 0; } else { dryCount++; wetCount = 0; }
  if (!wet && wetCount >= 3) {
    wet = true;
    wetSince = millis();
    if (!pushed) { pushAlert("WATER in the drain pan! Check the AC drain."); pushed = true; }
  }
  if (wet && dryCount >= 6) { wet = false; pushed = false; }
  if (wet && millis() > silenceUntil && millis() - lastBeep > 1500) {
    lastBeep = millis();
    beep(150); delay(100); beep(150);
  }
}

// ---------------------------------------------------------------- web page
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Moto Drain Watch</title>
<style>
body{margin:0;font-family:sans-serif;color:#e8f4ff;background:#05070d radial-gradient(circle at 50% -20%,#1b3a6b,#05070d 70%);min-height:100vh}
.w{max-width:520px;margin:auto;padding:14px}
h1{font-size:22px;letter-spacing:2px;text-align:center;margin:6px 0 12px;color:#7fd1ff;text-shadow:0 0 12px #2a8cff}
.card{background:#0d1626cc;border:1px solid #2a4a7a;border-radius:16px;padding:14px;margin-bottom:12px;box-shadow:0 0 18px #0a2a55}
#st{font-size:30px;font-weight:bold;text-align:center;padding:18px;border-radius:14px;transition:.3s}
.dry{background:#0f3d24;color:#7dffb0;box-shadow:0 0 20px #1a7a44}
.wet{background:#5a0f0f;color:#ffb3b3;box-shadow:0 0 30px #ff3030;animation:f 1s infinite}
@keyframes f{50%{box-shadow:0 0 6px #ff3030}}
canvas{width:100%;border-radius:12px;background:#000;display:block}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
button{flex:1;min-width:120px;padding:14px;font-size:16px;font-weight:bold;color:#fff;border:0;border-radius:12px;
background:linear-gradient(#3a6fd8,#1d3f8a);box-shadow:0 4px 0 #0f2350,0 0 12px #2a5fd8;cursor:pointer}
button:active{transform:translateY(3px);box-shadow:0 1px 0 #0f2350}
.g{background:linear-gradient(#2fa85a,#196b37);box-shadow:0 4px 0 #0c3a1d}
.r{background:linear-gradient(#d84a4a,#8a1d1d);box-shadow:0 4px 0 #4a0c0c}
small{color:#8aa3c7}input{width:100%;padding:10px;font-size:16px;border-radius:10px;border:1px solid #2a4a7a;background:#0a1220;color:#fff;box-sizing:border-box;margin:4px 0}
.ft{text-align:center;font-size:11px;color:#4a6a95;margin:10px}
</style></head><body><div class=w>
<h1>MOTO DRAIN WATCH</h1>
<div class=card><div id=st class=dry>DRY</div>
<div class=row><button class=r onclick="go('/silence')">Quiet 30 min</button><button onclick="go('/beep')">Test beep</button></div>
<div class=row><button class=g onclick="if(confirm('Is the pan dry right now?'))go('/dry')">Pan is dry now</button></div>
<small id=info></small></div>
<div class=card><canvas id=cv width=320 height=240></canvas>
<div class=row><button onclick=snap()>Take picture</button><button id=lb onclick=live()>Live: OFF</button></div>
<div class=row><button onclick="sw=!sw;draw()">Fix colors</button><button onclick="fl=!fl;draw()">Flip</button></div>
<small id=cm></small></div>
<div class=card><b>Setup</b> <small>(code needed)</small>
<input id=k type=password placeholder="Code (04170417)">
<input id=s placeholder="Wi-Fi name"><input id=p type=password placeholder="Wi-Fi password">
<input id=t placeholder="Phone alert word (ntfy app)">
<small>Sensitivity: <span id=sv></span>% <input id=se type=range min=5 max=60 onchange="go('/sens?v='+this.value)"></small>
<div class=row><button class=g onclick=save()>Save</button></div></div>
<div class=ft>(c) Moto Takeda 316-304-5398</div></div>
<script>
var raw=null,sw=localStorage.sw=='1',fl=false,lv=false,busy=false;
function go(u){fetch(u).then(r=>r.text()).then(t=>{if(t)document.getElementById('info').textContent=t;st()})}
function st(){fetch('/status').then(r=>r.json()).then(j=>{var e=document.getElementById('st');e.className=j.wet?'wet':'dry';
e.textContent=j.wet?'WATER! DRAIN IS FULL':'DRY - all good';
document.getElementById('info').textContent='Sensor '+j.now+' / dry '+j.base+'   Wi-Fi '+j.ip+'   '+(j.quiet?'(quiet)':'');
document.getElementById('cm').textContent=j.cam;document.getElementById('sv').textContent=j.sens;document.getElementById('se').value=j.sens})}
function draw(){if(!raw)return;localStorage.sw=sw?'1':'0';var c=document.getElementById('cv'),x=c.getContext('2d'),im=x.createImageData(320,240),d=im.data;
for(var i=0;i<76800;i++){var a=raw[i*2],b=raw[i*2+1],v=sw?(b<<8|a):(a<<8|b);var o=(fl?76799-i:i)*4;
d[o]=(v>>11&31)*255/31;d[o+1]=(v>>5&63)*255/63;d[o+2]=(v&31)*255/31;d[o+3]=255}x.putImageData(im,0,0)}
function snap(){if(busy)return;busy=true;fetch('/frame').then(r=>{if(!r.ok)throw 0;return r.arrayBuffer()}).then(b=>{raw=new Uint8Array(b);draw();busy=false;if(lv)setTimeout(snap,300)})
.catch(()=>{busy=false;st();if(lv)setTimeout(snap,2000)})}
function live(){lv=!lv;document.getElementById('lb').textContent='Live: '+(lv?'ON':'OFF');if(lv)snap()}
function save(){var q='/save?k='+encodeURIComponent(k.value)+'&s='+encodeURIComponent(s.value)+'&p='+encodeURIComponent(p.value)+'&t='+encodeURIComponent(t.value);
fetch(q).then(r=>r.text()).then(t=>alert(t))}
st();setInterval(st,2000);snap();
</script></body></html>)HTML";

void handleStatus() {
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("not joined");
  String j = "{\"wet\":" + String(wet ? "true" : "false") + ",\"now\":" + touchNow + ",\"base\":" + touchBase +
             ",\"sens\":" + sens + ",\"ip\":\"" + ip + "\",\"quiet\":" + String(millis() < silenceUntil ? "true" : "false") +
             ",\"cam\":\"" + camMsg + "\"}";
  server.send(200, "application/json", j);
}

void handleFrame() {
  if (!camOK) camInit();
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

void handleSave() {
  if (server.arg("k") != code) { server.send(403, "text/plain", "Wrong code"); return; }
  if (server.arg("t").length()) { topic = server.arg("t"); prefs.putString("t", topic); }
  if (server.arg("s").length()) {
    prefs.putString("s", server.arg("s"));
    prefs.putString("p", server.arg("p"));
    server.send(200, "text/plain", "Saved. Restarting to join your Wi-Fi.");
    delay(800);
    ESP.restart();
  }
  server.send(200, "text/plain", "Saved.");
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  pinMode(P_BEEP, OUTPUT);
  digitalWrite(P_BEEP, LOW);
  prefs.begin("drain", false);
  ssid = prefs.getString("s", "");
  pass = prefs.getString("p", "");
  topic = prefs.getString("t", "");
  sens = prefs.getInt("sens", 15);
  touchBase = prefs.getUInt("base", 0);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("MotoDrain", code.c_str());
  if (ssid.length()) {
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(200);
  }
  MDNS.begin("motodrain");
  udp.begin(4210);

  camInit();
  if (!touchBase) setDry();

  server.on("/", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/status", handleStatus);
  server.on("/frame", handleFrame);
  server.on("/save", handleSave);
  server.on("/beep", [] { beep(300); server.send(200, "text/plain", "Beep!"); });
  server.on("/dry", [] { setDry(); server.send(200, "text/plain", "OK - this is dry now."); });
  server.on("/silence", [] { silenceUntil = millis() + 30UL * 60UL * 1000UL; server.send(200, "text/plain", "Quiet for 30 minutes."); });
  server.on("/sens", [] { sens = constrain(server.arg("v").toInt(), 5, 60); prefs.putInt("sens", sens); server.send(200, "text/plain", ""); });
  server.begin();
  beep(80);
}

void loop() {
  server.handleClient();
  waterLoop();
  if (millis() - lastUdp > 5000) {   // let Moto Hub see this board
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
