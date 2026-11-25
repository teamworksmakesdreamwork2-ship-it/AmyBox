// MOTODRAIN_VERSION 1.3   (the build names the file with this number)
// Moto Drain Watch - ESP32-S3 N16R8 + OV7670 FIFO camera (22 pins) + 2 water wires + beeper
// 1.1: friendly water + smiley look, phone alerts anywhere (ntfy) with a picture,
//      good-morning "pan is dry" picture every day at 9 AM, Wi-Fi list to pick from,
//      Flip and Fix colors are saved on the board.
// 1.2: premium new look, code needed to open the page, "New picture" from anywhere
//      (ntfy button + link), password eye, Forget Wi-Fi, MotoDrain hotspot turns off
//      once home Wi-Fi works (comes back if Wi-Fi is lost), Wi-Fi list always works.
//      Christmas + UFO look. Page code and MotoDrain hotspot password: Shocker1
// 1.3: Wi-Fi holds on: no power-save sleep, full radio power, auto-reconnect,
//      restarts itself if Wi-Fi is gone 5 minutes. Water alert is MAX priority so
//      the phone keeps ringing. Every alert has "Open page" with the board's address.
//      Sends a new message if its address changes. Shows Wi-Fi signal strength.
//      Camera sends small JPEG pictures (10x less data = fast on weak Wi-Fi).
//      A water alert that could not be sent is retried every minute until it goes out.
//      Page opens with no code; only Save/Forget Wi-Fi ask for the code (Shocker1).
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
String ssid, pass, topic, code = "Shocker1";
uint32_t touchBase = 0, touchNow = 0;
int sens = 15;
bool wet = false;
uint32_t silenceUntil = 0, lastBeep = 0, lastUdp = 0, lastWetAlert = 0;
int wetCount = 0, dryCount = 0, lastDay = -1;
bool sendNow = false, joinedMsg = false;
String snapSince = "1m";
uint32_t lastPoll = 0, lostSince = 0;
bool apOn = true;

String snapTopic() { return topic + "-snap"; }
String snapLink() { return "https://ntfy.sh/" + snapTopic() + "/publish?message=snap"; }
bool ok() { return true; }   // alarm page opens without a code (Terry doesn't have to type)
bool okSetup() {              // only Wi-Fi changes still need the code
  if (server.arg("k") == code) return true;
  server.send(403, "text/plain", "Setup code needed");
  return false;
}

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
  h.addHeader("Actions", "http, New picture, https://ntfy.sh/" + snapTopic() + ", method=POST, body=snap, clear=true; view, Open page, " + myLink() + "/?k=" + code);
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
  static uint32_t lastTry = 0;
  if (wet && (lastWetAlert == 0 || millis() - lastWetAlert > 2UL * 3600UL * 1000UL) && (lastTry == 0 || millis() - lastTry > 60000)) {
    lastTry = millis();
    if (pushPhone("WATER in the drain pan!", "Please check the AC drain. Picture attached.", "warning,droplet", "max", true)) lastWetAlert = millis();
  }
  if (!wet) lastTry = 0;
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
    if (wet) pushPhone("Good morning - WATER in the pan", "The drain pan has water. Please check it.", "warning,droplet", "max", true);
    else pushPhone("Good morning! Drain pan is DRY", "Everything looks good today. Have a blessed day!", "smile,sunny", "default", true);
  }
}

// ---------------------------------------------------------------- web page
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name=theme-color content="#070b1a"><title>Moto Drain Watch</title>
<style>
:root{--g1:#2ee59d;--g2:#ff3b5c;--gold:#ffd166;--r1:#ff5f6d;--r2:#ffc371;--card:rgba(255,255,255,.06);--line:rgba(255,255,255,.12);--t:#eaf2ff;--m:#8fa3c7}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;font-family:-apple-system,"Segoe UI",Roboto,sans-serif;color:var(--t);min-height:100vh;
background:radial-gradient(1100px 600px at 85% -10%,#5a1022 0,transparent 60%),radial-gradient(900px 520px at -15% 35%,#0d4a33 0,transparent 55%),#060a14;overflow-x:hidden}
.sea{position:fixed;left:0;right:0;bottom:0;height:140px;z-index:-1;opacity:.35;pointer-events:none}
.sea svg{position:absolute;bottom:0;width:200%;height:140px;animation:w 12s linear infinite}.sea svg+svg{animation-duration:18s;opacity:.6}
@keyframes w{to{transform:translateX(-50%)}}
.w{max-width:540px;margin:auto;padding:18px 16px 150px}
.top{display:flex;align-items:center;gap:12px;margin:4px 2px 18px}
.logo{width:46px;height:46px;border-radius:14px;display:grid;place-items:center;font-size:24px;background:linear-gradient(135deg,#1fa86a,#c8102e);box-shadow:0 8px 24px rgba(200,16,46,.45)}
.top h1{margin:0;font-size:21px;font-weight:700;letter-spacing:.3px;background:linear-gradient(90deg,#fff,var(--gold));-webkit-background-clip:text;color:transparent}.top small{color:var(--m);font-size:13px}
.live{margin-left:auto;font-size:12px;font-weight:700;padding:6px 10px;border-radius:99px;background:rgba(62,230,196,.12);color:var(--g1);border:1px solid rgba(62,230,196,.35)}
.live:before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--g1);margin-right:6px;animation:p 1.6s infinite}
@keyframes p{50%{opacity:.25}}
.card{background:var(--card);border:1px solid var(--line);border-radius:26px;padding:18px;margin-bottom:16px;backdrop-filter:blur(18px);-webkit-backdrop-filter:blur(18px);box-shadow:0 20px 50px rgba(0,0,0,.35)}
.lbl{font-size:12px;font-weight:700;letter-spacing:1.4px;text-transform:uppercase;color:var(--m);margin:0 0 12px}
#st{display:flex;align-items:center;gap:18px;padding:18px;border-radius:22px;transition:.5s;background:linear-gradient(135deg,rgba(46,229,157,.2),rgba(255,209,102,.1));border:1px solid rgba(255,209,102,.45)}
#st.wet{background:linear-gradient(135deg,rgba(255,95,109,.28),rgba(255,195,113,.15));border-color:rgba(255,95,109,.6);animation:al 1.2s infinite}
@keyframes al{50%{box-shadow:0 0 40px rgba(255,95,109,.45)}}
.ring{width:96px;height:96px;flex:none;border-radius:50%;display:grid;place-items:center;font-size:56px;background:radial-gradient(circle at 35% 30%,rgba(255,255,255,.25),rgba(255,255,255,.04));border:2px solid rgba(255,255,255,.18);box-shadow:inset 0 0 20px rgba(255,255,255,.08)}
#word{font-size:24px;font-weight:800;line-height:1.15}#sub{color:var(--m);font-size:14px;margin-top:6px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:14px}.full{grid-column:1/-1}
button{appearance:none;border:0;border-radius:18px;padding:16px 12px;font-size:16px;font-weight:700;color:#fff;cursor:pointer;
background:linear-gradient(180deg,rgba(255,255,255,.14),rgba(255,255,255,.05));border:1px solid var(--line);box-shadow:0 6px 18px rgba(0,0,0,.25),inset 0 1px 0 rgba(255,255,255,.15);transition:transform .12s,box-shadow .2s}
button:active{transform:scale(.97)}
.pri{background:linear-gradient(135deg,#ff4d6d,#c8102e);border:0;box-shadow:0 10px 28px rgba(200,16,46,.5),inset 0 1px 0 rgba(255,255,255,.35);color:#fff}
.grn{background:linear-gradient(135deg,#34d399,#0f8a4c);border:0;box-shadow:0 10px 28px rgba(15,138,76,.45);color:#fff}
.warm{background:linear-gradient(135deg,var(--r1),var(--r2));border:0;box-shadow:0 10px 28px rgba(255,95,109,.35);color:#2a0710}
.cam{position:relative;border-radius:20px;overflow:hidden;border:1px solid var(--line);background:#0b1430;aspect-ratio:4/3}
canvas{width:100%;height:100%;display:block;object-fit:cover}
.cam .tag{position:absolute;left:12px;top:12px;font-size:12px;font-weight:700;padding:5px 10px;border-radius:99px;background:rgba(0,0,0,.45);backdrop-filter:blur(6px)}
.info{color:var(--m);font-size:13px;margin-top:12px;line-height:1.5}
a{color:var(--g1);font-weight:700;word-break:break-all}
input,select{width:100%;padding:15px;font-size:16px;border-radius:16px;border:1px solid var(--line);background:rgba(0,0,0,.25);color:var(--t);margin:6px 0;outline:none}
input:focus,select:focus{border-color:var(--g2);box-shadow:0 0 0 3px rgba(47,140,255,.25)}
select option{color:#000}.pw{position:relative}.pw button{position:absolute;right:6px;top:12px;padding:8px 12px;border-radius:12px;font-size:18px}
input[type=range]{padding:0;height:6px;accent-color:var(--g1)}
#snow{position:fixed;inset:0;pointer-events:none;z-index:0}
.w{position:relative;z-index:1}
.lights{display:flex;justify-content:space-between;margin:-6px 6px 16px}
.lights i{width:10px;height:14px;border-radius:50% 50% 45% 45%;display:block;animation:tw 1.4s infinite}
.lights i:nth-child(4n+1){background:#ff3b5c;box-shadow:0 0 12px #ff3b5c}.lights i:nth-child(4n+2){background:#2ee59d;box-shadow:0 0 12px #2ee59d;animation-delay:.35s}
.lights i:nth-child(4n+3){background:#ffd166;box-shadow:0 0 12px #ffd166;animation-delay:.7s}.lights i:nth-child(4n){background:#5ab0ff;box-shadow:0 0 12px #5ab0ff;animation-delay:1.05s}
@keyframes tw{50%{opacity:.25;box-shadow:none}}
.ufo{position:fixed;top:70px;left:-90px;font-size:38px;z-index:2;pointer-events:none;animation:fly 16s linear infinite}
.ufo:after{content:"";position:absolute;left:50%;top:38px;width:46px;height:60px;transform:translateX(-50%);background:linear-gradient(rgba(255,240,150,.55),transparent);clip-path:polygon(35% 0,65% 0,100% 100%,0 100%);animation:beam 2s infinite}
@keyframes fly{0%{transform:translate(0,0)}25%{transform:translate(30vw,14px)}50%{transform:translate(60vw,-6px)}75%{transform:translate(90vw,10px)}100%{transform:translate(120vw,0)}}
@keyframes beam{50%{opacity:.2}}
.ft{text-align:center;font-size:11px;color:#5b6f94;margin-top:18px}
#login{position:fixed;inset:0;z-index:9;display:none;align-items:center;justify-content:center;background:rgba(4,7,18,.85);backdrop-filter:blur(10px)}
#login .card{width:88%;max-width:380px;text-align:center}
</style></head><body>
<div class=sea><svg viewBox="0 0 1200 140" preserveAspectRatio=none><path fill="#2f8cff" d="M0 70 Q75 30 150 70 T300 70 T450 70 T600 70 T750 70 T900 70 T1050 70 T1200 70 V140 H0Z"/></svg>
<svg viewBox="0 0 1200 140" preserveAspectRatio=none><path fill="#3ee6c4" d="M0 95 Q75 65 150 95 T300 95 T450 95 T600 95 T750 95 T900 95 T1050 95 T1200 95 V140 H0Z"/></svg></div>
<div id=login><div class=card><div class=logo style="margin:0 auto 12px">&#127873;</div><b>Moto Drain Watch</b><p class=info>Enter your code</p>
<input id=lk type=password placeholder="Code"><button class="pri" style="width:100%;margin-top:8px" onclick="K=$('lk').value;try{localStorage.k=K}catch(e){};$('login').style.display='none';st();snap()">Unlock</button></div></div>
<canvas id=snow></canvas><div class=ufo>&#128760;</div>
<div class=w>
<div class=top><div class=logo>&#127876;</div><div><h1>Moto Drain Watch</h1><small>Merry Christmas! &#127877; AC drain monitor</small></div><span class=live>LIVE</span></div>
<div class=lights><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i><i></i></div>
<div class=card><div id=st><div class=ring id=face>&#128522;</div><div><div id=word>All dry</div><div id=sub>Everything looks good</div></div></div>
<div class=grid><button onclick="go('/beep')">&#128264; Test beep</button><button onclick="go('/silence')">&#128263; Quiet 30 min</button>
<button class="grn full" onclick="if(confirm('Is the pan dry right now?'))go('/dry')">&#128077; Pan is dry now</button></div>
<div class=info id=info></div></div>
<div class=card><p class=lbl>Camera</p><div class=cam><img id=cv alt="" style="width:100%;height:100%;display:block;object-fit:cover"><span class=tag id=ct>Picture</span></div>
<div class=grid><button class=pri onclick=snap()>&#128247; Take picture</button><button id=lb onclick=live()>&#127909; Live: off</button>
<button onclick="go('/flip')">&#128260; Flip</button><button onclick="go('/swap')">&#127752; Fix colors</button>
<button class="full" onclick="go('/send')">&#128241; Send picture to phones</button></div><div class=info id=cm></div></div>
<div class=card><p class=lbl>From anywhere</p><div class=info style="margin-top:0">1. Install the free <b>ntfy</b> app and subscribe to:<br><a id=al href=#></a><br><br>
2. New picture from anywhere - open this link (save it to your home screen):<br><a id=sl href=#></a></div></div>
<div class=card><p class=lbl>Setup</p>
<select id=s><option value="">Pick Wi-Fi...</option></select><button style="width:100%" onclick=scan()>&#128246; Find Wi-Fi</button>
<div class=pw><input id=p type=password placeholder="Wi-Fi password"><button onclick="var e=$('p');e.type=e.type=='password'?'text':'password'">&#128065;</button></div>
<div class=info>Water sensitivity: <b id=sv></b>%</div><input id=se type=range min=5 max=60 onchange="go('/sens?v='+this.value)">
<div class=grid><button class=grn onclick=save()>&#128190; Save Wi-Fi</button><button class=warm onclick="if(confirm('Forget the Wi-Fi?'))go('/forget')">Forget Wi-Fi</button></div></div>
<div class=ft>(c) Moto Takeda 316-304-5398</div></div>
<script>
var K='',lv=false,busy=false;try{K=localStorage.k||''}catch(e){}
var uk=new URLSearchParams(location.search).get('k');if(uk){K=uk;try{localStorage.k=K}catch(e){}}
function $(i){return document.getElementById(i)}
function A(u){return u+(u.indexOf('?')<0?'?':'&')+'k='+encodeURIComponent(K)}
function bad(r){if(r.status==403){$('login').style.display='flex';throw 0}return r}
function go(u){fetch(A(u)).then(bad).then(r=>r.text()).then(t=>{if(t)$('info').textContent=t;st();if(u=='/flip'||u=='/swap')snap()}).catch(()=>{})}
function st(){fetch(A('/status')).then(bad).then(r=>r.json()).then(j=>{var e=$('st');e.className=j.wet?'wet':'';
$('face').innerHTML=j.wet?'&#128552;':'&#128522;';$('word').textContent=j.wet?'Water detected':'All dry';
$('sub').textContent=j.wet?'Please check the AC drain':'Everything looks good';
$('info').textContent='Wi-Fi: '+j.ip+'  |  sensor '+j.now+' / dry '+j.base+(j.quiet?'  |  quiet':'');
$('cm').textContent=j.cam;$('sv').textContent=j.sens;$('se').value=j.sens;
$('al').href=j.alert;$('al').textContent=j.alert;$('sl').href=j.snap;$('sl').textContent=j.snap}).catch(()=>{})}
function snap(){if(busy)return;busy=true;var im=new Image(),t0=Date.now(),to=setTimeout(function(){im.src='';fail()},15000);
function fail(){busy=false;st();if(lv)setTimeout(snap,2000)}
im.onload=function(){clearTimeout(to);$('cv').src=im.src;$('ct').textContent=new Date().toLocaleTimeString();busy=false;if(lv)setTimeout(snap,200)};
im.onerror=function(){clearTimeout(to);fail()};im.src=A('/pic.jpg?t='+t0)}
function live(){lv=!lv;$('lb').innerHTML=lv?'&#127909; Live: on':'&#127909; Live: off';if(lv)snap()}
function scan(){$('s').innerHTML='<option>Looking...</option>';fetch(A('/scan')).then(bad).then(r=>r.json()).then(l=>{$('s').innerHTML='<option value="">Pick Wi-Fi...</option>'+l.map(n=>'<option>'+n.replace(/</g,'')+'</option>').join('')}).catch(()=>{})}
function save(){if(!$('s').value){alert('Pick a Wi-Fi first');return}if(!K){K=prompt('Setup code')||'';try{localStorage.k=K}catch(e){}}
fetch(A('/save?s='+encodeURIComponent($('s').value)+'&p='+encodeURIComponent($('p').value))).then(bad).then(r=>r.text()).then(t=>alert(t)).catch(()=>{})}
(function(){var c=$('snow'),x=c.getContext('2d'),f=[];function rs(){c.width=innerWidth;c.height=innerHeight}rs();addEventListener('resize',rs);
for(var i=0;i<70;i++)f.push({x:Math.random()*innerWidth,y:Math.random()*innerHeight,r:1+Math.random()*2.6,s:.4+Math.random()*1.1,d:Math.random()*6});
(function a(){x.clearRect(0,0,c.width,c.height);x.fillStyle='rgba(255,255,255,.85)';f.forEach(function(p){p.y+=p.s;p.d+=.02;p.x+=Math.sin(p.d)*.5;if(p.y>c.height){p.y=-5;p.x=Math.random()*c.width}x.beginPath();x.arc(p.x,p.y,p.r,0,7);x.fill()});requestAnimationFrame(a)})()})();
st();snap();setInterval(st,2500);
</script></body></html>)HTML";

void handleStatus() {
  if (!ok()) return;
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.SSID() + " " + WiFi.localIP().toString() + " (signal " + String(WiFi.RSSI()) + ")" : String("not joined yet");
  String j = "{\"wet\":" + String(wet ? "true" : "false") + ",\"now\":" + touchNow + ",\"base\":" + touchBase +
             ",\"sens\":" + sens + ",\"ip\":\"" + ip + "\",\"quiet\":" + String(millis() < silenceUntil ? "true" : "false") +
             ",\"cam\":\"" + camMsg + "\",\"alert\":\"https://ntfy.sh/" + topic + "\",\"snap\":\"" + snapLink() + "\"}";
  server.send(200, "application/json", j);
}

void handleFrame() {
  if (!ok()) return;
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
  if (!ok()) return;
  uint8_t *jpg = nullptr;
  size_t len = 0;
  if (!takeJpeg(&jpg, &len)) { server.send(503, "text/plain", camMsg); return; }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "image/jpeg", (const char *)jpg, len);
  free(jpg);
}

void handleScan() {
  if (!ok()) return;
  if (WiFi.status() != WL_CONNECTED) WiFi.disconnect();   // stop retrying so the scan can run
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
  if (!okSetup()) return;
  if (server.arg("s") == ssid && server.arg("p") == pass && WiFi.status() == WL_CONNECTED) {
    server.send(200, "text/plain", "Already saved and working.");
    return;
  }
  if (server.arg("s").length()) {
    prefs.putString("s", server.arg("s"));
    prefs.putString("p", server.arg("p"));
    server.send(200, "text/plain", "Saved! Restarting to join " + server.arg("s") + ". Your phones will get a hello message.");
    delay(800);
    ESP.restart();
  }
  server.send(200, "text/plain", "Pick a Wi-Fi first.");
}

// "New picture" from anywhere: watch the snap topic on ntfy
void snapLoop() {
  if (WiFi.status() != WL_CONNECTED || wet || millis() - lastPoll < 30000) return;
  lastPoll = millis();
  WiFiClientSecure cli;
  cli.setInsecure();
  HTTPClient h;
  if (!h.begin(cli, "https://ntfy.sh/" + snapTopic() + "/json?poll=1&since=" + snapSince)) return;
  h.setTimeout(4000);
  int rc = h.GET();
  String body = rc == 200 ? h.getString() : String("");
  h.end();
  bool want = false;
  int pos = 0;
  while ((pos = body.indexOf("\"event\":\"message\"", pos)) >= 0) {
    want = true;
    pos += 10;
  }
  int i = body.lastIndexOf("\"id\":\"");
  if (i >= 0) {
    int e = body.indexOf('"', i + 6);
    if (e > i) snapSince = body.substring(i + 6, e);
  } else if (snapSince == "1m") {
    snapSince = "15s";
  }
  if (want) sendNow = true;
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

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);                       // never doze - keeps the page and alerts fast
  WiFi.setTxPower(WIFI_POWER_19_5dBm);        // full radio power for a far router
  WiFi.setAutoReconnect(true);
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
  server.on("/beep", [] { if (!ok()) return; beep(300); server.send(200, "text/plain", "Beep!"); });
  server.on("/dry", [] { if (!ok()) return; setDry(); server.send(200, "text/plain", "Thank you! This is dry now."); });
  server.on("/silence", [] { if (!ok()) return; silenceUntil = millis() + 30UL * 60UL * 1000UL; server.send(200, "text/plain", "Quiet for 30 minutes."); });
  server.on("/sens", [] { if (!ok()) return; sens = constrain(server.arg("v").toInt(), 5, 60); prefs.putInt("sens", sens); server.send(200, "text/plain", ""); });
  server.on("/flip", [] { if (!ok()) return; flipCam = !flipCam; prefs.putBool("fl", flipCam); applyFlip(); server.send(200, "text/plain", "Flipped."); });
  server.on("/swap", [] { if (!ok()) return; swapBytes = !swapBytes; prefs.putBool("sw", swapBytes); server.send(200, "text/plain", "Colors changed."); });
  server.on("/send", [] { if (!ok()) return;
    if (WiFi.status() != WL_CONNECTED) { server.send(200, "text/plain", "Join a Wi-Fi with internet first (Setup)."); return; }
    sendNow = true;
    server.send(200, "text/plain", "Sending the picture to your phones...");
  });
  server.on("/forget", [] {
    if (!okSetup()) return;
    prefs.remove("s");
    prefs.remove("p");
    server.send(200, "text/plain", "Wi-Fi forgotten. Restarting - join MotoDrain to set it up again.");
    delay(800);
    ESP.restart();
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
  // MotoDrain hotspot: off while home Wi-Fi works, back on if Wi-Fi is lost
  if (WiFi.status() == WL_CONNECTED) {
    lostSince = 0;
    if (apOn && millis() > 120000 && WiFi.softAPgetStationNum() == 0) { WiFi.enableAP(false); apOn = false; }
    static String lastIp = "";
    String ipNow = WiFi.localIP().toString();
    if (joinedMsg && lastIp.length() && ipNow != lastIp)
      pushPhone("Moto Drain Watch has a new address", "Tap Open page: " + myLink(), "smile,droplet", "default", false);
    lastIp = ipNow;
  } else {
    if (!lostSince) lostSince = millis();
    if (!apOn && millis() - lostSince > 60000) { WiFi.enableAP(true); WiFi.softAP("MotoDrain", code.c_str()); apOn = true; }
    if (ssid.length() && millis() - lostSince > 300000 && WiFi.softAPgetStationNum() == 0) ESP.restart();   // stuck 5 min: fresh start
    static uint32_t retry = 0;
    if (ssid.length() && WiFi.softAPgetStationNum() == 0 && millis() - retry > 30000) {
      retry = millis();
      WiFi.begin(ssid.c_str(), pass.c_str());
    }
  }
  snapLoop();
  if (ESP.getFreeHeap() < 30000) ESP.restart();
}
