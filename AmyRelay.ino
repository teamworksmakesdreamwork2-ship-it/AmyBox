// AMYRELAY_VERSION 6.5   (the build names the files with this number)
// 6.0: WEB has sound: while a web page is open (YouTube and the like) the Jetson sends the page's sound and the box plays it.
// 5.9: Christian music first: Christmas Gospel, Christmas Classics, Christian Radio, Christmas Hits (the SONG button changes it).
// 5.8: the CALL button opens a CALL page: TALK (hold), TYPE (written messages box to box), CAM (the picture from a phone's camera),
//      EXIT. Phones join through the Jetson: its phone page talks, types and shows its camera to the boxes (https://<jetson>:8092).
// 5.7: CALL button (page 3): hold it and talk, let go, and your voice plays on the other Mary boxes. Box to box, two ways at once:
//      straight by radio (ESP-NOW, no router and no internet needed) and over the Wi-Fi they share. No Jetson needed.
// 5.6: real Christmas music from internet radio (MUSIC / SONG), plus MP3 songs from the SD card. Web pages: drag a finger to
//      scroll, the keyboard opens by itself on a typing box, only the changed part of the page is sent, a finger always gets through.
//      Mary knows the real Wichita weather and the time, tries twice before she gives up. Her key is kept on the Jetson and the SD
//      card, so a new flash does not lose it. The DRAIN button works from Doctor Magic too (the Jetson passes the drain board on).
// Known Wi-Fi (Doctor Magic, Mary's) joins with no password typing. GARAGE on pin 14.
// 5.5: the setup page saves the Google key even when no Wi-Fi is picked (keeps the Wi-Fi the box is on),
//      sharp web pages (the Jetson sends plain pixels, no blurry picture file), UP / DOWN scroll, the Wi-Fi page finds
//      Wi-Fi even while the box is trying a wrong password, Mary's page says plainly when her key is missing.
// 5.4 MARY'S MAGIC BOX: page 3 buttons WEB (real web pages through the Jetson), WI-FI (pick Wi-Fi on the screen),
//     TYPE (a keyboard on the screen to ask Mary), PHOTOS (pictures from the SD card), MARY. Real MP3 songs from the
//     SD card (MUSIC / SONG buttons). Put a picture called mary.jpg on the SD card and Mary has that face.
//     Mary shows her answer and switches the light right away, then speaks. Big Wi-Fi bars. Knows Doctor Magic too.
// 5.3 MARY'S MAGIC BOX: Wi-Fi bars at the top, plain words when Wi-Fi fails (TOO WEAK, WRONG PASSWORD, NOT FOUND),
//     Mary's Wi-Fi built in, the secret code is not needed, joins the strongest Mary Wi-Fi it can hear.
// 5.2 MARY TALKS (black 2.8 inch box): tap Mary's face at the bottom, HOLD the TALK button and speak, let go.
//     Mary answers on the screen and out loud in a natural voice (free Google Gemini AI - paste the key on the
//     setup page, part 7). Say "turn the light on" / "light off" / "open the garage" and she does it.
//     JOKE and BLESS buttons. The switches keep working while she thinks. Needs Wi-Fi with internet.
// Mary Box: on this box the helper is called Mary (was Amy) on the screen and pages.
// 5.1 (for Mr. Terry's garage): the DRAIN button shows the AC drain pan from the Moto Drain Watch board
//     (green DRY / red WATER). Tap it: the drain camera picture fills the screen (tap again to go back).
//     Water in the pan: the screen wakes, turns red, and the box sounds an alarm every 30 seconds.
//     The Amy AI page only opens when a Jetson is set on the setup page.
/*  AmyRelay 4  -  a Wi-Fi switch box for the Amy AI phone app.      (c) Moto Takeda 316-304-5398
    Board: ESP32-S3 (a plain ESP32 works too).  Needs only the ESP32 board package - no extra libraries.

    WHAT IT DOES
      - Up to 4 relays.  You say "garage light on" to Amy  ->  this box clicks relay 1.
      - It talks with Amy over the internet through ntfy.sh (a free message service), so it works from anywhere.
        The SECRET CODE is the password. Whoever has the code can flip the relays - keep it to yourself.
      - First start, or when it cannot get on your Wi-Fi: it makes its own Wi-Fi named  Amy-Box-Setup .
        Join it with the phone and the setup page opens (or open  192.168.4.1 ).
      - Hold the BOOT button 6 seconds = forget everything and start the setup again.
      - After the power comes back, every relay starts OFF.
      - NEW in 2 - BUZZER on pin 4:  one beep = a switch went ON, two beeps = OFF, one beep when the box gets power.
        When no relay uses number 4, "switch 4" is the buzzer alarm: name it "buzzer" in Amy and say "buzzer on" / "buzzer off".
      - NEW in 2 - SCREEN: on the red MaTouch board (pick it on the setup page) the 2.8 inch screen shows what is ON and OFF.
      - NEW in 3 - TOUCH SCREEN BOARD: the 2.8 inch ESP32-S3 touch board (the back says "2.8 LCD Display ESP32-S3 240x320 Capacitive Touch")
        is found by itself. Its screen shows the switches, and a tap on the screen flips them. The relay wire goes on its IO2 pin.
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <SPI.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <driver/i2s.h>
#include <math.h>
#include "img_converters.h"
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CYD_TEST)
#include <driver/dac.h>
#include <driver/rtc_io.h>
#define CYD 1                               // built for the YELLOW 2.8 inch board (ESP32-2432S028): screen 14 12 13 15 2 21, touch 25 32 39 33, speaker 26, relays on 22 and 27 (plug CN1)
#else
#define CYD 0
#endif
#include <esp_now.h>
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#include <SD_MMC.h>
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutput.h>
#define HAS_SD 1
#else
#define HAS_SD 0
#endif
// one reader for Google's answer: handles the chunked pieces
struct GRead { WiFiClientSecure* c; bool chunked; long left; uint8_t b[1024]; int n, i; unsigned long t;
  int raw(){ if(i >= n){ n = 0; i = 0; unsigned long t0 = millis(); while(n <= 0 && millis() - t0 < 15000){ if(c->available()) n = c->read(b, sizeof(b)); else if(!c->connected()) return -1; else delay(2); } if(n <= 0) return -1; } return b[i++]; }
  String line(){ String s; int ch; while((ch = raw()) >= 0 && ch != '\n'){ if(ch != '\r') s += (char)ch; } return s; }
  int get(){ if(!chunked) return raw(); if(left == 0){ String l = line(); if(!l.length()) l = line(); left = strtol(l.c_str(), NULL, 16); if(left <= 0) return -1; } int ch = raw(); if(ch >= 0) left--; return ch; }
};

#define NREL 4
#define BOOT_PIN 0
static const char HOST[] = "ntfy.sh";
static const char AP_NAME[] = "Mary-Magic-Box";
// The two root certificates of Let's Encrypt (ISRG Root X1 and X2). With them the box knows it is really talking to ntfy.sh.
static const char ROOTS[] =
  "-----BEGIN CERTIFICATE-----\n"
  "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
  "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
  "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
  "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
  "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
  "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
  "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
  "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
  "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
  "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
  "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
  "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
  "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
  "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
  "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
  "qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
  "rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
  "HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
  "hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
  "ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
  "3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
  "NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
  "ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
  "TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
  "jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
  "oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
  "4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
  "mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
  "emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
  "-----END CERTIFICATE-----\n"
  "-----BEGIN CERTIFICATE-----\n"
  "MIICGzCCAaGgAwIBAgIQQdKd0XLq7qeAwSxs6S+HUjAKBggqhkjOPQQDAzBPMQsw\n"
  "CQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJuZXQgU2VjdXJpdHkgUmVzZWFyY2gg\n"
  "R3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBYMjAeFw0yMDA5MDQwMDAwMDBaFw00\n"
  "MDA5MTcxNjAwMDBaME8xCzAJBgNVBAYTAlVTMSkwJwYDVQQKEyBJbnRlcm5ldCBT\n"
  "ZWN1cml0eSBSZXNlYXJjaCBHcm91cDEVMBMGA1UEAxMMSVNSRyBSb290IFgyMHYw\n"
  "EAYHKoZIzj0CAQYFK4EEACIDYgAEzZvVn4CDCuwJSvMWSj5cz3es3mcFDR0HttwW\n"
  "+1qLFNvicWDEukWVEYmO6gbf9yoWHKS5xcUy4APgHoIYOIvXRdgKam7mAHf7AlF9\n"
  "ItgKbppbd9/w+kHsOdx1ymgHDB/qo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0T\n"
  "AQH/BAUwAwEB/zAdBgNVHQ4EFgQUfEKWrt5LSDv6kviejM9ti6lyN5UwCgYIKoZI\n"
  "zj0EAwMDaAAwZQIwe3lORlCEwkSHRhtFcP9Ymd70/aTSVaYgLXTWNLxBo1BfASdW\n"
  "tL4ndQavEi51mI38AjEAi/V3bNTIZargCyzuFJ0nN6T5U6VR5CmD1/iQMVtCnwr1\n"
  "/q4AaOeMSQ+2b1tbFfLn\n"
  "-----END CERTIFICATE-----\n";

Preferences prefs;
WebServer web(80);
DNSServer dns;
WiFiClientSecure sub;                       // the line that stays open: Amy's commands come in here

String ssid, pass, code, wifiList, line, reportTag;
int pins[NREL] = {5, -1, -1, -1};
void aiTask(void* arg);
String gKey = "", gTalk = "gemini-flash-lite-latest", gVoice = "gemini-2.5-flash-preview-tts";      // Mary's AI (Google Gemini, free key)
volatile int aiState = 0; volatile bool aiRec = false; volatile bool vPlay = false; volatile int vN = 0, vPos = 0, recN = 0;      // 0 idle 1 listening 2 thinking 3 answer ready
String aiQ = "", aiText = "", aiCmd = ""; int16_t *recL = NULL, *recR = NULL, *vBuf = NULL; int micPin = 0; bool micOk = false;
// ---- 5.4: keyboard, Wi-Fi page, web pages, SD card songs and photos, Mary's own picture
bool kbOn = false, wfOn = false, webOn = false, phOn = false, kbShift = false, kbSym = false, webErr = false; volatile bool txtDone = false;
int kbFor = 0;                                   // what the keyboard is for: 1 web address, 2 Wi-Fi password, 3 ask Mary, 4 type on the web page
String kbText = "", wfPick = "", webAct = "", joinSsid = "", joinPass = "";
String wfName[5]; int wfRs[5]; int wfN = 0;
unsigned long webAt = 0, phAt = 0; IPAddress webIp;
uint8_t *webPx = NULL, *webJpg = NULL;
#define WEBJ 150000
#define MU_N 65536
#define NF_N 262144
String sdSong[60], sdPic[60]; int sdSongN = 0, sdPicN = 0, phCur = 0; volatile int sdCur = 0, sdWant = -1; bool sdOk = false;
int16_t* muBuf = NULL; volatile uint32_t muH = 0, muT = 0;
uint16_t *maryBig = NULL, *maryFace = NULL;
volatile bool callMode = false, callSent = false, callIn = false; bool nowOn = false; volatile uint16_t callId = 0, callTot = 0, callGot = 0; volatile unsigned long callAt = 0; uint16_t callMine = 0;
uint8_t* callBuf = NULL; uint8_t callMap[300]; WiFiUDP callUdpIn, callUdpOut; bool callUdpOn = false;
#define CALL_PAY 240
bool callOn = false, pcOn = false; String callMsg = ""; volatile bool callNew = false; char callTxt[208]; uint16_t callSeen[6]; int callSeenK = 0; unsigned long pcAt = 0; WiFiUDP callUdpTxt;
volatile bool muPlay = false;
bool webFull = true, webKb = false, webSkip = false; volatile bool muReal = false; int jpgMode = -1;
String wxText = ""; unsigned long wxAt = 0;
#define NRADIO 4
static const uint8_t RED_JPG[637] = {255,216,255,224,0,16,74,70,73,70,0,1,1,0,0,1,0,1,0,0,255,219,0,67,0,3,2,2,3,2,2,3,3,3,3,4,3,3,4,5,8,5,5,4,4,5,10,7,7,6,8,12,10,12,12,11,10,11,11,13,14,18,16,13,14,17,14,11,11,16,22,16,17,19,20,21,21,21,12,15,23,24,22,20,24,18,20,21,20,255,219,0,67,1,3,4,4,5,4,5,9,5,5,9,20,13,11,13,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,255,192,0,17,8,0,16,0,16,3,1,17,0,2,17,1,3,17,1,255,196,0,31,0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,16,0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,125,1,2,3,0,4,17,5,18,33,49,65,6,19,81,97,7,34,113,20,50,129,145,161,8,35,66,177,193,21,82,209,240,36,51,98,114,130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,225,226,227,228,229,230,231,232,233,234,241,242,243,244,245,246,247,248,249,250,255,196,0,31,1,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,255,196,0,181,17,0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,119,0,1,2,3,17,4,5,33,49,6,18,65,81,7,97,113,19,34,50,129,8,20,66,145,161,177,193,9,35,51,82,240,21,98,114,209,10,22,36,52,225,37,241,23,24,25,26,38,39,40,41,42,53,54,55,56,57,58,67,68,69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,106,115,116,117,118,119,120,121,122,130,131,132,133,134,135,136,137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,200,201,202,210,211,212,213,214,215,216,217,218,226,227,228,229,230,231,232,233,234,242,243,244,245,246,247,248,249,250,255,218,0,12,3,1,0,2,17,3,17,0,63,0,249,210,191,12,63,213,48,160,2,128,10,0,255,217};
String drainIp = ""; int drainWet = -1; unsigned long drainSeen = 0, drainAlarmAt = 0, camAt = 0, camOpened = 0; bool camOn = false;
int bzPin = 4, board = 0;                   // buzzer pin (-1 = no buzzer).   board: 0 = plain ESP32-S3, no screen    1 = red MaTouch with the 2.8 inch screen
bool bzFlip = false, bzClick = true, bzAlarm = false, saidReady = false, lcdOn = false;
int bzLeft = 0; unsigned int bzMs = 90, bzGap = 110; unsigned long bzNext = 0, bzIdleAt = 0, lcdAt = 0;
int lcdRot = 0, tpFlip = 0; bool lcdInv = true, tpOk = false, tpDown = false; unsigned long tpAt = 0, tpSeen = 0;      // screen turn, touch turn, colors, touch chip found
bool activeLow = true, checkCert = true;
bool on[NREL] = {false, false, false, false};
unsigned long pulseEnd[NREL] = {0, 0, 0, 0};
bool apUp = false, subOk = false, needReport = false;
int httpCode = 0;
unsigned long lastData = 0, lastTry = 0, retryMs = 3000, wifiLostAt = 0, apSince = 0, staTryAt = 0, bootHeld = 0, lastHello = 0;

void setRelay(int i, bool v);
void handleLine(const String& l);
void doCommand(String m);
void beep(int n, unsigned int ms, unsigned int gap);
void setBuzz(bool v);
void lcdDraw(bool force);
void fxNote(int hz, int ms);
void sayClip(int k);
void showMsg(const String& m);
void announce(int i, bool pulse);
int spkPin = 0, spkUsed = 0;                // speaker data wire: 0 = find it (pin 6 or 8)
bool spk = false;                           // the touch board's speaker chip answered: sounds, voice and songs come out of the box
int volSet = 2;                             // 1 quiet, 2 normal, 3 loud
String nm[NREL];                            // the names on the screen buttons
volatile bool musicOn = false;

// ------------------------------------------------------------ small helpers (plain text work, no hardware)
String jsonField(const String& s, const char* key){            // the value of "key":"..." in one line of JSON
  String k = String("\"") + key + "\":\"";
  int a = s.indexOf(k); if(a < 0) return "";
  String out = "";
  for(unsigned int i = a + k.length(); i < s.length(); i++){
    char c = s.charAt(i);
    if(c == '\\' && i + 1 < s.length()){ char d = s.charAt(++i); out += (d == 'n' ? ' ' : d); continue; }
    if(c == '"') break;
    out += c;
  }
  return out;
}
bool goodCode(const String& c){                                 // letters, numbers, - and _ only: the code becomes part of a web address
  if(c.length() < 12 || c.length() > 50) return false;
  for(unsigned int i = 0; i < c.length(); i++){ char ch = c.charAt(i); if(!(isalnum((unsigned char)ch) || ch == '-' || ch == '_')) return false; }
  return true;
}
bool buzzIs4(){ return pins[NREL - 1] < 0 && (bzPin >= 0 || spk); }      // no relay on number 4: "switch 4" is the buzzer alarm
String stateText(){                                             // "s 1:on 2:off 3:- 4:-"   ( - = no relay on that number )
  String s = "s";
  for(int i = 0; i < NREL; i++){ s += " "; s += String(i + 1); s += ":"; s += (i == NREL - 1 && buzzIs4()) ? (bzAlarm ? "on" : "off") : (on[i] ? "on" : "off"); }
  return s;
}
String esc(const String& s){                                    // safe to put inside a web page
  String o = "";
  for(unsigned int i = 0; i < s.length(); i++){ char c = s.charAt(i);
    if(c == '&') o += "&amp;"; else if(c == '<') o += "&lt;"; else if(c == '>') o += "&gt;"; else if(c == '"') o += "&quot;"; else o += c; }
  return o;
}
bool goodPin(int p){ return p == -1 || (p >= 1 && p <= 48 && p != 3 && p != 19 && p != 20 && !(p >= 35 && p <= 37) && p != 45 && p != 46); }
bool goodPinFor(int p, int b){ if(b == 4) return p == -1 || p == 4 || p == 5 || p == 13 || p == 14 || (p >= 16 && p <= 19) || p == 23 || (p >= 25 && p <= 27) || p == 32 || p == 33; if(b == 3) return p == -1 || p == 22 || p == 27; if(b == 1) return p == -1 || (p >= 4 && p <= 7); if(b == 2) return p == -1 || p == 2 || p == 14 || p == 21 || p == 44;      /* 6.5: 44 is the RXD wire of the UART plug */ return goodPin(p); }      // the red MaTouch has only 4 spare pins: 4 5 6 7.  The 2.8 inch touch board: 2, 14, 21 (its small plug)

// ------------------------------------------------------------ the relays
void setRelay(int i, bool v){
  if(i < 0 || i >= NREL) return;
  on[i] = v; pulseEnd[i] = 0;
  if(pins[i] < 0) return;                                         // no relay on this number yet: the button still works on the screen and in the app
  if(activeLow){ pinMode(pins[i], OUTPUT_OPEN_DRAIN); digitalWrite(pins[i], v ? LOW : HIGH); }      // LOW = relay on.  "off" lets go of the wire, so 5 volt relay boards switch off all the way
  else { pinMode(pins[i], OUTPUT); digitalWrite(pins[i], v ? HIGH : LOW); }
}
// ------------------------------------------------------------ the buzzer (a small beeper on one pin)
// tone() works for both kinds: the kind that beeps by itself when it gets power, and the kind that needs a tone (a tiny speaker).
#define BZ_HZ 2700
void bzIdle(){ if(bzPin < 0) return; noTone(bzPin); pinMode(bzPin, OUTPUT); digitalWrite(bzPin, bzFlip ? HIGH : LOW); }
void beep(int n, unsigned int ms, unsigned int gap){
  if(bzPin < 0 && spk){ for(int i = 0; i < n && i < 9; i++){ fxNote(1000, (int)ms); fxNote(0, (int)gap); } return; }      // no beeper: the speaker beeps
  if(bzPin < 0 || n < 1) return;
  bzLeft = n; bzMs = ms; bzGap = gap; bzNext = millis();
}
void setBuzz(bool v){                                           // the alarm: it beeps again and again until it is switched off
  if(bzPin < 0 && !spk) return;
  bzAlarm = v;
  if(bzPin < 0) return;                                           // speaker alarm: bzLoop keeps it going
  if(v) beep(1, 350, 350); else { bzLeft = 0; bzIdleAt = 0; bzIdle(); }
}
bool fxIdle();
void bzLoop(){
  if(bzPin < 0){ if(spk && bzAlarm && fxIdle()){ fxNote(960, 280); fxNote(720, 280); } return; }      // two-tone alarm from the speaker
  unsigned long now = millis();
  if(bzIdleAt && (long)(now - bzIdleAt) >= 0){ bzIdleAt = 0; bzIdle(); }
  if(bzLeft <= 0 && bzAlarm){ bzLeft = 1; bzMs = 350; bzGap = 350; }
  if(bzLeft > 0 && (long)(now - bzNext) >= 0){
    tone(bzPin, BZ_HZ, bzMs);
    bzLeft--; bzNext = now + bzMs + bzGap;
    bzIdleAt = now + bzMs + 20; if(!bzIdleAt) bzIdleAt = 1;
  }
}
// Commands:  "1 on"  "2 off"  "1 toggle"  "1 pulse"  "all off"  "status"  "beep 3"     (an optional " #tag" at the end is sent back with the answer)
void doCommand(String m){
  m.trim(); m.toLowerCase();
  String tag = "";
  int h = m.lastIndexOf('#');
  if(h >= 0){ tag = m.substring(h); tag.trim(); m = m.substring(0, h); m.trim(); }
  if(tag.length() > 12) tag = tag.substring(0, 12);
  int sp = m.indexOf(' ');
  String a = sp < 0 ? m : m.substring(0, sp), b = sp < 0 ? String("") : m.substring(sp + 1);
  b.trim();
  if(a == "all"){ if(b == "on" || b == "off"){ for(int i = 0; i < NREL; i++) setRelay(i, b == "on"); if(b == "off" && bzAlarm) setBuzz(false); if(bzClick) beep(b == "on" ? 1 : 2, 90, 110); } }
  else if(a == "beep"){ int k = b.toInt(); beep(k >= 1 && k <= 9 ? k : 1, 150, 120); }
  else if(a != "status" && a != "hello"){
    int n = a.toInt();
    if(n == NREL && buzzIs4()){                                  // "switch 4" = the buzzer alarm
      if(b == "on") setBuzz(true); else if(b == "off") setBuzz(false); else if(b == "toggle") setBuzz(!bzAlarm); else if(b == "pulse") beep(3, 150, 120);
    }
    else if(n >= 1 && n <= NREL){
      if(b == "on") setRelay(n - 1, true);
      else if(b == "off") setRelay(n - 1, false);
      else if(b == "toggle") setRelay(n - 1, !on[n - 1]);
      else if(b == "pulse"){ setRelay(n - 1, true); pulseEnd[n - 1] = millis() + 700; if(pulseEnd[n - 1] == 0) pulseEnd[n - 1] = 1; }      // a short press, like a door button
      if(spk){ if(b == "on" || b == "off" || b == "toggle" || b == "pulse") announce(n - 1, b == "pulse"); }
      else if(bzClick && (b == "on" || b == "off" || b == "toggle")) beep(on[n - 1] ? 1 : 2, 90, 110);        // one beep = ON, two beeps = OFF
    }
  }
  reportTag = tag; needReport = true;                           // always answer, so Amy knows the box heard her
}

// ------------------------------------------------------------ talking with Amy through ntfy.sh
void secure(WiFiClientSecure& c){ if(checkCert) c.setCACert(ROOTS); else c.setInsecure(); }
bool postState(){                                               // tell Amy what is on and what is off
  WiFiClientSecure c; secure(c);
  if(!c.connect(HOST, 443, 8000)) return false;
  String body = stateText();
  if(reportTag.length()){ body += " "; body += reportTag; }
  String req = String("POST /") + code + "-s HTTP/1.1\r\nHost: " + HOST + "\r\nUser-Agent: AmyRelay/4\r\nContent-Type: text/plain\r\nContent-Length: " + String(body.length()) + "\r\nConnection: close\r\n\r\n" + body;
  c.print(req);
  unsigned long t = millis();
  while(c.connected() && !c.available() && millis() - t < 6000) delay(10);
  String st = c.readStringUntil('\n');
  c.stop();
  return st.indexOf(" 200") > 0;
}
void handleLine(const String& l){
  if(httpCode == 0 && l.startsWith("HTTP/")){
    int sp = l.indexOf(' ');
    httpCode = sp > 0 ? l.substring(sp + 1, sp + 4).toInt() : -1;
    if(httpCode != 200){                                        // 429 = too many calls: wait longer before calling again
      Serial.print("  the message service said "); Serial.println(httpCode);
      sub.stop(); subOk = false; lastTry = millis(); retryMs = (httpCode == 429) ? 120000UL : 30000UL;
    } else { Serial.println("  connected. Waiting for Mary."); retryMs = 3000; if(!saidReady){ saidReady = true; beep(2, 60, 80); } }      // two quick beeps: ready for Amy
    return;
  }
  if(l.indexOf("\"event\":\"message\"") < 0) return;           // "open" and "keepalive" lines: nothing to do
  String m = jsonField(l, "message");
  if(m.length()){ Serial.print("Mary says: "); Serial.println(m); doCommand(m); }
}
void subLoop(){
  if(WiFi.status() != WL_CONNECTED || !goodCode(code)){ if(subOk){ sub.stop(); subOk = false; } return; }
  if(!subOk){
    if(millis() - lastTry < retryMs) return;
    lastTry = millis(); sub.stop(); secure(sub);
    Serial.println("Calling the message service...");
    if(!sub.connect(HOST, 443, 8000)){ retryMs = min(retryMs * 2, 60000UL); Serial.println("  no answer, I will try again"); return; }
    sub.print(String("GET /") + code + "-c/json HTTP/1.1\r\nHost: " + HOST + "\r\nUser-Agent: AmyRelay/4\r\nAccept: */*\r\n\r\n");
    subOk = true; lastData = millis(); line = ""; httpCode = 0;
    if(lastHello == 0 || millis() - lastHello > 600000UL){ lastHello = millis(); if(lastHello == 0) lastHello = 1; reportTag = "#hello"; needReport = true; }
    return;
  }
  int budget = 800;
  while(budget-- > 0 && sub.available()){
    char c = (char)sub.read(); lastData = millis();
    if(c == '\n'){ handleLine(line); line = ""; if(!subOk) return; }
    else if(c != '\r' && line.length() < 600) line += c;
  }
  if(!sub.connected() || millis() - lastData > 150000UL){      // the line dropped, or nothing for two and a half minutes: call again
    sub.stop(); subOk = false; lastTry = millis(); retryMs = 3000;
  }
}

// ------------------------------------------------------------ Wi-Fi: your network, or the box's own setup Wi-Fi
int tgtRssi = -127;                                              // how strong Mary's Wi-Fi was at the last look (-127 = not heard)
int tgtCh = 0; uint8_t tgtBss[6]; volatile int wReason = 0;      // the strongest one heard, and why the last try failed
int barsOf(int r){ return r >= -60 ? 4 : r >= -67 ? 3 : r >= -75 ? 2 : r >= -85 ? 1 : 0; }
int wifiBars(){ return WiFi.status() == WL_CONNECTED ? barsOf(WiFi.RSSI()) : barsOf(tgtRssi); }
String wifiWhy(){                                                // plain words for the bottom of the screen
  if(WiFi.status() == WL_CONNECTED) return String("");
  if(tgtRssi <= -127) return String("WI-FI NOT FOUND");
  if(barsOf(tgtRssi) <= 1) return String("WI-FI TOO WEAK-MOVE");
  if(wReason == 2 || wReason == 15 || wReason == 202 || wReason == 204) return String("WRONG WI-FI PASSWORD");
  return String("Looking for Wi-Fi...");
}
String normW(const String& s){ String o; for(unsigned int i = 0; i < s.length(); i++){ char c = s.charAt(i); if(isalnum((unsigned char)c)) o += (char)tolower((unsigned char)c); } return o; }
bool isKnown(const String& s){ String n = normW(s); return s == ssid || n == "doctormagic" || n == "mjcaviness2"; }
String knownPass(const String& s){ if(s == ssid) return pass; if(isKnown(s)) return String("Shocker1"); return String(""); }
void scanWifi(){
  wifiList = ""; tgtRssi = -127; tgtCh = 0; joinSsid = ssid; joinPass = pass;
  int n = WiFi.scanNetworks();
  int bS = -127, bA = -127, iS = -1, iA = -1;
  for(int i = 0; i < n; i++){ String s = WiFi.SSID(i); int r = WiFi.RSSI(i); if(!s.length()) continue;
    if(s == ssid && r > bS){ bS = r; iS = i; }
    if(isKnown(s) && r > bA){ bA = r; iA = i; }
  }
  int k = iA >= 0 ? iA : iS; if(iS >= 0 && bS >= bA - 8) k = iS;      // your saved Wi-Fi when it is good, or else the strongest Wi-Fi the box knows (Mary's, Doctor Magic)
  if(k >= 0){ joinSsid = WiFi.SSID(k); joinPass = knownPass(joinSsid); tgtRssi = WiFi.RSSI(k); tgtCh = WiFi.channel(k); memcpy(tgtBss, WiFi.BSSID(k), 6); }
  wfN = 0;                                                        // the 5 strongest names for the Wi-Fi page on the screen
  for(int i = 0; i < n; i++){ String s = WiFi.SSID(i); int r = WiFi.RSSI(i); if(!s.length()) continue;
    bool dup = false; for(int j = 0; j < wfN; j++) if(wfName[j] == s){ dup = true; if(r > wfRs[j]) wfRs[j] = r; }
    if(dup) continue;
    int at = wfN; while(at > 0 && wfRs[at - 1] < r) at--;
    if(at >= 5) continue;
    for(int j = (wfN < 5 ? wfN : 4); j > at; j--){ wfName[j] = wfName[j - 1]; wfRs[j] = wfRs[j - 1]; }
    wfName[at] = s; wfRs[at] = r; if(wfN < 5) wfN++;
  }
  for(int i = 0; i < n && i < 14; i++){ String s = WiFi.SSID(i); if(!s.length() || wifiList.indexOf("\"" + esc(s) + "\"") >= 0) continue;
    int b = barsOf(WiFi.RSSI(i));
    wifiList += "<option value=\"" + esc(s) + "\"" + (s == ssid ? " selected" : "") + ">" + esc(s) + (b >= 3 ? " (strong)" : b == 2 ? " (OK)" : " (weak)") + "</option>"; }
  WiFi.scanDelete();
}
void startAP(){
  if(apUp) return;
  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  scanWifi();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME);
  delay(300);
  dns.start(53, "*", WiFi.softAPIP());
  apUp = true; apSince = millis();
  Serial.print("Setup Wi-Fi is on:  "); Serial.print(AP_NAME); Serial.print("   page: http://"); Serial.println(WiFi.softAPIP().toString());
}
void stopAP(){
  if(!apUp) return;
  dns.stop(); WiFi.softAPdisconnect(true); apUp = false;
}
void startSTA(){
  stopAP();
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.setHostname("amy-box"); WiFi.setTxPower(WIFI_POWER_19_5dBm);
  scanWifi();                                                     // listen first: pick the strongest Mary Wi-Fi and remember how strong it is
  wReason = 0;
  if(tgtCh) WiFi.begin(joinSsid.c_str(), joinPass.c_str(), tgtCh, tgtBss); else WiFi.begin(ssid.c_str(), pass.c_str());
  staTryAt = millis(); wifiLostAt = millis();
  Serial.print("Joining Wi-Fi: "); Serial.println(tgtCh ? joinSsid : ssid);
}
void wifiLoop(){
  if(apUp){
    dns.processNextRequest();
    if(ssid.length() && millis() - apSince > 90000UL && WiFi.softAPgetStationNum() == 0) startSTA();      // nobody is using the setup page: try the saved Wi-Fi again
    return;
  }
  if(WiFi.status() == WL_CONNECTED){
    if(wifiLostAt){ wifiLostAt = 0; Serial.print("Wi-Fi is on. This box is at http://"); Serial.println(WiFi.localIP().toString()); }
    return;
  }
  if(!wifiLostAt) wifiLostAt = millis();
  if(millis() - wifiLostAt > 45000UL){ Serial.println("No Wi-Fi. Opening the setup Wi-Fi."); startAP(); }
}

// ------------------------------------------------------------ the small screen (only on the red MaTouch board: ST7789, 320 x 240, sideways)
int LCD_SCK = 48, LCD_MISO = 12, LCD_MOSI = 13, LCD_CS = 40, LCD_DC = 21, LCD_BL = 45;      // the red MaTouch.  The 2.8 inch touch board: 12, 13, 11, 10, 46, 45
#define LCD_W 320
#define LCD_H 240
#define FW 12
#define BOX_VER "6.5"
#define FH 24
#define FB 2
static const uint8_t FONT[] PROGMEM = {       // letters 32..126, made by a script from DejaVu Sans Mono Bold
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x60,0x0E,0xE0,0x0C,0xE0,0x0C,0xC0,0x7F,0xF0,0x7F,0xF0,0x19,0xC0,0x19,0x80,0x19,0x80,0xFF,0xE0,0xFF,0xE0,0x33,0x00,0x33,0x00,0x73,0x00,0x67,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x1F,0x80,0x3F,0xC0,0x3E,0xC0,0x36,0x00,0x36,0x00,0x3E,0x00,0x1F,0x80,0x07,0xC0,0x06,0xE0,0x06,0xE0,0x36,0xE0,0x3F,0xC0,0x1F,0x80,0x06,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x7C,0x00,0xCE,0x00,0xCE,0x00,0x7C,0x20,0x38,0xE0,0x01,0x80,0x06,0x00,0x18,0x00,0x71,0xE0,0x43,0xE0,0x03,0x30,0x03,0x30,0x03,0xE0,0x01,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x1F,0x80,0x38,0x80,0x38,0x00,0x3C,0x00,0x1C,0x00,0x3E,0x00,0x7E,0x30,0x67,0x30,0xE3,0xF0,0xE3,0xF0,0xF1,0xE0,0x71,0xE0,0x7F,0xE0,0x1F,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x80,0x03,0x00,0x07,0x00,0x07,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x07,0x00,0x07,0x00,0x03,0x00,0x03,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1C,0x00,0x0C,0x00,0x0E,0x00,0x0E,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x0E,0x00,0x0E,0x00,0x0C,0x00,0x1C,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x26,0x40,0x3F,0xC0,0x1F,0x80,0x1F,0x80,0x3F,0xC0,0x26,0x40,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x7F,0xE0,0x7F,0xE0,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0E,0x00,0x0E,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x1F,0x80,0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x00,0xC0,0x00,0xC0,0x01,0x80,0x01,0x80,0x03,0x00,0x03,0x00,0x06,0x00,0x06,0x00,0x0C,0x00,0x0C,0x00,0x18,0x00,0x18,0x00,0x38,0x00,0x30,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x1F,0x80,0x39,0xC0,0x39,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x76,0xE0,0x76,0xE0,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x39,0xC0,0x1F,0x80,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x3F,0x00,0x37,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x3F,0xE0,0x3F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x7F,0x80,0x71,0xC0,0x41,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x03,0x80,0x07,0x00,0x0F,0x00,0x1E,0x00,0x3C,0x00,0x38,0x00,0x7F,0xC0,0x7F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x3F,0xC0,0x31,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x0F,0x80,0x0F,0x80,0x01,0xC0,0x00,0xE0,0x00,0xE0,0x40,0xE0,0x61,0xC0,0x7F,0xC0,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xC0,0x07,0xC0,0x07,0xC0,0x0F,0xC0,0x1D,0xC0,0x19,0xC0,0x39,0xC0,0x71,0xC0,0x61,0xC0,0x7F,0xE0,0x7F,0xE0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xC0,0x3F,0xC0,0x30,0x00,0x30,0x00,0x30,0x00,0x3F,0x00,0x3F,0x80,0x23,0xC0,0x01,0xE0,0x00,0xE0,0x00,0xE0,0x01,0xE0,0x61,0xC0,0x7F,0x80,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x1F,0xC0,0x3C,0xC0,0x38,0x00,0x30,0x00,0x77,0x80,0x7F,0xC0,0x79,0xE0,0x78,0xE0,0x70,0xE0,0x70,0xE0,0x38,0xE0,0x39,0xE0,0x1F,0xC0,0x0F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xE0,0x7F,0xE0,0x01,0xC0,0x01,0xC0,0x03,0xC0,0x03,0x80,0x03,0x80,0x07,0x80,0x07,0x00,0x07,0x00,0x0E,0x00,0x0E,0x00,0x1E,0x00,0x1C,0x00,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x3F,0xC0,0x39,0xC0,0x30,0xC0,0x30,0xC0,0x39,0xC0,0x1F,0x80,0x1F,0x80,0x39,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x3F,0xC0,0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x3F,0x80,0x39,0xC0,0x71,0xC0,0x70,0xE0,0x70,0xE0,0x71,0xE0,0x79,0xE0,0x3F,0xE0,0x1E,0xE0,0x00,0xE0,0x01,0xC0,0x33,0xC0,0x3F,0x80,0x1F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x0E,0x00,0x0E,0x00,0x0C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x01,0xE0,0x0F,0xE0,0x3F,0x00,0x78,0x00,0x78,0x00,0x3F,0x00,0x0F,0xE0,0x01,0xE0,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xE0,0x7F,0xE0,0x00,0x00,0x00,0x00,0x7F,0xE0,0x7F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x78,0x00,0x7F,0x00,0x0F,0xC0,0x01,0xE0,0x01,0xE0,0x0F,0xC0,0x7F,0x00,0x78,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x3F,0xC0,0x31,0xC0,0x01,0xC0,0x01,0xC0,0x03,0x80,0x07,0x00,0x07,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x00,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x1F,0xC0,0x38,0xE0,0x70,0x60,0x67,0xE0,0xE7,0xE0,0xCE,0x60,0xCC,0x60,0xCC,0x60,0xCC,0x60,0xCE,0x60,0xE7,0xE0,0x67,0xE0,0x70,0x00,0x38,0x60,0x1F,0xE0,0x07,0xC0,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x0F,0x00,0x0F,0x80,0x1F,0x80,0x1F,0x80,0x19,0x80,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x3F,0xC0,0x7F,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0xE0,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x80,0x7F,0xC0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xC0,0x7F,0xC0,0x7F,0xC0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x7F,0xE0,0x7F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xC0,0x1F,0xE0,0x1C,0x60,0x38,0x00,0x38,0x00,0x38,0x00,0x78,0x00,0x78,0x00,0x78,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x1C,0x60,0x1F,0xE0,0x07,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x00,0x7F,0x80,0x71,0xC0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xE0,0x71,0xC0,0x7F,0x80,0x7F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xE0,0x3F,0xE0,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0xC0,0x3F,0xC0,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0xE0,0x3F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xE0,0x3F,0xE0,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0xC0,0x3F,0xC0,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xC0,0x1F,0xE0,0x3C,0x60,0x38,0x00,0x78,0x00,0x70,0x00,0x70,0x00,0x73,0xE0,0x73,0xE0,0x70,0xE0,0x78,0xE0,0x38,0xE0,0x3C,0xE0,0x1F,0xE0,0x0F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x7F,0xE0,0x7F,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xC0,0x3F,0xC0,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x3F,0xC0,0x3F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0xC0,0x1F,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x01,0xC0,0x41,0xC0,0x63,0xC0,0x7F,0x80,0x3F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x71,0xE0,0x71,0xC0,0x73,0x80,0x77,0x00,0x7F,0x00,0x7F,0x00,0x7F,0x00,0x7F,0x80,0x73,0x80,0x73,0xC0,0x71,0xC0,0x71,0xE0,0x70,0xE0,0x70,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0xE0,0x3F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x79,0xE0,0x79,0xE0,0x79,0xE0,0x79,0xE0,0x7F,0xE0,0x6F,0x60,0x6F,0x60,0x6F,0x60,0x66,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x78,0xE0,0x78,0xE0,0x78,0xE0,0x7C,0xE0,0x7C,0xE0,0x7C,0xE0,0x76,0xE0,0x76,0xE0,0x77,0xE0,0x73,0xE0,0x73,0xE0,0x73,0xE0,0x71,0xE0,0x71,0xE0,0x70,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x3F,0xC0,0x39,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x3F,0xC0,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x80,0x3F,0xC0,0x39,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x39,0xE0,0x3F,0xC0,0x3F,0x80,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x3F,0xC0,0x39,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x3F,0xC0,0x0F,0x80,0x01,0xC0,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0x80,0x7F,0xC0,0x71,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xC0,0x7F,0x80,0x7F,0x80,0x73,0x80,0x71,0xC0,0x71,0xC0,0x70,0xE0,0x70,0xE0,0x70,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x3F,0xC0,0x38,0xC0,0x70,0x40,0x70,0x00,0x78,0x00,0x3F,0x00,0x1F,0x80,0x07,0xC0,0x01,0xE0,0x00,0xE0,0x40,0xE0,0x71,0xE0,0x7F,0xC0,0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xE0,0x7F,0xE0,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x79,0xE0,0x3F,0xC0,0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x30,0xE0,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x19,0x80,0x1F,0x80,0x1F,0x80,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE6,0x70,0x6F,0x60,0x6F,0x60,0x6F,0x60,0x6F,0x60,0x7F,0xE0,0x79,0xE0,0x79,0xE0,0x79,0xE0,0x39,0xE0,0x39,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x39,0xC0,0x1F,0x80,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x1F,0x80,0x1F,0x80,0x39,0xC0,0x39,0xC0,0x70,0xE0,0xF0,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x70,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x39,0xC0,0x1F,0x80,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xE0,0x7F,0xE0,0x01,0xE0,0x01,0xE0,0x03,0xC0,0x07,0x80,0x07,0x80,0x0F,0x00,0x0E,0x00,0x1E,0x00,0x3C,0x00,0x38,0x00,0x78,0x00,0x7F,0xE0,0x7F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x0F,0x80,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0F,0x80,0x0F,0x80,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0x30,0x00,0x38,0x00,0x18,0x00,0x18,0x00,0x0C,0x00,0x0C,0x00,0x06,0x00,0x06,0x00,0x03,0x00,0x03,0x00,0x01,0x80,0x01,0x80,0x01,0xC0,0x00,0xC0,0x00,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x00,0x1F,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x1F,0x00,0x1F,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x1F,0x80,0x1F,0x80,0x39,0xC0,0x70,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xF0,0xFF,0xF0,
0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x1C,0x00,0x0C,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x3F,0xC0,0x21,0xE0,0x00,0xE0,0x1F,0xE0,0x7F,0xE0,0x78,0xE0,0x70,0xE0,0x71,0xE0,0x7F,0xE0,0x1E,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0x70,0x00,0x70,0x00,0x70,0x00,0x77,0x80,0x7F,0xC0,0x79,0xE0,0x78,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x78,0xE0,0x79,0xE0,0x7F,0xC0,0x77,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0xC0,0x1F,0xC0,0x3C,0x40,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3C,0x40,0x1F,0xC0,0x0F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x00,0xE0,0x00,0xE0,0x00,0xE0,0x1E,0xE0,0x3F,0xE0,0x79,0xE0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xE0,0x79,0xE0,0x3F,0xE0,0x1E,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x80,0x3F,0xC0,0x38,0xE0,0x70,0xE0,0x7F,0xE0,0x7F,0xE0,0x70,0x00,0x70,0x00,0x38,0x60,0x3F,0xE0,0x0F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xE0,0x07,0xE0,0x0E,0x00,0x0E,0x00,0x3F,0xE0,0x3F,0xE0,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0xE0,0x3F,0xE0,0x79,0xE0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xE0,0x79,0xE0,0x3F,0xE0,0x1E,0xE0,0x00,0xE0,0x21,0xC0,0x3F,0xC0,0x1F,0x80,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x3F,0x80,0x3F,0xC0,0x39,0xC0,0x39,0xC0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x3F,0x00,0x3F,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x7F,0xE0,0x7F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x3F,0x00,0x3F,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x07,0x00,0x7F,0x00,0x7E,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x39,0xE0,0x39,0xC0,0x3B,0x80,0x3F,0x00,0x3F,0x00,0x3F,0x00,0x3B,0x80,0x3B,0xC0,0x39,0xC0,0x38,0xE0,0x38,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00,0x7E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0F,0xE0,0x07,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7D,0xC0,0x7F,0xE0,0x67,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x66,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x80,0x3F,0xC0,0x39,0xC0,0x39,0xC0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x38,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x3F,0xC0,0x39,0xC0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x39,0xC0,0x3F,0xC0,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x77,0x80,0x7F,0xC0,0x79,0xE0,0x78,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x78,0xE0,0x79,0xE0,0x7F,0xC0,0x77,0x80,0x70,0x00,0x70,0x00,0x70,0x00,0x70,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1E,0xE0,0x3F,0xE0,0x79,0xE0,0x71,0xE0,0x70,0xE0,0x70,0xE0,0x70,0xE0,0x71,0xE0,0x79,0xE0,0x3F,0xE0,0x1E,0xE0,0x00,0xE0,0x00,0xE0,0x00,0xE0,0x00,0xE0,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1D,0xE0,0x1F,0xE0,0x1E,0x20,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x1C,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1F,0x80,0x3F,0xC0,0x38,0xC0,0x38,0x00,0x3F,0x00,0x1F,0xC0,0x07,0xC0,0x01,0xC0,0x21,0xC0,0x3F,0xC0,0x1F,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x7F,0xE0,0x7F,0xE0,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0E,0x00,0x0F,0xE0,0x07,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x71,0xC0,0x71,0xC0,0x71,0xC0,0x71,0xC0,0x71,0xC0,0x71,0xC0,0x71,0xC0,0x39,0xC0,0x39,0xC0,0x3F,0xC0,0x1F,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x30,0xE0,0x39,0xC0,0x39,0xC0,0x39,0xC0,0x1D,0x80,0x1F,0x80,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x30,0xE0,0x70,0xE0,0x70,0x66,0x60,0x6F,0x60,0x6F,0x60,0x6F,0x60,0x7F,0xE0,0x79,0xE0,0x39,0xC0,0x39,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x79,0xE0,0x39,0xC0,0x1F,0xC0,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x0F,0x00,0x1F,0x80,0x39,0xC0,0x39,0xE0,0x70,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0xE0,0x70,0xE0,0x78,0xE0,0x39,0xC0,0x39,0xC0,0x1D,0xC0,0x1F,0x80,0x1F,0x80,0x0F,0x00,0x0F,0x00,0x07,0x00,0x0E,0x00,0x0E,0x00,0x7C,0x00,0x78,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0xE0,0x3F,0xE0,0x01,0xC0,0x03,0xC0,0x07,0x80,0x0F,0x00,0x1E,0x00,0x1C,0x00,0x38,0x00,0x3F,0xE0,0x3F,0xE0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0xC0,0x07,0xC0,0x07,0x00,0x07,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x0E,0x00,0x3E,0x00,0x3E,0x00,0x0E,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x07,0x00,0x07,0x00,0x07,0xC0,0x03,0xC0,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x00,0x3E,0x00,0x0E,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x07,0x00,0x07,0xC0,0x03,0xC0,0x07,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x06,0x00,0x0E,0x00,0x3E,0x00,0x3C,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3C,0x20,0x7F,0xE0,0x43,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
SPIClass lcdSpi(HSPI);
String lcdShown = "";
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b){ return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }
void lcdCmd(uint8_t c){ digitalWrite(LCD_DC, LOW); lcdSpi.write(c); digitalWrite(LCD_DC, HIGH); }
void lcdOpen(){ lcdSpi.beginTransaction(SPISettings(20000000, MSBFIRST, board >= 2 ? SPI_MODE0 : SPI_MODE3)); digitalWrite(LCD_CS, LOW); }
void lcdClose(){ digitalWrite(LCD_CS, HIGH); lcdSpi.endTransaction(); }
void lcdWindow(int x, int y, int w, int h){
  uint8_t d[4];
  lcdCmd(0x2A); d[0] = (uint8_t)(x >> 8); d[1] = (uint8_t)(x & 255); d[2] = (uint8_t)((x + w - 1) >> 8); d[3] = (uint8_t)((x + w - 1) & 255); lcdSpi.writeBytes(d, 4);
  lcdCmd(0x2B); d[0] = (uint8_t)(y >> 8); d[1] = (uint8_t)(y & 255); d[2] = (uint8_t)((y + h - 1) >> 8); d[3] = (uint8_t)((y + h - 1) & 255); lcdSpi.writeBytes(d, 4);
  lcdCmd(0x2C);
}
void lcdFill(int x, int y, int w, int h, uint16_t c){
  if(!lcdOn || w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > LCD_W || y + h > LCD_H) return;
  uint8_t buf[128];
  for(int i = 0; i < 128; i += 2){ buf[i] = (uint8_t)(c >> 8); buf[i + 1] = (uint8_t)(c & 255); }
  lcdOpen(); lcdWindow(x, y, w, h);
  long left = (long)w * h * 2;
  while(left > 0){ int n = left > 128 ? 128 : (int)left; lcdSpi.writeBytes(buf, n); left -= n; }
  lcdClose();
}
int lcdText(int x, int y, const char* s, uint16_t fg, uint16_t bg, int sc){      // writes the words; gives back where the next letter would start
  if(!lcdOn || y < 0) return x;
  if(sc < 1) sc = 1;
  if(sc > 3) sc = 3;
  if(y + FH * sc > LCD_H) return x;
  uint8_t row[FW * 3 * 2];
  for(; *s; s++){
    int ch = (uint8_t)*s; if(ch < 32 || ch > 126) ch = '?';
    if(x < 0 || x + FW * sc > LCD_W) break;
    const uint8_t* g = FONT + (ch - 32) * FH * FB;
    lcdOpen(); lcdWindow(x, y, FW * sc, FH * sc);
    for(int r = 0; r < FH; r++){
      int k = 0;
      for(int col = 0; col < FW; col++){
        bool bit = (pgm_read_byte(g + r * FB + (col >> 3)) & (0x80 >> (col & 7))) != 0;
        uint16_t c = bit ? fg : bg;
        for(int e = 0; e < sc; e++){ row[k++] = (uint8_t)(c >> 8); row[k++] = (uint8_t)(c & 255); }
      }
      for(int e = 0; e < sc; e++) lcdSpi.writeBytes(row, k);
    }
    lcdClose();
    x += FW * sc;
  }
  return x;
}
void lcdInit(){
  static const uint8_t TURN1[4] = {0x60, 0xA0, 0x20, 0xE0}, TURN2[4] = {0x28, 0xE8, 0x68, 0xA8};      // 4 ways to lay the picture sideways (red MaTouch / touch board)
  if(board == 2){ LCD_SCK = 12; LCD_MISO = 13; LCD_MOSI = 11; LCD_CS = 10; LCD_DC = 46; LCD_BL = 45; }
  if(board == 3){ LCD_SCK = 14; LCD_MISO = 12; LCD_MOSI = 13; LCD_CS = 15; LCD_DC = 2; LCD_BL = 21; }
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH); pinMode(LCD_DC, OUTPUT); digitalWrite(LCD_DC, HIGH);
  lcdSpi.begin(LCD_SCK, LCD_MISO, LCD_MOSI, -1);
  lcdOpen(); lcdCmd(0x01); lcdClose(); delay(150);               // start fresh
  lcdOpen(); lcdCmd(0x11); lcdClose(); delay(120);               // wake up
  lcdOpen();
  lcdCmd(0x3A); lcdSpi.write(0x55);                              // 65 thousand colors
  lcdCmd(0x36); lcdSpi.write(board >= 2 ? TURN2[lcdRot & 3] : TURN1[lcdRot & 3]);      // sideways: 320 wide, 240 tall
  lcdCmd(lcdInv ? 0x21 : 0x20);                                  // this kind of screen needs its colors turned around (the setup page can change it)
  lcdCmd(0x13); lcdCmd(0x29);                                    // picture on
  lcdClose(); delay(20);
  lcdOn = true; lcdShown = "";
  lcdFill(0, 0, LCD_W, LCD_H, 0);
  pinMode(LCD_BL, OUTPUT); digitalWrite(LCD_BL, HIGH);           // light on
}
static const uint16_t BG[] PROGMEM = {
15149,17230,19310,19278,19278,17230,17198,17198,17231,17231,15150,15085,17165,19246,19278,19213,19213,19213,19213,19214,19181,19181,21294,23407,
21326,19214,19213,17133,17133,17165,17197,17198,17230,17231,19311,19278,19246,19278,21359,19278,17230,17230,17230,17263,17263,19376,21424,19343,
19375,19408,19408,19408,19408,21488,21488,21488,21488,21521,21553,21521,23601,25714,25746,27794,27794,27826,27761,27793,31986,29873,29840,31921,
34034,34001,34001,36114,38162,38129,40210,42289,42289,44401,46481,48594,48562,48595,46514,40273,40242,40274,40307,38259,38291,38292,38292,38324,
38325,40373,40405,40406,38357,40438,42551,42551,42551,42551,44631,44631,44631,44567,42454,42453,42453,42485,44566,44565,44501,44468,44533,46646,
46614,46646,48759,48791,48759,48759,50840,52920,52952,52920,52952,52952,52920,52919,52919,52919,52919,50839,50871,50839,48759,48759,50840,50840,
48759,48694,46614,46646,46646,44533,44501,42453,42421,44567,44599,40277,38196,40277,38099,33938,38131,40244,38131,36051,33970,33905,33872,33904,
33872,33839,33839,33839,33839,33840,35920,33839,31727,33807,33775,33774,33807,31694,31694,33774,33774,33775,33775,33742,33775,33775,35920,35920,
31694,29646,31694,33840,35921,33840,31728,31760,31825,29745,31858,31858,29778,29811,31956,31956,29811,29843,31956,29843,29875,29875,31956,36181,
36182,36214,34166,32053,29940,27892,27892,29973,29973,29973,29941,27861,25748,25716,25716,25748,25716,25748,25780,23700,21619,23700,23700,23668,
23635,23538,21425,21425,21489,21425,19344,21424,21425,21489,21522,19441,19473,17459,13331,11282,11282,9234,11314,11315,9202,7153,7153,9298,
9363,9363,9330,9331,9331,9299,9331,9299,7218,7218,7218,5105,5073,5073,7153,7185,5040,2959,5008,4975,2895,4976,7088,7088,
7023,7056,7056,4943,7088,9136,9136,9071,6958,6926,6958,6926,4878,4878,4910,7023,7023,6991,4878,4877,4878,4910,6991,7023,
6991,6926,4878,4878,2797,2830,4878,2797,15149,15149,17262,17230,17198,17198,17230,15150,15150,17231,17231,15085,17198,21359,23471,21358,
19213,21261,19213,19181,19181,19181,23375,27601,23375,21294,21294,19213,17133,15085,15117,15150,15182,17263,19343,21359,21294,21326,21326,19245,
17165,17230,17230,17263,19343,21424,21424,19311,19375,21488,21488,19440,21520,19440,19440,19440,19375,19440,21553,23633,23634,25681,25746,27859,
27794,27793,27793,27793,29906,29873,29873,31953,31921,31888,33968,38129,38162,40209,42322,44402,44368,46481,48561,50609,48561,48561,46482,42354,
40273,40241,38193,40307,40372,40372,38324,40405,40405,40373,38293,40405,40406,42551,42551,42518,42486,42486,42519,42519,42519,42519,42486,40373,
40373,40372,42453,44533,42453,42420,46582,46646,46646,46614,46646,50840,50840,48759,52920,52920,52952,52952,52952,52952,52920,55000,55000,55000,
52952,52952,52919,50839,50807,48759,50840,50872,48792,46647,46614,46646,44533,42453,42421,42421,42453,46680,46680,40342,38197,40277,38164,36051,
38131,38163,36051,33970,31825,31792,35985,38098,35985,33872,33839,33840,33839,31727,31727,31727,31726,31727,33775,33807,33807,31694,31662,33775,
33775,33775,33775,33742,33775,33775,33775,31694,29614,31727,31759,31759,33840,31760,31760,31793,31793,31793,31858,31858,29778,31891,31956,31891,
29843,31924,31923,31956,31956,31956,34069,38262,38295,38295,36214,32020,29908,29940,29940,29973,29973,29941,29941,27860,27796,25748,25748,27796,
27828,27861,25812,23700,23699,25845,27925,25748,23635,23538,21425,21392,21457,21457,21392,25618,27731,25651,23602,21554,21619,19604,15476,13363,
11314,11347,13460,13427,9202,7121,7153,9298,9363,11444,9363,9331,11443,9331,9299,9299,7251,7218,7186,5106,5105,7186,7218,7153,
2960,5008,5008,5008,5008,5008,7056,7056,7024,7056,9136,7056,4975,4974,4942,7023,6958,6958,9039,9071,4910,4878,4943,9104,
7056,6991,4910,6990,6990,6991,7023,6990,6958,6958,4910,4878,2830,2829,2797,2797,15181,13069,15150,17230,15118,15118,15183,15150,
13070,15086,15118,15118,19310,23504,25617,23406,19212,21293,19181,17068,19149,19181,21262,21326,21294,19213,19214,17165,15085,15117,15150,15182,
17263,17263,19311,21359,23374,25454,21293,19213,19213,17198,17262,19343,19343,21424,23536,19375,19343,21488,21521,19440,19440,19440,19408,19408,
19408,21488,23601,25746,25714,23600,25681,27826,29907,27826,27826,27825,27793,29873,31921,31920,31855,31887,36048,40242,40209,42257,44369,48562,
48529,50609,52689,52722,50609,44335,44336,44369,42289,40241,40241,42419,40339,40340,38260,38292,38325,38293,38260,38293,40438,42551,42519,42519,
42519,40406,40373,40406,40406,40406,40374,38293,42486,44599,42485,42485,44501,42420,48695,48727,46646,46614,44533,48759,50840,50839,52920,52952,
52952,52952,55000,52952,55000,55000,55000,55000,52952,52919,52920,52920,50872,50872,50872,50872,48759,46679,46647,46647,44533,42453,42421,40340,
42453,46680,46648,42422,36116,38164,36083,36051,36050,33937,33970,33937,31824,31792,33906,33905,31791,31791,33872,35952,33840,31727,33807,31727,
31759,33807,33807,31727,33775,33775,31662,31662,31662,31662,31662,31662,33710,31694,31694,33775,31694,29646,31759,33872,33905,31793,29712,31793,
29745,29713,31826,31890,31858,31923,33971,31859,34004,36149,36149,38230,38230,36149,36117,38197,38294,40375,36214,34133,32020,31988,32020,34133,
32054,29908,27828,27860,27828,27796,27828,27861,29942,29974,29974,27925,27990,30103,27990,23699,21554,23570,23538,23505,23537,25651,25650,27698,
25651,23538,23602,23667,21651,19571,17524,13395,11347,11346,13459,15573,13459,11379,9266,9266,11379,13492,15637,13556,11443,9363,9363,7251,
7218,7218,7186,7186,7186,7186,7153,5040,5040,5073,2927,2928,7089,7088,5008,7024,7056,7024,7056,7056,4975,5007,4975,7023,
6991,6958,6991,6991,4910,6991,9136,7056,4943,4910,6991,9104,9136,7023,6958,6991,9071,6991,4942,2862,2862,781,749,717,
15182,13069,13037,15150,15150,15150,15150,13070,13038,13005,15085,17230,19343,21391,21358,19245,21293,19213,17133,17100,17100,19181,19213,19213,
19246,19278,17165,17165,15117,15150,15215,15247,17328,17263,17198,21294,25487,31728,27567,23373,21293,19278,19310,19311,19343,21423,21456,19375,
19343,19375,21488,19440,19440,17359,17327,19407,21488,23568,23600,23600,23568,23567,23600,27793,27826,27794,25680,25712,27760,27793,29873,29840,
31855,33935,38096,40209,40176,42256,46417,48529,50641,54834,56947,56947,52721,46383,46416,44336,42288,44402,44402,42419,40307,38227,38259,36179,
38260,38293,38325,38325,42486,42519,42486,42518,42519,40406,38325,40406,40406,40438,40406,40373,40406,42518,42518,42518,46646,46614,46614,48694,
46646,46614,46613,48759,50839,50807,50807,52920,52952,55000,55000,55000,55000,55000,55000,55000,52920,50839,52920,52952,50872,50872,50872,48759,
48727,46679,48760,46647,44534,42453,42421,42421,42421,44534,44535,42422,38229,38164,36083,36083,36050,33969,36082,38163,31857,29680,29680,29712,
29679,29679,31759,31759,33839,33840,31727,31727,33807,33840,31727,31662,33775,33775,31662,29549,29549,29581,33743,33775,31662,31630,31662,31694,
33807,31726,31759,33872,33905,31793,29713,31793,31826,29745,29745,31826,31826,31891,33939,33939,36084,36117,38165,38165,40278,40310,38197,36084,
36149,38262,36214,36246,34133,32020,34133,36214,34134,32053,29908,29908,29909,27861,27861,29909,29974,29941,27893,30006,36409,34361,25844,21586,
21554,23634,25651,23570,23602,27796,27764,25618,23505,23570,25715,23667,19538,17458,17524,17588,15508,11314,9201,13395,11346,11379,11379,11379,
13492,13459,15637,13556,9362,9363,9363,7250,7218,7186,7154,5073,5041,5073,5073,7153,7153,7121,5040,2960,5040,5040,7088,7088,
9136,7056,7023,7056,7088,7088,5007,7023,7023,6991,6991,7023,7023,7023,7056,7023,7023,7023,7023,7023,9071,9104,9071,9071,
9104,6991,4942,2862,2894,2862,2830,2830,13101,13069,13069,13102,15215,15183,13070,13038,13070,13005,13005,15118,19310,21358,19245,21326,
19246,19213,17165,17165,17165,17100,17133,19213,19278,17197,17197,17229,15150,15182,17296,15247,15215,15150,17165,21261,29615,38067,35954,25486,
21325,21326,21358,19342,23568,27795,23536,19343,19375,19376,21488,19440,19440,17360,17327,17327,21487,23600,25680,25648,23535,21487,23600,23633,
25713,25681,23600,25680,25712,27793,31921,31888,31888,36048,38096,40176,40144,44369,52755,52754,54834,59060,59125,59061,54835,50609,48496,44336,
44336,46515,46515,42355,40275,38194,38227,38228,38260,38293,38325,38325,40406,42486,40374,38293,38325,40438,38325,38293,38293,40374,40406,40374,
40373,40373,40373,42485,44566,46646,46646,46614,46646,48726,48726,48726,50807,50839,52887,52888,55000,55032,52952,52952,55000,57081,55032,55000,
55000,52952,52952,52952,52952,52952,50872,48759,48727,48728,46680,44566,44534,46646,44566,44534,42421,42421,40341,40341,40374,38196,38196,38163,
36082,38130,36050,33937,29711,27599,27567,27567,29647,29679,29647,29614,29679,31759,29646,31726,33807,31759,33808,33775,31727,31727,31694,31662,
31630,31662,31694,33743,31662,29549,29549,31694,33840,33840,31791,31824,33905,31793,31793,31826,31858,33939,31858,31793,31858,33971,36019,36084,
36084,36084,36084,38164,38197,38197,38165,36084,34036,34068,34068,36214,38326,36181,36181,34101,34101,34101,32021,29941,29941,29973,29941,27860,
27861,27828,25780,27957,36377,32215,23763,21618,21586,23667,25715,25683,25715,25683,25650,25650,25618,25650,25683,23634,19473,15345,15411,15507,
15540,13427,11314,13427,11347,9298,11379,13460,15540,13459,15604,17717,11378,9298,9330,7283,7283,7219,7186,5073,5040,7121,5041,7121,
7121,5040,5073,5073,5040,5008,5008,7089,9169,7088,7056,7088,7121,7120,4975,4975,7023,7023,7023,6991,7023,7023,7023,7023,
9136,9169,9136,7023,9071,11216,11184,9104,7023,7023,7056,5007,2927,814,814,2895,11021,13101,13134,13134,15215,15215,13070,13005,
15118,15086,13005,15053,17165,19278,19245,21326,19245,17165,19246,19246,19214,17133,17132,19213,19213,17165,15117,15117,13069,15182,15247,13135,
13070,15117,17165,23341,27534,38066,38099,27566,23340,21325,23406,21358,27762,38327,29907,21423,19375,19375,19408,19441,19441,17360,19408,19375,
23534,27695,29807,27727,25648,23568,21520,23601,23601,23633,25714,25745,23632,25712,29841,29809,33937,36017,40177,42257,42256,48498,52787,52786,
56979,59093,59159,59128,59062,54900,50674,46416,42288,44369,44402,42322,38194,36114,36147,38260,38260,38293,38293,38325,40374,38325,38326,36212,
36212,38325,36245,36212,36180,36180,38293,40308,40341,42453,42453,42485,44501,46581,46614,46581,46581,50806,50807,50774,50807,52887,52920,52920,
55000,55032,55000,55032,55033,55033,55033,55033,55000,55000,55033,55001,52985,52952,50872,50840,46711,44566,44566,44566,44566,46646,44598,42453,
40373,42454,40374,40341,40309,36083,38229,38196,36018,36018,33905,33904,33872,29680,27535,27535,27567,29615,29615,29647,29647,31727,31759,31695,
29614,29615,35921,33841,29614,29614,29614,29582,29581,29581,29549,29549,31630,31662,31630,31662,33775,35888,33872,33937,33905,31792,31825,31858,
31826,31826,33938,33938,33906,33938,36019,36051,36052,38164,38164,36051,36051,36052,38165,36117,34068,34068,34099,36245,40439,38261,38229,38261,
38230,34069,32021,32021,32053,32086,29973,27860,27860,27861,27892,30005,32151,30103,25909,25845,25812,25748,25715,25650,25682,23602,25682,25682,
25617,25618,23602,21554,21586,17490,13362,13362,15572,13492,13428,13428,13428,11380,11379,11347,13427,13459,13459,15571,13426,9298,7250,5170,
5170,5138,5106,5073,7121,7154,7121,7121,5041,5040,5073,7121,7153,7154,7154,7154,7154,7153,7121,7121,7120,7153,7088,5008,
7088,7056,7023,6991,6991,7023,9103,9104,9136,9103,7055,9103,9135,11216,9103,9103,9071,9071,9104,7023,4943,2830,782,2895,
11020,11021,13134,13102,15215,15183,13037,13037,13005,13005,15085,15117,17165,17165,19245,21358,21358,17165,17165,19278,19246,17133,19213,19245,
19213,17165,15117,15085,13069,13070,15183,13102,15150,15117,17133,23374,27534,31727,33840,29614,23341,23406,23438,21390,25584,29843,25681,21423,
19375,19375,19375,19408,19441,19441,19440,21455,25615,29807,29807,27727,25615,23567,23568,23601,23601,23633,25714,25713,25680,25680,25648,27696,
33937,36017,38032,40144,42192,44305,48497,48560,52785,54900,59127,59160,59127,56980,50640,46481,44336,42256,42289,40241,38194,36114,36147,36179,
36148,38261,38325,40374,38325,38293,38325,36212,34132,34164,36212,36213,36212,36148,38260,40340,40341,42453,42421,42453,44501,44468,44468,46581,
44500,46613,48694,50839,52920,52887,52920,52920,52920,55000,55032,55032,55033,55065,55065,55033,54968,52920,55001,55033,52953,52920,50872,50904,
50872,46646,46614,46646,44566,44566,44566,42453,40341,42486,42454,40341,38196,33970,33970,36051,33938,33873,33905,33905,33873,31760,27567,29647,
29647,27534,27534,29647,29615,29647,31727,31695,29550,29582,31728,35954,33873,29614,27501,27501,27501,27469,29549,31630,31662,31630,29581,31662,
33807,33807,31759,31792,31793,33938,40245,38164,31826,31826,33938,31858,31858,31858,33938,36084,40277,38197,38164,36051,36084,38197,38262,38229,
34100,34100,36212,38358,40406,40406,42422,42422,40375,36181,34101,34101,36214,34166,32053,29973,29941,29973,32053,29973,30038,32183,27990,27990,
30006,29941,27763,25682,25682,25650,27763,27762,25682,25650,25650,23635,23699,21684,17555,13427,13459,13492,11347,11315,11315,11347,11412,11411,
13459,15507,15507,13426,13394,11378,9298,5170,5170,7186,7186,7186,7186,7154,7186,7186,7121,7121,7154,7154,7121,7154,9266,7186,
7154,7153,7121,7153,9201,9201,9201,7088,7088,9136,7056,7023,7055,9104,9136,9103,7023,7023,9103,9136,9103,9070,9038,9071,
11183,11184,9103,6991,4943,4943,2863,2863,11021,11021,11021,11022,13102,13102,13069,13005,10957,13037,15117,15085,15052,15052,19245,21359,
21326,17165,15052,17133,17165,17133,19213,19213,17133,15117,15118,13069,13070,13070,13070,13102,15150,17198,17165,21293,27534,27502,27502,25421,
23373,25487,23439,23471,23536,25584,25617,21456,17295,17262,19343,19376,19409,19409,19375,21455,29841,34034,29807,27695,23534,21487,23568,21520,
21520,23601,23633,23600,23632,25648,25648,29776,31857,33904,33839,38031,42192,44272,44304,46415,48592,52787,57046,59127,56981,52754,48495,46416,
44336,42256,42289,40242,38162,36114,36146,36179,36180,38293,38325,38293,38293,38293,36245,36212,34132,34099,34132,36180,36180,36147,36148,38260,
40341,42453,42486,42453,44533,44533,44501,46613,46581,46580,48693,50806,52887,50807,52887,55000,55000,55000,55000,55033,57113,57114,57114,57114,
55001,52920,52920,55000,52920,52952,50904,50904,50904,48727,46647,46647,46646,44566,44566,42453,40340,38260,38196,36148,36083,34003,31857,29744,
31792,31760,29680,31760,31760,31760,29712,27600,25454,27534,27534,27502,27502,29615,29582,27501,27501,29582,29647,35921,35954,29614,27469,27469,
27469,27468,29581,33775,31695,27469,27469,31695,33808,29679,27566,29712,31858,36019,36052,33971,31793,31858,33939,31858,31858,33938,33971,36052,
36116,38197,38229,36116,38197,40342,38262,36181,34101,34132,38325,40438,40438,44567,42422,40309,40342,40342,38294,36213,36213,34133,32053,32021,
29940,32021,32053,27892,29973,30038,30038,30005,32021,32020,29843,27762,27763,27762,27795,27795,29843,25683,25683,23699,21651,19603,19636,15539,
13427,13427,13428,11347,11315,11347,11379,11379,13459,15508,17620,17620,15507,11347,9298,7218,7186,9234,9266,9234,9266,7186,7153,9234,
9266,9234,9234,9234,7153,7121,7154,7154,7154,7153,9201,9201,7153,7121,9201,9169,9169,11249,11249,11216,9104,9104,9104,7023,
7023,7055,9103,9136,9103,9070,9071,11151,11216,9103,9071,6991,4975,4976,2895,2927,13101,13101,10989,10989,10989,13070,13070,10957,
10957,13070,15117,13005,15085,17132,17133,17133,17165,19213,17132,15052,17132,17100,17133,17133,15085,15118,15150,13070,13070,13070,13103,15151,
15150,15150,19246,19213,23341,23341,23341,21261,23374,23439,21391,21391,25617,25649,27730,23536,19343,17263,17262,19376,19409,19408,17262,21390,
31922,36115,31856,25582,25582,21454,21455,21488,21520,23600,23633,23600,23600,25648,27696,29744,29744,31791,33839,35951,40112,42160,42191,44303,
48560,52787,54868,54869,50642,46415,46415,44336,42256,42224,40176,40241,38161,36114,38227,38292,40373,38293,38260,38261,38261,36212,36212,36213,
34132,34099,34099,34100,34100,36115,36115,38228,40341,42421,42454,42453,44533,46646,46646,46581,46613,48693,48693,50806,52887,50806,52887,55000,
55000,55000,55033,57113,57114,57147,57115,57114,57081,55000,55000,52952,52920,52952,52920,50840,50840,50840,48760,46647,44566,44566,44566,42421,
38228,36115,36083,38196,38196,33971,31825,29712,29680,29680,29647,29647,29680,29680,29712,27600,25421,25454,25421,23276,25356,27502,27469,27469,
27501,27502,29582,31728,31695,27502,27469,25388,27436,27468,29517,29549,29550,27469,27501,29615,29647,27567,29680,31793,31858,33906,31826,29745,
31826,31826,33939,33971,31858,33939,33939,33939,34004,36116,38165,36116,38229,40342,38262,36181,36181,36213,38293,40406,40373,40374,40341,38228,
40309,42422,42454,38261,34100,34133,34133,34101,32020,34101,32053,29940,30005,32086,34166,32085,34069,34068,31923,31891,29843,27795,29908,29908,
27795,25682,23667,23732,21684,19603,19603,17588,13459,13427,13427,13395,13428,13460,11379,11379,15540,17621,15508,15508,15475,13395,11347,11347,
11315,11315,11315,9234,9266,7218,7186,9234,11315,13363,11282,11282,9202,9169,9169,9202,9202,9201,9233,9201,9201,9201,9201,9201,
11249,11217,13330,13362,11217,9103,9071,9103,9103,9103,9103,11183,11184,11151,11119,11119,9071,9071,9071,9104,7056,4975,4975,5008,
13069,13069,10989,10989,10989,10957,10989,13037,10957,10989,13037,13005,15085,17197,19213,17132,17133,17133,15052,15052,15052,15052,15085,15085,
15117,15150,15183,13135,13102,13070,15183,13103,13102,15150,19278,19213,19180,21261,21261,21261,21294,21391,21391,19343,21424,21391,21391,19343,
19343,21521,21488,19375,19376,19408,17295,21391,27696,29776,27663,27663,27696,23535,21487,25714,25714,21520,23601,23601,23568,25616,27729,29745,
29712,29711,31791,35919,35919,38031,40144,42223,46449,48594,48562,50643,48562,44335,42255,44304,42256,40144,38064,38128,38161,36114,38227,38259,
38260,38228,36180,38293,40406,38293,38261,36180,34132,36180,34132,34100,36148,36148,36115,38228,40341,40341,40341,42421,44534,48727,46678,44500,
46581,48726,48725,50838,52886,50806,54968,55000,55000,57081,57081,57113,57147,57147,57115,57114,57081,57113,55033,55033,52952,50872,50840,50840,
50840,50872,50840,48695,46614,44502,42421,40308,38195,36082,34002,38164,40245,33906,29712,29712,29712,31793,29647,27567,27567,27600,27567,25486,
25454,25454,25454,23276,25356,25389,27469,27469,27501,27502,27534,29615,29582,27469,25388,25388,27436,27436,29516,29517,29549,27469,27470,27535,
29615,29680,31825,33938,31858,31858,31858,31890,31890,31890,31890,31890,31891,31891,31858,33971,34004,34004,34003,31923,34004,34036,36181,38294,
36213,36213,36212,38293,38293,40309,42422,38196,38228,40341,40374,38261,36181,36213,36213,36181,34101,34101,34101,32053,32021,32053,34133,34133,
36213,36116,34002,33971,31956,29875,32020,31988,27795,27795,25780,23764,21683,19603,17523,15475,15508,15540,17621,17589,15476,13459,13459,11379,
15540,17621,15475,15443,13363,13395,13395,11347,11315,11283,11315,9266,9266,7218,9266,9234,11314,13362,13330,11282,11282,11282,9202,9201,
9234,11314,9233,9233,11281,11249,11249,11249,11249,11216,11249,13330,13297,11184,11184,11184,11183,11183,11184,13264,13232,11151,11119,9038,
9038,11184,9136,9136,7056,7056,7056,4975,10989,13069,10989,10989,11021,10957,10956,10989,10957,10956,10956,13037,15085,17165,19278,17165,
15053,15052,15052,15052,15052,15053,15085,15086,13038,15151,19442,19441,15248,13135,13103,13070,13102,15150,17198,19246,19181,19213,19246,19213,
19278,19343,19376,19343,17230,19278,19310,19343,17230,21488,23569,19375,19376,17328,19376,19375,21423,23502,25583,25648,27729,23600,23568,25746,
23633,21488,21520,23601,23568,25617,27729,29777,29744,29744,31791,33871,33871,35951,38031,40143,40175,44369,46482,48562,46482,44368,42256,42224,
40144,38096,35983,33935,36081,38194,36147,36147,36147,36180,36148,38293,42519,40406,38293,36180,34099,34132,34132,34099,36148,38260,38196,36115,
40341,42389,42421,42454,44534,46647,46646,44501,46613,50806,48726,50806,50806,50774,54968,55000,57080,57113,57081,57081,57114,57147,57147,57115,
57113,55033,55032,55033,52952,50839,50839,52952,52952,52952,48759,46646,46582,42421,40276,38195,38162,36114,36050,36018,36019,33905,31760,29680,
31760,31825,27600,27567,27567,27599,25487,23341,23374,25454,25454,25389,25357,25356,27469,29582,29614,27502,27502,27502,27502,27469,25388,27468,
27468,27436,27436,29517,27437,27437,27502,29615,31761,31794,29745,31825,31826,31825,31858,31858,31858,33939,31890,31858,33939,31858,31858,31891,
31891,33971,31923,31891,34003,31923,34101,36213,36181,36180,38261,38293,40341,40309,42389,40309,40277,40309,40374,40406,40406,38326,36213,36213,
38294,34101,34101,34133,32021,32020,34101,36181,40374,36148,34002,34003,34036,31956,31988,32020,31988,27828,27860,25845,21716,19635,17555,17555,
17620,19701,19701,21782,17588,13427,11379,13395,15508,17588,15475,13395,13395,13363,13395,13363,13363,11282,11282,11314,9266,9298,11346,9266,
11314,13362,13330,11282,13362,13362,11250,11250,11282,11314,11314,11313,11281,11217,11249,13329,13329,13329,13329,15377,13264,11184,11216,11216,
11216,11184,13232,13264,13232,13232,13232,11119,11151,11184,9136,9104,7056,7088,7056,7023,10956,10989,10989,8908,10989,10957,10956,10989,
13037,10957,10924,13004,15117,15085,17165,17165,15052,15052,15052,15052,15085,13005,13005,13005,13038,13103,19442,19442,17361,15216,13103,13103,
13103,13102,15118,15117,17133,19214,19246,17198,17231,17295,17296,17295,17263,21424,23536,23504,17230,17229,21423,21423,17295,17296,17328,19375,
19375,21422,23535,23568,23568,23568,21488,21520,21487,19440,21553,23601,25649,25649,27697,27696,29744,31857,33937,31824,31791,33871,35951,38064,
40176,44369,46482,44369,42289,42257,42257,40209,38064,35984,36016,33936,34001,38227,36115,34066,36147,36180,38228,40373,40438,40406,38293,36148,
36180,34100,34099,34099,34099,36148,38228,38228,40309,40309,42421,42454,42422,40308,42421,44501,44533,48758,48726,48693,52886,50774,52855,55000,
57048,55000,55000,55000,57081,57114,57114,57114,55033,55000,55000,55000,50807,48726,50807,52920,52920,50872,48726,44533,44501,42388,40276,40275,
38162,38162,38162,33937,33873,33872,31792,31760,31760,31793,27599,27567,27600,25487,23406,21293,23309,23309,23341,25421,25389,25389,27502,31695,
31728,27502,25421,25389,27469,27469,25388,29549,29549,27436,27436,29485,29517,29550,29582,31696,31793,31793,29680,29680,31793,33905,33938,31857,
31858,31858,31793,31826,33939,31890,31890,33939,33971,34004,36084,36116,38197,36116,36149,36213,34100,34099,36180,38293,42487,42454,40341,40342,
40342,38229,40374,40439,40407,38326,36213,36213,38294,36181,34133,34101,34101,32020,34101,34101,36148,36115,36115,34003,31955,31988,32053,32020,
32053,32021,29973,25844,23797,21748,19636,19668,19701,19701,19669,19669,19637,15507,13394,13395,15508,15508,15475,13427,13427,13362,13363,15475,
13395,13395,13395,11347,11379,11411,13459,13427,13395,13362,15443,15443,13362,13330,13330,13330,11249,11249,11281,11313,13329,13297,15410,15442,
15378,15377,15377,13296,13232,13264,13264,13264,13264,13232,13232,13264,15345,13264,11184,11184,11216,9136,9136,9136,9168,7088,7055,7088,
10924,10957,8876,8876,8908,8908,8908,13069,13069,13037,10956,13004,15117,15117,15084,15085,15085,12972,13004,15117,15117,13005,13005,13038,
10990,13135,13167,13167,13200,13136,11055,11054,13102,13102,13070,13069,15085,15118,15118,15118,15183,15247,15248,15247,17295,19376,23537,29908,
27762,19309,19342,19375,19376,17328,15280,15279,17327,19407,21455,21455,21455,21456,21520,21488,19375,21488,23601,23602,23601,25649,25649,25616,
27728,31889,31889,31824,31791,31791,33871,36016,42290,44370,44402,44369,42289,42257,40210,40210,38129,33936,33936,36082,36082,34034,34034,34034,
36115,38228,40341,40406,38293,38325,38261,36180,34099,34132,36212,34132,34067,34067,38228,40341,38228,38196,40341,42422,40309,36116,42421,44534,
44534,48726,48694,48693,50806,50774,52855,54967,54967,54967,55000,55000,55000,55033,55001,55000,52952,52952,55000,52919,50806,48694,50807,50839,
50807,48726,46645,46613,44533,42420,42356,42356,42388,38162,36016,33872,33840,33840,33840,31760,29647,27599,25519,23406,23406,23374,23374,23374,
21261,23309,23341,25422,25389,25389,25389,27469,31727,27534,23308,25389,25389,27437,27436,29582,31629,29516,29484,27436,29517,29517,27469,29615,
31696,31728,29712,29680,31825,33938,33905,31825,33905,31857,31825,31826,31858,31890,33971,33939,33971,34003,36084,36116,40342,42423,38261,36213,
36180,38260,40373,40373,42454,42454,40342,38229,38262,38294,40406,40406,38294,36246,36213,36213,38326,38326,34133,32020,34133,34133,34133,36181,
36180,36180,36148,34035,31988,32020,34133,32053,32053,32053,30005,25845,23797,21781,19700,17620,19668,19668,19636,19636,19604,17556,15475,15475,
15507,17588,15508,13427,15507,13427,13395,15475,13427,11314,11379,17653,15572,11346,13427,13427,13427,13394,11314,11281,13330,13330,13330,13362,
13330,13329,11249,11248,13296,13296,13297,15410,15409,15345,15345,15345,13296,15377,15409,13264,13232,15345,15377,15377,15345,13264,11216,11216,
11216,9136,9168,9136,9201,9201,9201,11249,10924,8876,8844,8876,8908,8908,8875,10956,10956,13005,13005,13004,13004,15085,15084,13004,
13005,13004,13005,17198,15118,13038,13070,13070,11022,13103,13135,11054,11055,11055,11055,11022,11022,13102,13102,13070,13069,13069,13070,13070,
13135,15248,15280,15280,15215,17295,23569,36182,34100,23503,21390,19374,19408,17360,15280,13232,15280,19440,21488,21488,19375,19375,21521,21521,
19408,21488,23601,21489,21488,23569,23568,25648,29809,29841,29744,29744,29743,31823,38162,40242,40209,44435,44435,44402,42322,40145,36017,36082,
36049,33936,33937,38228,42454,36115,34002,34067,36115,36115,36180,38228,36148,36148,34099,34099,31987,32019,34132,34099,36180,38260,38261,40374,
40309,38228,40341,40309,38229,38229,42422,44534,44534,46646,48694,48694,50742,50742,52854,52887,52919,54999,55000,54999,54967,52919,52920,52920,
52919,52919,52919,50807,50774,48694,50807,50775,48694,46613,46645,46613,44500,42420,42355,40275,42356,40275,33936,33839,33872,33840,31727,29647,
27566,25486,25454,23406,23406,23341,21261,23309,21261,23309,23341,25454,25421,23341,23340,23308,27501,27501,23308,25389,27469,27469,27469,29549,
29549,29517,27436,27404,27404,27437,29549,27502,29582,29647,29647,29680,31792,31760,31792,33905,31824,31825,31825,31826,33938,33971,33971,33971,
34003,36051,36116,36116,42422,44600,40374,40373,40373,40406,42454,42487,42422,40341,40342,38262,38326,42487,40439,38326,36213,36214,36246,38326,
38359,38359,34133,32020,34133,36213,36214,36246,36213,38261,36148,34100,36181,34133,32085,34133,34166,32053,30006,27958,25909,21781,19733,19733,
19701,17556,17523,19604,19604,19636,19668,19668,17555,17588,15539,13427,15540,15572,13427,13362,13395,9266,9266,17685,17653,13395,13395,11346,
11346,11346,11313,11313,13362,13362,15410,15410,15410,13329,13329,13361,15442,19603,17490,15409,17490,17458,17457,17457,17457,17490,17489,15376,
13296,15377,15377,15345,13296,11216,13296,11216,9135,9135,7055,7055,7088,9136,9136,9168,10956,8844,8876,8876,10989,10988,10956,10956,
10924,10924,10924,12972,10924,13004,13005,13004,10956,13005,13005,15118,13070,13070,13070,10990,11022,11055,11087,13167,13200,11087,11022,8942,
11022,11054,13102,13102,15150,15118,15150,13070,13070,15215,15280,15280,13167,15215,21489,23601,25616,23535,23535,19375,15214,15247,15280,15280,
15280,17392,21521,21521,19408,19376,19408,21521,21521,21489,21521,21488,21488,21488,21487,23535,25648,27696,27728,27695,27695,29776,40308,42421,
38130,40242,44403,44403,40210,36017,33904,31888,31856,29808,31888,36082,40341,36148,34034,34067,36116,34067,34067,34068,34100,34067,31986,31987,
34035,34035,34067,36147,36180,40373,40341,40309,40341,38196,38196,40276,40244,42357,44469,44502,44501,46614,48727,50774,50774,50774,52854,52855,
52887,52887,52886,52886,52919,52919,52919,55032,52919,52887,52919,52887,50774,50742,50774,50742,46580,46548,46645,46580,44468,42388,42355,38162,
36049,40210,38097,33839,33839,33840,29647,27534,27534,25454,25454,25487,25487,23406,21229,21228,23374,25422,23341,25454,27534,25421,23308,23276,
23276,25356,25389,25421,25389,27437,27436,27436,29517,29516,29484,29517,29484,29517,29549,29582,29582,29582,29615,27567,27599,29647,31792,36018,
33938,33873,31792,33873,36018,36019,38131,38132,36051,38132,40277,40277,40342,44567,44535,44566,44567,44567,42454,40374,40374,40374,40374,40374,
40407,40439,38327,36246,36246,36246,38326,38359,38359,38326,36213,36181,36181,34101,36213,38294,38261,38294,38293,36213,36246,34165,34165,34165,
34133,34133,32086,30038,28022,23894,19700,19733,19668,19604,19571,19539,19603,19604,28023,32313,25974,21749,17620,17620,17653,15572,15507,13427,
11314,11346,11346,11347,13459,13427,13459,13427,11346,11346,13426,13426,15475,17555,17523,15410,15378,15410,15410,15474,17555,21684,21651,17457,
17457,17458,15377,17458,17458,15377,15409,17490,15409,13264,13232,13297,13329,13297,13297,11249,11248,11216,9136,9136,7056,7056,7056,9104,
13037,10924,10924,8843,10956,10956,13037,15117,10924,10892,10892,13005,13005,10924,10956,13037,10956,10957,13037,13037,13070,11022,11022,11022,
8974,11054,11055,13167,13167,11054,8974,8974,11055,11087,13103,13103,17231,15151,15150,13102,13102,15214,15280,17360,15247,15247,17360,17295,
19343,21456,19375,17295,17295,15247,15247,15280,15280,17360,19408,19408,19408,19408,19440,21553,21586,21553,19472,19440,21488,21488,21487,21423,
23502,23535,25648,25648,29809,31889,31922,34002,31921,36050,42291,42291,38130,36017,31888,29840,31888,31856,31856,29776,31889,34002,31954,31922,
34067,34068,34067,34100,34164,32052,31987,31987,34067,34002,36083,36115,36115,38228,38228,38195,40308,40308,40243,40243,40211,42259,44404,44404,
46517,46581,48694,50807,50774,50806,52854,52886,52854,50773,50773,50805,52886,52919,52886,52951,52919,52886,52854,52854,50774,50741,52854,52822,
48628,44467,46580,46613,44500,44468,42323,38098,36017,38098,38098,35952,31759,29679,27567,27567,25455,25422,23374,25455,25454,23373,21261,21293,
25454,25486,25453,25454,27567,31760,31792,25389,23276,25389,25389,25389,25388,25356,27436,27436,29516,29549,29516,29517,29549,29549,29550,29582,
29550,29583,29615,29647,29680,29712,31793,31825,33873,33905,33840,33872,35985,33905,38098,38099,40211,40276,40276,42389,40341,44535,46647,46647,
44599,44599,44599,42519,42519,44600,40406,38294,42487,42520,40375,38294,38326,38359,38359,38327,38327,38326,38294,38293,38261,36149,38229,38262,
38294,38294,36278,36246,34165,36213,38326,38326,36279,34166,32118,32118,30038,25942,21781,21716,21748,21716,21684,23764,23764,23700,27958,34361,
30200,23894,19733,19733,19798,17621,15507,13427,13395,13395,13427,13427,13459,13460,13459,13459,11346,11346,13427,13426,13458,15507,17490,15410,
15409,15410,15442,15474,17522,21618,21618,21617,21618,19538,19538,19571,19506,17392,15344,17457,15377,13264,13264,13329,15410,13297,11184,11184,
11184,11217,11216,9136,9104,9104,9136,9137,15085,12972,10924,10924,10891,10924,13004,13004,12972,10892,10892,10956,10957,10956,10956,10957,
10957,10956,13037,13070,10989,11022,11022,8974,8974,8974,8975,9007,8974,8941,11022,11055,11022,13103,15183,15150,17198,17198,15150,13070,
13069,13134,15247,15280,13199,13167,13167,13166,17294,17263,15182,15214,17327,17328,15280,13232,15280,15247,17295,17295,17328,17360,19440,19473,
21553,21553,19472,19440,21488,21488,21488,21455,21454,23535,23567,25648,29906,31986,29808,29808,31888,33969,38098,38097,38130,36082,34001,31953,
31953,31921,31888,31856,31856,31889,31922,29873,29874,31987,32019,32052,32052,34132,32051,34067,34067,36115,36083,34034,36115,38228,38227,36147,
38227,40276,40275,40276,42291,42259,44372,44469,46549,44501,46549,48726,48694,52855,52887,52887,52854,50773,50806,52854,52886,52918,52886,52886,
54999,52887,50774,52854,52854,50741,50741,48661,48596,46580,44467,46580,44532,42355,42355,40210,38065,35985,35952,33872,31792,31760,29647,29647,
29648,27535,23342,23342,25454,23406,21293,23373,25454,23373,23341,25421,27567,33873,35986,29615,25389,25388,25389,25388,27469,27436,29549,29517,
27404,29549,27468,27436,27469,29550,29550,29582,27502,29583,27567,27599,31793,33906,31858,31793,31793,33905,35953,35920,35986,35986,36018,38098,
42324,44469,42389,44534,42421,44534,46680,46679,44599,42518,44632,44632,42551,42519,40439,40407,42487,42520,42520,38326,38326,40439,40471,40407,
40375,38294,36245,38293,38293,38261,38229,38229,38294,36213,36213,36278,34165,36213,36278,40439,42618,36311,32085,30038,27957,25909,23829,21748,
23829,23829,23764,25845,25845,25845,27926,28055,28087,23894,21814,19733,19733,17620,13427,13395,15475,15475,15443,13395,13427,13459,13427,13427,
11379,11347,13427,13459,15507,15507,17555,19603,19571,17458,17490,17522,19570,23666,23665,25713,25746,25779,23698,23666,23666,23633,21585,21618,
21618,17457,17458,19571,19603,15410,13264,13264,13297,13298,13297,13297,13361,13362,11281,9168,15052,12939,12972,13004,10891,12972,12972,12940,
12972,10924,10892,10924,10924,10956,10956,10956,10957,10957,13070,13103,11022,11022,11022,8942,6893,6894,6926,9039,9006,8973,11022,13103,
13070,15118,17231,17198,17165,19278,17230,15118,15150,15182,13134,13134,13199,13167,11054,13166,15215,15214,13166,13167,13166,13199,13200,15280,
15280,15247,15247,15247,15247,15247,15279,15279,17360,19440,19440,19408,19407,21488,23600,25681,25680,25648,25680,25713,27826,27761,27696,27728,
29808,29775,33904,33936,36049,36114,34034,31954,31954,31921,33969,33969,33969,31921,31954,31954,31954,31987,34100,34100,32019,32019,32019,32019,
34067,36148,36148,36147,34067,36179,38292,38227,38163,38195,40308,42356,44404,42324,42324,44469,44469,42356,42355,44468,48694,50807,52854,52887,
52887,52854,52854,52886,52886,52918,52886,52886,52919,52854,52854,52854,52854,50741,48628,46548,46548,48660,46580,48693,46581,42388,40275,40243,
38098,35985,33872,31791,29679,31760,31793,29680,27600,25455,23374,23342,23406,25519,25486,23373,23341,23341,21260,23341,25421,25454,27566,27534,
25421,23308,23340,25389,27469,27469,29614,29582,27436,29549,27501,27436,27468,29582,27502,27502,29582,29647,27567,27567,31793,33906,33906,31826,
31760,31792,33873,35953,38066,38098,38098,40178,44436,46517,46550,46582,44469,44534,46679,48760,44599,42519,44599,44632,44600,42519,42520,42520,
42487,42487,42552,42520,40407,42552,44633,42520,40407,38261,36180,38325,40374,40374,38229,36148,36181,38261,36213,36246,36278,34165,34165,38359,
40504,36279,32085,30005,27925,23797,23797,23861,25910,25909,25877,23764,23764,27958,25910,23829,25942,23926,21846,19701,15540,15507,15475,15443,
15443,17523,17523,17556,17588,15508,13459,13459,13427,13427,13427,15507,15540,17588,17587,19635,17522,17490,21683,23731,25714,25681,27761,31954,
31986,29874,27794,29907,32019,31954,27793,27826,27859,23666,19505,21618,21651,19570,19538,19538,19506,19506,17458,17490,19603,19635,13361,11247,
17132,15019,15052,15084,12971,12939,12939,12939,12940,10892,10924,10924,10924,10956,10956,8876,10956,10989,13070,13070,10990,8942,8974,8942,
6893,4845,6926,9071,9039,8974,11022,13070,15118,17198,19311,19246,19245,19278,17198,15118,15182,15215,15215,13134,13167,11087,11054,11086,
13167,15280,13199,13167,11119,11119,13200,15280,13232,15279,15279,15279,15247,15247,15247,15279,17360,17392,17392,19440,19439,21488,23600,23600,
25648,23600,25713,27794,27794,25681,25648,25615,25647,29743,31823,33904,33936,31921,31953,31954,34067,34034,31889,31888,31889,31889,31922,31954,
31987,31987,32019,31987,29939,32019,31987,31987,34035,34067,34067,36147,34067,34066,40340,40340,36114,38195,42356,40276,40244,42357,42324,42356,
42389,42356,42355,42355,46581,48693,48661,50774,50806,52887,50806,52854,52886,52854,52854,52886,52854,50741,50774,50741,48661,48661,48628,46548,
46548,48628,48693,50806,46614,44468,40275,38130,36017,35953,33904,31791,27566,29711,31793,29680,25487,23374,25454,25487,25487,25454,23406,23341,
21261,21261,23308,23309,23341,25454,27567,27534,25421,23340,23308,25421,27501,25420,27469,27501,27468,27468,29549,27501,27468,27469,27501,27502,
27534,29647,29647,27567,29713,31793,31793,31825,31760,31792,33905,35953,38098,42259,42291,44371,46549,46549,48662,46582,44469,46615,46679,48760,
46680,44632,46712,44665,44665,44665,44633,44633,44600,42552,42552,42552,40472,42520,44633,44633,42520,40374,38293,38293,40374,42454,40342,36148,
36181,38293,34132,34133,36278,34133,34133,34198,36278,34198,32085,32118,32151,27990,25909,25909,25909,25909,25909,25877,23797,25877,25877,21748,
21781,21781,19733,17621,15508,13395,17556,17523,17523,19571,19603,19636,21749,19701,17620,15507,13459,13459,15507,15508,17588,15507,15474,17554,
19570,21650,25746,27794,29842,33969,38195,44533,44534,38260,34066,36147,42453,44533,40339,38227,34034,29841,25712,25713,25746,23666,25746,25779,
23634,23634,23698,23666,23698,21682,17488,15376,19180,17067,15019,15052,15052,12939,12907,12939,12940,10892,10924,10924,10924,10956,10989,10956,
10924,10957,10990,10990,8941,8942,8974,8974,6926,6926,4878,6959,9039,9006,11054,11022,15118,17231,19311,19245,19213,19245,19246,17198,
15150,15215,15280,13167,11086,11119,11087,9038,9038,11151,13232,11119,11119,13200,13232,13232,13199,13199,15279,13199,13199,13167,15247,17360,
17393,17393,17393,19440,21552,25746,23600,21487,23600,23601,25681,25746,25714,25681,23600,23567,25615,27695,31856,33937,31856,29808,29873,31955,
36180,34035,29808,31888,31888,29840,29873,29874,29906,29874,29939,29939,29939,32052,32052,32019,31987,31987,31954,31986,34066,34034,36179,40340,
40276,38227,42389,42389,40244,40276,40276,40276,42357,42389,40308,40275,46581,48694,48661,48694,50774,50774,50741,50774,52854,52854,52854,50773,
50774,50774,50741,50741,48628,46548,48628,46548,46580,48693,48661,44500,46581,44501,40275,38163,38130,33904,33872,31792,29678,27598,27599,27567,
27535,25455,25519,27633,27632,25487,23341,23341,21261,23341,23309,23308,25421,27567,29647,25454,25421,25421,23340,25388,27501,27501,25420,25420,
27468,27468,27533,29581,27501,27501,27501,27502,27502,29615,29680,29680,29680,31761,31793,33906,33873,33873,33873,35953,40146,44339,44372,46452,
48597,48597,48662,48630,44501,46647,48760,48792,46712,46680,46712,44665,44664,44665,44633,46713,44664,42552,44632,42584,42520,42520,44600,46713,
46681,44567,40406,38293,40374,40374,40374,38261,36181,36181,34100,34133,36246,34165,34133,34198,34166,32150,32118,30102,32183,30103,28022,25942,
25909,25909,25909,25877,23764,23764,25910,23829,21781,21781,17620,17588,15508,15475,17556,17556,19604,23732,25845,23797,21748,19701,19668,15508,
15507,15508,15508,17588,17588,17555,17555,19635,21650,25746,27826,34002,40275,46516,48661,52822,52887,50807,46581,42322,46580,52853,50773,48628,
42355,38162,34001,34001,31954,31954,31954,31986,31954,31986,31986,31954,29938,29939,25745,21552,21259,19179,17067,15019,14987,12939,12907,12907,
10891,10892,10924,10956,8876,8876,8908,10957,10956,8876,8909,10989,10989,8941,11022,9006,9006,6958,4878,4878,6990,6958,6926,8974,
13070,15150,17230,17197,17165,19245,17198,17231,15150,13102,13167,13167,11119,11151,11119,9071,9006,9038,11119,9071,9038,11151,13232,13199,
11119,13199,13231,13166,13166,15280,15280,15280,15312,17425,17393,17360,25746,32117,25746,19439,21520,23601,23601,23633,23601,23569,21488,21487,
23567,27695,31856,33936,31856,29808,29841,29874,34100,34035,29840,29808,29775,27760,27761,29874,27826,27794,27826,29907,29939,29939,32019,31987,
31987,31987,31954,31986,32018,34066,34034,38227,40340,38227,42389,42421,40309,40276,38196,40276,40308,40340,42388,42356,42388,46581,48661,46548,
46581,48661,50741,50742,50774,50774,50773,50741,50773,50774,50774,50774,48661,46548,46548,46548,48629,46581,46548,44436,44468,44501,42356,40243,
40243,33904,31791,31759,29679,27598,27567,27567,25487,25455,27600,31826,31826,27600,23406,21261,21261,23374,23309,21228,23309,25421,23341,23308,
23341,25389,25388,25388,27533,29614,27501,25388,25420,27468,27501,27501,27533,27501,27533,29582,29615,29615,29615,31728,31760,31761,35986,40212,
38099,35986,35953,38066,42259,44339,44371,44404,46484,50678,50742,50743,48662,48695,48760,50873,48792,46712,44632,44664,44664,44665,46713,44632,
44632,44632,44665,44665,42552,42520,42520,46680,46712,44632,42519,42454,40374,40342,40342,38294,38326,40407,36246,36214,36247,36246,34166,34165,
32085,32117,32150,30070,30070,30135,28055,28022,27990,27958,25877,25877,23764,21716,23797,23829,21813,21814,19668,17620,17588,17556,19636,19636,
21652,23764,25845,23797,21716,19636,17588,15540,15508,15540,15508,17556,19668,19668,19667,21650,23697,29906,36114,44469,50774,52854,52789,52756,
52789,54934,54967,52854,50708,50642,50610,48562,48530,48596,46516,42322,40275,44501,46581,42388,42420,44500,44468,42420,42421,38228,31954,29842,
25419,23307,17067,14987,12907,12939,12907,12972,12972,10924,10956,10956,8844,8876,8908,10956,8876,8876,8876,11021,13102,11022,11022,8974,
6926,6926,6990,4910,4910,4910,4878,6926,9006,11054,13069,15149,17230,17197,15118,15118,13102,13134,13167,11086,9038,11119,11152,11152,
9039,6958,9071,9071,9038,9038,11119,11118,11118,11151,13231,13199,11118,15344,17425,13232,15280,17393,15312,17360,21553,25746,23633,21520,
21455,21520,21553,23601,23601,21521,19407,21455,23567,27695,36050,38163,34002,31921,29841,29842,31955,31922,31889,31888,29840,27728,27728,29874,
29874,27826,27826,27826,27858,29907,31987,31987,31955,31987,29938,29906,31986,31986,31954,34034,38228,40276,40309,40309,38228,38164,36116,38228,
40309,40308,42388,42388,42355,44500,46549,46549,46549,46581,48661,50742,50741,50741,50741,50741,50774,52822,50741,48628,48628,46516,44435,46516,
46549,44468,44436,44436,42388,42389,42389,40243,36050,31824,29711,29678,29679,29711,29679,27567,23374,23374,25487,29713,29713,25487,23374,23309,
21228,19148,21196,21228,21228,21196,21196,21228,23308,23308,23276,23275,27469,29581,27501,27469,27501,27468,25388,27468,27501,27501,29582,29614,
31695,31695,31695,31695,31728,31760,35954,38099,38066,35985,38033,40146,42226,44339,46484,46452,46484,52823,52855,50743,50775,48695,50808,50840,
50872,48825,48792,46744,46745,46745,48826,46713,44632,46712,46745,46746,44665,44633,44665,46713,46712,46680,44567,42487,42454,40406,40406,40375,
38327,40439,38359,36214,38327,36279,34198,34198,32085,30037,32150,30102,30135,30103,28055,30103,30070,27990,25877,25877,23796,23796,21748,21749,
21781,21781,17620,17620,17621,19669,19668,19668,21684,21652,21717,21749,19668,17588,15540,15540,15540,15540,17588,17588,19636,19636,21715,25779,
29938,38227,48694,52887,52789,54869,54901,54869,54901,54934,57048,57047,54836,46383,44206,44272,46385,50611,48564,44370,44370,48596,54935,54935,
52822,52822,52822,52790,52823,50710,42324,38163,29612,23339,19114,14986,12939,15053,12940,10892,13004,13037,10956,10956,8876,8908,8908,8908,
8876,8876,8876,10989,11022,11021,8941,6861,6893,4845,4877,4910,2830,2830,2862,4943,9103,9071,11086,15214,17230,15150,15150,13070,
13102,13134,11086,11119,9039,9039,9071,11152,13265,9071,9039,9038,9006,9070,11119,9038,9070,11150,11150,13231,13199,13232,15344,13232,
15280,15281,15280,17360,19472,19472,19440,19407,19407,21488,21488,21488,23602,23634,19408,21455,25615,27663,36050,40276,36050,31922,29841,27762,
27762,27729,31889,31921,31921,29873,29841,27794,27826,27826,27858,25746,25745,29906,29939,29939,31987,29906,29906,29906,31987,31986,31954,34035,
38164,38228,40309,40309,38196,36116,38228,38228,38228,38228,42389,42420,42420,44500,44469,46581,46581,48629,48661,50709,50741,48629,48628,50741,
52822,50773,48628,44435,44435,44435,44435,46516,46516,44436,42323,42356,42323,40243,40243,36050,33937,33904,29711,27565,27598,29679,29679,25519,
23374,23374,23374,23374,25487,23406,23341,23374,23374,21228,21196,21228,23309,23309,21196,21195,23276,25356,23276,25324,25356,25388,25388,25420,
27468,25388,27469,29614,29550,29582,31662,29582,29582,31663,31663,33776,35889,35921,38034,35953,38034,38066,38065,40146,42226,42259,46452,48597,
48629,52855,52855,50775,50808,52888,52920,50840,50840,50905,50905,48792,46712,48793,48793,46713,46712,46712,48826,46746,44633,44633,46745,46712,
44632,46712,44600,42487,42487,42520,40407,38327,38327,38327,38359,36279,36247,36246,36246,34230,32118,30037,30102,32215,32215,28022,27990,30103,
30070,27958,27925,25844,23828,23829,23861,23829,21749,19669,17620,17588,19669,19669,19668,19668,21716,19636,19636,19668,19668,17620,17620,17621,
17620,17588,17620,19668,19668,21683,21650,25746,34067,46581,54935,57014,54901,54900,56981,59094,59095,57047,54934,52821,52723,48464,40013,40014,
42127,44305,44306,40079,38031,42225,46418,52789,57015,54870,52790,52724,52725,54870,52790,48597,33805,25419,21259,19180,15052,15085,12972,8811,
10892,10924,10956,10956,8876,8843,8875,6795,6795,8844,8908,10956,10956,10989,8941,6828,6860,6925,4877,2797,2830,2830,2862,2894,
4975,7023,9071,13167,15182,15150,15182,13134,11053,11053,11054,11119,11151,11119,9039,11152,17491,13233,9038,9038,6958,11151,11150,9005,
9070,9070,11118,11183,13231,13232,13264,15312,15313,15281,15313,17392,17392,17392,19439,17327,19407,21488,21488,21488,21521,21553,19440,21488,
27728,29744,33937,38098,33970,29841,27761,25649,25649,25616,27728,29808,31953,36180,31987,29874,27826,27859,29939,25746,27794,29939,32019,29939,
29906,29906,27826,27858,31987,31987,34035,34003,36051,38164,38196,38196,38228,38229,38261,38229,38228,38260,40341,44501,42388,44436,44468,44469,
46549,46549,50742,50742,48661,48661,48628,48628,50741,50741,46547,44403,44435,44403,44435,46516,44436,44403,42355,42356,40276,38130,36017,33937,
33969,31824,29678,27598,29679,29647,27567,25487,23406,23374,21294,21261,21293,23374,23341,23341,23374,23341,21261,23309,23341,23309,21196,23276,
25357,25389,25324,25324,25324,25355,25356,25388,27501,27469,29582,31695,31695,31695,31695,31695,31662,29582,31662,33775,33808,35921,40179,40147,
40146,40178,40178,42258,44339,44404,48565,50710,52823,54904,54936,52920,52921,52921,50840,50840,52952,50905,50937,50905,48792,48792,48793,48793,
48792,46745,48825,48826,44665,44600,44665,44664,44631,46712,46713,44600,42487,42520,40440,38359,38327,36279,38360,38392,36247,36247,36247,34198,
34198,32117,30102,32215,30135,28022,27990,30070,27990,27990,27957,25877,25909,23861,23861,23829,21717,19669,19669,19701,19701,19701,19701,19668,
19636,19669,19669,17620,19701,19733,17653,19701,19669,19668,19701,19700,19668,23764,25779,29906,40340,50806,54966,59094,59126,57013,54868,56981,
59095,57047,52821,52756,48529,46351,42159,35853,35854,37999,40080,35886,35854,37966,40079,40112,44339,46419,48499,46386,44240,46386,50611,50676,
33837,25419,21259,19213,17133,12940,12940,10892,8844,8876,8908,8876,8876,6828,6795,6827,6795,8876,8908,10956,8908,8908,10989,8908,
6893,9006,6957,4877,4910,2862,2862,814,2862,4910,6958,9038,11086,13134,13167,13134,11054,9038,9070,11151,11184,9103,9071,9071,
13265,11184,9038,9038,9038,9070,9070,6957,9037,9037,11151,13264,13264,13264,13264,13264,15313,15313,15313,15313,17393,17360,17360,17359,
19407,19408,21488,21521,21488,19440,19472,21520,25649,29809,31890,33970,33970,27761,25681,23601,23568,23535,25615,27663,34002,40341,31954,29874,
29907,27858,27858,27794,27826,32052,32019,29906,29874,29906,27826,25745,29906,31955,31954,34035,36115,36083,36115,36115,38228,38261,38229,36181,
36180,38261,40309,40341,42388,42356,44436,42388,44468,44468,46548,46548,46516,46548,48628,48628,50709,50741,46515,46483,46483,44435,44436,46517,
44436,42355,40275,42356,42356,38130,33872,31824,36018,36017,29679,27566,27631,27599,25486,25454,23374,23374,23342,21261,21261,21261,23309,23341,
23341,23309,21196,21163,21228,23276,21195,23275,25356,25356,25356,25324,25323,25355,25355,25388,27436,27468,29549,29549,31662,31695,31662,31662,
31695,31662,31663,33776,33808,33775,35888,40113,44339,44306,42226,46419,48564,48597,50677,50742,52855,54968,55000,55033,55033,52920,52920,52953,
55065,53017,52985,52986,50905,48792,50905,50905,50873,48793,46712,46745,46745,44664,44664,44664,44664,44664,44664,44632,42520,40439,40439,40440,
40440,38392,38359,38359,38327,36278,36278,34166,34198,32118,30038,30070,30103,30070,30038,30038,30038,30038,30038,27990,25909,23796,23796,21717,
21717,21717,21749,21782,21781,21781,21814,21814,21749,21782,21782,19701,19733,19766,19733,19701,19668,19668,19668,19668,19635,23731,29939,38261,
48694,54934,59095,59094,59061,59028,52722,52722,57046,57046,50708,52756,48497,42093,37933,37934,31693,31693,35855,31661,29580,31660,33773,29548,
29549,33775,35920,38000,35854,33773,40079,44304,31725,23372,17099,17100,17100,12939,10891,10892,8843,8876,8876,6828,6828,6828,6796,6828,
6827,8876,8908,10956,8876,8876,10989,11021,8941,8973,6925,4877,4910,4910,4943,4942,4910,6991,6990,6958,9038,11086,11119,11119,
9006,6958,6990,6990,9071,9103,9103,9071,7023,9071,9071,6990,6958,9070,9038,6957,9005,6957,9070,11183,11151,11184,11184,13232,
13265,15346,15314,15313,17426,17425,17360,19440,19440,21553,21520,19440,19408,19440,19472,19440,21488,23569,27697,27730,27729,25648,23632,21585,
21520,21487,23567,25583,31857,36083,29809,27794,27826,27858,27826,27794,27826,29907,29906,27793,27793,29841,29906,27826,27793,29874,31986,34067,
36115,36115,38195,38195,38228,38261,38261,38261,36181,36181,40342,40341,40308,42421,44469,44469,44468,44468,44468,42355,42355,44435,44467,44467,
46515,48563,46515,46483,46451,46484,44403,44403,44436,42356,38162,40275,40243,35985,33872,33840,33872,35985,31857,27630,25518,25518,25518,23438,
23374,23342,23341,23309,23309,23309,21260,23308,23308,23308,23308,21196,21196,23276,23276,23308,25355,23275,25323,25356,27436,25356,25355,25356,
25355,27436,31662,33775,31695,31662,29614,29614,31695,31727,31728,33808,33808,33807,35887,40112,46418,46419,44338,48564,52822,54870,52790,52822,
54968,55000,55000,55000,55033,55033,55033,55033,55033,55066,55066,53017,52985,50905,52985,52985,50873,50840,48793,48857,46809,44697,44664,44632,
44632,44665,44664,44600,42552,42552,40472,40472,40472,40440,38359,38359,38359,36278,34198,34198,34231,34231,32118,30038,30038,32118,30038,30005,
27957,27925,30038,27957,25845,25845,23797,23765,23797,23797,21749,23829,21749,21749,21782,21749,21749,19701,19701,19669,19701,19701,19733,21781,
21749,21717,19668,19667,21651,23698,29874,42388,50774,57014,59094,59028,56882,54735,52623,50608,56980,57045,50610,50642,52690,40012,29481,35853,
35887,29580,29581,29548,25322,27467,25420,21194,23308,27534,27534,29680,29647,25453,29645,35951,25451,23372,17067,12907,15020,12972,10891,8844,
8876,8876,6827,6828,6828,6796,6795,6795,6827,8908,8908,8908,8876,8876,8941,8973,8973,6925,4844,4845,4878,4910,4943,7023,
7023,6991,6990,6958,6957,9006,11119,11119,9006,6958,6990,6990,7023,9103,7023,7023,6990,6990,6990,6958,6990,9070,6990,6957,
6990,6957,9070,9103,11183,11216,11216,11184,13233,15314,15314,15313,15345,17425,19440,19440,21553,23633,23601,21488,19440,19408,19440,17359,
17359,19375,23536,25649,25616,23535,21520,21585,21552,21487,23535,25583,29744,29777,29809,27793,25713,27826,27826,25746,25746,25713,27793,27793,
29874,27793,29874,29939,31987,29906,31954,34035,34002,36115,38196,40308,40308,38228,38228,38261,36213,36181,40341,42421,42421,42421,42388,42420,
42388,42387,44468,44435,42387,44435,42354,44402,44369,46450,48563,48563,46451,46483,44403,44403,44436,42388,40275,42323,40211,38065,35985,35985,
35953,33904,33905,31792,25518,25486,25518,25486,21293,21229,23309,25422,23341,23309,23308,23308,23308,23308,23341,23308,23276,23276,23243,23275,
23307,25323,25356,27436,29549,27436,25355,25356,25356,27468,29581,33775,33808,31695,29614,29614,31695,31727,33808,33873,33840,35919,37999,40112,
44305,46418,48499,52757,54903,54903,52822,52855,55000,55000,52920,52952,55033,55065,55066,55034,55001,55033,55066,55065,52985,52985,52985,52985,
52921,50873,48825,48825,48857,46809,44664,42551,44600,44632,44632,44632,42552,40471,42552,42585,40472,38359,38359,38359,38391,36278,34166,36246,
34198,34231,34231,32086,30005,32119,32119,30005,27924,27892,27924,27925,25877,25845,25845,23797,23797,23797,23797,23797,21717,23797,21749,21717,
21717,19701,19669,19668,19668,19668,19701,21716,21716,21716,21748,21716,23731,25746,31986,44501,52854,59095,59093,56882,54703,52557,50477,50510,
54834,59061,50609,48496,52658,42093,29448,29514,37967,31694,27435,31694,25355,21161,23307,21260,21228,21261,23406,25519,25551,23438,23405,27565,
21226,19212,17100,10859,12972,13004,8843,8876,8908,8908,6827,6795,4747,6796,6828,6795,6827,8908,8908,8876,8908,8908,8908,8941,
6925,6892,4877,6957,4942,4878,4878,7023,7023,6991,4910,6925,6925,6925,6958,6958,6958,6990,7022,6990,6990,6990,4910,6990,
7023,6990,6958,6990,6990,7022,6958,6958,9038,4877,6990,9103,9103,11184,13297,13232,13200,15281,17394,17426,17392,17392,19440,19440,
21520,23600,23601,21488,19407,19376,19408,17359,17360,19440,21521,25649,23568,23535,21487,21552,21552,21519,23567,25583,27664,27664,27697,27761,
23633,25746,27859,25713,25681,27794,27761,25680,29874,29906,27793,27793,29874,31954,31954,34035,34035,34035,36115,38228,40308,40341,38293,38293,
38261,36180,38228,40308,42421,42388,40307,40307,40307,42387,42387,42355,42354,42386,42386,44434,46482,46482,46483,46451,44371,44403,44403,44436,
44468,44468,42355,42323,42323,40178,38065,38098,38098,33937,33904,29744,27598,25518,23438,23406,23341,21261,23341,25422,25422,25389,23340,23341,
23308,23308,25389,23308,25356,25357,23308,25356,25388,25356,25356,27437,27436,27436,27436,27468,27436,27436,27436,27501,31662,31727,31662,29582,
31695,31727,33808,35920,35920,38032,40079,42192,44305,46418,48531,50644,50709,50709,52822,52855,54968,54968,52887,55033,55033,55033,57114,57114,
55033,55033,57114,57114,55066,55033,52985,52953,50873,50873,50905,48825,46777,46777,46777,44632,44632,44632,44632,44632,42552,40472,42552,42553,
40472,40440,38359,38358,40471,38359,36278,36246,36246,36279,36312,34199,32086,32053,32086,30005,30005,25811,25812,25844,25844,25845,25813,23797,
23797,23797,23797,23797,23797,23797,23797,23798,21749,21749,19669,19668,19668,21748,23829,21716,23764,23797,23796,23796,25811,27858,36147,46581,
57047,59029,59028,54801,50444,52523,50444,48396,54768,59028,50609,44238,50544,42125,31561,25321,27435,33807,27501,29549,29549,21162,19081,21227,
21229,19148,17165,19311,21423,19343,19278,19245,17066,15051,15085,10892,10891,10924,8843,8876,8908,8908,6795,4747,4747,4748,6828,6796,
6827,6828,8876,8876,8908,8940,8908,6893,6925,6924,6957,6957,6958,4878,4878,6958,6991,6991,6958,9038,6958,6925,6957,6958,
6958,6990,7055,4942,4910,4942,4910,6990,7023,6990,6990,6990,6990,6990,4909,6958,6990,4910,4910,4942,6990,9071,11184,13265,
13200,15280,17361,19474,19505,19473,19472,21520,21488,21520,21520,19407,19375,19375,17328,17328,17392,19440,21521,23601,23568,23535,19407,19440,
21552,21520,23535,23502,25583,27664,27696,25681,23633,23633,25746,25714,25713,27793,27793,27761,29841,34100,31987,29842,29841,29842,31954,34035,
36116,36115,36115,36115,38228,40405,40373,38293,38293,38261,36180,38195,40308,40340,40308,40340,40340,42420,42387,42355,42354,42386,44467,44435,
44402,44402,46483,44403,44370,44403,44435,42355,42323,42355,42355,42356,44404,40210,38065,38098,38098,35985,33937,29743,27598,25518,23438,25486,
25519,25454,23373,23341,25389,23341,23341,25389,23308,23308,25388,23308,25356,25356,25389,27469,27469,27436,25356,27436,27436,29516,29516,27468,
27468,29549,31694,33775,31662,31694,31695,29614,29614,31694,33839,38032,38032,38031,40079,42192,44305,46418,50611,50644,48564,48628,52822,54968,
54968,54968,52920,55001,55033,55033,57082,57082,55034,55034,57082,57114,57114,57114,55066,52953,52921,50841,50873,50905,46744,46745,46777,46712,
44632,46712,44632,44600,44632,44633,42553,40472,40440,40472,38391,36278,40439,38391,36278,36278,36279,38359,36311,34166,32085,32021,32053,30005,
32086,29972,27892,25844,25812,25812,25813,23764,25813,25813,23765,25878,25878,23797,23765,23798,23797,21749,21749,19668,19668,21749,23861,23796,
25877,27957,25877,25844,29940,31987,40308,50774,57046,52722,54802,54768,50444,52458,46217,48331,54736,56947,54769,44237,48463,44205,29448,23207,
21095,25387,27533,25355,25356,21162,17001,17034,19148,19181,15020,15085,15182,15150,15150,17198,15019,12939,10924,10892,8843,8843,6763,6828,
6860,6828,4747,4747,4747,4780,4780,4747,4747,6795,6795,6828,8908,8940,6892,6892,6924,6925,6957,6924,6925,6958,6926,6958,
9006,9006,9038,11119,9071,9038,6990,6990,6958,6990,7022,4942,4877,4942,7023,6990,6990,6990,7023,6990,4910,6990,6990,4910,
6990,4942,2862,2830,4910,6991,11151,13232,15280,13167,15248,19441,19473,19473,21553,21552,19440,19439,21487,19375,19343,19376,19408,17392,
17392,17360,19440,23601,23568,23535,19375,17359,19472,21520,23568,25648,27663,27664,27696,25648,25681,25713,25713,27793,27793,27761,29841,29841,
27761,31987,31955,29841,29874,29874,31922,31954,34035,36115,36115,36115,34067,38293,38293,38293,38261,38261,36180,36179,40340,40340,38227,38227,
40340,40339,40307,40307,40306,40274,42354,44435,42322,44402,44402,42322,40242,42355,44501,42387,40275,42388,42387,44436,42355,40210,38097,35985,
36017,35985,31791,29679,27631,25518,25518,27632,27599,25486,25454,25389,25421,25421,25389,25389,25356,25388,25388,25356,25356,25356,25356,25356,
25356,27436,25356,27403,27436,29517,29516,29549,29581,31629,33807,35920,33775,33807,33807,33807,33775,31694,33774,38032,40112,40080,40079,42192,
46418,50644,50644,48531,48596,50742,52855,54968,55000,55000,55033,55033,55033,55033,57082,57114,57114,57114,57114,57114,57114,57114,57114,55001,
52953,52953,50841,50873,48792,48793,48857,46745,46712,46745,44664,42552,42552,42552,44665,42585,40472,40440,38359,36278,40439,38326,36213,36278,
38359,38359,34166,32020,29940,29940,29940,29940,29972,29972,27859,25812,25812,25845,25813,25780,25813,25813,25813,25845,25845,23765,23765,23733,
23797,23829,21749,21716,21748,23796,23764,25844,27925,27925,27925,29973,32052,36180,44469,54967,56981,50609,54801,54768,52556,52491,46217,46283,
52623,54769,56882,48430,48366,46285,29415,23142,16902,19016,25388,25420,21194,19017,14889,17002,19115,17068,15052,12972,13004,13069,15150,17166,
12939,10891,8844,8844,8843,6795,6795,6828,6860,6827,4747,4780,4780,4747,4780,4747,4779,6860,6860,6828,8940,8973,6892,6892,
6892,6924,8973,8973,9005,9005,9006,9006,9006,9006,9006,11119,11151,9071,6990,6990,6990,4910,4942,4942,4910,4910,6990,6990,
6990,7022,7023,6990,4910,4910,4942,4942,4942,4942,4910,4910,6991,9071,11119,13199,15312,15247,15279,19440,19440,19440,19473,21552,
19440,17359,19407,21488,19375,19376,19408,19440,17392,17327,21488,23601,23568,21487,21455,19440,19472,19440,23600,29874,29809,27696,25583,23535,
25713,27794,27793,27793,29841,29841,31922,29873,29874,29874,29842,29874,31954,31954,29874,31954,34035,31922,34035,36116,34035,34099,38293,38293,
36180,36180,38260,38292,38260,38260,38292,38227,38227,38227,40307,40307,40275,40307,40307,42355,42322,42322,42322,40242,40242,40275,42387,42388,
42388,42388,42355,42355,40210,38097,38097,38098,38098,33904,29711,29711,29711,27631,27631,29712,27599,25486,25486,25453,25421,25389,25389,23308,
25389,27469,25388,25388,25388,27469,27469,25356,25356,27436,27404,27404,29516,31597,29548,31596,31661,31661,33774,35855,33742,33742,35855,35887,
33807,33774,33774,37999,40112,40112,40080,42160,46418,50644,48563,44338,48596,50774,52855,54968,55000,55001,55033,57114,55066,57114,57114,57114,
57147,57147,57147,57114,57114,57115,57114,55001,55002,52921,50873,50873,48792,48793,48857,48793,46712,46745,46713,44632,42552,42520,42585,42585,
40440,38327,38359,38391,36246,36213,36246,38326,38359,38359,36214,32020,29908,31956,31988,31988,29940,29940,27892,27892,27860,25812,25780,25780,
25813,25813,25813,25813,23765,23732,23765,23797,27990,28055,25910,23797,23796,23796,23796,25844,27924,27924,29940,32020,36180,40308,48693,57047,
56981,56883,58995,54801,50477,50477,46283,46316,52656,52688,52721,52656,48397,46285,31527,23142,16869,14822,19049,23307,23243,19050,16969,14921,
19083,19083,17035,12939,12972,13005,15085,15085,12907,8811,8844,8844,8843,6795,4747,4780,4812,4812,4779,4780,2732,2732,2732,2699,
4779,6860,6860,6828,6860,8973,6924,6892,8972,6892,8940,8972,11020,11053,11053,11054,11054,11054,11054,11086,11151,11151,9038,6990,
6990,4909,4910,4909,4942,6990,4910,7023,9103,7023,4942,4942,4942,4942,4943,7023,4975,4943,4943,4910,9071,13232,13167,13134,
15279,17360,17327,17360,17360,17327,17327,17359,17359,17327,17327,19408,19375,19343,19375,17327,17360,17327,21488,23569,23536,21487,21488,21521,
21553,21520,23632,31987,31922,29777,25583,25648,27793,29841,29841,29840,29808,29808,31889,31921,29874,29874,27793,29906,32019,29906,29874,31954,
31955,31954,31954,31955,31987,34067,34132,36180,36180,38261,40374,38293,36180,36148,38293,38260,36147,36147,40340,40340,40307,42388,40307,40274,
42322,40242,40274,40275,40275,38227,40275,42420,42453,40308,42356,40243,38097,38098,40210,40243,38131,33937,33905,31857,29744,29712,27631,27631,
27631,25518,25486,25453,25421,25389,25389,25356,25388,25388,25388,27469,27501,29581,31662,29614,27501,27468,29549,29516,31597,33709,33709,31628,
31628,33709,33773,37935,35854,33773,35887,35887,33774,33774,35887,37999,38032,37999,37967,42193,46419,46418,46419,48564,50709,52855,52887,57049,
57081,55033,57113,57114,55066,55066,57114,57115,57147,59195,59195,57147,57115,57147,57114,55002,55002,52953,52921,50873,50841,48793,48793,48761,
46680,44632,44664,44664,44665,44633,42585,42585,40440,36246,36278,38358,38326,38326,38359,38359,36181,36214,36181,32020,31955,31955,31988,34068,
31988,29940,29972,29972,27892,25780,23700,25780,25780,25812,25812,25813,25845,25813,23765,25878,27958,27990,25878,23797,23829,25877,25876,27924,
27924,27924,29972,34100,36180,42388,50806,57014,56981,59093,59060,56882,48463,48430,48463,52721,54867,54835,54866,54801,50510,48365,33640,25222,
18982,14789,14823,19017,19082,19082,19114,12776,16969,19050,17002,14955,14987,15020,12940,12972,10859,8811,6763,6795,8843,6795,6796,4780,
4780,4811,2699,2699,2699,2731,2732,2699,2731,4812,4812,6860,6860,6924,8973,6892,6860,8908,8908,10988,11020,13101,13101,13134,
13134,13134,13134,13166,11118,11118,9070,9070,6990,4909,4877,4877,4910,6958,6958,7023,9136,7055,7023,4975,7023,4975,4942,4975,
4975,4975,7023,6991,9039,13264,15312,15214,15246,17327,17359,17360,17360,17327,15247,15246,17327,17359,17295,17327,17327,19375,19375,19375,
19375,19375,23536,23536,21455,21487,19439,19440,19473,19472,23600,29874,29842,29777,25583,25616,27728,29808,31888,33969,33968,31856,31888,29841,
27761,29842,27794,27793,31987,29906,27793,29906,31955,31955,32019,31955,31987,32019,32019,36180,36212,36213,38293,38261,36180,36180,36180,36180,
36180,38228,38260,40308,38227,40275,38162,38162,40242,40274,38194,40307,40372,38227,38227,40372,40340,40340,40308,38162,36017,38162,42324,42324,
38163,36018,33937,31824,31825,29744,27599,27632,29712,27567,25454,25453,27534,27502,27469,25389,25356,25388,27468,27501,29581,29614,31694,31694,
31694,29549,29581,31629,33677,33709,33741,33741,35821,35821,35821,37934,37934,35854,37935,37935,35854,37934,37999,38000,38000,37935,37968,44274,
46420,44338,44371,48629,50742,52855,55000,57081,57081,55033,57114,57146,57114,55033,55066,57147,57147,57147,57147,57115,57114,57115,57114,55034,
55034,55001,52921,52921,50873,50841,48792,46712,46712,44632,44632,44632,44665,44633,42553,42585,40472,38326,36278,38326,40439,40439,34133,34100,
34100,34068,31987,31987,34068,31988,31955,31988,31987,29907,29940,29940,27860,23699,23700,25780,25780,25812,25812,23732,25813,25813,25813,25845,
25845,25845,25878,25877,25877,25877,27957,27957,25844,27892,32052,34099,38227,44533,52886,54901,56981,59094,56948,50609,48464,50675,54868,57013,
57013,54900,56947,56914,52622,50444,37833,25222,18982,14757,12710,14823,14855,16904,19082,12776,14857,16970,14890,16970,14923,14955,12907,12940,
10891,8843,6763,6795,8843,6795,6796,4780,2731,2764,2699,651,651,651,2731,2731,2764,4812,4811,4812,6860,8973,8973,6892,
8908,8908,10956,13036,15148,15181,15181,15181,15182,15214,15214,13166,11118,9037,9070,11183,9071,6958,4909,4910,4877,6958,9039,6990,
4942,4942,7023,7023,4975,4975,4943,2862,4975,7023,6991,6990,9006,11118,15279,15246,15213,15214,17326,17360,17328,17327,15279,15278,
15279,17359,17328,17327,17295,17327,17327,19375,19375,21423,23536,25584,23535,21519,19471,19440,19473,19440,21487,23535,25583,25583,25551,25583,
25615,29807,34001,38194,36081,31855,31856,29808,27729,27761,27794,27793,27826,29906,27793,29874,29907,29906,31987,32019,31987,32019,34068,34132,
36180,36213,36180,36180,36181,36148,34100,34100,36180,36180,36115,38227,40308,38195,38162,38195,40275,40275,38227,40372,42453,40372,38260,38292,
40373,42421,40275,38162,36049,38130,40243,40211,36050,33969,31857,29744,29776,29744,29744,27632,27632,27599,27534,27534,27566,27501,27469,27469,
25389,27469,29549,29581,31662,35888,38033,33839,31726,29581,31629,33741,33709,35821,35821,35821,35853,37901,37933,37934,40014,40014,37966,37934,
35854,37934,37967,37967,37935,37967,38000,42194,44339,42291,44371,46581,52855,52919,54968,52887,52887,55001,55033,57114,55066,55033,55034,57114,
57115,57114,55066,57114,55066,57115,57115,55034,55001,55001,52953,50841,50841,50873,48793,48793,46712,44632,44632,44632,44633,44633,40472,42552,
40472,38359,36278,38326,40439,40407,34100,31987,36181,34068,31955,31988,36181,34036,31955,31955,29907,29939,29972,27860,25747,25747,25812,25812,
25780,25780,25812,25812,25812,25845,25845,25845,25845,25846,25878,25878,25877,25909,27990,27957,25811,27891,32052,36147,40340,48694,52886,52821,
56981,52787,48529,48497,50675,54935,57080,57047,54933,52820,54834,56914,52622,48331,39946,27302,18949,14789,12677,12710,14791,12678,14823,16969,
14857,14857,14857,16938,14858,12842,12875,10860,10891,8843,6795,6795,6796,6763,4747,4748,2731,2764,2731,651,651,651,683,2731,
2732,2732,4811,4811,6892,6924,6924,6892,8908,10956,15084,19179,21291,23436,23437,21324,19277,19278,17230,13134,13166,9070,9070,11183,
9103,6990,6958,6958,6957,6926,6958,6990,4910,4878,4910,4975,4943,4943,4943,4943,2895,4975,7023,6990,6990,9038,11118,13134,
13134,15214,17262,17327,15247,15247,15279,15278,15279,15279,17359,17327,15247,17327,17295,19375,21423,21423,25583,25615,25615,23599,21552,19473,
19441,19440,21488,21487,21455,23470,23470,23503,25615,31888,38227,42355,36049,29742,29743,27695,27728,25713,25714,25746,25746,25746,25746,27826,
29906,29939,29939,32019,32019,32019,34067,34100,34132,36245,38293,34100,34100,34100,34100,36180,36180,36148,34067,36147,40340,40340,40276,38227,
40308,38227,38227,40340,40372,40372,38260,38260,40405,42421,38194,36082,36082,36050,38163,38163,36018,33905,31824,29744,29744,31792,31825,31792,
29712,31760,29679,27566,27534,27502,29582,27501,27469,29549,29581,29581,31661,37968,40145,35887,31693,31629,31628,33709,35789,37869,35821,37901,
39981,42061,42061,44207,44207,42127,40046,37934,37934,37935,37967,40016,37968,37935,37967,38032,40145,44371,46516,48662,52920,55000,52920,52887,
50807,52952,55033,55065,55066,55033,55033,55001,55034,55001,52953,55066,57114,57114,57114,55034,55001,55033,55001,52889,50840,50841,50873,48761,
46680,46680,46680,46713,46713,44600,40439,40407,40439,38359,38326,36246,38326,38294,34100,34035,34068,34068,31955,34035,34068,31955,31923,29875,
29907,29940,29940,27827,25747,25779,27893,27860,25780,25780,30006,30006,25845,27893,27925,27925,27925,25845,25845,25878,25910,25909,28022,27957,
27924,27924,32052,38292,44533,50806,52853,54901,54900,46417,46450,54870,57048,55032,57081,54968,52789,52787,54800,56847,54702,46218,39881,29383,
20964,16837,12677,12645,14791,10598,10565,14856,14856,12744,14856,14857,12744,12777,12810,10826,8875,6762,6763,6796,6796,6796,6796,4812,
2732,2732,2732,651,651,683,683,684,2732,2764,4812,4811,4811,4811,6892,6860,8908,15084,19212,25420,33774,44273,44338,38097,
29679,23406,21326,17230,15279,13198,11118,11151,11183,11151,6990,6990,6990,6958,4877,4878,4910,4910,4943,7023,4975,2894,2862,2894,
4975,5007,5007,4975,4942,6990,9071,11118,13102,13102,15215,17327,15246,15279,15246,15279,15279,15246,15246,15246,15246,17327,17327,19375,
21455,21423,25583,29776,27728,25647,21519,19440,19440,19408,19408,21520,23536,21455,21422,23502,25615,31888,36114,38194,33968,29742,27662,27695,
27761,25713,25714,25714,25746,27827,25778,25778,27793,29874,29938,31986,32019,34100,32019,31987,34068,36213,38326,34132,32020,34132,34100,34100,
34132,34099,34067,36147,38260,40340,40340,38228,38195,38227,38227,38195,36147,36147,36146,36179,40340,40372,38194,36114,38162,38162,38195,38163,
33969,33937,33905,31792,31824,31857,31825,31792,31824,31824,29679,27566,27566,29582,29550,27501,27469,29549,29548,29548,31629,33773,33774,31661,
31661,33709,33709,35789,37837,37901,39949,42062,44174,46286,44206,46319,44239,44239,42159,40047,37967,37967,37968,40049,40081,40080,40048,37999,
40112,44338,46517,50743,52855,52919,52920,52920,50839,50871,52985,55065,55033,55065,55033,55001,55033,55033,55033,57114,57114,55066,55066,55001,
55001,55001,52953,52921,52921,50841,48728,48761,50841,50842,46713,48794,46713,44568,42520,42520,40439,38326,38326,38326,38326,36213,34068,34035,
34068,36084,34035,34035,34035,31955,31923,31923,29907,29908,27860,25747,23699,25779,25812,25812,27860,27860,30038,30038,27861,27925,27893,27925,
27893,25844,25845,25845,25878,25878,27990,30038,27924,29972,34132,40308,44533,50806,52853,56981,52788,48563,52822,57081,57113,52984,52920,52855,
52756,54802,54767,54733,56749,48267,39849,31431,18819,18853,14725,10564,12678,10630,8485,12710,14856,12775,14824,14856,12743,10663,10729,10794,
8843,6762,6763,6796,8844,8876,6828,6860,2732,2732,2732,651,684,716,683,683,684,2764,2764,4844,4811,4779,6860,6860,
8908,15084,21260,29581,42063,48304,48336,50515,48500,40115,29616,21358,19342,15246,13199,11151,13264,13232,9070,6990,6990,6958,6958,6958,
4878,4910,4910,6991,9136,7056,4975,4943,4975,7056,7055,4975,4942,4910,9038,11119,11086,11054,13102,15214,15279,15279,13166,13166,
15279,15279,15246,13166,15246,17327,17327,19375,21455,23471,25583,34002,31954,23534,21487,21521,19441,19408,19440,21520,21488,21455,21422,21454,
25615,27695,29775,29807,31855,29776,27663,27696,25713,25714,25714,25747,23666,25746,25778,25778,25745,27794,29906,29906,32019,34100,34100,32020,
32020,34132,34165,34133,36213,34164,34132,34132,36180,34099,34067,34067,34067,36147,38228,40308,38195,36115,38227,36114,36114,36114,36146,38227,
40340,40307,38227,40308,40275,38162,38163,36050,33937,33937,33937,33938,33905,33873,31824,33873,33938,31792,31727,29647,27566,29614,29582,29582,
29549,29549,29581,29548,31661,33806,35854,33741,31628,31596,35789,37869,37869,37869,39981,42094,46287,50512,48432,48400,46319,44271,44239,40079,
40079,40080,40048,42129,40080,40080,40080,40079,40112,42258,48597,52823,52855,52920,52919,50839,50807,50839,52952,52985,55033,55065,55066,55033,
55066,55066,57114,57114,55033,55001,55033,55033,52953,52953,52920,52920,50873,48760,48728,46648,48761,50841,46681,48729,46680,42487,42520,42520,
40439,38326,38326,38326,38294,36180,34068,34035,34035,34035,34035,34035,31954,31922,31955,31955,29875,27827,29908,25779,25747,25747,25747,25812,
27860,27860,27893,27893,27893,27925,27893,27893,25812,25812,25845,25845,25878,25910,25877,27957,27924,29972,34132,40307,46581,50806,52853,54934,
52821,54934,57113,57114,55065,52887,50742,50709,52723,54833,54766,54668,56685,48267,37703,33544,23012,18852,14692,8451,10597,10630,8485,10598,
14856,12743,12743,14824,12744,10696,10728,10729,8811,6795,6763,6763,8876,8909,8941,6860,4780,2764,2732,2732,684,716,683,683,
683,684,2764,4844,4812,4812,6860,6860,8876,13003,19179,29581,41867,39656,37543,39690,46127,52532,46357,31728,23406,19310,15247,13199,
13199,11183,11151,9071,6991,6958,9104,11217,6959,4878,6959,4910,7023,7056,7088,7056,7023,7055,4975,7055,4975,4910,4910,6958,
9006,11086,11022,13134,15247,15279,15246,13198,15311,17392,17359,15246,15246,17326,17327,17327,19375,23470,25582,31889,31954,23534,21487,21521,
19440,19408,19408,21488,19375,19375,21455,21487,23535,23534,25582,25582,27663,27663,25615,25648,25682,25714,23666,25746,25746,23666,23665,23665,
23665,25745,27826,27858,29907,32020,34100,34133,34132,34132,34133,32052,34133,36213,36245,36213,36180,34099,34067,34034,34034,36115,38228,40341,
40276,36050,38163,38228,38227,38227,38227,38227,38227,38227,40308,40308,38163,38162,40243,40211,36050,33937,35986,36018,33905,33905,38098,36018,
36018,33905,33873,31760,29647,31727,31695,31663,29549,29549,29581,29581,31661,33774,33774,33741,31628,31596,35756,37836,37869,37868,42029,44207,
50545,54739,54771,50578,46384,46352,44272,42159,42160,42128,40048,40048,40048,40015,40047,40080,42192,44306,48596,50742,52855,52920,50807,50774,
50807,50871,50872,52952,55033,55065,55066,55066,55066,55034,55066,55033,52953,52953,55033,55033,52953,52953,52952,50840,50808,48760,48760,48728,
46680,46648,46648,46648,44568,44535,42487,42487,42487,40407,40407,40374,38261,34067,34035,34035,36116,34067,34035,34003,31922,31922,31922,31955,
29907,27827,27827,25779,25747,23699,25747,27860,25780,25780,27860,27893,27925,27925,27925,27893,25813,25813,25845,25845,25845,27958,27925,27925,
29972,32085,36180,40307,46613,50773,50773,54934,54967,57113,57114,57114,55033,50806,48596,48563,48529,52687,54733,54635,56653,50315,37671,37705,
29286,20965,14660,8419,10565,8549,6372,8517,12744,12743,12711,14824,12744,12744,10696,8648,8810,6795,6795,8844,8876,8909,8909,6860,
4812,2700,651,651,651,683,651,683,683,684,2764,2764,4812,6860,8941,8909,10956,12971,17099,29549,39754,39526,37413,37381,
35398,39723,48306,44276,33808,23405,19310,15215,15215,13199,13232,11152,9071,6991,7023,9072,9071,6991,6991,4910,4910,6991,7056,9136,
9136,7056,4975,4943,4910,4910,4910,4910,6925,6958,9006,11086,15247,17392,17392,15311,15279,15279,15247,15246,15246,15246,17327,17294,
19342,21390,25518,27695,27695,23534,21455,21520,19440,19440,19408,19407,17327,19375,19439,21520,23567,23535,23535,23535,25616,25583,23536,23568,
23601,23634,23634,23666,25779,23698,23698,23698,23665,23665,25746,27826,27826,29939,32020,32052,32052,32052,34132,32052,32052,32052,34100,34100,
34067,34099,34099,34034,34034,36115,36115,40308,42421,38163,36115,40308,38260,38227,38195,36147,38227,38227,38195,38162,38162,38163,40243,38163,
36018,33905,35986,33938,33905,36018,40244,38099,33905,36018,35986,31760,31760,33841,31727,29614,27501,29549,29614,29613,31661,31661,31628,31660,
31628,31595,35756,37836,39949,42029,44142,48400,54804,59030,59030,54804,48465,46352,44272,44207,42127,42095,40015,40015,40015,40015,37967,40079,
42225,44370,48532,52790,52823,50774,50742,50807,50839,50839,50872,52952,52952,55033,55066,55034,55033,55033,55034,55033,52953,52953,52953,52953,
52953,52953,52920,50840,50840,50840,48760,48760,48760,46647,48728,46616,44503,44535,44567,44567,42487,42454,42487,40374,38293,36180,34067,36148,
38261,36148,34035,31954,31954,31954,31954,31987,32020,29907,27827,27828,25747,25780,27860,27860,25779,25780,25780,27860,27893,27893,27860,27860,
27893,27925,27926,25845,25845,30071,30070,30037,32052,34133,36179,42355,48660,48660,50773,54967,57113,59194,57114,57114,54968,50742,48563,46449,
46350,50509,54669,54636,56685,52396,39784,39818,33512,23046,16740,8419,8484,8517,4324,6404,10663,12711,12711,12744,12711,12744,12777,10697,
6730,6763,6796,6828,8876,8909,6828,4780,4780,2699,651,651,651,651,651,684,683,683,2764,2764,4780,6860,8941,8941,
8908,12971,14986,25387,39787,39590,37413,37348,33220,31206,33450,42064,46355,35920,23438,19310,17262,15247,15280,13232,11152,9104,7023,6991,
7023,9103,6991,4910,4910,6991,7023,7023,9136,9136,7024,4943,4910,4943,4943,6990,4878,4845,6926,9006,11086,15279,17360,17359,
15246,13198,15246,15247,13166,15214,15278,17327,19375,21390,23470,25582,23469,23502,21455,19408,19440,19408,17360,17392,17359,19440,19472,21520,
21520,21455,19374,21455,23536,23536,21488,21488,21489,23602,23634,23634,23666,21586,23698,23730,23666,23666,25746,25746,25746,27826,29907,29939,
29939,32020,32019,32052,34132,32052,31987,32019,34067,34067,34067,34034,34002,34002,34034,40276,42389,38196,36082,36115,38195,38195,38195,38227,
38260,38195,38163,38162,38163,38163,38130,38131,36050,33873,33873,33873,33873,33905,38099,36018,36018,42324,38099,33840,35953,35953,31727,29614,
29582,29549,29581,29581,29548,29548,29580,31628,31628,31595,33643,37836,39949,42062,44174,48367,54771,59063,59030,52724,48498,46384,44239,42126,
42095,40014,39982,37935,40015,40015,40079,42192,44305,44370,46451,50709,52855,50774,50742,50807,52919,50807,50840,52952,52920,52920,55033,55033,
55001,52953,55033,55033,55001,55001,52953,50840,52952,52952,50872,50872,50872,50840,48760,48760,48728,46615,46615,44470,42389,44502,44535,44503,
42455,42454,42487,40374,40309,40309,38196,36148,38228,36115,34003,31922,31954,34035,34035,31987,31955,29875,29908,27860,27860,29941,25780,25779,
27860,27828,25779,25747,25779,27860,27860,27860,27925,30006,27926,25845,27958,30071,30038,32085,32052,34132,38227,42386,48661,50774,52919,57081,
59161,59162,57114,55001,50742,48628,48530,46351,44237,46251,50444,56685,56653,52396,35526,37672,39851,27239,18821,10499,8452,8517,4323,4356,
10630,12711,12711,12711,12679,12744,14857,12809,6762,4715,6795,6796,6796,6861,4748,2699,2732,651,683,684,683,683,683,716,
716,683,2764,4813,4812,6860,6892,6893,8908,10956,12938,19179,33643,39721,37445,37348,35268,31172,29190,31433,42031,48435,35953,23405,
21357,19342,17328,15312,13264,11184,9103,7023,6991,7023,9071,6990,4910,4910,4910,6991,9103,9104,9104,6991,4943,7023,7023,7023,
6990,4878,6926,8973,8973,11086,15279,15279,15279,15246,13166,15247,15247,15246,15246,15279,17327,19310,21390,25550,23470,21422,21455,19375,
19440,19408,17360,17392,17392,19472,21553,19473,19440,19439,19407,19407,21488,21488,21488,21488,21521,21553,21554,21554,23666,21586,21553,23698,
21618,21585,23666,25746,25714,27794,27827,27827,29907,29939,32020,29939,32052,32020,29906,34067,31987,29874,31954,34034,34035,34002,36082,38228,
36115,36083,36050,33969,36082,38227,38195,38227,40308,38195,38195,38195,38163,36082,36050,38131,36051,33938,33938,36018,35986,36018,38163,38131,
38131,40244,38099,36018,38099,35953,31727,29614,29614,29614,29614,29581,29581,29581,29580,31628,31628,33708,33708,35723,37868,42061,42061,44174,
50546,54772,52691,48465,48433,48465,44272,42094,40046,42127,40047,40048,40080,40047,40079,42192,44305,44338,46418,48596,50742,50742,50742,50774,
52887,50807,50839,52920,50840,50839,52920,52921,52953,52953,55001,55001,55033,55066,55033,52920,52920,52953,50840,50840,50808,48727,48695,50808,
48760,46582,44470,42357,42357,44470,44502,44502,44535,46615,44535,42421,40308,38195,36115,36115,36148,34067,34035,34003,31954,31954,31954,31954,
29907,27827,27827,29940,29940,27828,25715,27827,29940,27860,27892,27860,25747,25812,27860,27893,27893,27893,25813,25813,27925,30006,30005,29972,
32019,34099,38227,44435,48693,52920,57081,57113,57113,59161,59161,54903,48596,46450,44304,42126,39979,39914,44074,52493,56654,52428,37671,33446,
41931,35691,20966,12612,10532,8484,4323,4356,10631,12744,10631,10631,12711,12711,12777,12777,4714,4714,4747,4715,4748,4780,2699,651,
684,683,683,683,684,716,683,683,684,2732,4812,4813,4812,4812,6860,6860,6893,8940,10955,15051,25386,35691,37576,35333,
35268,33253,31238,31303,33481,41966,44274,38000,25453,21357,17295,15279,13232,11216,9136,7023,7023,7023,7023,7023,6991,4910,4910,4910,
6990,7023,9136,6991,4910,6990,7023,9103,9071,6958,9039,9039,9006,11054,11085,13166,15278,17359,13166,13166,15279,13230,13198,13198,
15214,19342,21358,23502,23535,21423,19375,19408,19440,17359,17392,17424,17424,17425,19537,19505,19440,17392,19440,19440,21521,21488,21489,19441,
19441,21553,21554,21554,21586,21586,21553,21585,21586,21553,23666,25746,25714,25714,25746,25746,27826,29939,32020,29939,29939,29907,29906,31987,
31954,29841,29841,31954,34067,34002,34002,33969,33937,36050,36050,34002,36082,38195,38195,36114,38195,38195,38163,38195,36082,33969,36082,36083,
36050,36051,38164,38164,38163,38163,42357,42357,38131,36018,36050,38099,35986,33841,31695,29614,31695,31694,31662,29581,29581,29581,29581,29548,
29547,31628,35756,35788,37869,39981,39948,42093,46352,48465,48465,46352,44238,44239,44239,40046,40014,42127,42128,40048,40048,40080,40112,42192,
46418,44370,44370,46484,48564,48629,50742,48694,48694,50839,50807,50840,50840,50840,52920,50808,50808,52920,52953,55001,55001,55034,55001,52920,
52920,52920,50840,50808,50808,48727,48695,48695,48695,46582,44437,44405,42357,44437,44470,44534,46615,46615,42422,40309,42357,38163,36115,38195,
36115,34002,34002,34035,31954,31922,29841,29842,29874,29875,27827,32021,27860,27795,27827,27827,27892,27860,27860,27892,27860,25780,27860,27925,
27893,25812,25812,25845,27893,30005,32053,32052,34099,36179,42420,48693,52919,55032,57081,57113,59161,59161,59128,52789,48498,44304,42158,39981,
37835,37802,39850,44043,52461,54541,44010,31365,35657,41997,25191,12612,10564,8484,6404,4324,8550,10663,10631,10598,12711,12711,10696,10728,
4682,2666,2699,2667,4715,4748,2667,619,684,716,683,683,716,716,683,651,619,2699,4780,4780,4780,6827,4812,4812,
6860,6892,11020,13035,17098,27402,35659,37512,35366,35334,33286,31270,31238,33384,39885,46354,40145,25518,19310,15279,13232,11184,11184,9104,
9104,7055,6991,7023,7055,7023,4942,4910,4878,4878,9071,9071,6958,6958,6958,9039,9071,9039,9038,11119,11119,11086,11021,11053,
13165,15278,15279,15279,15279,13198,13198,15279,15279,19342,21390,21390,23503,19374,17294,19375,19440,19440,17392,17424,17457,17457,17457,19505,
17425,17360,17360,19440,19440,19441,19440,19441,19440,19440,21554,23634,23602,21586,21554,19473,21553,21553,21554,23634,23634,23666,25746,25746,
29907,29972,29939,32020,29939,29939,32019,29906,29841,29841,29809,31922,34035,34034,31922,31889,33970,33969,33970,34002,36083,38195,38195,38195,
36147,36115,36147,38195,36115,36082,36083,36083,36051,36083,36083,36051,38196,40276,40309,42357,38163,36018,36018,36018,33937,31792,31727,31695,
31695,31695,29582,29581,29549,27468,27468,27467,29515,29515,31595,35756,37869,39948,37867,37868,42061,42126,46319,46319,44206,42126,40013,40014,
37934,40047,42128,40080,37999,37999,40112,42192,42257,44370,46451,44371,44371,48596,50742,50775,48661,50807,50807,48727,50840,52920,50840,50840,
50840,52888,52920,52921,52921,52953,50808,50775,50807,50807,50775,48694,50775,50808,50775,50743,48630,46550,46517,44405,42324,42324,44437,44502,
46615,44534,42389,40244,42324,40275,38195,38196,36115,34035,31954,34035,34035,31954,29874,29809,27794,29907,29907,27827,27859,27827,29908,29972,
29940,29940,27860,27860,27860,27892,29973,32086,29973,27893,27893,27892,27924,30005,32085,34132,36179,38259,44533,50839,52952,55033,57113,57114,
59161,59096,54869,50578,46351,42158,39981,37868,35723,37771,39819,37705,44042,52461,48236,37704,31398,37771,31498,14692,10499,8485,6404,4324,
6437,10663,10631,8550,10631,10631,8615,10728,4714,2634,2667,2699,2667,2667,2700,651,683,716,716,716,716,716,684,651,
2699,2699,2699,4747,4779,6860,6892,4844,4812,6892,8940,11020,15115,19146,29450,39723,39592,35366,33285,31205,31205,33286,33416,39885,
46354,38098,25583,19342,15312,13232,9136,7087,7055,7088,7055,7022,7055,7023,7023,4942,4910,4910,6990,9071,9071,9039,6958,6958,
9006,9038,9039,11119,9038,11118,11118,11053,11053,13166,15246,15247,15312,13231,13199,15312,15279,17294,19374,21422,19342,19342,17262,17262,
19407,17359,15279,15311,15344,17425,17425,17425,17392,17392,17392,17360,17360,19440,19440,17360,17392,19440,19473,21554,23634,23634,21586,21553,
21553,19473,21521,23634,23602,23633,25714,25746,29972,29940,27794,29907,29939,29907,29907,29874,27793,29809,29841,29841,31922,34002,33970,33970,
36082,33969,33970,33970,36083,36115,36115,36115,36115,36115,36115,36115,36115,36115,34002,33970,36051,38164,36083,36083,40309,42422,40276,40276,
38163,36018,33938,33938,33905,33840,31760,31727,29646,31694,29614,29581,29549,27468,29581,29548,29547,29515,31595,33643,35788,37901,37868,39981,
42126,42093,42093,42158,44207,42126,40013,37933,37966,40047,42161,40113,38000,37967,40112,40112,37999,42225,46451,44371,44371,46516,48629,50742,
48661,48694,50807,48726,48726,50807,50807,50808,52920,52920,52888,50807,52888,52920,50775,48695,50775,50807,50775,48662,48662,50775,50775,50743,
48662,48630,46517,46485,44437,44437,44469,44469,44502,44502,42389,40243,42356,42356,38195,36115,36115,36115,34035,34002,31954,31954,31955,29842,
27794,29875,29940,29940,29908,27827,27859,29940,29973,29973,29973,25747,25779,29973,30005,32086,30006,27925,27925,27924,29972,32052,34132,36212,
40372,44533,48759,52952,55033,55065,57114,59161,57015,54837,50610,46383,44238,42093,37867,35691,35691,35659,37707,35626,35624,48268,50381,41963,
33544,31497,33643,21031,12580,10533,6404,4292,4324,8582,10663,8550,8550,8518,8582,10761,4714,4747,2699,2667,619,651,652,652,
651,716,716,716,684,716,716,2732,2700,2667,2699,2699,4779,4812,6893,6925,6892,6860,6892,8940,11020,13003,19114,31499,
37707,35496,33349,33285,33252,33253,31271,33449,37805,44274,40178,27631,19342,15279,11248,7120,7087,7087,7087,7055,7023,7023,6991,6991,
4942,6990,6958,9038,11119,9071,9038,9038,6958,9039,9071,11119,9006,11086,13198,11118,11085,13166,13198,13166,15279,15279,13231,15311,
15246,17294,19375,19374,19341,19374,17261,15214,19407,17327,13166,15279,15311,17424,17425,15377,15345,17425,17393,15312,17360,17392,17392,15312,
15312,17425,17425,19505,21586,21618,21553,19473,21554,19473,19441,21554,23666,23633,23601,25714,27859,27859,25714,27794,27826,27794,27794,29874,
29842,27761,29842,31922,31890,33970,33970,33970,36082,36050,36050,36050,36050,36083,38163,36115,36083,36115,36115,36116,36116,36083,34035,34002,
33970,34035,36115,36115,38196,40277,38196,38164,38131,38131,36051,33970,33905,33905,33873,31760,31759,31727,31727,31662,29613,29613,31694,31694,
31661,31628,33708,33708,35756,37869,37901,37934,42127,40046,40013,40046,42126,42126,40046,40046,40046,40046,37999,38000,35919,35919,38032,38032,
38032,38032,42290,44371,44436,48629,48662,46549,46581,46613,48662,48727,48694,48694,50807,50807,48727,52920,52953,50807,50807,50807,50743,48662,
48694,50775,52855,50742,48662,50743,50743,50710,48662,50678,48565,48565,48597,48597,46517,46517,44437,44404,42356,40275,42356,42388,40276,38228,
38196,36115,34003,34003,31955,31955,31923,29874,29842,29874,31988,29940,29907,29907,27827,27827,27860,29940,32085,27860,25780,27925,27893,27925,
27957,27925,30005,30005,32052,34132,38260,44566,48760,52953,53018,55066,57114,57113,59128,56982,50675,46417,42223,40110,40045,37868,33642,33610,
35659,35594,35594,35626,33481,37771,48302,44076,37738,27303,25191,21031,14758,10533,8452,4292,4292,6469,8583,8550,8518,6438,8518,10695,
4714,4747,2699,618,651,651,619,652,651,2764,2765,716,716,716,716,2732,2700,2667,2699,2731,4812,4812,4812,6893,
6893,6860,6860,6860,8940,10956,14986,21129,29451,37739,35528,33349,35365,33285,31238,31271,31400,35691,44241,44339,29711,17294,15344,11280,
9200,7087,7055,7088,7055,7023,4942,4942,4910,6958,6958,6925,9039,11151,9071,9071,9071,9071,11151,11151,11118,9005,9005,11118,
13199,13199,13166,13167,15247,15279,13199,15279,15246,13133,17294,17294,19342,19374,19374,17294,19439,19439,15214,15214,15247,17392,17425,15377,
17425,19505,17425,15312,15280,15312,17392,15344,15312,13264,15344,17457,19538,21586,19506,19473,19505,19473,17392,17392,21585,23666,21553,23633,
25746,25714,25681,25681,25681,25681,27762,27794,29842,27761,29842,31922,31922,31921,31889,31889,33969,36050,38131,36050,36018,38131,40277,36115,
36083,34035,34036,36149,38229,36116,36115,36115,34035,34002,34035,34035,36083,36116,38228,38229,38196,38164,38131,36050,35986,35986,33905,33840,
33840,31759,31759,31727,31726,31726,31694,31726,31693,33773,33741,35789,35756,35789,37901,37902,37934,40014,40014,40014,40046,42127,42127,40046,
37966,37998,37999,40080,38032,35919,35919,38032,40178,38064,38064,40177,42291,46549,48630,44468,46516,46581,46549,46614,48694,48727,48727,48694,
48694,52888,52920,50807,50775,52855,50742,50710,48661,48661,50742,50742,50742,50742,50742,48662,48629,50677,48564,48564,48597,48565,46517,46517,
42323,40211,42291,42323,42356,42388,42421,42389,40309,36115,34003,34003,31955,31987,31955,29874,29842,29875,29907,29940,29907,29907,27827,27827,
27827,29908,29940,29973,27892,27892,27925,27925,27925,27925,30006,32085,34132,38260,44566,50905,55066,57147,57147,57147,57146,57048,54869,52691,
46384,42158,37965,33772,33739,33643,31530,33610,35627,33482,33514,33546,31433,29352,35658,39851,37803,23078,14692,14725,14791,12678,8452,6372,
4259,6437,8550,8550,8518,8485,6437,8583,4714,4714,2699,619,651,651,651,651,652,684,717,716,716,716,716,683,
2700,4780,2732,2699,2731,4780,4812,6860,6925,6893,6860,6860,8940,8908,12971,17034,19081,29483,35756,35593,35430,35397,33318,31270,
29254,29319,33610,44241,44372,31890,21423,15311,13296,9168,7087,7087,7088,7055,4942,4878,4877,6958,9006,6926,6926,9039,11119,9071,
9038,9038,11151,11151,11119,9038,6893,9005,13199,13199,13166,15214,15247,15279,13199,15279,15246,13165,15214,17294,17294,19374,17294,17327,
17359,17359,17359,15246,15214,15279,15312,15312,15344,17425,17425,15312,15280,15280,15312,15344,15312,13231,15312,17457,19505,17425,19505,21586,
21554,19473,17392,17392,19472,21585,23634,23633,23633,23601,23601,25649,27730,25649,25681,27729,27761,27761,29809,29809,31889,31889,31889,31889,
31889,34002,36083,36050,33970,36051,38196,36083,34002,34003,31923,34003,36116,34035,36116,38228,40341,36180,34035,36115,36116,38229,40342,40309,
38229,38196,38163,36083,36050,36018,35985,33840,31760,31759,35920,35953,35920,35888,33839,33806,33807,35887,33773,33741,33741,35789,35821,35822,
35821,37901,37934,37934,39982,40014,37934,37934,35853,35886,37967,35887,35919,35919,33838,33838,35952,38097,38064,38064,40178,42323,44436,44468,
44436,46549,46549,46549,48662,50743,48662,46516,48629,50742,48662,48662,50742,50742,48662,50710,50742,48629,48629,50710,50710,50710,50677,50677,
48629,48596,48532,48532,48532,46484,48565,46549,42323,40210,42324,44469,44469,42356,40276,40276,38196,34003,34003,31954,29842,31955,31987,29842,
29907,29907,29875,29940,29972,29940,29907,29908,31988,32021,29940,29940,32053,29941,27925,30038,32086,30006,32054,32085,34132,40341,48760,55066,
57147,57146,57147,57114,57049,52822,50611,46385,42126,39981,37901,31595,31594,31595,31563,33611,31498,31433,31433,31433,29353,25159,27206,27271,
29352,23078,16773,12645,14758,14759,10565,8485,4259,6404,8550,6470,6437,6405,6405,6502,4714,4715,4780,2731,619,619,651,652,
652,652,684,716,716,716,684,651,2699,4812,4780,2699,2699,4780,4780,4812,6925,6925,6860,6892,8973,11021,10956,13004,
12970,17034,25387,35724,35593,35463,35430,33349,31269,29221,29287,31498,42129,46518,38099,23536,15311,11216,9135,7087,5039,7088,4975,4910,
6958,9038,9071,9071,9006,9038,11119,9039,9071,9038,9006,11119,9038,11151,11086,9005,11086,13199,15247,15246,15279,13231,13198,15279,
13198,15214,15246,15213,15181,19374,17294,15246,15279,15279,17327,17360,15247,15247,15279,15280,15312,15344,17425,15344,15312,15312,15344,15344,
15344,13263,13231,15344,19538,17425,17392,21586,21586,19473,19505,21553,21521,21553,25746,25714,25682,25649,23569,25617,27698,27730,27729,25648,
27697,27761,29809,29841,31889,31857,31857,31889,31889,34002,36051,34003,33970,34002,34035,34035,31955,31955,31955,31955,34036,34036,34068,36181,
40374,38261,34068,36181,38229,38261,38261,36180,38228,38228,38196,36083,36050,36018,35986,33872,31792,33872,38033,40146,40179,40114,35920,33839,
35887,35919,33774,33741,33773,35854,35854,33741,33773,35821,35821,37902,37902,37934,37902,37934,37934,35854,35886,33806,33806,35887,35919,33839,
35919,38097,40177,40145,42291,44436,44436,44404,44436,46484,46517,46517,46484,46517,46484,46484,46516,46548,48597,48661,50742,50742,50742,50709,
48629,48629,50709,50742,48597,48597,50677,48596,48629,50677,48564,48499,48532,48532,46516,46517,44436,42323,46517,48662,44502,40275,36082,36115,
36083,34002,31922,31955,31955,29842,29907,31988,29875,27794,29907,29908,29940,29972,29907,29908,31988,32021,32053,32053,34134,32054,29941,29941,
30006,32086,32054,34101,36180,42486,50873,55066,57146,57113,57081,57049,52823,50612,46386,44240,39982,37869,33708,29483,29515,29547,31595,31595,
31498,29386,29418,27273,27272,23079,23078,25191,20966,16805,16838,14758,12710,14791,12646,8485,6372,4356,6469,6469,6405,4324,4324,4389,
4715,4747,2699,4812,2732,651,651,652,652,652,684,684,684,716,2764,651,619,2699,2699,2699,4779,4812,4812,4779,
6860,6893,6893,6893,8973,8973,8941,8973,10988,10955,14985,27435,37837,37706,35462,33316,33316,31269,29221,29254,31466,42096,50678,40309,
21520,15279,13263,9167,5039,5039,7055,7023,9071,6958,9039,9071,9071,9071,9071,11151,11119,9006,8974,9039,9039,9071,11151,9038,
9005,11086,13166,15279,17327,13198,11118,15279,13166,13166,15247,15214,15182,17262,17295,15278,13198,15247,15247,17327,17327,15279,17360,17360,
17360,15344,17393,15344,15280,15280,15344,15344,15344,15344,13231,13231,17457,19538,19505,19505,21585,21553,21553,21586,21553,23633,27859,27827,
25714,27794,27762,25617,25585,27730,29842,25648,25616,27697,29809,29809,31889,31857,29776,29776,34035,38261,34067,34035,34003,34003,34068,31955,
31955,31923,31923,31955,31955,31988,31988,34068,34133,34100,34100,36181,36213,36149,34068,34068,36148,38229,38228,38163,36083,36050,35985,33905,
35985,38066,40178,42291,42291,38033,35920,33840,31694,31726,35919,35887,33741,33774,33774,33741,33741,35822,35822,35789,35822,37902,35789,37934,
37999,37999,38032,35919,35887,35887,35919,35952,35984,35952,38064,38097,42323,44436,44436,44436,44404,44371,44404,44404,44371,44372,46452,46484,
48564,48629,48629,50709,50742,50709,48629,44403,44403,48596,50677,52790,48596,48532,48596,50677,50709,52757,50612,48531,50612,50644,48596,48597,
46517,44404,44436,46582,44502,40276,38195,36083,34002,34002,31922,34068,34068,31923,29842,29907,29875,27794,29875,29907,27827,32020,32053,29875,
31988,34069,32021,34134,32021,34134,32086,29941,29973,32054,34133,34101,38293,46647,52953,57113,57113,57080,54903,50710,48564,46418,44273,44240,
40047,35854,31596,29483,27467,29580,31628,29515,29418,27337,27305,27273,27273,23079,20998,25192,20999,12580,10500,10565,12710,14791,10598,8453,
8485,4356,6469,6470,6405,6405,4292,4324,2634,2634,2634,2699,2699,587,651,652,652,2764,716,684,684,684,2797,2764,
619,619,2699,4780,4812,4812,4779,4747,4780,6860,6860,6860,6861,8941,6893,6861,6893,6860,10955,17066,27435,39917,37640,33349,
35397,33317,31236,29189,29255,31498,42128,50711,40342,23600,15278,11215,9167,7087,5007,7055,9071,6990,6991,6990,6990,9071,9038,9039,
11119,9038,6925,6926,9006,9039,11151,9070,9038,8973,11021,15246,17359,13230,11117,13198,13198,13166,15279,17327,15214,15214,15214,13198,
13198,15247,15247,15279,17360,17360,17392,17360,17392,17393,15344,17393,15344,15312,15312,15312,15312,15312,15312,13199,15311,19505,19505,17392,
19440,21553,21553,21553,21553,21553,23666,23634,23601,25650,23569,23537,23504,25617,29842,25649,25584,25648,27696,27729,29809,29809,29777,29809,
31922,36148,34068,31987,31987,31955,31987,31987,31955,29875,29908,31956,31956,31988,31988,31988,32020,32020,34068,34101,34133,34101,34100,32020,
34067,34068,36115,38163,38163,38131,36018,36018,38098,40211,40243,40211,38098,35985,40178,40178,33872,31726,38033,35952,31693,31661,31661,33741,
35887,37967,35854,33741,35822,37935,35822,33774,35855,37967,37967,35887,33774,33774,33806,38032,38064,33871,35984,38097,40242,40210,40178,44404,
46517,42291,42259,42259,42226,46420,46452,46451,48532,48597,50677,50742,50709,50677,46516,44370,46451,48531,48564,52757,50677,48564,50644,52725,
52789,52757,50644,50579,50644,52757,50677,50677,48597,46516,44469,44469,42388,42356,42389,38163,34002,34003,34003,34035,34035,31923,29874,29875,
31987,31955,29875,29907,29875,31988,34101,31956,31956,34069,31988,32021,29940,32053,32086,29973,29973,32053,36182,36213,40341,46647,52952,57113,
57047,54869,48595,44370,42225,37999,37999,37999,35919,31693,29580,29548,27467,27467,29548,27370,25225,23144,25192,27305,27338,25192,18886,20999,
18919,10532,6339,8452,8485,10565,10598,8485,8485,6437,6469,6469,6405,6404,4292,2244,4715,2635,2667,2667,587,587,2700,2700,
652,684,2764,684,652,684,2764,2797,651,619,2699,4844,4812,6892,4812,4779,4779,4779,4780,4812,6860,6861,6861,4781,
4813,6892,8908,10922,14986,27435,37804,35560,33350,35365,33284,31236,29221,29287,31465,37935,48630,40341,21487,15279,11248,7119,7055,4974,
7022,9103,7023,4910,6958,6990,9038,9071,9071,11118,9038,8974,6926,9071,11184,11119,9038,9005,6924,9005,13198,13231,11118,9037,
13198,13198,13166,15279,15247,15247,15214,13134,13199,15247,15215,15247,17328,17328,17328,17360,17360,17393,17392,15312,15344,17393,15312,15280,
15312,17360,15312,15279,15247,17392,19473,19472,19440,21520,21520,21520,23569,21488,21488,21520,21521,23601,21488,21456,21424,23472,25617,23536,
25649,27729,27697,27729,29810,29810,29777,29810,29842,31955,31988,29907,29907,29874,29874,31955,31988,29907,29907,29908,29940,31988,31988,32021,
34101,32021,32020,32021,34101,34133,34133,34100,32020,31987,31954,36083,38163,38131,40179,40211,40243,42324,40211,38130,38130,38131,38130,38098,
35985,33840,31791,31759,31694,31661,33774,35887,35887,35887,35854,35822,33742,35822,37935,35854,33774,35887,33807,33774,33806,33839,35952,35952,
35984,35952,35984,36016,38097,38098,38098,40178,42324,42292,42259,42227,42259,42259,44339,46419,46419,46484,50677,50709,48597,50677,50710,48596,
48532,46451,48564,48564,48563,48499,50644,52724,52724,52724,52660,52692,54837,54870,52757,50709,50709,48630,46517,44436,42324,40243,38130,38130,
36083,36083,36083,34003,31890,31890,31890,31955,34036,31988,31955,31955,29874,31987,34101,34069,31956,31956,32020,31988,29940,29941,29941,29973,
32053,32053,36181,38261,38260,46614,55032,59128,54869,48498,44272,38031,35918,33806,33774,31693,29613,27500,27500,27500,27436,25355,25355,25290,
23177,25225,25225,25225,25160,23112,18886,18919,16838,10532,8420,8420,6372,10565,10598,8452,6372,6436,6469,6469,6437,6404,4259,2211,
4715,2635,2667,587,651,652,2700,2700,651,652,684,684,651,684,684,4845,2797,651,2699,4812,4812,4812,6892,4812,
4779,4747,4780,6828,6828,6828,6828,4780,4812,4812,6892,8908,10923,14986,27435,35691,35528,35397,35364,33316,31268,29254,29255,29353,
40015,48662,38228,19439,13231,11248,9135,5006,4974,7023,4975,4942,6990,6990,6958,6990,9070,11119,11119,9071,6926,6958,11151,9070,
9038,9037,6957,6925,8973,11150,11151,11086,13198,13231,13166,13198,15247,15246,13166,13166,13166,15247,15247,15247,17360,17360,15279,15312,
15312,15312,17393,15280,15279,15312,15280,15312,17393,19441,17392,17392,17360,15279,17360,19440,19407,21520,23568,21455,23536,23569,21520,23601,
23601,25714,23602,23537,23504,21424,21424,23504,25682,27762,25649,25649,27730,27762,27730,27729,29875,29907,29875,29875,27827,27794,27794,29907,
29908,29875,29908,32020,32053,32021,29940,32085,34134,32021,29972,32053,32053,32085,34165,32085,32084,32019,34003,36115,38196,40211,40178,38098,
40211,40243,38130,36050,36050,38163,36050,33937,35985,36018,33905,31759,31727,31727,33839,35887,33774,33741,35822,35822,33741,33709,33774,35855,
33806,35887,33806,33774,33839,35919,35952,35952,35984,35984,38065,38097,38098,38066,38098,40178,38066,40146,42227,40146,42291,44372,44372,44339,
44339,46484,48565,48532,48597,50677,50677,50677,48564,50612,48531,46386,48466,48498,50611,52724,52692,52724,52692,54805,54869,54869,52757,50677,
50709,48630,46485,44404,42323,38130,38098,38131,38163,36083,34003,34035,33970,31922,31922,31922,34003,34003,31955,31955,31955,32020,36181,34101,
34101,34101,32020,32020,32021,29941,29908,29973,32053,34133,36181,36181,38228,44501,57048,59095,52659,44141,35787,29514,27434,29580,29581,29613,
29613,27500,27500,25420,25355,25290,25290,23210,23177,25225,27306,27274,25128,23015,16806,16806,16806,12613,10532,8484,6339,6404,8484,6371,
6339,6404,6469,6469,6501,6437,4292,2178,4715,2635,587,586,2699,2700,619,2700,652,652,684,684,651,684,2732,2796,
4845,2764,2699,2699,4812,4780,4812,4812,4779,4779,6860,6860,6828,6796,6796,6828,6828,4780,4779,4812,6860,8875,12937,27467,
37771,37543,37445,37412,33284,31205,31238,31303,31465,42160,48663,29907,15246,11215,11248,7087,5039,4974,4943,7055,7023,4942,6990,6990,
9038,11119,9070,9071,9038,6958,9038,9070,6990,6957,6957,6989,6957,9038,11118,11118,11118,13198,13231,13199,13198,13198,13198,15247,
15215,13134,15215,17360,17360,17360,15279,15279,15312,15280,15312,15280,15312,15312,15280,17360,19409,19441,19441,21521,19441,15279,17327,19407,
17295,23568,25682,23568,23569,25682,23601,23601,21553,23666,23634,23601,23569,21456,21424,21488,25649,25649,23536,23536,25617,27730,27730,25649,
27794,27762,25681,25746,27827,27827,27827,29940,29908,29907,29940,29972,29940,29940,29940,29941,32021,32021,32021,32053,34134,32085,32085,34165,
32084,32019,34067,36148,40276,42292,40211,40178,40179,38130,38130,38162,36082,38163,36082,33937,31824,33905,33905,31759,31759,31791,33807,35887,
33839,35854,35854,33774,33742,33742,33774,35887,33806,33806,33806,33807,33839,33839,33839,33871,35952,35984,38097,38130,38098,35985,38065,38066,
40146,40146,40146,40146,40146,44372,46485,46452,44404,46484,48565,48565,50645,50677,50645,48532,48499,50612,50579,46386,46385,48466,50611,52724,
54804,56885,56885,56917,56917,54837,52789,50676,48629,48597,44372,42291,42291,42291,40243,38162,38130,36083,36050,34002,36051,34003,34003,31922,
34002,34035,34035,34003,31955,34035,34100,36181,36214,34133,32020,32053,32053,32086,32086,32086,32086,34133,36213,38293,40308,46581,57080,56982,
46319,37769,29383,21062,18918,21096,21162,23275,27501,29581,29581,27533,25356,23242,23178,21097,23113,21032,23080,25128,23016,23015,16773,16806,
16838,12645,10533,10565,6404,6339,4259,4226,6339,6436,8549,6469,6469,6469,4324,2179,4715,2635,2667,619,619,619,587,2700,
2732,652,652,2732,652,684,2764,2764,2764,716,651,2699,2731,4812,2764,2731,2699,4812,6892,6860,6860,6796,6795,6796,
6796,4779,2731,2764,2764,4811,6794,17097,33708,41834,39557,37444,35331,33252,31205,31270,29287,33611,44372,42454,23600,13166,13263,11248,
7119,5007,4975,4975,4975,4942,6990,7022,6990,6958,9070,11119,11151,9071,6990,6990,6957,6990,6990,7022,9070,9006,9006,11118,
11118,11086,13199,15311,15279,13166,15247,15279,15247,13166,15215,15279,17327,17360,15279,13199,15312,15280,17360,17393,17360,15312,15312,17360,
17360,19441,19473,21553,17360,17295,17327,17327,19407,23536,23536,25617,25649,23601,23601,21488,19440,21521,21553,21489,23537,21456,21456,23537,
23569,23537,23536,23569,25649,25649,25650,25649,25649,27762,27794,27794,27859,27892,27859,29908,27827,27859,29972,29973,27860,27860,29941,29940,
29941,32021,29973,34134,40473,34198,32085,34230,34165,32052,34100,36147,38228,42389,42324,40243,40244,40211,38131,38163,36115,36083,34002,33970,
33937,31857,31824,29711,29711,31759,33872,35920,35919,37967,35887,33774,35822,35887,33774,35854,33774,33806,33839,33839,33806,31726,33806,33839,
35952,35984,35984,36016,35984,35985,35985,35985,38098,40211,40178,40179,40211,42292,44405,46452,46452,48565,48597,48597,48565,48532,46452,46419,
48467,48531,48466,46353,48434,48498,50611,52691,54836,59031,59063,59063,56950,54837,52788,48628,46516,46516,42323,42258,42323,42323,42291,40211,
40211,38131,38130,38131,36050,36051,36115,36083,34003,34035,34035,34035,34035,34035,34068,34101,36214,34101,32021,34134,34166,34134,32086,32086,
34134,36214,36213,38325,42421,48693,59128,54804,42061,31463,25158,18950,16838,18951,18984,14790,16903,21129,25388,29581,27468,25291,21065,21032,
21032,21000,20967,20935,23015,20935,16741,16806,16838,14693,10565,10565,8484,8452,6339,4226,4259,6404,8582,6469,4356,4357,4292,2211,
4683,2635,2667,2699,619,619,2667,2700,4780,2700,2700,2732,2732,2764,684,716,716,684,683,683,683,716,716,684,
2732,4844,6893,6860,6860,6860,6827,4747,4779,4779,4779,4811,4843,2763,4779,8841,23307,39884,41768,39589,39524,33316,31236,29189,
29222,31400,35822,46485,40341,23535,15278,13296,9169,7088,7023,4975,4942,4974,4942,7022,7022,6958,6958,9071,11183,11183,9103,6990,
4877,6990,7022,7022,7022,6990,6957,9070,11118,9038,11118,15279,15279,13166,15214,15247,15247,15214,15247,13166,15247,17360,15280,15279,
15279,15312,15312,17360,17360,15279,15279,15279,15279,15279,17392,17392,17327,17294,17326,19342,21455,23536,23503,25616,23568,21488,23601,19440,
17327,19408,21488,21456,21423,21423,21456,23537,23537,23536,23504,25617,27730,27730,25617,27697,27730,27762,29843,27795,27827,27827,25747,25746,
25779,25811,25844,27892,25812,27892,30005,30005,27893,29973,29973,34166,38424,34198,32150,34230,32085,32052,32052,34100,38228,40309,40244,38163,
38163,38163,38131,36083,36115,34035,34003,34002,34002,31857,31824,31856,31792,31792,35952,33840,35920,37967,35886,35887,35919,35887,33838,33806,
33838,35919,33839,31759,31758,31758,31759,35952,36017,36017,36017,38098,38098,38098,38130,38098,38098,40179,40179,40179,42259,42259,44339,44339,
46452,46485,46485,46452,46452,46452,46419,46451,46451,46418,44305,46386,48466,48466,50578,50611,52756,56982,59063,59096,56982,52757,50676,50676,
48564,46484,44371,42291,42291,42291,40178,40178,42291,40243,38130,38131,36018,33970,36083,38196,38196,36083,36083,36116,36116,36116,36116,36181,
36181,34133,34101,34134,34166,32053,32021,32086,34167,36182,38262,40373,44500,50741,59096,50514,37803,27271,18885,12645,12678,16871,21064,18951,
16805,14693,18983,25290,27403,25290,21065,21032,21032,18952,18887,20936,20968,20903,16741,16773,16838,14726,10565,8517,8485,8484,8452,6339,
4259,4323,6469,6469,4325,4357,2276,2244,4683,2634,587,2667,619,2668,2700,2700,4780,2732,2700,2732,2732,2765,2764,716,
716,684,683,683,684,684,716,684,684,4844,4845,4812,4812,4812,4780,4779,4779,6828,6860,4811,4811,4812,4779,8842,
14985,29547,44011,43848,41637,35363,31236,29156,29157,29255,29385,35854,46517,38195,21455,13231,11249,9137,7056,4975,4974,4974,4974,4942,
4910,6990,6958,9038,9071,11183,11183,9070,6925,4877,6990,6990,6990,7022,6958,6958,9038,11118,11151,13199,13199,15247,15247,15279,
15279,15279,15279,15247,13134,15247,15280,15312,15279,15312,13199,13166,15312,17360,15279,15279,13199,13199,15279,15279,15279,15214,17294,19342,
19310,21390,23503,23568,23536,21488,23601,21521,19408,17327,19408,21456,21423,23504,23536,23569,23536,23504,23471,23504,27730,29811,25585,25617,
27730,25649,25682,27762,25682,23633,23633,23633,25779,25811,25779,25811,25811,25812,30005,30005,25812,27893,30005,30005,32053,32118,32118,32117,
32085,32052,32020,34068,34067,36115,36115,38163,38163,38164,38164,38196,36116,34035,34035,31922,31922,31889,31857,31889,33937,35985,33872,33807,
35920,35887,35887,38000,37999,35951,35919,33806,33806,33806,31758,31726,31759,33871,31791,33871,33936,38097,38130,40210,42324,38131,38098,38098,
38098,38098,40146,40178,40178,42291,44372,44372,46452,44372,46452,46452,44372,46452,46484,46484,44371,42257,44305,46418,46386,48434,50546,52691,
52724,52724,56918,59063,56918,52724,48563,48596,48596,46516,44404,44436,44372,40178,38065,40178,42291,42291,40211,40243,38163,36083,38163,38196,
38164,36083,36083,36116,38196,38197,38229,38294,36214,34134,34166,34166,34167,32054,32053,34134,36214,38262,40341,42453,44532,52854,56950,42062,
29385,20965,16773,14759,14726,12645,16838,25225,29451,21063,16805,18886,21064,23177,21064,18952,18920,16839,18920,20968,20968,18855,16741,14693,
16806,14758,10565,8517,10597,8517,8484,6372,4323,2275,4356,6470,4357,2277,4389,4357,2635,2602,586,619,2667,2667,2668,2700,
4748,2732,2732,2732,2732,2764,2764,684,716,716,684,684,684,684,684,716,684,2764,4812,4812,4812,4812,4780,4780,
6828,6828,8908,8908,6828,6860,6860,6827,8841,19177,37900,43946,39589,35363,33283,29155,29156,29190,29288,31498,37967,44437,31955,17327,
11151,11216,9168,7055,7055,7087,5007,4910,4877,4910,6958,6990,6958,9071,9103,9103,9070,4877,6925,6990,6958,9038,9006,6925,
9038,11118,11151,13199,13166,13199,15279,15279,15279,15311,15279,15247,13166,13166,15280,17360,15312,15312,15279,13198,15279,17360,15279,15279,
13199,13166,13199,17359,17327,15214,17294,19342,19310,19310,21422,21423,21423,21455,19408,21521,21521,19408,17295,21456,23536,23536,23536,23536,
23536,21424,23471,23504,27697,29778,27633,27697,25617,25617,25682,25682,25714,25714,23634,25747,25746,23698,25811,27892,27892,25811,27892,27925,
25844,27925,27957,27892,30005,34231,32085,30037,32085,32052,32020,34068,34067,36115,36115,36115,36115,38196,38228,38228,36148,31987,34100,36148,
36148,34035,31889,31889,33937,33904,33839,33807,33839,33838,35919,35951,35919,38032,35920,33839,31758,31726,31726,31726,31758,31791,33839,33871,
33872,36017,38097,38130,42324,40211,35985,35985,38098,38098,38098,40146,40178,42291,44372,44372,44339,42259,44404,46484,46484,44371,44371,44339,
42258,42257,44306,44305,44305,48466,50579,52659,52691,50611,54772,54837,54804,50644,48531,46450,46483,46516,44371,44372,42291,40178,40146,38097,
40178,40211,40243,40276,40244,36115,38196,38196,36115,36115,36116,36148,38229,38261,38261,38262,36181,34166,34199,36279,34231,32086,32085,34133,
38261,40341,42389,44501,50774,54902,46288,31497,27272,18886,16806,18985,16872,12613,12580,16806,29419,31596,23111,16773,16805,16870,18984,18952,
16807,16807,18888,20935,20968,20903,16741,16773,14725,14790,12710,10597,10630,10630,8517,6404,4356,4356,4388,6502,6502,2309,2309,4390,
2634,2602,554,587,2699,2700,2668,2668,2700,2732,2732,2732,2732,2764,684,684,684,716,684,684,684,684,684,717,
2797,2764,2764,2796,4844,4812,4812,6860,6860,8908,8908,8908,8876,6796,8909,6892,6891,10953,27498,39883,39655,37444,33283,31235,
31236,29189,29255,31401,31498,40113,44470,29874,15247,13232,11249,9104,7055,7088,5007,4974,4942,6990,6990,6990,6958,6957,6990,11151,
11183,9006,6925,6958,9006,9006,9006,8974,8974,11151,13231,13231,13231,13199,15279,15279,13199,15279,15279,15247,13167,13167,15247,15279,
15280,15312,15312,13231,13199,15279,15279,15279,15279,11118,11086,15247,17327,17327,19374,19342,19374,19342,19342,19342,19375,19375,19375,19440,
21521,19440,19408,21456,21456,19375,21423,21423,21423,21423,23471,23504,27697,27697,27664,27697,27697,25617,25649,27730,29875,27827,25714,27827,
25747,23666,25779,27892,30037,25812,25779,23764,23764,25876,25877,25844,30005,34231,32118,30037,30037,29972,29972,32020,34100,34067,34099,34099,
34067,34067,34068,34100,34068,32019,34133,36181,38229,36180,31954,31922,36083,36017,35985,35952,35951,35951,35984,35984,38064,38065,38065,38065,
33872,33807,31759,31726,31726,31726,33904,35985,33936,38098,38130,38098,40211,40211,38098,35985,38098,40179,40179,40146,40179,40178,40178,42259,
42259,44339,44372,46452,48565,44404,44339,42291,42258,42258,44338,44338,44306,48466,50578,48466,50578,50578,50579,52691,50611,50611,48563,46451,
44371,44403,42258,40178,40178,40211,40210,38098,38098,38130,38163,38196,38196,36148,38196,38228,36148,36148,36148,38229,40341,40342,38294,36181,
36181,36214,34231,34232,34199,32085,34133,36213,38260,42420,48694,52854,50676,42127,31465,25095,23079,18886,16806,18921,16840,12581,10435,14660,
16773,27273,33611,23079,14692,14692,16806,18919,16839,16807,16807,18887,20967,18855,18821,16773,14692,14725,14758,10662,10630,10662,8550,6437,
4388,4389,4389,4421,6535,4389,228,4422,2634,2634,2634,2635,2667,2700,2667,2635,619,2700,2700,652,2732,2732,684,651,
684,716,684,684,716,717,684,684,717,716,2764,4845,4844,4844,4844,6860,6892,8940,6860,6860,6859,8875,8941,9005,
6988,8970,17161,35820,43978,39622,35395,33315,31268,31236,29189,29222,29287,35692,44307,42389,27729,15279,13264,11184,7023,7055,7088,7088,
4975,7022,6990,6990,6990,6957,6958,9071,11183,11119,6925,6925,9006,9006,9006,9006,9006,13232,15344,13232,13232,13231,13231,13231,
13199,15247,15280,15280,15279,15312,15280,15247,15280,15344,15312,13231,13199,15247,15247,13199,13199,13199,13166,13101,15214,17327,17294,17262,
21423,21423,17262,17262,19343,17295,19375,19408,21488,21521,21488,19408,21456,19375,21423,21423,21423,23504,25617,27665,27697,27665,25551,27632,
27729,27697,25649,25649,27762,29875,27795,25747,27827,25747,25747,27860,34231,30038,25811,23731,23731,23796,25876,25844,27892,32150,32118,32118,
32085,29972,29940,32020,34100,34100,34099,34100,34100,34100,34067,34132,34100,34132,36278,36246,34100,34035,31955,31954,34002,38130,38098,38065,
35984,38096,38097,38097,40210,42291,40211,38098,35985,33872,31727,31759,31758,29678,33904,35984,33904,38098,38162,38162,40243,40211,38131,38098,
38066,40179,42291,42259,40211,40178,40178,40178,42258,44339,44371,46452,46452,44371,42258,42291,42323,42290,42290,44371,46419,46418,48466,48466,
50546,50546,50514,50611,50611,48498,48498,48531,46419,44338,42258,40146,40146,40211,40178,38065,38098,38163,38163,38196,38229,38229,36148,38229,
38229,36148,36180,38293,40374,40374,40342,38229,36181,36246,36279,34231,34166,34165,36213,38292,42453,50806,54967,50676,37901,29352,27175,23015,
20935,18920,16840,21033,14727,8387,12581,18854,14627,16740,33611,31498,16740,12579,12612,14726,18919,18887,18855,18887,18887,18822,16773,16772,
14692,12644,12709,12710,10630,8582,8550,8550,6469,4421,4389,2308,4422,4454,2309,2342,4682,2634,2634,2635,2667,2699,2667,2700,
2700,619,619,651,2732,2764,684,684,684,684,684,684,684,717,684,684,684,684,2764,2764,4812,4779,4812,4811,
6892,6924,8971,8971,6891,8907,10988,9004,6988,9003,10985,27530,42028,39751,37508,35395,31267,31236,31237,29157,29190,29320,35789,44372,
44470,29875,19408,15247,13199,9071,9136,9201,7055,4877,4910,6958,4877,4909,6958,6958,11151,11151,9038,9006,9006,9006,9006,8974,
8974,11087,13232,13199,11151,13264,13232,13231,15279,15279,15280,15312,17360,17360,17360,15247,15279,15312,15312,15312,15280,13167,13167,13199,
13231,15279,13199,13134,13134,15181,15181,17262,19342,21423,19375,19343,17262,17295,19407,19408,21488,21520,21520,21521,21456,21456,21424,21423,
21423,25617,31923,29778,29745,27632,25519,29745,29810,27729,27697,25649,27730,27762,27795,25747,27827,27860,25747,25747,32053,34199,25812,23731,
23763,23796,23796,23764,25812,27925,30038,30038,29973,29940,29940,29940,32020,32052,32019,34100,34132,34100,32019,34100,34165,34165,36278,36246,
34100,31987,31987,34035,34002,36050,36050,36017,36017,38097,38129,40242,42323,42323,40211,38130,38066,33872,31759,31791,31791,31759,31791,31759,
31824,36050,38098,40211,42356,40243,38130,38130,40211,40211,40178,40178,40178,40178,42259,42258,42226,42258,46451,46484,44403,42258,42258,42323,
44404,44404,44404,44371,44371,44337,44305,46385,48466,48466,48433,50546,50579,46418,46418,46418,44338,44371,42291,38097,38065,38130,38130,38098,
38098,38163,38228,38228,36148,36181,36149,36181,38229,38261,40374,40407,40374,40342,38261,38261,38261,38326,38327,36246,36213,38229,40341,46614,
52855,52822,46451,37934,29450,27240,27176,25064,20935,18920,16840,18920,12581,8355,16774,20967,18821,18788,23014,33611,23111,14627,14660,14660,
18887,18887,16807,16806,18854,16773,16741,16773,14692,12612,12644,12677,10662,10630,8550,10630,8614,8582,6502,2341,2341,6535,4454,2341,
4714,4715,2602,554,2667,2667,2699,2700,2732,652,619,651,652,2732,684,652,684,651,684,684,684,684,684,684,
652,684,2764,4812,4779,4779,6860,6891,6923,8971,8971,11051,11018,10986,13067,11019,8972,9004,8938,17129,33771,39849,37541,35395,
31234,29155,29156,29156,27109,27142,29353,37935,48598,46584,31891,19310,15214,13232,11216,11216,7055,4942,6990,4942,4909,4909,4910,4910,
6958,9071,9039,9038,9006,9006,9006,8974,9006,9006,11119,13200,13232,13232,13232,13199,13231,13232,15280,15279,15280,17360,15280,15247,
13199,13199,15279,15312,15279,15247,13166,13166,13199,15279,13167,15214,15182,15214,17294,19342,17262,21423,21488,21456,19375,17263,19375,21456,
21488,21488,23569,23602,23537,21456,21424,23504,23536,23536,27664,27697,27696,27599,25519,29712,27632,27664,27665,27697,27730,27730,27762,25715,
25715,27795,27827,27827,25747,29973,25812,23699,23699,23763,23796,23764,23731,25780,27892,30006,27892,27828,27859,29907,32020,32020,32019,32052,
34068,32020,31987,31987,32020,32052,34133,34100,34068,32020,34068,34067,36147,36147,36115,38130,36050,36049,36049,38130,40243,38130,38098,36017,
35985,35985,33872,33839,31759,31759,31791,31791,33872,36017,36049,38163,40276,40243,38130,38098,40210,40178,40146,38097,38065,40146,42291,44339,
42258,42225,44305,48531,48564,44338,42258,42290,42323,44404,42323,42258,42258,42225,42225,42192,44240,46321,46354,46418,46386,44305,46418,44338,
42258,42290,40146,38065,38065,38097,38130,38130,38098,38163,38196,36148,36148,36181,36181,36181,36181,38294,40406,40407,40374,40374,40342,38293,
38293,40374,40374,40341,40341,44502,50743,50775,46484,42128,35757,31531,27306,27241,27209,25129,23049,21001,16840,16839,16807,21033,31565,35791,
31532,20966,14627,27272,29417,18853,14627,12547,16774,18887,16807,16774,16806,16773,16741,16773,16741,12612,12612,12645,10597,12710,12711,10663,
10695,10696,8583,6502,2309,2342,4487,2341,4682,2634,2602,2602,2635,2667,2700,2667,2732,2732,651,651,651,651,684,684,
684,684,683,684,684,684,684,684,652,2732,4812,4844,4811,6860,6924,8972,11019,11051,15179,15179,15146,17194,15113,13066,
13100,11052,11052,13066,25482,39980,39654,35363,31202,27041,27042,27043,24996,25028,27142,31466,42096,50647,48632,36051,21423,17327,15344,11216,
9103,7055,7022,4942,7022,4910,4877,4877,4877,6925,6958,9071,9006,9006,9038,9006,11119,11119,11119,13232,15281,13232,13167,11151,
13199,13199,13199,13167,15280,17360,13167,13166,13167,13166,13199,15279,15247,15247,13134,11054,13166,13167,13167,15214,15182,17295,19407,19375,
19342,21423,21423,21455,21456,19376,19375,19375,21488,23568,23568,23537,21488,21456,21423,23504,21423,21390,21358,25551,27632,27599,27599,27632,
27631,25583,25584,25584,25616,27698,27795,27795,27763,25714,25682,27795,25714,25779,25779,23731,23731,21651,23731,25812,25812,23732,25780,27925,
27893,25747,27827,27859,29940,29939,32020,32020,31987,31955,29907,29874,29874,29907,32052,34100,34068,31987,31987,31987,34067,38228,38261,38163,
38163,40308,42356,38163,38162,38130,36049,36017,36049,38130,38065,33904,31791,31791,33904,36017,36050,38130,38130,38098,40211,40243,40211,40210,
40210,38065,38065,40146,40178,40146,42226,44371,44338,44305,46386,48531,48531,46450,44371,42258,42258,42258,40210,40177,40145,42225,42257,42160,
42160,42193,42193,44273,42193,42192,44306,42225,40144,40145,38065,38097,38098,38098,38098,38130,38131,36115,36116,36148,36149,36181,36181,36213,
36213,38294,38326,40406,42487,42487,40374,40374,40373,42421,42421,44469,50743,52823,48565,42129,33709,31499,29387,27306,25226,27242,25162,25097,
23082,21001,21001,31565,42130,46356,50614,48468,35790,23079,14627,20933,31497,25159,16740,12547,12580,16807,16807,16774,16774,16805,16773,16773,
16741,14660,12612,12612,10532,8484,12711,12743,10663,12776,10696,8583,2309,229,4454,4454,2602,2602,2634,2634,2602,2635,2699,2700,
2699,651,651,651,651,652,2764,2764,2764,2764,716,716,684,684,684,684,2732,2732,4812,6892,6892,6924,8972,11052,
13099,15147,19275,23468,27629,27661,23531,21419,17260,15181,13165,13131,19273,37964,41865,37444,33282,29090,24962,24995,24995,24996,25029,27143,
33580,42065,48501,50744,38229,23568,15279,13264,11216,9135,7055,7022,6990,4910,4877,2797,4877,4877,4877,6990,6958,9006,9039,9039,
11119,11152,11120,13200,13232,15281,13200,11119,11119,13199,13232,13199,13199,15279,13166,13134,15215,13167,15247,15247,15215,13135,13134,13102,
13134,13167,13134,15182,15182,15182,17294,19342,19343,19375,19342,19343,19376,21456,21456,21456,21456,23536,23568,21456,21424,23537,23536,23536,
21423,21358,23438,23438,25518,25518,25518,29744,29744,25551,25551,23503,25584,25617,27762,29908,29876,25715,25682,25715,27795,25715,25747,25779,
25780,23699,21618,21651,23731,23764,25780,27861,27893,25779,27827,27827,29907,29940,32020,34068,31955,31955,31923,31955,29906,29906,31987,34100,
36148,32019,32019,34067,34067,36148,38261,36148,38196,40308,40276,38195,40275,40243,38162,38130,38130,38130,38098,35985,33937,33872,33904,35985,
36050,38131,40211,40243,42324,40275,40210,40210,40178,38065,38033,40178,40145,40145,40145,42225,44338,46418,48498,50611,48531,48531,46451,42258,
42258,42258,40210,40177,40145,40177,42258,42193,44242,44306,40113,40113,40080,38031,42225,42225,40112,38032,38032,38065,38098,38130,38163,40243,
38163,36083,36115,36116,34100,34133,36214,38327,38326,38326,40374,40439,42519,42487,40406,40406,42454,44534,50743,54903,52725,44241,37838,31532,
29387,29323,27242,25226,25227,25195,25130,23049,23049,23114,31565,50582,52695,50582,46388,40016,29451,20999,18853,23078,31498,27304,18853,14628,
12580,16774,18855,18854,16806,16805,16773,16741,16741,14660,12579,12612,10532,8419,10565,12711,10663,10695,10728,8648,6535,6503,6535,6535,
2634,2570,2602,2635,554,586,2667,2700,2700,651,652,684,651,684,2797,2764,684,684,684,716,683,651,684,2764,
2732,4812,4812,6860,6924,9005,9004,13132,15179,21355,33903,44402,46515,44466,40337,38160,31823,23469,19308,17226,19208,33738,46091,39589,
35363,31203,27042,24994,24963,22915,22915,22948,29256,33579,39984,48566,50809,38261,21487,13166,11150,9135,9135,9103,6958,4910,4877,2764,
2796,4877,4877,4878,4910,6958,9006,9038,9006,11119,11152,13200,13200,13167,11119,11119,11086,13199,15312,13232,13199,13199,11086,13134,
15247,15247,15247,15247,15215,13134,15182,15215,13134,13134,15215,17263,17295,19343,17262,17229,17262,19342,19375,21423,21424,21424,21424,21456,
21423,21423,23536,23504,23536,23537,23536,23569,23536,21391,21390,23438,25486,25486,27566,27599,25519,23470,23471,23471,23536,23536,25617,27795,
27762,25650,25682,25682,25715,25682,27827,25747,23667,23634,23634,23666,23667,23700,25780,25780,25812,25780,27827,27827,29940,32052,32052,32020,
32020,31955,31955,31987,31987,31987,29907,32020,34132,32020,32019,34067,34067,34067,36148,38261,38228,36115,36115,38163,40275,40276,40211,38162,
38162,38130,38130,38098,38065,35985,36017,38098,36017,33937,38162,40308,42389,42356,40243,38130,38130,38065,38033,40178,40145,40113,42226,42225,
46386,48531,50578,50643,50644,48563,46450,44338,42290,42290,40177,42290,40178,40145,40145,40113,42226,44307,38032,38032,40113,40145,42258,44339,
42258,38032,38000,38065,38098,38130,40211,38163,36083,36115,36116,36116,34100,34133,36246,40407,40407,38359,38326,40406,42487,42486,42453,44502,
48695,54937,52791,46355,39951,33580,29354,27273,27274,27242,27243,27275,25227,25227,27308,25227,25195,29420,44243,52726,40016,35758,35790,31532,
25225,20998,25159,27304,31497,27304,20900,16740,14628,16774,18887,18919,18887,18854,16774,16741,16741,14660,12579,10531,12612,10532,8419,6339,
8517,8582,10695,10696,10728,10793,8680,8648,2634,2602,2634,2667,2667,2635,2667,2700,2732,651,652,684,651,651,716,2764,
684,684,684,684,684,684,651,651,2731,4812,6860,6860,6925,9005,11084,13131,21387,38128,52756,56917,56948,54900,54933,54869,
50611,44272,37998,29643,25351,35721,48172,41735,33315,31170,27042,24930,22882,22850,22883,22915,25029,29320,35692,42096,48598,50744,36116,21423,
13166,11183,9168,9072,6991,4878,2829,748,716,2797,4877,4910,4943,6990,6958,6990,6958,6926,9071,11119,9006,9006,11086,11086,
11054,11119,13232,13232,13232,13199,13199,13167,13167,15215,15215,15247,15215,13134,13102,15215,15183,13102,15182,19343,21488,21488,19342,17229,
19310,19342,19375,21456,23504,21391,19343,19343,19310,21391,21423,23503,23568,23536,21456,21424,23504,23471,23439,23438,25486,25454,25453,25453,
23438,23438,23471,23471,23504,23504,23536,23569,23569,25649,25682,25650,25682,27795,31989,27795,23601,25650,25682,25715,25747,23634,23667,23700,
23731,25779,25747,25779,29972,32085,32117,32052,31988,31987,29907,29907,31987,32019,29907,29939,31987,29906,31986,32019,34099,34067,34099,38261,
38260,36179,36147,36114,40307,42356,40275,40243,40211,40211,40211,38130,40178,38097,38098,38130,36017,33936,36050,38195,40243,40276,40275,38130,
38130,38065,35952,38032,38033,40113,42193,44273,46418,50643,52723,54837,56949,52724,48531,46451,44371,42290,42258,42290,40178,40113,38032,38064,
42226,40145,35951,35952,40145,42226,42226,40146,40178,40178,40145,35985,36017,38098,38130,38130,38163,38196,38228,36148,36180,34133,36213,38326,
38359,38359,38358,40406,42518,46614,48694,54871,54872,50613,42097,33645,29387,25193,25129,25129,27274,27274,27275,27308,27340,29389,29453,29421,
29453,35791,50581,50580,37870,29418,27337,27337,25224,20965,25191,27271,27271,27238,20932,16740,14692,16773,18887,18887,18887,18887,16773,16741,
14692,14660,12579,10466,10499,10532,8419,4226,6339,8485,10631,10695,10728,10760,8680,8681,2634,2635,2635,2635,2667,2667,2700,4780,
2700,651,652,684,684,684,651,684,684,684,684,651,652,684,651,651,4779,6860,6892,8972,9004,9036,11083,17259,
33967,50708,56884,56883,56915,54802,54706,52561,50416,48304,50449,46287,39883,44011,52236,43783,33316,29122,27010,24898,22818,20770,22851,22883,
22916,27142,31466,35757,40081,46453,46583,31890,17262,13231,11184,9071,6959,4878,2829,748,716,2764,4877,4942,4943,4910,4910,6990,
6958,6958,6926,6958,9006,8974,9006,11086,9006,11086,13199,13199,13167,15247,15247,15215,15214,15214,15214,15246,15214,13134,13101,15215,
15215,13134,15182,17262,21455,21455,19342,19310,19310,21390,21390,21391,21391,21359,19310,19310,19310,19342,19342,21423,23568,23536,21456,21423,
21423,23503,25551,25518,27566,27566,25453,25485,25518,25518,25551,23503,23503,21423,21423,23504,23536,25649,27762,25682,25649,27731,29876,27795,
25650,25617,25682,23602,23602,21586,23699,23699,23731,25780,23699,23666,27827,29972,32084,32084,32020,32020,32020,31987,32019,32019,32019,31987,
29907,31987,29906,29906,34067,34067,34067,36180,38228,38260,38227,38195,40307,42388,42355,40275,40275,40243,40211,40210,40243,38098,33936,36017,
33937,31856,31856,36050,38163,38163,38130,38098,38065,35984,35952,38000,38000,40080,40112,44305,48498,50579,52724,56982,59095,56949,50676,50644,
46451,42258,44371,42226,40145,38064,35952,38032,40145,38065,35984,35984,38032,40113,40113,40113,40146,40211,40178,36017,36017,38098,38162,38163,
38163,36115,38228,38228,36180,36181,36213,36278,38326,38326,38358,40405,44533,50774,54902,50580,42096,35725,31532,29387,27242,25162,25162,27275,
29387,29387,29388,29421,31502,31534,31534,31566,33646,42130,52694,50548,44241,37870,31530,31498,27304,23046,27271,25159,25158,27239,23013,18852,
18853,18886,18886,18886,18887,16806,16773,16773,16773,14692,12579,10499,8386,10531,10532,8451,6339,6340,6372,6437,8583,8615,8647,8615,
4715,2635,2602,2635,2667,2667,2732,2700,2732,651,651,651,2764,2764,651,651,716,716,684,651,651,651,651,2699,
2699,6859,8972,9004,9004,11052,13098,25580,44433,56884,56851,56818,54738,52625,52528,50318,48174,46061,46093,48238,44012,41768,43784,39590,
33348,29156,24962,22850,20770,20738,20770,20803,20803,22949,29321,33644,39983,42194,48599,42358,23536,13166,11118,9071,6958,4910,2829,748,
2764,2797,4845,4910,4910,4877,2797,6958,6958,6958,6925,6925,9006,9006,9006,11119,11151,11119,11119,13199,13199,15247,17295,17294,
17262,17261,19342,17294,17294,15213,13133,15214,15214,13134,15214,19375,19342,19374,21422,19309,21358,23438,23470,23470,23438,21358,21357,21358,
21358,21358,19310,19310,21423,21455,21423,21423,21390,23470,25518,25518,27598,29646,27533,29678,31792,27598,27599,25551,23503,21423,21423,21423,
23504,23536,25649,25649,25650,25618,25650,27730,25650,25617,25649,25682,23634,23667,25780,23699,23699,25780,25747,25747,25714,25746,27859,29972,
32052,32053,34101,32052,34133,32052,32020,32020,31987,31987,31954,29841,31954,34099,34067,34035,34067,36179,38227,38227,40307,40307,40275,40275,
40275,40275,38162,40243,42356,40275,38130,36049,36049,33969,31856,31856,36082,38130,36050,36049,36017,35985,38032,38032,38032,40113,40112,44273,
46418,48498,52723,56949,59062,54869,50675,48563,44371,42258,42258,42226,40178,40145,38065,38032,35952,35952,35984,38032,38033,38065,40145,40178,
40146,38097,38098,38098,38098,38130,38163,38163,36083,36115,36148,38229,36180,36181,36214,36246,36246,38326,38358,40405,46581,52789,46320,35692,
29354,27209,27242,25130,25162,27210,27210,29356,31469,31501,31501,31501,31534,33647,33679,33679,37872,48437,52694,52693,52725,48467,40046,35756,
29384,25191,25191,20933,20933,25159,23014,18853,20966,20967,20967,18886,18886,18854,16773,16773,16772,14692,14659,12579,8386,8386,10564,10564,
8452,6340,4291,4291,4356,6437,8550,8550,4748,4715,2634,2635,2635,2667,2699,619,651,651,651,651,652,684,651,651,
716,2796,2764,2732,651,2699,2699,2699,4779,6859,8972,11052,9004,11051,15178,31853,50674,54770,52560,54705,56819,54771,54706,52496,
54577,54545,48206,46126,46157,41866,39655,37575,31301,27108,24995,22851,20770,18658,18690,20771,18723,18755,23014,29386,35757,42097,46388,48567,
38165,19375,11085,9038,6958,4910,2861,2829,2829,4878,4878,4910,6990,4910,2797,4910,7023,6958,6926,6958,6925,6958,9006,9038,
11119,11119,13199,15247,15279,17294,17262,19342,23470,23502,23502,21422,19374,17261,15213,15214,15214,13134,15182,17262,17229,19310,21422,21357,
21357,23438,25518,27599,27598,25518,23405,23405,23438,21390,21358,19310,19342,19375,19342,19310,19277,21390,25518,27598,27598,31791,35919,33871,
31824,29679,27598,25518,23503,23471,21423,21423,23504,25617,27697,25617,27698,25617,25585,27698,25617,25585,25650,27763,23634,23602,23667,23699,
21619,23699,25779,27860,27859,25779,25779,27827,29972,32020,32020,32020,34133,34133,32053,34068,31988,29906,29874,29874,31954,34067,34067,34067,
34067,36179,36179,38259,40307,40275,40275,38194,38194,38194,38162,38162,42356,42388,40308,38195,38163,38130,36050,33969,33969,36050,38098,36017,
35985,35952,38032,38032,38032,40112,42160,42192,46353,48498,50610,54804,54836,52723,48530,44337,42257,40145,40177,40145,40178,40145,38065,35984,
35919,35952,35984,38065,38065,40146,40178,40178,38098,36017,36049,36050,38130,38162,38163,36115,34034,36148,38228,38261,36181,36181,38327,36246,
36278,38358,38357,40372,48628,50611,37803,31401,29289,25096,27177,27210,27243,27243,25162,25162,29356,31469,33550,33582,33615,35727,35760,35792,
40050,50614,52727,54871,56919,50644,46385,39981,33675,29449,23046,18821,20902,25127,23015,20934,18886,20934,20967,18886,18886,18853,16773,16772,
16772,16740,14659,12579,8386,6273,8418,10532,8484,8452,6372,6339,4291,4291,4291,4292,4715,4748,4747,2635,2635,2667,2667,619,
651,684,684,684,651,651,684,684,716,2764,2764,2699,2699,2732,2699,4747,6859,8940,8971,11019,8971,11018,19307,36047,
50642,50512,50447,54706,54707,52627,48433,46288,52562,56756,54643,54611,52594,52562,50449,50448,41932,31335,22948,20771,18691,18691,18691,18658,
18691,18723,18788,25127,31466,37839,44178,46421,46552,29843,17294,11086,6958,4910,2861,4942,4877,4877,6958,4910,4910,4910,4910,4910,
6990,4910,6958,9070,6925,6925,9038,9006,9006,9038,13199,13198,15214,17262,19310,23470,27630,29742,27630,25582,23470,19309,17229,17262,
15214,15182,15181,15181,15149,19309,21422,21389,23373,25453,27566,29679,33872,31791,27565,25485,23438,21390,21358,21391,21391,19343,19342,19310,
19309,21390,23438,27630,29678,33839,44371,38065,29678,29711,27598,23438,25551,23503,23471,23471,25616,29810,29811,25585,25585,25617,25617,25585,
25617,25617,25649,27762,23634,23602,23699,23667,21586,23699,25747,25747,27860,27860,27859,25747,27860,29940,29940,34133,34133,32053,34133,34101,
31988,31987,31955,34067,34035,34067,34067,36148,36180,36212,36179,38259,40307,40307,40307,38226,38194,38162,38162,36114,36114,38162,36114,36082,
38163,38162,36082,36050,33969,33969,33937,33872,33872,33871,35952,35952,38032,40113,42192,46385,46385,46385,48498,50610,50610,48498,46417,44337,
42225,40177,42258,40210,42258,38065,35984,35984,35984,38098,38130,38130,38098,38098,40211,38163,38098,33969,33969,36082,38163,36115,38163,38196,
36115,38228,38261,38261,38261,38294,38359,36246,36278,36310,36277,40372,48628,50578,37771,33482,31370,29290,31404,31469,35695,42034,33615,27243,
25162,27275,31501,33614,35695,37840,37872,37872,44243,52759,56952,59032,56919,54805,52692,46385,42127,35756,29451,29386,29386,29419,25193,23047,
20967,20967,20934,20934,18854,18821,18821,18788,16740,16740,14691,12579,8418,6305,6337,8386,10532,8452,6339,6339,6339,4291,4259,4259,
4683,6796,6828,2667,2667,619,619,619,2700,684,2732,684,652,684,716,684,716,716,651,2699,4780,4780,4780,6827,
6859,8939,11019,11019,11019,10986,23436,44402,52756,54706,54707,50514,46256,42031,35756,37837,44111,48337,50450,52563,52563,50482,48369,50482,
52530,48304,41966,29288,18724,16611,16611,16611,16643,16643,18756,23014,27273,35693,44145,46324,46486,40277,23503,13101,6893,4845,2829,4942,
4910,4877,4910,4910,4878,4877,4910,4910,4910,4877,6958,9038,6958,6925,9038,9071,9038,11086,11151,13166,17294,19342,21390,25550,
33872,42258,38097,29711,23437,21357,19277,19310,17230,15149,15181,15148,15148,19309,21390,23438,23405,25453,27565,35952,44404,40210,31759,27598,
25518,21390,19310,19310,19310,19310,19342,21390,21358,21389,23438,27598,29679,31726,38065,35952,29679,29679,25518,23470,23503,23503,23471,23471,
25584,27697,27698,25552,25552,27665,25584,25552,25584,25584,25585,25650,23602,23569,23667,23667,21554,23667,25747,23634,23634,25714,25747,23666,
25746,27859,29972,34166,36246,34133,34133,34133,34068,31987,34067,34100,34099,34067,34067,36148,36180,36180,36179,36179,38227,38227,38227,38227,
38227,38227,38227,36146,34033,34001,33968,33968,36082,36082,36082,34001,33937,33936,31856,31791,31791,33871,33871,33871,35952,40145,42192,44305,
46353,46353,48466,48466,46353,44304,46417,44337,42257,42258,40145,38065,40178,35984,35984,38097,35985,38098,38130,38098,36050,38066,38098,36050,
33969,34002,36115,36115,36115,36083,38196,38228,38228,38261,38293,38261,38261,38326,40407,38359,38359,36310,38357,40372,46548,52691,39916,29320,
27176,27209,29323,31469,39921,48406,46228,33615,27276,27211,31469,35695,35759,39953,39985,42162,50614,57017,59097,59097,59031,56918,52725,50579,
44273,39983,37871,33678,31565,31532,29387,25161,23080,23047,20934,20902,20902,18821,18789,18821,18788,16740,14692,12579,10498,8386,6305,6273,
8387,10533,8420,8387,6372,6372,6340,6340,2602,4715,4747,2667,2667,2667,2699,651,651,684,2764,2764,684,684,716,716,
716,684,651,2732,4780,6828,6828,8907,8907,10987,11019,11019,11018,17227,33968,50709,56885,58998,58966,48369,39885,31434,29288,29321,
33482,37707,41901,39853,39886,37773,33547,33579,39950,46289,50516,48403,37806,23015,16643,14563,16643,18756,18788,20902,25160,31499,39952,46356,
44309,42325,27664,13069,6860,4812,4877,4942,4942,4878,4877,4910,4878,4910,7023,4910,4877,4845,6925,6925,6925,6925,6925,9038,
9038,9038,11119,13167,15214,17262,21390,27599,38098,48597,44403,31791,25518,21358,19310,19310,17230,17229,15149,13068,15116,17197,21357,23438,
23373,25453,27533,33839,42258,40145,33839,27598,25486,23470,21390,21358,19310,19310,19310,21423,21358,21357,25518,25518,27598,29678,29678,29646,
27598,25486,23438,25519,25552,23503,23471,23439,23438,25519,25520,25519,23471,25552,25552,25552,25584,25584,25584,23569,25650,23602,23634,23666,
21554,21554,23699,23667,23634,25747,25779,23666,25747,27892,30005,34199,38360,36246,34101,36246,36181,31987,34068,34067,34099,34067,34099,34100,
34067,34099,36180,38228,36179,38227,40307,40339,40372,40372,38291,36146,34066,36114,34033,31920,34001,34002,34001,33937,33937,33969,33904,31824,
33904,33904,33839,33871,35952,38064,42193,42192,42193,44273,46353,46353,44305,44272,44304,44305,44338,42290,40145,38032,38064,35984,36017,36017,
35985,36017,36050,38098,36050,36017,36017,33969,34002,36083,34034,34034,34035,36115,38228,38261,38261,38293,38293,40374,38326,38326,40439,40472,
38391,38358,38325,40372,44468,50644,44175,27240,23015,23015,23016,25162,29389,33647,39954,37808,31469,27275,29388,35727,39952,42097,44210,48468,
56920,59097,59096,59031,56886,50612,44241,40048,37935,35823,33710,31597,29452,27371,27339,25161,23047,23015,20934,20902,20902,18821,18821,20901,
18853,18821,14692,12547,10466,8386,6306,4225,6339,8452,8420,8420,8420,8452,8453,6405,4683,2602,2635,2667,2667,619,619,651,
619,651,2764,2797,2765,716,684,684,684,716,2764,4812,4780,6828,6828,8875,10955,10987,11019,13066,15114,27630,46517,54806,
50547,50514,54773,48369,33514,25030,24998,24998,27110,29255,31368,31336,33482,27208,20902,25127,29418,31499,35757,46258,50549,46291,33580,18854,
14595,16676,18789,18853,23080,25225,31532,42162,44276,46422,38100,19278,8876,4812,4845,4942,4910,2797,2797,4878,4910,7023,7023,4878,
4845,4813,6925,6893,6925,6925,6893,6925,6957,9006,9038,11086,13101,15149,21390,27599,33872,42226,38065,29711,25518,21390,19342,17230,
15182,15149,15149,13068,15149,17197,19277,21325,23340,25420,25420,29613,33806,33806,31726,27566,23438,21390,21391,21358,21390,21390,21390,21358,
21390,23471,25551,25551,25518,27599,29679,27566,23470,23438,23438,23503,25552,23472,25552,23471,23438,25487,25520,25552,25519,23471,25551,25551,
23503,23504,23504,23536,23569,23569,21554,23667,23699,21586,21586,23667,23667,25747,27860,27860,27892,30005,30005,32118,38360,38360,36214,36246,
36246,34133,34100,34100,34099,32051,32051,32019,32019,34067,34099,38260,38260,38260,40340,38259,40372,42453,38292,36179,34098,36114,34066,31921,
34001,36082,33969,31856,33969,38163,33937,31823,33904,33936,33839,33871,35952,35952,40113,40113,42161,42193,44273,44241,44273,42192,40079,40112,
40177,42258,40177,38097,38064,35984,36017,36017,33937,36017,36050,38130,36050,36050,33970,34002,34034,34002,31954,34035,36115,36148,38229,38261,
40374,40374,40374,40406,40374,38326,40438,42519,38358,38326,36277,38292,40340,46548,48564,35724,20934,18854,16742,14662,16743,20969,29388,33582,
33646,31501,29388,33646,39984,44210,46387,52693,59032,59064,56951,54773,48466,42128,35822,33677,31598,29484,29484,29484,25291,25258,25193,23112,
23015,20934,23014,20902,20902,20902,20902,20934,20934,18853,16773,12579,10466,8386,6306,6306,8419,6338,6306,8420,8420,6373,8518,8551,
4683,4683,4715,4715,2699,619,619,651,651,651,2764,2797,2764,684,651,684,684,684,2732,4812,6828,6827,8875,8875,
8875,10955,13067,13066,21355,38097,50612,52660,50450,39917,39917,44078,31335,22884,22852,20804,20836,22948,23013,25126,25095,20869,18788,20967,
23079,25160,27306,31532,37839,44243,48436,42097,29387,18789,16676,18854,23112,23145,25226,37904,44308,46389,44374,25617,10956,4780,4844,2829,
2829,2797,2829,2829,2829,6990,6990,6926,4845,4812,6893,6893,6893,6893,6861,6893,6925,6925,9006,11054,13134,15149,21390,25551,
27598,31758,29678,25517,21389,19277,17262,15182,15182,13101,15149,15117,15116,15116,17164,21325,23405,25420,25420,27532,31726,31726,29613,25518,
23438,21390,21358,19310,21391,21423,21391,19310,21358,23471,25551,25584,25519,25486,27567,25519,21358,21358,21390,21358,21391,23471,23471,23471,
25519,25519,25520,25552,25551,23471,23471,23471,23471,23503,23471,23536,25682,23634,23602,23634,23699,23700,23667,25747,23667,25747,25779,27860,
30005,30038,29973,27892,34166,36279,34134,34133,34133,34133,34132,34132,32051,32019,32051,32052,34132,34100,34100,38260,38260,38292,40340,38227,
38227,38292,38292,36211,34098,34065,34033,36114,36114,36114,36050,31888,33937,33969,33904,33904,33936,35984,33871,33839,33839,38032,38032,38032,
40048,40048,42129,42161,40080,40080,37999,37999,38064,38064,40145,40177,38097,38097,36017,33937,33937,33937,36050,36050,38131,38163,36083,36050,
34035,31954,34035,36148,36148,36180,38261,38293,40406,42519,40406,38294,38326,38358,40406,42519,40406,38326,36277,36244,38292,40371,48628,48467,
35757,25193,16774,12581,12582,14662,18855,25195,31566,31501,29387,33678,39984,44242,50548,54838,56951,56951,52724,48433,39982,33677,31565,29420,
27340,25259,25259,25291,23177,23113,21032,20999,20966,22982,23015,22982,20902,20902,20902,22983,20934,20934,18853,14693,10531,8418,6338,6306,
6339,6306,4226,6339,8420,6373,8518,10664,6763,6763,4715,4715,2699,2699,651,651,651,651,2732,2764,2732,684,651,684,
684,651,651,2699,6795,6795,6795,8875,8906,10954,13034,19242,35983,50644,50644,50547,50450,46191,35562,29255,29190,22851,20771,20771,
20803,18755,18755,18755,18755,18788,18788,18853,16773,18853,20934,23047,25226,31565,35791,40049,44243,37839,25162,18855,21000,21065,23178,29485,
37937,42195,46453,33971,17230,8907,4844,2829,2829,2829,2829,2797,2796,4845,4877,6925,6893,4812,6861,6861,6860,6860,6861,4845,
4813,6925,9006,11054,11021,15149,19278,21358,23406,27566,23438,21357,21358,17230,15182,15182,15182,13102,15149,15149,15117,15084,17165,21325,
23373,23308,25420,25453,27533,27566,25485,23405,23471,21391,19310,19310,19310,21391,21391,19310,19310,21358,21358,23471,25519,23406,21326,23438,
21390,21358,21358,21358,21358,23439,23438,23438,25519,25551,25519,27632,27664,25519,23471,23471,23471,25584,25585,25617,25682,25682,23602,21554,
21586,23667,23699,27828,25748,25747,25747,25780,27893,30038,29973,29941,32053,34134,34133,32053,32052,32052,34165,32084,32052,32084,32084,32084,
34132,34132,34132,36180,36180,38260,38260,36147,34066,36179,36179,36179,34098,34066,34066,36147,36146,36050,33969,31856,29743,31824,33904,31823,
33872,33904,33872,31759,33839,35984,35952,35919,38000,38000,37968,38000,40080,38032,38032,35951,35951,35984,35984,35984,36017,35985,33937,33969,
33969,33969,36050,36083,38163,38196,36115,36115,36148,36148,34099,36148,38229,40342,40374,40406,40439,42519,40406,40374,40374,40406,40406,42486,
40439,38326,36278,36245,38260,38292,42420,50710,54774,44242,27339,16807,12614,12614,14727,18920,23178,25226,27339,33710,40048,46322,52628,54741,
54740,50579,46289,37902,31564,29419,27307,25194,23114,23113,23145,21065,21032,20999,20967,20966,20934,23014,23014,22982,22950,22982,22982,23015,
23015,20934,18854,16773,12612,10499,6338,4226,4258,6338,6306,6306,6372,6372,6437,8583,10923,8843,6795,4715,2699,2699,619,619,
651,651,2732,2732,2732,2732,2732,651,2732,651,619,2667,4747,6795,8875,8907,8906,13001,17097,31725,50579,54772,50546,44207,
41997,43949,35464,27012,24964,20803,22851,22819,22819,22852,20771,18659,18691,18755,16707,16707,14659,14692,16740,16741,18854,21000,27307,29484,
35823,42195,42196,35825,31598,25260,23114,25227,29485,37871,42227,38164,25584,13036,6859,2796,780,781,2829,2796,2764,2764,4845,6925,
6893,6860,6860,6860,6861,4812,4812,4813,4813,6893,8974,11054,11022,15150,17231,19279,19279,21358,19246,19246,17198,17198,15150,13069,
13069,13069,13069,15117,15117,15117,17197,21358,19245,19180,23341,27534,25454,23406,23406,23406,21358,21390,21358,19310,19310,19310,19342,19310,
21422,23471,21358,23406,23406,21358,21325,19278,21358,19278,19245,19310,21358,21390,23470,23471,25551,25551,27599,27632,29713,25551,23471,23471,
23504,25617,25649,23569,23569,23602,23602,23634,25747,25747,23634,25747,27796,25747,23667,25812,27893,27925,29973,29973,32053,34133,34166,32117,
30036,32084,32117,34197,34197,34229,32149,32117,32149,34197,34165,34132,34100,38293,40373,36147,34066,32018,31986,34098,36179,34066,34034,34034,
34001,31921,31856,31823,31823,33904,33904,31824,33872,33872,33839,31791,33872,33839,31758,33871,35952,38000,35887,35887,35920,38000,35984,35952,
33871,35952,35985,35985,33937,31856,33937,33970,33970,33970,34002,36082,36083,36083,36083,36116,36180,36148,36180,36181,40374,40407,42487,42519,
40438,40439,42487,40406,42454,40406,40406,40406,40439,38326,36278,38325,38292,38292,44500,48629,50645,52694,39984,23049,14694,12614,14694,14727,
14759,19017,27437,33742,37935,44241,50515,50482,48369,44143,35725,31500,27307,25194,25162,23081,21000,20968,21000,20967,20966,20966,20934,20933,
20901,22981,22981,22981,22949,22949,22982,25063,23014,22982,20934,18853,14693,10532,6339,4258,4258,4258,6306,6306,6372,6372,6437,6502,
19310,15116,8811,4682,4715,2667,587,619,651,619,2732,2732,2700,2732,2700,651,651,619,618,2667,4747,6795,8907,8907,
8842,15017,25419,44304,54674,52529,44141,37738,33479,31302,29124,27044,27044,24995,22883,22851,22851,24932,22884,20803,20771,20803,18723,16675,
14627,14627,16675,16676,16708,16741,20935,25226,29452,33710,40050,42131,44277,42164,33712,27405,27372,29517,35889,38132,31858,17229,8907,4876,
2829,781,2829,2764,2764,2764,4844,6925,6925,6893,6860,4780,6860,6861,6893,6893,6893,6893,6893,8974,11022,13070,15182,17231,
17230,19246,17197,17198,17198,15150,17230,15150,13069,13070,13069,15117,15117,15149,17230,19277,19213,19212,21261,23373,23374,21293,21325,21326,
19245,21358,21391,19310,19278,19310,19342,19310,19310,21390,23471,21390,21358,21358,19278,19278,19310,19278,19278,19310,21358,21358,23470,25551,
23471,25519,27631,27631,27632,25551,23471,25552,25584,25617,25617,23569,23569,23569,21521,23602,25715,25747,25715,25683,25715,23666,23667,25812,
27893,27925,27893,27860,29973,32053,34165,32117,32117,32117,32117,34197,34230,34229,32149,32149,32117,34165,34197,34165,34099,36212,40406,34099,
32018,32018,31985,34098,38292,36147,31953,34001,31921,31888,31856,29775,31791,31759,29711,31791,33872,33872,33872,31791,33872,33872,31791,33839,
33871,33839,33807,33839,35919,33839,33839,35952,35984,38065,38098,36017,31824,31857,33969,31889,34002,34035,33970,33970,34002,34002,34035,36148,
34068,34067,36213,40406,40407,38326,40406,40438,40406,40471,42487,42486,42487,40406,40406,38326,38326,38326,38358,40373,40339,42419,42386,42321,
44369,48532,42096,25226,23113,21001,16807,16807,18887,25291,33775,33742,37870,42096,48370,46224,42030,37772,31434,27274,27241,25129,23048,23015,
20967,20934,20966,20966,20934,20933,20901,20901,20900,22949,22981,22981,22949,20869,22949,22982,22982,20933,20901,18821,14660,10500,6306,6339,
6339,4258,4226,4226,6339,8453,6405,6438,21358,15117,8810,4650,4682,4715,2667,619,619,651,683,684,651,2732,2699,2699,
651,619,618,2698,4747,6795,8876,8875,10890,14952,29547,48401,54609,48205,37672,31333,29155,27010,29091,29124,29188,31269,29189,27076,
24931,24931,24964,24964,24964,24996,20835,18722,16674,16674,16675,16675,16675,16676,16709,20935,23080,25226,29452,29517,35824,44244,46390,42164,
33776,31599,31664,38035,38164,23504,10988,6925,4877,2797,2829,2797,2797,4845,4845,4845,6893,6893,6860,4780,6860,6861,6893,6893,
6925,6893,4812,8973,11054,11022,13102,15150,15150,17230,17198,15117,15117,13069,15150,15183,15150,13070,13069,15118,15150,15150,17262,17197,
17197,21294,19213,19180,19213,21293,21326,21326,19245,21358,21391,19310,17230,19310,19310,19278,17197,19310,23471,23471,21390,19310,19310,19343,
19310,19278,19310,21390,21390,21358,23438,25551,25518,25550,29712,29680,27631,25519,23471,23503,25584,25584,25617,25617,23569,23569,23569,21521,
23634,25747,25715,23634,25715,23667,23667,25747,25780,27893,29973,27892,27860,29940,32085,32150,32117,30037,30037,32149,32149,32149,32084,32084,
32116,32084,32084,34132,32051,32051,34132,34131,32018,32018,31986,31986,36179,34099,31985,31921,31888,31888,31856,29775,29711,29678,29678,29678,
29711,31792,33872,33904,33904,33904,31791,31791,33839,33839,33839,31759,33871,33872,33871,33904,36017,40243,36050,33937,31856,31889,31921,31922,
34035,34002,31922,34002,34035,36115,36148,36180,34100,34132,38326,42552,40439,38294,40406,40438,40439,42584,42519,42487,42487,40438,38390,38358,
38358,38358,38325,42452,42386,42353,42287,40206,44335,44304,35788,27305,29419,27339,25226,27339,29484,33743,37968,35790,33709,39918,42064,39918,
35692,31434,27208,27208,25128,23015,23014,22982,20934,20933,22981,20933,20901,20901,20900,20868,22916,22948,22948,22949,22949,22917,22949,22981,
22981,20901,20901,16772,12579,10467,8386,6339,6371,6339,4226,2113,4226,6404,6405,4357,12971,10891,6729,4650,4682,2667,2667,619,
619,651,651,683,651,651,2699,2667,2667,2699,2699,4746,6795,6827,6795,6795,10890,14952,27434,46223,52431,46027,33413,27074,
27042,29155,31269,29123,24995,29221,31302,29189,27044,27043,27043,27011,27044,27077,27077,24996,22916,20803,18722,16642,16611,16643,16676,16676,
16709,20934,23112,25258,29452,33710,40083,44309,44309,40083,31728,33809,38132,29746,15149,8941,6925,2797,2797,2797,2797,4845,4812,4812,
4845,6893,6860,4812,4780,4780,6861,6861,6925,6925,6860,8973,8974,11021,11021,11021,11021,15150,15149,13069,13069,13069,13069,15118,
13070,13070,13037,13069,15150,17263,19343,17263,17198,19246,19213,19213,21293,19245,21326,21326,19278,19278,19311,19311,17230,17263,19310,19310,
17197,17229,19342,21423,21423,17262,19310,19343,19310,17230,19310,21423,21391,21358,23438,25518,27598,31792,36018,29744,27631,25551,25519,23471,
23503,23536,23536,23536,23569,23601,23601,23569,23602,25715,23602,25747,25747,25715,23634,23667,23699,25747,25812,27860,25779,25747,29940,32086,
32086,30037,30037,32149,32182,34229,34229,32116,32116,32084,30036,32084,32052,32051,34131,34164,34099,29937,31986,31986,34066,34066,34034,31953,
31888,29808,29808,29776,29743,31791,29743,29711,29743,31824,33936,36017,33937,33904,31824,33904,33936,33904,35985,33904,33904,33937,33936,33904,
33937,36050,33937,31889,31857,31889,31889,34002,31954,31922,31954,31954,34067,36116,36180,36180,36180,36245,36278,38358,38326,38326,40439,40439,
42552,44697,44600,42519,42551,40438,40438,40438,40470,40437,40404,44499,46546,46480,44301,44269,44270,42093,33545,27272,29418,25225,27371,31564,
33677,35855,40049,37903,33677,33644,37805,35660,31434,27208,25095,25095,25062,22982,22981,22949,22948,22948,22948,22948,22948,22948,22948,22916,
22916,22916,22916,22916,22917,22917,22949,22949,22948,20900,18820,16740,12547,8386,6338,6306,4291,6339,4258,2113,2113,4291,6405,4324,
8777,6762,4681,4682,4747,2667,2699,651,619,651,683,651,651,2699,2699,2699,2667,2667,4714,4714,6795,6795,6795,6827,
8874,15017,23305,35722,39754,35462,29123,27010,29188,33414,31301,25028,24996,27108,27108,27011,24930,27043,29156,29124,27011,27012,27044,27045,
25029,22916,20835,18690,16642,16643,16611,16643,16676,18788,20965,23111,25192,27338,31629,37968,42195,42196,35889,33776,35955,36020,21391,10956,
6893,4812,2765,2797,2797,4845,4844,4812,4812,4812,4812,4780,4780,4780,6861,6893,8974,9038,8973,8973,8973,8973,8973,8973,
10989,11021,10989,13069,13102,13069,13037,13070,13070,13070,13070,11021,13102,17263,17263,17263,15118,15085,17101,19181,19214,17165,19245,19278,
19311,19311,19311,19343,17263,17263,17230,19310,17230,15149,17262,17262,17262,17262,17262,17262,17230,17198,19278,21423,23439,21358,23438,25518,
27598,31791,35985,31792,27598,27599,25551,25519,23471,23504,25584,23536,25617,25649,23569,25650,25682,25650,25682,27796,25747,23635,23634,23634,
23635,23699,23699,25780,25811,25779,27892,29973,30005,30070,32150,32150,32182,32182,32181,32149,30036,30036,32117,32117,32084,32051,32083,34131,
34099,29938,31986,34099,34099,31986,31954,31953,31921,29808,29808,29808,31856,31824,31824,31824,31824,31824,31856,36017,36050,36017,33937,36018,
33969,33969,38163,36050,33969,36115,38163,33970,33937,33970,34002,34002,34002,31922,31922,31954,31954,31954,31954,31955,34035,34068,34100,34100,
34133,36245,36246,36278,36278,38358,38358,40439,42551,42552,42519,44599,44600,42519,40470,40470,42550,42517,44531,48659,52818,52687,48396,46284,
48301,39850,33480,31400,31466,27273,25193,27338,27371,31629,40049,37936,33645,31499,31499,29386,27175,25062,25030,25029,25029,25029,22948,22948,
22947,22947,22947,22947,22947,22947,22948,22915,22883,22916,22916,22916,22916,22916,22916,22916,20868,20868,18787,16675,12547,8386,6306,6306,
4258,4258,4258,2146,2114,2146,4292,4292,4616,4681,2601,2601,2666,618,619,651,651,651,2732,651,651,2699,2699,2667,
2667,2667,4714,6762,6795,6795,4746,4747,6826,12969,17031,21029,22948,22851,22850,24898,29189,35560,31399,29286,27174,25028,24963,22849,
24898,27043,29156,29124,27011,24931,24931,22851,20803,20803,22884,20836,18755,18723,16642,16611,18723,18755,18787,20932,20933,20998,23144,27403,
33742,37969,40115,38002,35889,38067,29745,15117,8908,4780,4813,2797,2797,4845,4845,4812,4812,4812,6893,6893,6861,6860,6861,8973,
9006,9038,9006,8941,8941,8974,8974,8974,11021,11021,10989,11021,13069,10989,13069,13070,13070,13070,13070,11022,13070,13070,13037,13069,
15085,15053,15053,15053,17133,19246,19311,19279,19279,17230,17231,17263,17199,15150,15182,15182,15182,15149,15181,15149,15149,15182,17263,19343,
17263,15149,17230,21391,23439,23406,25486,25517,27597,27565,29646,29678,27630,27631,27631,25583,23471,23504,25584,23536,25649,27730,27698,25650,
27730,25682,25682,25682,23602,23634,23635,23602,23667,25780,23731,25779,25811,27859,27924,27925,27925,30070,30102,30101,32182,34262,32117,32084,
32117,30036,30036,30036,30036,32084,32116,34132,34132,34132,34099,31986,31954,31954,31986,34067,31954,31889,29808,29776,31856,31856,33969,38163,
33937,31792,31856,33904,31856,33937,33937,34002,34002,34002,36083,36083,34034,36115,36148,36083,36083,34003,34002,31954,31954,31922,31922,31987,
31987,31954,29906,32019,34100,34100,32020,34100,34133,34165,34230,36311,38391,38358,38358,38358,40438,42487,44599,44600,44599,42518,42486,42518,
42517,42516,44499,48626,54866,54767,50476,52525,54543,41833,31270,31336,29353,27273,21031,23112,25290,29516,33775,35790,31596,29386,27273,27208,
23014,22949,22949,25029,25028,25028,22947,24995,24995,24995,22947,22947,22947,22915,22916,22883,22883,22916,22916,22916,22916,22884,22883,22883,
20835,18755,16675,14627,12546,8386,6274,4226,2178,4258,4291,4259,2178,2114,2147,4259,2569,2601,2634,553,586,586,586,2699,
651,619,2731,2699,619,619,2699,618,619,2699,4747,4714,6762,6794,4714,4746,6794,10889,14952,18982,18788,20836,24996,24996,
27109,31400,35691,31465,25094,22948,24931,22850,22850,27043,29124,27011,24930,22851,22851,20771,18691,18690,18723,20836,20868,20868,20836,18755,
18723,18722,16674,16674,18787,18819,18885,20998,25257,31629,37969,42195,42163,35954,33906,23471,13037,6860,4812,4845,2797,4812,4812,4812,
4812,4812,4845,6893,6893,6893,6861,6925,8974,8974,11054,8941,8909,8974,11022,8941,8941,10989,10989,13070,11021,10989,13070,13070,
13037,13070,13070,13070,13070,10989,10957,13037,15118,15053,15085,17134,17166,19278,19311,17231,19311,17263,15150,15151,17231,15183,15182,15182,
13134,13101,13101,13101,13101,15150,17262,17295,15182,15150,17230,21359,21358,23406,25518,25485,25484,27532,27564,29645,27598,27630,27631,25551,
25551,23503,23504,23536,25616,27730,27730,25617,23537,23537,23570,23602,23602,23602,23602,23602,23635,23700,25812,25812,25811,25811,25844,25844,
27925,27989,28022,27989,28021,32182,34230,34198,32149,32117,30036,30036,30036,30036,32084,34164,36245,36212,34132,32018,29906,31954,29906,31954,
29873,29841,27696,27663,29776,31889,36082,36115,31889,31856,33969,31856,29743,31856,33970,36083,34035,34067,34035,34035,34067,34067,34035,31954,
34003,34035,31954,31922,31922,29842,31954,34067,32019,32019,31987,32020,34068,31988,32020,34133,34198,34165,32149,36311,36343,38391,36310,38358,
40438,42487,42519,42519,44599,42518,42485,42485,40437,42517,44531,46546,50673,54768,54703,52526,48236,37640,31270,29190,25063,20999,18919,21032,
23177,25291,29484,31630,31597,25258,25192,25159,23014,22948,22948,25028,25028,25028,25027,25027,25027,25027,24995,22947,22915,22916,22916,22884,
22883,22884,22884,22884,22916,22916,20835,20835,20835,18755,16642,14594,10466,8386,4226,4226,2178,4226,6339,6340,4227,2147,2146,2179,
521,521,2602,2602,2634,554,586,2667,2667,618,651,619,618,618,2699,619,586,2699,4747,4715,6762,6762,6761,6762,
6762,10825,17034,29549,27273,20837,22917,24997,27110,29319,31497,29352,25094,22916,24964,24931,22850,27043,27043,24930,22850,20770,20803,20771,
18723,16643,16610,14562,18755,20868,22949,22981,20836,18723,18690,16642,16674,16707,18820,20966,23112,29419,35758,35824,40082,38034,33938,27632,
15149,8908,6860,4845,4845,4812,4812,4844,4812,2732,4780,4812,6861,6893,8941,6893,6893,8973,8941,8974,8941,10990,11022,8974,
11021,10989,10989,10989,10989,13070,13102,13070,13070,13070,13038,13070,13070,13070,10989,13037,15118,15118,17166,17166,17166,17230,17231,17231,
19312,19312,15151,15183,17231,15151,13102,13134,13101,11020,11021,13133,13133,13102,15150,15182,15182,17230,17230,19278,21359,23438,27566,27565,
29677,31725,29645,29645,27565,27598,27631,25551,25584,25552,23504,25584,25584,25584,23471,23472,25585,23537,23537,23570,23570,23570,23570,23635,
21554,21587,23699,25812,23731,23699,25811,25812,25844,25909,28021,27989,30069,32150,34295,34231,34198,32150,30069,30036,27955,30003,32051,34132,
36213,36180,32019,32018,31986,29905,27792,27760,29841,27728,27695,29776,29776,31889,31921,31856,31888,31856,31856,31856,31856,31856,36083,40374,
38229,36148,36181,34067,31987,34100,31987,31922,31922,31955,31955,31954,31954,31954,31955,31987,32019,32052,32052,32052,32020,31988,32020,34133,
36279,36343,34230,34230,36342,36310,36310,38358,40471,42519,40438,42486,42519,42486,40405,40438,40437,40469,42517,44499,46481,50608,52689,50511,
44075,33479,27141,22917,20870,20935,16806,18887,21065,23178,25291,29516,27436,23177,23112,23079,23013,20900,22948,22947,25060,25060,25059,25059,
25028,25028,22948,22948,22916,22916,22884,22851,22852,22851,20803,22883,22915,20835,20835,20835,18787,18723,14594,12514,10434,6306,4226,2178,
2178,2178,4291,6372,4292,4259,4227,4227,489,522,522,521,2602,2634,2634,2667,2667,619,2667,619,586,651,2699,619,
619,619,2699,4747,6762,6762,8809,8809,10858,19212,33841,46421,39951,23014,22917,24964,25029,27206,29351,27206,25061,24964,24964,24931,
24963,27043,24930,22850,20802,20803,20803,18723,16675,16643,14530,12481,14530,16675,20836,22948,22948,22917,20804,18723,18723,18723,18756,20901,
23047,25161,31500,33710,35823,37969,38099,31825,21358,10956,6860,4845,4877,4845,4812,4845,4812,2764,4780,4812,6860,6860,8941,6925,
8974,11054,8941,10990,11022,11022,11022,11022,11022,10990,13070,13070,10990,13070,13070,13102,15150,13037,10989,13070,11022,13070,10989,13037,
13037,15085,15086,17134,15118,15118,15150,15118,15150,15151,15151,15151,13070,13070,13102,11054,11053,11053,11021,11053,13134,15182,15150,15150,
17263,19376,19311,19310,23439,25519,29679,31790,35951,35951,31725,29677,29678,27598,27598,25551,23471,23471,23471,23504,23503,23503,23472,23439,
25585,25618,23537,21489,21489,23570,23570,21554,23635,21586,21586,23699,23699,23698,25811,25811,23731,25876,30102,27989,27989,30102,32151,34166,
34198,32150,30069,30036,30004,29971,30003,32084,34132,34067,31986,29938,29906,29873,29873,29873,29841,29808,29776,29776,29776,29776,29743,29776,
31888,29776,31824,33937,31856,31889,38228,42455,40374,34100,36181,34133,32019,31987,31987,31955,31922,29874,29874,31955,32019,34068,32019,31987,
32019,32052,36246,36278,32020,32020,32085,34166,34231,36311,34263,34263,34295,36342,36310,38391,40439,42519,42519,40438,42486,42486,40438,42519,
38390,38390,40437,42484,42386,42288,44335,46350,42093,39916,29320,20868,18789,16774,16742,14693,16806,16871,21065,23178,23178,23145,21064,23079,
20933,20900,22948,22947,25060,25092,25059,25059,25027,22980,22948,22948,22916,22884,20835,20803,20804,20771,20771,20803,20835,20834,20835,20835,
18787,16675,14562,10434,8354,4226,4226,2178,2178,2178,2178,4291,6340,4292,4259,4227,522,522,489,489,2602,2635,2635,2635,
2667,2667,619,619,586,619,2667,619,619,618,2667,2699,4714,6794,8810,10825,19180,38067,50679,58968,46225,29255,27077,25028,
25028,27205,29319,27206,25061,24996,24963,24963,29156,27044,22818,20770,20802,20803,20771,18723,16676,14563,12482,14530,14530,16675,18755,20803,
20835,22884,20837,20836,18723,16643,16643,16708,20901,23079,25225,31564,37904,35855,38001,35986,25519,15084,8876,6860,4812,4844,6893,6893,
4812,4812,4780,4780,4780,4780,6860,8973,8974,8974,11054,11022,10990,11022,8909,8941,10989,10989,13102,15183,11022,10990,13070,13102,
13070,10989,10989,11022,11022,11022,10989,13070,13070,13038,15086,15118,15118,13038,13070,13070,15118,15151,15151,13070,13070,15183,13135,11054,
11054,11086,11086,11053,13134,15214,17263,17262,17230,19375,21391,21358,25486,29678,35952,40144,40111,35917,31757,29677,29678,27598,25517,29711,
27632,21390,21391,23471,23471,23471,23504,23504,23536,23537,21456,21489,23569,23569,23570,21554,23634,21618,21586,23698,23698,23698,25778,25811,
25779,27924,30102,27989,27989,27957,27925,30005,30038,32150,32117,30036,30036,30003,29971,32084,32052,31987,29906,29906,29906,29906,27793,27761,
27793,27793,27793,27761,27760,27728,27727,27728,29808,29808,31856,31889,31856,31921,38228,40374,38293,34132,32019,32019,32020,32020,31987,34100,
34036,29842,31955,34068,34068,34068,34100,32020,31987,34100,36246,34198,32053,32085,36278,36311,34198,34231,36311,34262,34262,36310,36310,38358,
38391,42551,42552,40438,40438,40438,42486,42552,38358,38325,40405,42485,42387,42321,40110,35819,35754,39948,42029,31465,20901,16708,14628,12548,
10468,10533,14759,18952,19017,21064,21031,21031,20965,20900,20900,23012,23012,25059,25059,25059,22979,22947,22948,22948,20868,20835,20803,20803,
20804,20771,20771,20803,20803,20803,20803,18755,18723,14562,10434,8354,6306,4226,4226,4226,4226,4226,2178,2178,4259,6340,4292,4227,
554,521,489,489,2602,2635,2635,2635,2635,587,586,586,586,586,618,618,618,2666,2666,2634,4714,8842,10858,14954,
29647,48500,52661,54643,48206,35463,29156,27108,27108,29253,31334,27173,27140,24995,27010,29156,31237,27011,22850,22818,22851,22851,20771,18755,
16708,14595,12515,12514,14530,16643,18788,18755,20771,18755,18756,18756,16675,16675,16675,14627,18820,23079,25192,27306,31564,35823,35888,38001,
31760,19213,10924,6860,4812,4812,6892,6893,4845,4812,4780,4812,4813,4812,4780,6861,8941,8941,8973,8941,8941,11022,8909,8909,
8909,10989,11022,13102,11022,11022,13102,13070,10989,10990,10990,8942,11022,10990,10990,10990,10989,15183,15183,15150,15151,13070,13070,13070,
13103,15183,15151,15151,15183,15216,13135,11054,13134,13166,13166,11054,13102,15214,17263,17262,17198,19310,21358,23470,27598,31758,38031,46385,
48432,42190,35950,31757,29645,27598,27630,29743,27631,21357,19310,21391,21423,21423,21424,21423,21456,21488,21456,21456,21489,21489,21521,23602,
21554,21586,21618,23698,25811,27891,25810,25811,27891,27956,30037,30037,27989,27989,27957,27925,27924,30037,30036,29972,29971,32084,32084,29971,
27891,27826,27794,27826,29874,29874,27761,25680,27761,27793,27761,27761,27760,27793,29841,27761,29841,31889,29809,31857,34002,34034,36180,38293,
36180,34132,32052,29939,31987,31987,31955,34069,34068,29875,29875,31988,31988,31988,34101,34133,32020,34101,34133,32053,34166,34198,34230,36343,
36311,38424,38392,34230,36342,36343,36310,36310,38423,40504,40471,40438,40438,40406,40406,42519,40406,38325,40373,40340,42355,42257,37998,37932,
40013,42126,44175,35659,23014,18789,16741,12580,12548,10500,10533,14726,16871,18951,18951,20998,20965,18852,18819,20932,23012,23011,25059,25027,
22946,22947,22915,20867,20867,20835,18755,18723,20804,20804,20803,20803,20803,20803,18755,18755,16643,12482,10434,8354,6274,4226,4258,4258,
4226,4226,4226,2178,4227,6340,6340,4259,554,522,489,521,2634,4715,4715,2635,2634,586,586,586,586,586,619,618,
2666,2666,2634,2633,6762,8874,10857,21291,40112,44143,41899,43914,50188,41768,31268,29155,27108,31301,29253,29155,29155,27042,29123,31236,
29124,26979,24898,22851,22883,20803,20771,18723,14627,14595,14595,12515,12514,16675,18788,20835,18723,18723,18755,18755,18723,16675,16675,16707,
18788,20966,23111,25193,27306,31564,35823,35920,33873,25487,13004,8876,6860,4812,4812,6893,6893,4812,2732,2732,4812,4812,4780,6860,
6861,8941,8941,8941,11022,11022,11022,8909,8909,10989,11021,11021,11022,10989,10990,11022,10989,11022,11054,11022,10990,11022,10990,10990,
11022,13070,15183,15183,13103,13070,13102,13102,13135,13135,13103,13103,13103,15215,13134,11054,13166,13167,13166,13102,13101,15182,15182,19310,
21390,25584,25583,25583,29710,33871,40144,48530,52723,48497,40143,35918,29678,27598,29679,27599,25518,21358,19310,21391,21423,21456,21424,21424,
21424,21488,23569,21489,21489,21457,19409,21522,21554,21586,21618,25779,30037,38423,34197,27891,27892,27957,27957,30069,27989,25877,25876,27925,
27924,30004,32085,32084,29971,30003,30036,27923,27891,25746,25745,29939,29907,27761,25681,25648,25680,27761,27794,27761,27793,27825,29906,29874,
31889,31922,31954,34002,36115,36115,36148,36212,34132,34132,32084,32020,29907,31955,34068,34036,31955,29875,29875,31988,31988,31988,34068,34101,
34101,32020,32020,32085,34166,34230,34231,36311,36311,38392,38392,36310,36310,36343,38391,36311,38391,40471,40439,38391,38390,40406,40439,40439,
38325,38325,38293,38227,42355,42289,44337,48530,52724,52659,42062,35659,27208,20902,18821,12580,12548,10500,10500,10500,12613,14758,18918,18918,
18885,18820,18819,20900,23012,23012,23011,25027,22946,22914,20867,20835,20867,20868,20803,18755,18755,18755,20803,18755,18755,18723,18755,16675,
14562,10402,10402,8354,6274,4226,4258,4258,4226,4226,4226,4226,4226,4259,6339,6339,521,554,522,522,2634,4715,4747,2635,
2634,586,586,554,586,2667,2667,2667,2666,2666,2634,2633,6762,8842,10856,27565,44305,41964,37575,43783,50154,43848,31268,29156,
29221,29253,29188,29155,31203,31203,33284,29124,29059,26978,24898,24931,22884,22883,18755,16643,14595,12515,14595,14595,12514,14595,18788,20901,
20868,18755,18723,18723,18755,16675,16675,16707,18788,18853,23046,25192,27305,27370,31596,33742,35889,31761,19245,8876,6860,6860,4812,6893,
6957,6925,4812,2732,4780,6860,4780,4780,6860,6861,8941,8941,8941,8941,10989,8941,10989,8909,8909,10989,11021,10989,10989,11022,
10989,11022,13103,11055,11055,11022,8942,11022,13103,13102,11022,13070,11022,11022,13102,13103,13135,13135,13135,13102,11022,15183,13167,13102,
13134,15247,17327,13134,17262,19343,17262,19375,23503,27696,27664,27631,31791,35983,42224,50675,56916,50610,42191,35918,31758,29678,29679,25518,
23438,21423,19343,19343,21456,23537,21456,21456,21456,21489,21521,21489,21489,19409,19376,19409,19506,21619,21650,23698,27924,36343,34230,27892,
25844,27957,27957,25908,25877,25844,25812,27925,30005,32084,36310,40439,34132,30003,30003,27923,27891,27859,25746,27826,27859,27826,27794,25713,
25713,27794,29875,27729,27761,27793,29874,29874,29841,31955,34067,34035,34067,34099,36180,36212,34099,32051,32052,29971,31987,29907,29875,31955,
31955,29875,29875,31956,31988,31956,32020,32053,34101,32053,32020,32085,34166,34198,34230,34230,34230,34230,36311,38391,36278,34230,38391,38391,
38391,38391,38391,38391,38390,38391,38391,38358,36277,38325,38292,40340,46580,50774,54902,52789,52724,52660,44143,33482,29256,25095,18789,14628,
12548,10468,10500,10468,10500,12645,16838,18918,18853,18820,18788,18820,20900,22980,22980,23011,22979,20866,20867,20867,20835,20835,20835,18755,
18755,18723,18755,18755,18755,16674,16675,14595,12482,10402,8322,6274,6274,4226,4258,4258,4226,4226,6275,6274,4226,4226,4259,6339,
521,522,554,522,554,2635,4715,2635,586,586,586,586,586,2667,586,586,618,586,2634,2634,4682,6761,8776,23403,
42192,46189,43913,43815,41767,37574,31301,27205,27271,27238,29286,33414,35429,35429,33349,29123,27011,26978,24898,24931,22884,22884,18755,14563,
14595,12515,12547,14627,12514,12514,16739,20901,20900,20868,18755,18723,18723,16675,16643,16643,16675,18788,23046,25160,25193,27305,29451,31596,
31695,33873,25519,13004,8908,6892,6892,6925,6925,6925,6893,4845,4812,6860,4780,4748,6828,6860,6861,8941,8941,8909,8909,8941,
10989,8908,8908,10989,10989,8941,10989,8941,8909,8941,10990,11055,11087,11055,8974,11055,11055,11022,11022,11022,13103,11054,11022,11022,
11054,13102,13103,11054,11054,13135,15214,13134,15214,17295,17327,15182,17294,19407,19343,19342,21422,21390,23437,25550,29710,33870,38030,44303,
48529,46416,40079,33806,29678,29678,27599,25518,23470,23503,21456,21424,21456,21489,21489,21456,19408,21456,21489,21489,21489,19409,19408,19441,
19474,21619,21618,21618,27891,30004,29972,25844,23763,25844,25909,25876,25876,25844,23764,25877,30037,32117,38358,40471,34164,29971,27923,27923,
27923,29939,25778,25746,25778,27859,29972,29907,27794,27794,29875,27762,27794,29906,29907,29874,31954,31987,34035,31987,34099,36180,34132,34132,
34100,32019,32019,29971,29939,29939,29907,31955,29907,29843,29907,31988,29908,29907,29908,32053,34133,32053,34133,36279,34231,34230,34230,34263,
36311,34231,36279,38391,36311,36278,36311,38391,38392,38424,38391,38391,36310,38423,38391,36277,38357,38325,40372,46613,50807,54967,57015,52822,
48499,42095,39853,35562,29191,25063,18789,16741,12580,12548,12548,10467,10467,10532,14725,18854,18821,18820,18788,18788,20900,23012,22980,22979,
22947,20866,20867,20867,18787,18787,18755,18755,18755,16675,16675,16707,16675,16674,14595,12515,10434,8322,6274,6274,6274,6306,6306,4226,
6274,6307,8387,8355,6274,6274,6306,6339,521,522,554,554,522,2602,2635,2634,554,554,554,554,586,586,554,586,
2666,586,586,2634,2634,4681,6728,15049,29644,37835,37640,37574,33478,31431,27271,23143,23144,21095,25224,35658,37673,33414,29221,27044,
24931,24930,24931,24931,22883,20836,18723,14595,12547,14595,14627,14627,12514,12514,14627,18820,20900,20868,18755,18755,18755,18755,16675,16675,
16675,16675,20901,23047,25160,25192,25225,29451,31565,31727,29712,19245,13036,8940,6860,8941,6893,6925,6893,6893,6860,6860,4780,6828,
6860,6861,6861,8909,8909,8909,8876,8876,8909,10957,8909,10989,8909,8941,11022,8909,8909,8909,8909,8942,11055,9007,8974,9007,
11088,11055,8974,11055,13135,11054,8941,11054,11054,11087,13135,11054,11054,13102,13134,15182,15182,17327,17295,17262,17262,19342,19342,19310,
19309,19277,21357,25517,25517,29677,33837,35884,38029,40078,35886,29645,27598,25518,25486,25486,23438,21423,21489,21489,21489,21489,21457,21456,
19376,19408,21489,21521,21489,21489,19441,19441,19474,19506,19506,21618,25812,27892,27892,25844,23764,23763,23796,25909,25909,25877,25844,25876,
27957,30004,34133,36245,32084,27890,25842,27923,27923,27859,25778,25810,25811,25779,29972,29940,27794,27762,27762,27762,27827,29972,32020,31987,
34068,32019,31954,31987,34099,34132,36212,34164,34132,32051,29971,29971,29971,32020,32020,29907,29875,29875,29907,29908,29907,27827,27827,29940,
34133,32020,32053,36279,36343,36311,34231,36344,36376,36311,34230,36279,36278,36279,36279,36311,36343,38424,38391,36310,36310,36310,36310,36277,
38325,40404,44565,52919,57048,57047,59095,57015,50612,39917,35562,33449,31271,25030,20869,16740,14660,12580,12548,10467,8387,10499,14693,16805,
18821,18788,16708,16708,18820,20900,20900,20899,20899,20867,20867,18819,18787,18755,16707,16707,16707,16707,16675,14626,14627,14594,12514,10434,
8354,8354,6274,6274,6306,6306,6274,6274,6274,8387,8387,8387,8354,6274,6306,6339,522,554,554,554,554,554,2635,2602,
522,554,586,554,554,554,554,554,586,586,2635,2635,586,2634,4681,8744,17098,29580,31594,31465,27336,23176,16935,12775,
12775,12775,14823,25289,31595,29384,25126,22980,22915,24963,24931,24931,22883,20803,16675,14595,12514,12547,14595,12547,12546,12546,14626,18787,
20900,20867,18722,18722,18723,18722,16674,16674,18755,16675,16675,18788,23014,25127,25160,25225,29387,31598,31728,23406,15052,10988,8908,8908,
8941,8973,6893,6893,6925,6892,6860,6860,6861,6861,6861,6860,8909,8909,8876,8876,8909,10989,10989,10989,10989,11021,11022,11022,
8941,8941,8941,8974,8974,8975,9007,9007,9007,9040,9039,9007,11022,8974,11022,13135,13135,13135,13135,11054,11022,11054,13134,13134,
13102,17263,19375,19375,17262,17197,17229,17261,19277,19244,19309,21357,23404,25484,31790,35950,33804,33772,31725,27565,25485,23405,23406,23471,
21390,19342,21456,19408,21489,23570,21489,21489,21456,19408,19408,21456,21489,23602,21489,19441,19473,19473,19473,19506,21586,23699,25812,25844,
25877,23764,21716,25909,25941,25877,23796,25844,27924,27892,29940,29939,27891,27891,27923,25875,27923,27859,25746,25810,27891,25811,25746,25746,
25682,25714,27795,27795,27827,29940,32052,32020,29907,31955,31987,29939,32019,34132,34132,34132,32019,32051,32052,29971,29939,32020,32020,29939,
29907,29907,27795,27794,27794,27795,29940,32053,34166,34133,32053,34166,36311,34263,34231,36376,36376,34263,34230,34198,34198,34198,34198,34231,
34231,36279,36311,36311,36310,36310,36310,38357,40372,42484,50806,54966,57014,59094,59095,59030,56853,50450,44013,35529,31336,29223,23014,20933,
18886,16773,12580,10467,8354,8419,14693,16805,16741,16708,16708,14660,16740,18820,18820,20899,18819,18819,18819,18787,18755,16707,16675,16675,
16707,16707,14627,14595,12547,12514,10434,8354,8354,6306,4226,4226,6306,6306,6274,6274,8322,10435,10435,10402,8354,8321,6274,8387,
554,554,586,2635,2635,554,554,554,522,522,586,2635,554,521,554,554,554,586,2634,2666,2634,2602,4682,4649,
6664,15018,21259,21194,17000,12840,8679,6632,6632,6632,8680,10695,17001,21161,23176,23078,22980,22947,22915,22883,22883,18723,16643,14563,
12515,14563,14595,14595,12546,12546,14626,16739,20900,20868,18755,18722,16642,16642,16642,16642,16675,18723,16642,16643,16675,20901,23079,25160,
27273,29419,29582,27534,17132,10924,10956,8876,8908,8941,6861,6860,8941,6893,6892,6860,6828,6860,6860,6828,6828,8909,8876,8876,
8909,10989,10989,10957,10989,11022,11022,11022,8941,8909,8909,8974,8975,9007,9040,6960,6927,9040,9040,9039,11022,11022,11054,13135,
13167,13135,13135,13135,11087,13135,13102,13101,13102,15182,19343,19375,17262,15149,15149,17262,17261,19309,19309,19277,21357,23437,27597,31758,
31725,29645,29646,25518,23406,23438,23471,23471,23471,19310,19343,19376,19376,23569,23602,21488,21456,19376,19375,21488,23569,25650,21489,19408,
19440,19473,19506,19506,19506,21587,23699,23764,23796,23796,21716,23829,25909,23796,23764,25876,27925,27892,27892,27892,27859,27891,27923,25875,
25843,25810,25778,27891,27891,25746,23633,23634,25747,25747,25714,27795,27859,29940,29940,27827,27794,29874,34133,34132,29938,32051,32019,32019,
32051,32019,32019,29939,29907,29971,32020,29907,29907,29908,27827,27794,27827,29907,32053,36279,34198,38392,36311,34198,36311,34263,34231,36343,
36343,34263,34231,34230,32117,32118,32150,34231,34231,34199,36279,36311,36311,36278,36277,38357,42420,48693,54966,56949,54771,52657,52625,50448,
48303,46125,39754,33415,31303,29287,27239,25191,23112,20967,14693,10499,8387,10499,12645,14725,16740,16708,14660,14628,16740,18820,18820,18820,
18819,18787,18787,18787,16707,16675,14627,14627,14627,14594,12546,12514,10434,10434,8386,6306,6306,6306,4226,4226,6306,6306,6306,8322,
10402,12515,14627,12514,12482,10434,6273,6339,586,554,587,2667,2635,522,522,522,554,522,554,2634,554,522,521,554,
586,553,553,2666,2634,2569,4650,6730,4585,4552,6664,6664,6664,6665,4617,4650,2569,2569,4617,4584,4519,10727,16935,21063,
20998,20900,20835,22883,20835,18723,14563,14563,14563,14563,14595,14595,14595,14594,14594,16707,20868,20900,20835,18722,18722,18755,18723,18722,
16642,16610,16642,16643,16675,18788,20901,23014,25127,25193,27371,29647,23406,13004,10924,10924,8876,8909,8973,6861,6860,6860,6860,6828,
4780,4748,4748,4748,6796,8909,8909,8909,10989,10989,10989,8909,8909,8909,8941,11022,8941,6828,6828,8942,8975,6927,6959,6960,
6960,6959,9007,9007,8974,11054,13135,13103,13135,13135,13167,13167,13167,13167,11054,13101,15182,15214,17263,19343,17262,17230,15149,15181,
17229,17262,19310,19309,21357,21357,23437,27598,29678,27565,25486,23406,21358,23471,23471,21391,21423,21391,19376,19408,19343,19376,21488,21456,
21423,19375,21455,23569,25649,25649,23569,21488,19441,17393,19506,19506,17458,19506,19571,19603,21716,21748,21716,23828,23796,21716,21683,25844,
25877,25844,25844,27892,27892,25843,27923,25875,25810,25778,27859,30004,29972,25746,23665,25714,25747,25714,25715,25714,25747,27859,27859,25746,
27794,29907,34133,34197,29971,29971,32051,29971,32051,29971,29939,29906,29939,32052,34165,32020,29940,31988,29940,29940,32021,32085,32085,34134,
36279,38392,36279,32118,36311,36343,34263,36343,34263,34230,32150,32150,30037,30037,32118,34199,34199,34231,36279,36279,36278,38358,38390,40405,
42420,48660,56981,56915,54705,50414,48268,46124,41866,39721,35527,31302,29221,27174,27206,27271,27272,23112,16806,10532,10499,10499,10532,12612,
14692,14660,14628,14628,16708,18788,18820,18820,18819,18787,18788,18788,16707,16675,16675,14627,14627,12514,12514,10434,10402,8354,8354,6306,
6306,4225,4258,4258,4258,6306,8353,8353,12483,14627,14627,14594,14595,12514,8354,6339,554,554,586,2635,2635,522,522,522,
522,554,522,522,2634,2634,554,554,554,553,553,2601,4682,2569,2538,4650,2570,457,489,2569,2569,2569,2569,2570,
2602,2570,2601,2569,4585,4551,8646,14855,21031,20932,20835,20834,18722,16610,14563,14595,14563,14563,14595,14595,14595,14594,14594,16675,
20900,20900,20867,20835,20803,18722,18755,18755,18722,18722,16642,16643,18755,18755,18756,18788,20869,23015,25226,31663,31793,19246,12940,13005,
10957,10989,11022,8941,6861,6860,6892,6893,6860,4747,4715,6796,6796,6828,8909,10989,8909,10957,10989,10989,10989,8941,8941,8941,
8941,6861,6829,6861,8942,6894,6927,4879,6959,6959,9039,9039,8974,11054,11022,11054,13135,15215,15248,15215,13135,13135,13102,13134,
13134,15182,17262,17263,17230,15149,15182,15181,15181,15214,17294,19342,17229,19245,21357,25518,25517,25486,25518,21358,19278,21391,21391,21391,
21391,21423,19376,19376,21488,21456,19375,19375,21456,21455,21455,23536,23569,23568,23569,23601,21554,19473,19506,19539,17459,19507,19539,19603,
19636,21716,21716,21716,21716,21715,21651,23796,23764,23764,25844,25812,25811,25778,25810,25843,25810,25810,27923,27924,27891,27891,25811,23666,
23634,23666,25747,25747,23666,25747,27827,25746,27827,29907,29939,32052,29939,29971,32084,29970,29938,32051,29906,27858,34133,36278,32085,29972,
29940,32053,34134,32085,29972,32085,34166,32053,36279,36311,36279,32085,32117,34263,34263,34231,34230,32117,32117,32150,30070,30070,32150,32150,
32118,34199,34199,34198,36246,38326,40438,42453,46580,52787,56947,58962,56784,50380,44042,43946,39688,35495,33414,31301,29221,27173,25158,27271,
27272,23145,16870,12612,10499,8387,10531,12612,12612,14628,12580,12580,14660,16740,18820,18820,18788,18788,16740,16740,16708,14627,14627,12546,
12546,10466,10434,10434,8354,8322,6306,6306,4258,4258,4257,4257,4258,6305,8353,8353,12482,16675,16675,14595,14594,12514,8354,6306,
554,522,554,554,554,554,522,522,554,586,586,521,586,2634,554,586,586,554,553,553,2634,2602,521,2570,
2570,489,2537,4682,2602,2570,522,2570,2602,2602,2601,2569,2569,4584,6599,10727,16935,21029,20932,18786,18722,16642,14595,14595,
14562,16643,16643,14595,14595,14594,14594,14626,18852,20932,22947,22947,20835,20802,20802,20802,20835,20834,18722,18755,20836,20836,20804,18756,
18788,20901,23079,27404,33841,29713,19213,12972,10924,10957,8909,8941,8941,8908,8940,8973,8941,6828,6796,6763,6795,6828,8909,8941,
8941,8909,8909,10989,11021,11021,11021,8941,8909,8941,11022,8942,8974,9007,6926,4846,6927,6959,9007,9039,11087,11054,10989,11022,
13102,15182,13134,13102,13135,13135,13135,15215,15183,15150,15150,15182,15182,15149,15181,15149,15149,15182,15182,17262,15181,17197,19277,21325,
23437,25550,25518,21358,19278,19310,19310,19343,19376,19343,17295,19376,21488,21489,19408,19408,21456,19375,21423,23536,23536,23536,23536,23569,
21553,19474,19474,19506,17490,19571,19571,19571,19603,19636,21716,19667,21715,21716,21683,21683,23764,23764,25812,25811,25810,25810,25778,25778,
29972,29972,25811,25779,25812,25844,25811,23698,23666,23634,25747,25747,25715,25747,25747,25746,25779,29972,29971,29971,29972,29971,32084,29971,
29938,29971,30004,30004,32084,32085,30037,29972,29940,29972,30005,32085,27892,29972,32117,30005,32085,38392,36343,32117,32085,34230,34230,32117,
32149,32150,34230,32182,32150,30070,32150,32150,32150,32150,34198,34230,36278,38293,42421,46580,50739,56946,59026,58960,54638,48332,41961,37705,
33447,31302,29254,27173,27142,25126,25159,29353,25225,21064,16839,12613,10499,6338,10499,12580,12580,12579,12580,12579,12580,14660,16741,16740,
16740,16740,14660,14659,14659,12579,12578,10466,10466,10466,8386,8386,6306,6306,6306,4258,4226,4226,4226,4258,4257,6305,10401,12449,
14562,18755,16675,12482,10369,8321,6274,4194,2570,522,522,522,522,522,522,522,554,586,586,554,586,2634,586,2634,
2634,2634,554,586,2634,2634,2602,2602,2602,2569,2569,2602,2602,2602,2570,2569,2570,2602,4649,2601,2569,2569,4584,6631,
12775,18982,23045,20867,18722,16642,14595,14595,16642,16642,16643,16643,14595,14594,14594,14594,18787,20932,23012,25028,22947,22915,20835,20834,
20834,20802,18754,18754,18755,20836,22917,20804,18755,18820,20934,23177,31663,33874,25519,17133,12972,8844,8909,8941,8941,8941,8940,8941,
8941,8909,8876,6795,6796,8876,8909,8941,8941,8941,8909,8941,8909,8941,8908,8908,8941,8941,11022,11022,11054,11087,9007,6926,
6959,6927,6959,9039,11087,13134,11021,13102,13102,13070,13102,15183,13135,13135,13168,15216,15183,15150,15150,15182,15214,13101,13101,13133,
13101,13101,13101,15182,17262,17197,17197,19244,21324,25518,25518,21325,19277,19278,17230,17263,17295,17295,17263,17295,19376,19376,19376,19408,
19408,23569,25681,25649,25617,25616,23568,21488,21521,21554,19473,17426,17426,19571,19571,19571,19636,19603,19603,19668,21716,21716,21683,21683,
25844,25844,23763,25811,25810,27858,27859,25778,27859,27892,25812,25779,25844,25812,23699,25779,25779,23666,23699,23699,23634,25747,25779,25779,
25811,27891,29972,29972,29972,32052,32084,32051,30003,29971,30004,32084,29972,27891,27891,27892,29972,30005,30004,30004,27956,30004,30005,30005,
32085,34198,32117,32117,32117,34165,34230,32117,32085,32149,32150,32150,32150,30070,30102,32150,32117,32149,34197,34197,36245,42453,48693,52885,
56979,59025,58959,58894,52556,46218,39816,33479,29254,25093,25061,25061,23013,25159,25192,27338,25226,18952,14791,12645,8452,6306,8419,10499,
10499,10499,10499,12547,12547,12580,14660,16740,14660,14660,12611,12579,12579,10530,10498,10498,8418,8418,8418,8386,6306,6338,4258,4258,
4258,4226,4226,4258,6305,8353,10401,12450,18690,18723,14562,10336,8289,6273,6242,4226,489,521,2602,2602,522,522,522,522,
521,554,586,586,586,2634,2634,2634,2602,2634,2634,586,554,553,553,2602,2570,521,2569,2570,2570,2570,2570,2602,
2602,2602,2602,4650,4649,2569,4585,4584,8646,14854,21030,20965,18755,16674,16675,16642,16642,16642,16642,16643,14594,14562,14562,14594,
16675,20867,25060,25060,25027,24995,24995,22915,22915,20834,20834,18754,18722,20836,20869,20836,18755,18820,20933,23079,29484,35889,33842,25455,
17101,10892,10957,8909,8941,8973,8973,8941,6860,6860,8908,6796,8876,8908,8909,8909,8909,8941,8941,8909,8941,8909,8909,8909,
8941,8941,8941,11022,8974,9007,9007,6927,6959,6959,6959,9039,11087,13135,13167,15182,15182,15150,13102,15215,15215,15215,15216,15216,
15183,15182,15182,15182,15182,13069,13069,15214,13133,13069,13101,13101,15182,17198,17197,19245,21324,23405,23405,21325,19277,17230,17198,17230,
17230,17263,17263,17263,17295,17263,19375,21488,19375,25649,31988,27762,25649,25616,23568,21488,21488,19506,19474,17426,17458,19571,17491,17523,
19636,19603,17523,17555,19635,21683,21683,21683,23764,25877,25844,25811,23730,23698,25778,25778,25779,25779,27860,25779,23731,23731,21618,21586,
23698,23666,23699,23699,23666,23667,25779,25779,25779,25811,25811,27859,29939,32052,32084,30003,29971,29971,29971,29971,30004,29971,27859,27891,
27892,30005,32085,30037,30037,30004,30004,32117,32117,32085,29972,32085,32085,32085,34198,32117,32085,34198,34230,32117,30037,30070,32182,30102,
32150,34198,36277,38292,40372,46613,52885,57012,56978,56911,56813,54668,50410,44072,35654,29317,25157,23012,20932,23046,20998,25225,25257,25258,
23177,16872,12646,10565,8452,6339,8419,8419,8387,10467,10499,10467,10467,12515,14628,14660,14628,12612,12579,10531,10499,10498,8418,8418,
8418,8386,8386,6338,6306,6306,4258,4258,4258,4226,4226,4225,6305,6273,10401,14530,18690,18723,16642,12449,10401,8354,6241,6274,
2537,489,2570,2602,521,489,489,489,489,522,554,554,522,522,2602,2634,2602,522,553,521,521,521,521,521,
489,489,521,2570,2570,2570,2569,2570,2602,2570,2570,2602,2602,2569,2569,4584,6599,10695,16903,20999,20900,18755,18722,18722,
16674,16642,16610,16610,14562,14562,14562,14562,14594,18754,22947,27140,27140,27108,27108,25027,22947,22915,22914,20866,18786,18787,20835,20835,
18787,18787,18852,20966,27338,33775,40147,33874,23374,15020,10892,8844,6828,8909,8973,9006,8941,6860,8908,8908,8876,8909,8908,8909,
8941,8941,8941,8909,8909,8909,8909,8941,10990,11022,11022,8941,8942,6894,6894,6894,4846,6926,6894,6894,8974,13102,15247,17295,
17295,17263,15182,13102,15215,15216,15216,13167,13135,13134,13134,15182,13166,13102,11021,13101,13101,10988,11021,15182,17263,17230,19277,19277,
21357,23405,21325,21358,21358,17197,17198,17230,17230,17263,19311,19343,17263,15214,19375,19376,17295,21456,25682,25649,25617,25616,23568,19407,
19408,19473,17393,19506,19506,17458,17458,17458,19571,19603,19603,17522,19635,21684,21683,23731,23732,25845,25877,23763,25779,27891,27924,27859,
25779,23666,23699,23731,21650,21650,21618,19473,21586,23699,25779,23699,25747,25779,23699,23731,23730,25811,25811,27891,30036,32084,32052,29971,
29939,29939,29972,29972,29972,29972,29940,27859,27859,27892,27924,27956,27956,27924,30037,32150,32150,32085,30004,32085,32085,32085,32085,32085,
32117,34230,34262,32149,30069,30070,32151,32151,32150,36278,40405,42452,46612,52852,57012,56978,58992,56846,54635,50442,46152,44008,35622,27269,
25190,20996,20997,21063,23177,25322,23242,21097,18984,12711,8485,8484,6371,6339,10500,8419,8387,10467,10467,10467,10434,10435,12548,12580,
12580,12579,10499,8451,8451,8451,6370,6338,6338,6338,6306,6306,6306,6306,4258,4226,4226,4226,4226,6306,6306,8353,10401,14530,
18690,20771,20836,18755,16675,12515,8322,6274,2537,2537,2537,2537,489,489,489,489,489,522,522,522,489,489,2570,4683,
2602,2570,521,521,521,521,553,554,489,489,489,489,2570,2570,2537,2537,2569,2570,2570,2570,2570,2602,2601,2569,
4584,8648,12776,18952,20966,20900,20834,18721,18722,16642,16642,16642,16610,16610,16610,14562,14562,16609,20802,27108,29221,29253,29220,27140,
27108,25059,25027,22979,22979,20867,18755,18755,18755,18755,18787,20901,27273,31565,35824,40083,33874,23374,12940,10892,8844,6828,8941,8974,
8973,6860,6860,8909,8909,8909,8909,8909,8941,8941,8909,8941,8909,6860,6860,8909,11022,11022,11022,8941,6861,6861,4813,4813,
4813,6894,6894,6862,8941,11054,15215,17328,19344,19376,17263,13070,15183,15216,13168,13135,11054,11054,13134,13134,13102,13166,13134,13101,
13101,13101,11021,13102,15182,17262,19278,21358,23471,21358,21325,21357,21358,17197,17230,17262,17262,17262,17230,17263,17263,17295,19408,19375,
19343,21423,21423,21423,23536,21488,21488,21488,19440,19473,17393,17425,17426,15345,15378,17491,17491,17523,19603,19603,21651,21683,21651,23731,
23731,23764,25877,25811,23698,27924,30005,27924,27892,25747,25779,23763,23731,21650,21618,21618,23699,23731,23731,23699,25779,25844,23731,23731,
23763,25843,27923,27924,30036,32084,32020,32020,32020,29939,29971,29972,29972,29972,29972,29972,27891,27924,27924,27956,27956,27956,30037,30037,
30069,32118,32117,30037,30037,32117,32085,30037,32117,34230,32182,32149,30069,30069,32151,32183,34198,38325,42452,46579,52852,57012,59059,59025,
58959,54700,50377,46184,44007,41928,31429,25189,23109,21029,23175,25322,27468,27436,23275,19017,12711,8517,8485,8484,6371,6339,8420,8387,
8354,8387,10467,10435,10434,10435,10467,12580,10500,10499,8451,8451,8451,6371,6338,6338,6338,6306,6306,6338,6306,6306,4258,4226,
4226,4226,4258,6338,6306,8353,10402,14530,20770,22884,24997,22949,20836,14628,12515,8386,4617,2537,2537,2537,489,489,489,489,
521,554,522,489,489,457,489,2602,2602,2602,521,521,521,521,489,521,489,489,457,456,489,2537,489,489,
2537,2537,489,2570,2602,2602,553,2601,2569,4585,8648,14824,18951,20965,20867,20834,18754,18722,18722,18690,16642,16643,16643,16642,
16610,18657,20769,24995,29220,31301,31301,29252,29220,27172,25092,25059,25092,23013,20835,18722,18754,18754,18787,20901,27208,31532,35758,35823,
40083,33842,21294,15020,10924,8908,8909,8941,8973,8908,6860,6860,6861,8909,8909,8909,6828,6828,6860,6861,8909,8909,8909,8909,
8909,10990,11022,8973,6893,6893,6894,4813,4846,6894,8974,8942,11022,13102,13102,15215,17263,17263,15183,15150,15183,15215,13167,13135,
13102,11054,11054,13134,11053,13134,15247,15215,13134,13134,13070,13069,15182,17262,19278,21359,23471,23438,21357,19277,19277,17230,17262,17262,
17230,17230,15150,15182,17262,19375,19375,19375,21488,21488,21455,21423,21423,19407,19407,19440,19473,19473,17425,15345,15313,15345,17458,17458,
15378,17490,21651,21684,21651,23699,23699,23731,23731,25812,25844,25811,25811,27860,27892,27892,25812,25812,25812,23763,23763,21651,21618,21650,
23699,23731,23731,23699,23699,25812,25779,23731,25811,27924,27956,27956,27924,29972,32052,32052,32052,29972,29939,29939,29972,30004,29972,29972,
27891,27924,27956,27924,27956,27956,27988,30037,30037,32117,32117,30005,30037,32118,32117,30004,30037,32117,32117,32149,32117,30069,32118,32118,
34165,38325,44500,50740,56980,59027,58994,58960,56814,52555,48265,46120,41960,39816,29316,25157,23110,23143,27434,29548,27436,23276,23243,16937,
12679,8485,6437,6404,6339,6339,6372,6339,6307,8387,8419,10467,10435,10467,10467,10467,10499,8451,8451,6371,6371,4323,4290,4290,
4258,4258,4258,6338,6338,6306,4258,4258,4226,4258,4258,4258,6305,8321,10401,16611,22884,24964,24996,24997,20837,16676,12515,10435,
4617,2537,2537,2537,489,489,489,489,489,522,554,521,489,489,457,489,489,2570,2569,489,489,489,457,489,
489,2569,489,457,457,457,457,457,457,489,489,2570,2602,2570,2601,2602,2570,4585,6600,8647,14823,20998,23013,22948,
20835,18754,18722,18722,18690,18691,18723,18691,18722,20738,20769,24930,27107,29252,31332,31333,31333,29253,27172,25091,25059,22980,20835,18755,
18754,18754,18754,20868,25127,29386,35725,37871,40017,38035,31761,21293,13004,13004,10956,8940,8908,8908,6860,6860,6861,6861,6861,6861,
6828,6828,6828,4748,6861,8974,8909,8909,8909,8909,8941,8941,6893,4813,6894,6926,4846,6894,8974,8974,11054,13134,13102,13070,
15118,15182,15183,15150,13135,13135,13103,13102,11054,11086,11053,13134,13134,11021,15215,15215,13102,13102,13134,13102,15182,17262,19311,19310,
19310,21358,19277,19278,19278,17198,17230,17230,17230,17230,15182,15214,17294,17295,19407,21456,21488,21488,21488,21456,19375,19375,19407,17327,
17360,17393,17393,15345,15280,17393,17458,17426,15377,17426,23732,23732,23667,23698,25747,25811,25811,25811,27859,27859,29972,30005,27892,25779,
25779,25812,23731,21650,21650,23699,21651,21650,21650,21650,23699,23699,23731,25811,25811,25844,27924,27956,30037,30005,30004,29972,32052,32052,
32020,29972,29972,29972,29972,29972,30004,29972,25811,27891,27924,25843,25876,27956,27956,30036,30037,32085,30037,30037,32118,34230,32118,30037,
30037,30037,30069,32150,32150,32118,32085,34165,36213,40340,48660,56981,59060,58994,56880,56814,54636,50410,46152,46121,41993,35655,29317,29384,
29449,29483,29548,27436,23275,23244,21131,14857,10599,6437,4324,4291,4291,6339,6339,6339,6339,8387,8387,8419,8387,8387,8387,8387,
8419,8419,6339,6339,4291,4290,4290,4258,4258,4226,4226,4258,4258,4290,4258,2177,4226,4258,4258,6306,6305,8353,10369,18723,
25029,24997,22851,20804,20804,16676,12515,10435,4585,2537,2537,2537,457,489,2602,522,522,522,554,522,489,489,489,457,
489,489,2537,457,457,2537,489,457,489,2537,2569,2537,2537,2537,457,457,457,457,489,2570,2570,2570,2569,2570,
2570,2569,4585,6599,10695,16870,20998,23013,22948,20835,20803,18723,18690,18691,18723,18723,20771,20770,22850,24962,29155,29220,31300,31332,
31365,29284,29252,27140,25027,22915,20835,18755,18755,18754,18754,18787,22981,27207,31498,33612,37871,42162,38067,29712,19245,15052,10924,8908,
8908,8908,6860,6861,6860,4812,6861,6861,6861,4780,4780,4748,6828,8941,8909,8909,8942,10990,8941,8909,6861,4812,4813,6926,
6894,6926,6926,8974,11022,11022,11022,13070,13070,15150,15215,13102,13070,13103,13103,11022,11022,11054,11086,13134,13134,13070,13102,15215,
13102,15183,15215,15150,15150,17231,17263,17230,17262,19278,17197,17198,17198,15150,15150,15182,15150,15182,15182,17262,17295,17294,19407,23569,
23569,21488,21488,21488,19408,19408,19375,15214,15280,15312,15313,17426,17393,17393,17425,17393,17426,19474,23699,25780,23666,25746,25746,27859,
27891,27858,27826,29972,32020,29972,29940,25779,25779,25812,23731,21650,21650,21651,21650,21651,21650,21618,23731,23763,23731,23731,25811,27924,
27957,27924,27924,30004,29972,29972,29972,29939,29939,29972,29939,29972,29972,29972,29972,29972,27924,27924,27956,27924,27956,27989,27988,30004,
30004,30037,30037,30037,32150,32150,32085,32150,32182,32150,32150,32150,30070,32117,34133,36213,42453,46613,52853,59093,59060,58961,56815,54701,
50410,46152,41959,41992,39848,35687,31528,31562,33676,29548,25355,23276,21195,19115,16970,12744,10631,6437,4292,4259,4291,6340,6339,6339,
6339,6339,6339,8419,8387,6338,6306,6306,6307,6339,6307,4258,4258,4258,4258,4258,4258,4258,4226,4226,4258,4258,2210,2177,
2177,4226,4258,4258,6273,8353,10369,18724,25029,24997,22852,20771,18691,14531,10402,8355,2504,2504,2537,2570,489,489,2603,555,
522,522,554,522,521,521,521,489,521,489,2537,2505,2505,2537,2537,2537,2537,2537,2537,2537,2537,2537,2537,457,
457,457,489,490,2570,2570,2537,2537,2570,2569,2536,4551,8647,12742,18918,23014,25029,22916,20803,20803,20771,18722,20771,20803,
20803,22850,24930,27043,29155,31267,31267,29251,29251,29252,27172,27172,25060,22948,22916,20868,18787,18755,18754,16675,18788,23013,27272,29418,
29418,35823,42227,38099,27664,21358,13004,10924,10957,8941,6860,6893,4812,4780,4812,6893,6861,4780,4780,6860,4748,6828,6861,8909,
11022,11022,8941,8941,6893,4813,6926,6926,9039,9039,8974,8974,8941,8941,10989,13134,15215,15182,13102,13102,11021,11054,13135,11022,
10989,11022,13134,13134,13102,13102,13070,15151,15183,15215,17296,15150,15150,17231,17263,17263,17263,17230,17197,17198,15150,15150,15183,15183,
15182,15150,15182,15214,17262,17295,19375,21488,23569,21456,21488,21489,19440,19408,17327,15214,15215,15280,15280,15313,17393,17393,17426,17425,
17425,21586,25747,25747,23634,25714,25746,27859,29939,29938,31987,32019,29939,29939,27859,25779,23699,23731,23731,21650,21683,21651,21650,21651,
23731,23731,23763,23763,25779,25812,25844,25812,27892,27924,27924,30005,27892,29972,29940,27859,27859,27891,27859,27891,27891,27924,27923,27923,
27892,27924,30005,27956,27924,27956,27956,30004,30037,32085,32118,32118,32118,32085,30037,32150,34263,32182,32182,32150,30070,32117,34165,38293,
44533,52854,57047,59094,59060,58993,56814,54636,50378,44071,44039,39911,35687,37833,33705,31562,31628,25355,23275,21260,19115,17002,12809,8583,
8582,6437,4292,4292,4292,4292,4291,4291,6339,6339,6339,6339,6338,6306,6306,6306,6274,6306,6307,4258,4258,4226,4226,4226,
4258,4258,4258,4226,4226,4226,2210,2210,4226,4226,4258,4225,6273,8353,12450,16611,22884,24997,22917,22884,18724,14563,10402,8355,
424,424,457,2570,2602,522,522,554,522,522,522,522,522,521,489,489,489,521,2537,2537,2537,2505,457,2505,
2537,2537,2537,2537,2537,2537,2537,2505,2537,489,489,457,490,2570,2537,2537,2570,2570,2569,4584,6599,10662,14790,20966,
25094,22981,22884,22883,22851,20771,20771,22851,22883,22850,24898,27042,29155,31300,31300,29187,27139,27171,27172,29252,27173,25060,22948,22948,
20835,18755,18787,18787,18787,18820,20965,27272,27273,29451,37936,40179,38099,31825,21358,15085,13037,10989,8908,6861,6860,4780,4812,4812,
4812,4748,4780,6828,4748,6828,6828,8909,8941,8942,11022,9006,6925,6893,6926,6926,9039,9039,9007,8942,8941,8941,10989,13134,
15215,13102,11021,13134,13134,11022,15151,13103,11022,11054,13134,13134,13102,13070,13070,13070,15150,15183,17263,15183,15150,15150,17263,17295,
17263,15182,17198,17263,15150,15150,15183,15183,15183,15150,15150,15182,15182,17295,19375,19343,21456,21488,21521,21489,19376,17327,17327,15247,
13167,15280,15312,15280,15312,17393,19474,19473,19506,23667,25780,25747,25682,25681,27793,29906,34099,38227,36114,34067,31987,29907,27827,23698,
23666,23731,21683,21650,21651,23763,23731,23731,25844,25844,23764,25844,27892,30005,30005,25779,25812,27892,29973,27892,27860,27892,27891,27891,
27891,27859,27859,27891,27891,27923,27924,30004,27924,25843,27924,27956,27956,27956,27956,30037,32117,32117,32118,34198,32118,30069,32118,32150,
32182,32182,30102,30070,30070,32117,36245,40341,46581,54934,59094,59093,56946,56879,54733,52587,50377,46152,46153,39847,35719,37898,33706,29514,
25355,23275,23308,21228,19148,17067,10696,6469,6437,6373,4260,4292,4292,4292,4291,4259,4291,4291,6339,6339,4258,4226,6306,6307,
6307,6307,6307,4258,4226,4226,4226,4226,4226,4258,4258,4258,4226,4226,4258,4258,4226,4226,4226,4225,6273,8321,12450,14497,
16578,20803,22917,22949,20869,18756,12483,8322,456,457,489,522,2570,522,522,522,522,522,521,521,522,522,489,489,
489,2570,2570,2537,2537,489,457,456,489,2537,2569,2602,2537,2537,2537,2537,2537,489,457,457,490,2570,4650,2569,
2569,2569,2601,4617,4552,6568,8615,12743,21030,23046,22981,24964,24900,24899,24899,22851,22883,24930,26978,27010,29123,31235,33380,33413,
31333,29253,29253,29285,27172,25060,22947,20835,18787,16706,16707,16739,16739,16739,18820,20933,25126,27305,31563,37935,40113,38066,31825,19245,
13004,10957,8909,6861,6861,6861,6861,4780,4780,6796,6796,6828,6796,6796,6828,8909,8941,8941,8973,8973,6893,6926,6894,4813,
6862,8974,9007,8974,8973,8973,11021,13134,13134,11053,11021,13166,13101,10989,13070,15150,13102,13102,13102,13102,13134,13101,13101,13070,
13070,15151,17263,17263,17263,19376,19408,17296,15215,15182,15150,17263,15183,15151,17231,15183,15151,15150,15150,15118,15150,17263,21456,21424,
19376,21424,19376,19408,19408,15247,15247,15247,13167,15247,15280,15280,17360,19473,19506,19473,19506,23699,25747,25747,25715,29810,31889,36081,
42354,44434,42322,36115,31955,27795,25779,23699,21618,21651,21651,21651,21651,21683,21683,23731,23764,23796,23763,23796,25877,30005,30005,29973,
27893,32086,36344,32118,27924,27924,25876,27956,27924,27891,27891,25811,27891,27923,25843,27924,27924,25843,25843,27956,30037,27988,27956,30036,
30036,30036,32117,34230,32182,30069,30069,30101,32182,30134,30102,28022,30070,30069,34196,40372,50741,59062,59060,59026,54766,52620,52555,52523,
48296,50345,48298,39880,35752,35786,29481,25290,21162,19081,17001,17034,19148,14922,8583,6437,8518,6405,4292,4292,4324,4292,4291,4259,
4259,4291,6339,6307,4259,4259,6307,6307,6306,6274,6306,6306,4226,4226,4226,4226,4226,4226,4258,4226,4225,4225,4226,4226,
4226,4258,4258,6273,6273,10401,12482,12449,14465,16578,18691,18756,18756,14627,10402,6274,457,457,490,522,490,522,522,489,
489,522,521,489,522,2602,522,489,489,2570,2602,4650,2569,2537,2537,457,457,457,2537,2570,2570,2537,2537,2538,
2538,2538,457,457,489,2570,4650,2602,2569,2569,2569,2569,4585,4552,6568,8615,14823,18983,23079,25061,24963,24963,24931,24931,
24931,24963,27011,27043,27075,27107,29220,31300,31333,31365,29252,27139,25027,22947,22947,18786,18754,16674,16674,16706,16739,16739,18787,18852,
20933,25126,27272,31564,35822,37968,40211,27664,17165,10924,10957,8909,6861,6893,6861,4780,4748,6828,6828,6829,6861,6828,6828,6861,
8909,6861,6860,4780,6893,6926,6894,4813,6862,6894,6894,8974,11022,11054,11086,13134,13102,11053,13134,13101,10988,10988,13037,13070,
13102,13102,13102,13101,13102,15150,15149,15150,15150,15150,15182,17263,17295,21456,19376,17263,15182,15150,15150,15183,15151,15151,17231,15151,
15150,15150,15150,15150,15150,17230,19344,21424,19376,19376,17295,17295,17328,15279,13166,15215,13166,13134,15215,15280,17328,17360,19473,19473,
19473,21618,23699,25715,27763,29842,34002,40242,48660,50741,44435,36050,29842,27795,25747,23699,21650,21618,21618,19570,21651,21651,21683,23796,
23796,23764,21715,23796,25844,27925,32150,36344,30038,30038,36344,34231,27957,25844,25844,25876,25843,25843,25811,25811,25811,25843,25843,25811,
25843,27956,27956,25876,27956,27956,27956,27956,30004,30036,32085,32117,32117,30037,30069,30069,30102,30134,30102,28054,30102,30036,34164,40404,
50773,59029,58995,56815,50507,50474,50410,50442,50378,52491,46185,39880,33640,27335,25288,23242,19017,14823,12776,14921,14922,10696,8550,6373,
6405,6437,4324,4324,4324,4292,4259,4259,4291,6339,4291,4259,4259,4259,6307,4258,4226,4226,6306,6307,6306,6306,6274,6274,
4226,4226,4226,4226,4225,4225,4226,4226,4226,4226,4226,6273,8321,10434,12481,12449,14497,14497,14497,14530,12482,10401,8289,6242,
2537,2537,2570,2570,2603,2635,2570,489,521,489,489,489,489,521,489,457,457,2537,2570,2569,2569,2569,2537,2505,
2537,2537,2505,2537,2537,489,489,489,2538,2570,489,457,489,489,2570,2569,2569,2569,2569,2537,2537,4585,4584,6568,
8616,12743,19015,23110,25092,25027,24995,24963,24963,27044,27076,27108,27108,27107,27107,27107,27107,27107,27075,24994,25027,22979,20867,18787,
18754,16674,16707,16707,16707,16706,16706,18787,18819,20900,23046,25192,31564,37903,40082,38100,23439,15052,10957,8941,6893,6926,6893,6861,
6828,4780,4780,6828,6861,6861,4748,6828,6829,6828,6861,6861,6861,6894,6894,6926,8975,9007,8974,8974,11022,11054,13134,13135,
13134,13102,13101,13102,11021,11021,13037,15150,13070,11021,13134,13134,13102,15150,15150,17230,17198,17230,17198,17263,19311,19311,17263,17230,
15182,15150,15150,15118,15151,17231,15183,15183,15183,17263,15182,15150,15182,15182,15182,17263,17263,17295,17263,15214,15247,17327,15247,13134,
15215,13167,13134,15280,17360,17360,19441,19473,19472,19473,21586,25682,27763,29810,31889,38161,42387,44499,40275,31922,27794,25747,23698,23699,
21651,21650,21651,19602,19570,21651,21715,21715,21683,23796,21715,23796,25844,23763,30070,34296,30038,27924,27892,27924,25844,23763,23763,23763,
23763,25843,27891,25811,25811,25778,25875,25843,25843,27956,30069,30069,27956,27956,27956,27956,30036,30037,30037,30037,30004,30036,30069,30069,
30069,30134,30102,27990,30038,32084,34163,42419,52788,56915,58897,52589,50443,48330,46217,46217,48298,46218,39848,33607,27302,21062,23176,19049,
14823,12710,12743,12776,8583,8550,6437,6437,4292,4324,4324,4324,4324,4292,4259,4259,4291,4259,4259,4259,4259,4259,4258,4258,
4226,4226,6306,6339,6338,6306,6306,6274,4226,4226,4226,4226,4226,4226,4226,4226,4193,4225,4225,6273,8321,10401,12482,14530,
14497,14497,14497,14562,12482,10369,6241,6274,2537,4650,2603,2570,2602,2602,490,489,2569,489,489,2570,2569,2537,2537,489,
457,2537,2537,2537,2537,2537,2537,2505,4585,4553,2472,2472,457,489,489,489,2570,2570,489,489,489,489,2537,2537,
489,489,2569,2569,2537,2537,4585,4585,4554,6568,10727,19014,23109,25124,27107,27075,27076,27076,27108,27108,27107,27075,27074,27075,
27075,25026,25026,25027,25059,22979,20867,20835,18755,16674,16674,16707,16706,16706,16706,18787,18787,20835,22981,23046,27305,33676,35790,40082,
33874,21326,13036,11021,8974,8974,8974,8909,6829,6861,6861,6828,6828,6829,8941,6861,6828,6829,6861,8941,6861,6861,6926,6926,
6926,8975,8974,8974,11022,11022,11022,11054,13135,13102,11021,13102,15182,13101,10989,10989,13070,10989,13069,15182,15150,15117,19278,19278,
19245,19246,19278,21359,21391,19311,19310,19311,17230,15150,15150,15150,17231,17264,17264,17231,15183,15215,15215,15182,13134,15215,15215,15215,
15215,15215,15215,13134,13166,15247,15247,15215,15280,17328,17328,17328,19408,17360,17328,19441,21553,21552,21553,25714,25715,27730,29776,33969,
34000,34000,31920,27729,25682,23666,21650,21683,21683,21651,21651,19602,19603,21683,21683,21683,21651,21715,19602,21683,23764,23763,25876,27957,
25812,23731,25844,25812,23731,23731,23763,23763,23731,23730,25811,25779,23698,23698,23762,25811,23730,23730,27924,30037,27989,27956,27956,27956,
30036,27956,27924,30037,30069,30037,30069,27989,28021,30069,30102,30070,32118,32084,36178,44499,52787,56849,56718,50411,48266,46153,44007,46153,
44073,39880,33575,25222,18949,18950,19016,14823,12743,10630,8550,8583,8551,6437,6437,8486,6437,4325,4325,6405,4324,4292,4291,4291,
4291,4259,4259,4291,4259,4259,4258,4258,4258,4226,4258,6306,6306,6306,6306,6274,4226,4226,4226,4226,4225,4225,4226,4193,
4193,4193,6241,6273,8321,10401,14530,14562,14529,14529,14529,14562,12482,8354,6274,6274,2505,4618,4651,2570,490,490,2538,2570,
2537,489,2537,2537,2537,2537,2537,2505,457,456,2504,2537,2504,2472,2504,4585,4585,4553,4520,2472,456,489,489,2537,
2602,2570,2538,2538,2538,489,2537,2537,489,489,489,521,489,2569,2569,2537,4618,4585,6567,12775,18983,25158,27173,27107,
29155,31236,31235,29155,29155,29123,27074,27107,27075,25027,25059,25059,25027,22947,22947,20867,18787,18755,16674,14626,16707,16706,16706,16706,
18754,18754,20868,22981,25094,29385,35693,37904,40115,31793,19277,13036,11021,8941,8942,8942,6861,6861,6861,6828,6861,6861,8941,8941,
8909,8909,8941,8974,8974,6861,6862,6926,6894,6894,6861,8974,11055,11022,8974,11022,13102,13102,10989,13069,13102,13069,13069,13069,
13102,13102,13069,15150,17230,19278,21358,23374,23373,21325,21325,23438,21391,21358,19278,19310,19311,17230,17198,15150,17263,17263,15183,17263,
17263,15215,15215,15214,13134,13167,15215,15247,15215,15215,13134,13102,13134,15215,15215,17360,17328,19408,17360,17295,17360,17360,17360,19440,
19472,21552,21553,21521,21489,23536,25584,27695,29840,29840,25680,23601,23666,23666,19537,19570,21683,21683,21683,21715,23796,21683,19570,21683,
19603,21683,19602,21651,21683,21683,23764,21683,23731,23731,23731,21651,21651,23731,23731,23763,23763,23731,23698,23763,25843,27891,25810,25778,
25746,23698,23698,25843,27956,27924,27956,30069,30069,27989,27923,27956,30069,30101,30069,27989,27989,27989,27989,30038,32118,34133,38259,44498,
52754,56783,54573,48201,44008,39815,39815,39848,37703,33575,27238,20965,16869,16903,14823,14824,10630,10630,8550,6438,8551,6437,6405,6405,
6437,6437,6405,6405,4324,4292,4291,4291,6339,6339,4291,4259,4258,4258,4258,6339,6339,4258,4226,4258,6306,6306,6274,4226,
4226,4226,4226,4225,4225,4225,6273,4193,4193,6241,6241,8321,10401,12482,14530,16578,16610,14529,14529,12482,10434,8353,6274,6306,
2473,2537,2538,2538,2538,2538,2537,2570,2570,2537,2537,2537,2537,2505,2505,2504,2472,2472,2504,2504,2504,2472,2440,2472,
4553,4553,4520,4520,2504,489,489,489,2569,2570,2570,2538,2537,2537,2537,2537,489,490,489,521,489,2569,2601,2569,
2570,4618,4521,8616,12744,18952,25126,27173,29220,31268,33315,33315,31267,31235,29155,27107,27107,25027,25060,25060,22947,22915,22947,20867,
18787,18755,16674,16674,16707,16707,16674,16674,16706,18754,18787,20868,22981,25127,31466,35726,37936,40115,27632,17164,13069,11021,10989,10989,
8909,6861,6860,6860,8941,8941,8908,8909,11022,11054,11022,8974,8974,6926,6894,6926,6926,6862,6861,6893,8974,11055,8974,8974,
11022,11022,11021,11021,13069,13069,13069,13069,13069,13102,15150,17197,19278,23406,25519,27534,27533,27533,25485,25486,25518,23406,19278,19278,
19310,19311,17198,15150,17231,17263,15183,17264,17296,15215,15215,15215,13167,13167,15215,15247,15215,13135,13134,13134,13167,15215,15247,17328,
17328,17328,17328,15215,15182,15247,17327,17360,19440,19440,19440,21488,19408,21456,21488,23535,23599,23599,23633,23666,23634,21651,21618,19570,
19602,19635,21683,21715,21748,21715,19602,19602,19570,19602,19570,19570,21650,21618,21651,21651,21683,23763,21651,19538,19538,21651,21651,23731,
23764,23763,21650,23731,25843,27924,25811,25778,25746,25746,25779,25811,27956,27956,25876,27989,27989,27956,27956,27956,30069,32182,30069,27989,
27989,27989,27989,30069,32118,34133,36179,44466,52721,54702,52492,46088,39783,37638,33477,31364,29350,27270,20965,18917,14790,14823,12743,10598,
6437,8550,8551,6470,6470,6437,6437,6405,6405,6405,6405,4356,4292,4292,4291,4291,4291,4291,4259,4291,4291,4258,4258,4258,
4258,6338,4258,4258,6306,6306,4226,4225,4226,6274,6274,4225,4193,6273,6306,6241,6273,6273,8321,10401,12449,14530,16610,16578,
16609,14529,14529,12514,10434,8354,6274,6306,392,2473,2538,2538,2538,2506,2505,2537,2537,2505,2505,2505,2504,2505,2505,2472,
2472,2472,2504,2472,2472,2504,2472,2439,2472,4520,4520,4520,2504,456,2537,2537,2537,2537,2538,2570,2538,2537,489,489,
489,489,489,489,489,489,521,2601,2569,2569,4586,6602,8617,12712,18919,23110,27237,29284,31300,31300,31268,31268,29187,29188,
27140,25060,23012,22979,22947,20867,20867,20835,20835,18787,16675,16674,16707,16675,16674,16674,16706,18754,18754,20835,22948,22981,27208,33580,
35758,35856,35986,27599,19245,15117,13069,10989,11021,11021,11021,8941,8941,10989,10989,10989,11021,13102,11022,8974,8941,6894,6894,6894,
4813,4813,6894,6926,8974,8975,8975,8974,11022,11022,13102,13102,13069,13101,13101,13069,13069,15150,19310,19278,21293,25453,29614,31694,
31693,31693,31693,27565,27566,23438,21357,19310,19278,17230,15150,15118,15150,17231,17263,17263,15215,15214,15214,13167,13167,13167,13167,15215,
13135,13135,13167,13167,13135,13135,15215,15248,15247,15248,17328,15215,13102,15214,15247,17327,19440,19439,17359,19440,21521,21489,21521,23600,
23600,21552,21553,21522,19473,19506,21618,19570,19570,19571,19603,19603,21683,21683,19602,19570,19570,19505,19506,19506,19538,21618,21618,21651,
23731,21683,21683,19538,19538,21618,21651,23732,23731,23731,23731,23731,23698,23698,23698,25779,25779,23666,25779,25843,27924,27957,25876,25876,
27957,25908,25908,27988,30069,30134,30101,27989,27989,27989,27989,30069,32149,32117,36180,44467,50641,54703,54605,46121,39816,33477,27107,27172,
29319,23046,18885,16870,14791,14823,10663,8485,6437,8518,8551,8518,6438,6437,6437,6437,6437,6437,6405,4324,4292,4292,4292,4291,
4259,4259,4291,4291,4258,4258,4291,6338,6338,6338,4258,4226,4225,4257,6306,6274,4226,6274,6274,4225,4193,6273,6306,6273,
6273,8321,10369,12449,14529,16578,18658,18658,16577,14529,14561,12514,10434,8354,6274,4226,2440,2473,2506,2506,2505,457,2505,2505,
2505,2505,2504,2472,2472,2504,2505,2472,2472,2472,2472,2472,2472,2472,2504,2472,2472,4552,4520,2472,2504,2504,2537,2537,
2537,2537,2537,2570,2570,2537,489,489,2570,490,2570,489,489,522,2569,2569,2569,2569,4618,6667,6634,8649,12776,14855,
19015,25190,29286,29286,29286,29253,29221,27140,27108,25059,23011,22979,20867,20835,20835,20835,20836,18787,18755,16674,16674,16675,16675,16674,
16674,16674,18722,18755,20868,22981,25094,29385,33612,33677,37969,38034,29712,21326,15117,15117,13069,15182,15182,13069,10956,10989,11021,11021,
11021,11021,11022,11054,8942,6861,6926,6926,4814,4813,6894,6926,6894,6894,6926,8974,11054,11022,11022,11054,13102,13069,13069,13101,
15117,15149,17230,21326,23373,27501,31661,35854,37933,37965,35853,29644,27565,23437,21325,19310,19310,17230,15117,15118,17231,17263,17263,15215,
15215,15247,15214,13167,13167,13167,13135,13135,13102,13102,13135,13135,13135,13135,15215,15248,15247,15215,15247,15279,15247,13134,13166,15279,
17360,17392,17359,17360,19441,19441,19441,19473,19473,19505,19473,17393,17393,19506,19538,19570,19570,19603,19603,19602,21651,19602,19538,19570,
19538,19505,19506,19505,19473,21586,21651,21586,21651,21651,19570,17457,19538,19570,21619,21683,21683,21651,21650,21650,21650,23698,23665,25811,
27859,25747,25779,25779,25812,27957,27989,25909,25909,25908,25940,27988,28021,30101,30069,30069,27989,27989,27989,28021,30101,32117,36180,44436,
50610,54703,52525,46153,39816,31333,24995,27173,25158,18885,16838,14790,14823,14824,8550,6405,6438,6438,6438,6438,6406,6437,8518,8550,
6469,6405,4325,4324,4292,4292,6372,4291,4259,4259,4291,4291,4258,4258,4290,6339,6339,6338,4258,4258,4257,4225,4258,6306,
6306,6306,6274,6274,6273,6273,6306,8353,8321,8320,10401,14529,16609,18658,18690,18658,16609,16577,14561,12514,10434,8354,6242,4194,
2440,2473,2473,2505,2505,2473,2505,2473,2505,2504,2472,2504,2504,2472,2472,2472,2472,2472,2472,2472,2472,2472,2472,4585,
4585,4585,2504,2504,2504,2505,4585,4585,4585,2537,2537,2537,2537,2537,489,489,2570,2538,2603,2538,489,2538,2538,2570,
2569,2569,2602,2602,4618,6665,8713,10728,12776,14823,20998,23111,25159,27206,27206,27141,25060,25059,25027,22947,22947,20867,20835,20835,
18787,18787,18787,18755,16675,16675,16643,16674,16674,16674,16706,18755,18788,20901,23014,27208,31466,35693,37871,40049,40115,29712,21358,17229,
15117,15182,17263,13101,10988,10988,11021,11021,11021,11053,11022,11054,11022,8974,6926,8975,9007,6926,6894,6893,6894,6894,6894,6926,
11087,11087,11054,11054,13102,13102,13069,15150,17230,17230,17197,21325,25420,27500,35854,40014,42126,48465,44304,35885,29613,25517,23437,21390,
19343,17262,15118,15117,15182,17263,15183,15215,15247,15247,13166,13167,13167,13135,13135,13103,13070,11022,13103,13135,15215,13167,15248,15248,
15215,13134,13134,15279,17360,13166,13166,13199,15247,15311,15311,17360,19441,19474,17393,19441,19473,17425,19474,19474,17426,19538,19570,19570,
21651,21651,21651,19602,21651,19570,19538,19505,19505,19505,21586,21586,19473,19441,21586,21586,19570,21650,19602,19570,19570,19570,19538,19570,
21651,21651,19570,19570,21650,25779,23666,25746,27859,27827,25779,23699,25811,27989,30070,27989,25909,25908,25940,25940,27988,27988,27989,28021,
27989,27957,27989,28021,28053,30037,34100,40275,48530,52623,50444,46154,39848,29285,25028,25093,23013,18886,14758,14758,14791,10630,6437,4357,
6438,6470,6438,6406,4325,6437,8550,8518,6437,6405,4324,4324,4292,4292,6372,4291,4259,4259,4291,4291,4291,4291,4290,4290,
6338,4258,4258,4258,4258,6305,6306,6306,6306,6306,6274,6274,6273,6273,8353,8385,8353,10433,14561,14561,16641,18690,18690,18690,
18689,16609,14561,14562,12483,8354,4193,4194,360,392,2472,2473,2473,2505,2505,2505,2505,2472,2472,2505,2472,2440,2472,2472,
2440,2472,2472,2439,2472,2472,2472,4553,4617,4585,4553,2504,2472,2504,4553,4585,4585,2537,2537,457,2537,2537,2537,489,
489,489,2538,2538,457,457,490,2570,2602,522,2602,2634,4650,4617,4617,6665,8713,8680,10727,12743,14791,18886,20966,25126,
25093,25060,25027,25027,22948,22948,20868,20835,18787,18787,18787,18755,16707,16675,16643,16642,16675,16675,16675,18723,18755,20868,20901,25095,
29353,33580,35693,37871,40082,40179,36051,25519,19309,19310,19310,15182,13101,13069,13101,13101,13101,13101,13102,13102,11022,9006,8974,6926,
8975,9007,6894,6861,6894,6894,6894,6926,8974,11087,13167,13166,13134,13101,11021,13069,17230,19278,21326,23438,25420,29548,37934,42126,
48431,56916,52756,40111,31725,27597,23437,21390,19310,17262,17230,15117,13069,13070,13102,15215,15247,13166,13134,13134,13135,13102,13102,13102,
11054,11054,13135,13167,11087,11086,13199,15248,13167,13167,13134,13199,15279,15311,13167,13167,15247,15279,13199,15279,17393,19506,19506,17426,
17393,17393,17394,19506,17426,19538,19538,19602,21651,19603,21683,19602,19570,19570,21618,19506,19473,19441,19505,21586,21554,19441,21521,21586,
21651,23731,21683,19602,19570,19570,19538,19538,19570,21651,21683,21651,21650,25843,25811,23698,25747,25747,25779,23731,25844,27990,30102,28022,
25909,25908,25908,25908,25908,25940,25940,27989,27989,27956,27956,28021,28021,27988,32052,38228,42354,46416,46285,42026,39849,31398,25092,23013,
20933,16773,14726,14823,12711,8485,6437,6438,6438,8486,6405,4325,6405,6438,6437,6405,6437,6405,4324,6404,4324,4292,4291,4291,
4291,4291,4291,4291,4291,4290,4258,4258,4258,4258,4258,4257,4257,4225,4225,4225,4225,6274,6274,6273,6273,6273,8353,8353,
10433,12513,14593,18689,18722,20770,20738,18690,18690,16609,16610,14595,12515,8322,6241,4194,359,359,2440,2472,2472,2472,2505,2505,
2505,2504,2472,2473,2440,359,2440,2472,2440,424,2471,2471,2471,2472,2472,2472,4585,6665,4585,4585,4552,4552,4584,4585,
4584,2537,2537,2505,489,2537,2537,2569,2537,2537,2537,2538,457,457,490,490,2603,522,2570,2602,4650,4618,4618,4617,
4650,4649,6696,8681,10696,12712,14791,18919,23046,25060,25027,25027,22947,22915,22916,20835,18787,18787,18787,18787,16674,16674,16643,16642,
16643,16675,16675,16675,18755,18788,20901,22982,29289,31467,33580,35725,35823,40114,48566,44437,38131,29745,21358,17229,15181,15181,13134,13134,
13101,13069,13101,13102,11054,11054,9006,6894,6894,9007,9039,6894,6829,6862,8942,8942,8974,9006,11086,13166,13134,13101,11021,13069,
15117,17165,21326,25487,27501,29613,35885,40110,50577,56949,50610,40078,33805,29678,25485,21357,19310,17262,17230,15150,13070,13102,13102,13134,
15215,15215,13167,13102,13135,13135,11054,11054,11087,11087,11119,11119,11087,8974,11086,13167,13167,13167,13167,13167,13231,15311,15279,15247,
15280,13199,13167,13199,15312,15344,17425,17425,17425,17425,17425,17425,17457,17457,17490,19602,21651,21651,19602,19537,17457,19537,19538,19538,
19473,19473,19473,19473,21586,19505,19473,21586,21618,21650,21683,19537,19537,19570,19538,19570,21651,21651,21650,21650,21618,23763,25844,23698,
23731,25812,25811,25812,25844,27990,30070,27989,25909,25941,25940,25940,25908,25941,25941,25941,27989,27956,27988,27989,27989,27988,27987,32083,
38258,40273,40110,39947,37769,31430,25157,25125,20965,16805,16839,16872,10599,8518,8485,8485,6405,6405,6404,6405,8485,6437,6405,6405,
8485,8485,6404,6372,6371,4291,4323,4291,4291,4291,4291,6339,6339,6306,4258,4258,4257,6338,6337,4257,6305,4257,4257,4257,
4257,4225,4225,6273,6273,6273,8321,10401,12481,14561,18689,18721,20769,20770,20738,18690,18690,16610,16610,14595,12515,8322,6241,6274,
391,359,391,2472,2472,2472,2473,2473,392,2472,2472,2440,2472,2440,2440,2472,2472,2472,2472,2439,2439,2471,2472,2472,
4552,4585,6666,4585,4552,4552,4552,4584,4584,2504,2504,2504,456,456,2537,4585,2537,2537,2538,2538,2538,490,457,490,
522,522,522,2538,4618,4650,4650,4618,2569,2569,4617,6665,8714,10729,10696,14823,18950,23045,25060,25028,22947,22915,22916,20868,
20835,18755,18755,16707,16674,16675,16643,16643,16643,16675,16675,16675,18755,18788,20869,22950,25128,31434,33580,35693,35758,35823,44339,54937,
55002,44535,31857,21390,19309,17261,15213,15213,13133,13101,13133,13133,13134,13166,11086,8973,6861,8974,9006,8974,6862,11023,11087,8974,
8974,11055,11086,11054,11054,11053,11021,13101,15149,15117,19213,21293,25389,27500,33805,40078,44271,48464,44304,37933,31692,27565,25486,21358,
17262,17230,17231,17263,15182,13134,13134,13134,13134,13167,13135,13135,13135,13167,13167,11087,11119,9039,9006,9071,11119,9039,9006,11087,
11087,11086,11087,13167,13167,13199,13231,17361,15280,13167,13167,13199,15312,15344,15312,17393,17425,17425,19506,19506,17490,17457,17457,19570,
19602,19602,19537,17457,19505,19505,19505,19505,19472,19473,19473,17392,19473,21553,19473,19473,19505,19505,19537,17457,19537,19538,19538,21651,
21651,21650,21618,21586,21618,23699,23731,23699,25844,25812,23731,25844,25844,25877,27989,27957,25909,25908,27989,28021,25908,25909,25941,25876,
27989,30134,30102,27988,27989,27988,27955,30035,34130,38226,40175,40045,37867,33609,29383,25158,20998,18918,18919,14759,12646,10566,10533,8484,
8484,8485,8485,8485,8484,6372,8485,6404,6404,8485,8485,6372,6404,6339,6372,6371,4291,4291,4291,6339,6307,6307,6339,6339,
6338,6338,6338,6338,6306,6306,6338,6338,4257,4225,4225,6273,6273,8321,10401,12513,14593,16673,18721,20801,20802,20770,20769,18722,
18690,16610,16610,14595,12515,8354,6241,6274,2439,391,2440,2440,2472,2473,2472,2473,2473,2472,2440,392,2440,2472,2440,391,
391,2472,2472,2472,391,391,2504,2504,2472,4552,6665,6665,4552,4520,2472,4552,4585,2504,2504,2504,456,456,2472,4585,
6731,2505,2537,2538,2538,2570,490,490,489,490,490,489,2537,2570,4650,4650,4650,4649,2601,4617,6698,6665,6632,10727,
14822,18949,23045,25060,25027,22947,22948,22916,20835,18755,18755,16706,16707,16675,16675,16643,16643,16675,16675,16675,16675,18756,20868,20901,
23047,27273,31499,33645,33709,35822,40114,50711,55001,52954,44502,31824,23437,21357,19341,17294,17294,15214,13133,13133,13166,13166,11086,8973,
6893,6925,8973,9006,11087,13168,13168,8974,6894,8974,9038,11086,8973,8973,11021,11021,15149,17230,19278,19213,21261,25420,29645,35885,
37932,37933,37966,33805,29613,25453,21325,21358,17230,17263,19343,17295,15215,15215,13134,11054,11054,11054,11022,11022,13103,13135,11087,11087,
11087,9007,9039,9039,9039,9071,9071,9039,11087,11087,11087,11119,11119,11086,11119,15280,15280,13200,13199,11119,13232,17425,15344,17393,
17425,17393,17393,17457,17457,17457,17490,17489,19570,19538,17457,19505,19537,19473,17392,19472,17392,17392,19472,19505,17392,17392,19505,17392,
17425,21618,19537,17457,19538,17457,19505,21618,21586,19538,21618,21618,21650,23698,21650,21618,23699,23764,23731,21651,25812,25876,25909,27989,
27990,25941,27989,28021,25941,23861,23861,23828,25908,30102,32215,28021,25941,25908,25907,30035,32083,36146,40242,42257,42159,37900,31562,25256,
23144,23145,21065,16871,14758,14726,12645,12645,12645,10564,10564,10532,8452,8452,8484,8484,8484,10500,8452,8452,8452,6339,6339,6340,
6340,6372,6340,6339,6307,4259,6307,6339,6339,6339,6338,6306,6306,6338,4258,4257,6305,6305,6273,6272,8353,12514,14594,16674,
18754,20802,20801,22849,22882,22882,20769,18721,18690,16642,16642,14595,12547,10402,6274,6306,2472,2472,2472,2472,2473,2505,2473,2505,
2505,2472,2440,2440,2472,4553,4520,2472,2472,2504,2504,2504,2472,391,2472,4552,4520,4552,6633,6633,6633,4552,4520,4552,
2504,2504,2472,2472,456,424,2440,2472,4585,4585,4618,2538,457,490,490,490,489,522,2602,489,457,2570,2570,4650,
4682,4682,2601,2570,4618,4650,4617,6631,10726,14822,18917,23012,25027,25027,22948,22915,20835,20803,18755,16707,16707,16675,16675,16643,
16643,16675,16675,16675,16675,18756,18820,20901,23014,25160,29386,31499,31596,33710,37968,44339,44404,50743,55001,46549,35984,27566,23469,23535,
21487,15278,13133,13101,13166,13166,11053,8973,9005,11086,11086,13166,13199,13167,13167,8974,6893,6894,6926,8974,8974,8974,11022,11021,
13069,15150,17166,19214,21293,23373,25485,29612,31692,31660,31692,29612,27565,23405,19245,17198,15150,15183,15183,15182,15183,13134,11054,11054,
11054,11054,11022,11022,11055,11087,11087,11055,9039,9039,9072,9104,9071,9071,9072,9039,9071,11120,11120,11119,9039,11119,11151,11087,
13200,15280,13199,11118,13199,15312,15345,15344,19473,19506,17393,17425,17425,15376,17457,17457,17457,19538,19505,19473,19505,19505,17424,17424,
17425,17392,15312,17424,17425,17392,17425,19505,19505,19537,19538,19538,17489,19538,19505,19505,19538,19538,21586,21618,21618,21618,21618,23698,
21618,21651,23764,23731,23731,25844,25844,25876,27989,28021,28021,25941,25941,25909,23829,25942,28022,27989,30102,28021,25941,25940,25908,27988,
30036,32051,36146,38194,40242,38064,33773,29548,29516,25355,25323,25290,21064,16871,16838,16838,14757,12644,12645,12644,10532,10532,10564,10564,
12613,12581,10500,10532,8452,8420,8420,8420,8420,6372,6340,6340,6340,6307,6307,6339,6339,6338,6338,6338,6338,6338,6338,6338,
8386,8386,10433,10433,12513,14626,18754,18753,20834,22914,22881,22849,22882,22881,20769,18721,18722,18722,16675,16675,12547,8354,6274,6306,
2472,2472,2472,2472,2472,2472,2473,2505,2505,2505,2473,2472,2472,2472,4553,4553,4552,4552,4552,2504,2472,2472,2472,4520,
4520,4552,4552,6600,6665,6665,4552,2472,2472,2504,2472,2472,2504,2472,2472,2440,2440,2505,4618,2538,2505,489,489,522,
490,522,2635,2602,489,489,489,2537,2602,4650,4618,4618,2538,2570,4649,4616,8679,12742,16870,23045,25060,22980,24995,22915,
22883,20835,18755,18755,16707,16707,16675,16675,16675,16675,16675,16675,16675,18756,18788,18853,20933,23014,27273,29451,31564,31597,33742,38000,
42258,46548,55000,57049,52823,42291,33936,31921,27825,21486,17293,15214,15214,15247,11054,11053,9006,11054,11118,13199,13167,13199,13199,11087,
8974,6926,6926,6926,6926,9039,11055,11022,10989,13069,15118,17198,21327,21325,23405,25485,27532,27500,27565,25485,23405,21357,19277,17230,
15182,15215,15183,13070,13134,13134,11054,8974,11087,13135,11054,11022,11055,13168,11120,9007,9007,6959,9072,9136,9104,7023,9072,9104,
11152,11120,11152,11120,9039,11119,13264,13167,11119,13200,11151,11119,11151,13231,15312,15312,17425,19506,19505,17392,15344,15344,15376,15344,
17392,19505,19505,17392,17392,19473,17424,17424,17424,17392,15311,15344,17425,19505,19505,19505,19538,19505,17457,17457,19537,19538,19505,19505,
19506,21618,21618,21618,21618,21618,21618,21618,21650,21618,21650,23763,23763,23796,25876,25844,25876,27989,27989,25941,25909,25941,25941,25909,
25909,25908,25941,25941,25941,25973,25940,27988,30036,30036,32051,34099,38227,40275,38065,33872,33904,31759,27533,29614,29549,25322,23209,23144,
18951,18918,18918,16838,16805,16805,16806,16806,16773,16774,14693,14693,14693,12613,12581,10533,10501,8420,8388,8420,8420,8388,8387,8419,
8387,8387,8386,8418,8418,8386,8418,8386,10466,12546,14594,14625,16674,18722,20801,20800,22881,24961,22881,22881,22913,22881,20801,20801,
18722,18722,16674,16675,12547,8354,6274,6306,2440,2440,2472,4553,2473,2473,4553,2505,2473,2473,2472,2472,2472,2472,2504,4552,
4520,4552,4552,4520,4520,4520,4519,4520,4552,4520,4552,6600,6632,6665,4552,2471,2439,2472,2472,2472,2504,2537,2472,2440,
2440,2472,2473,2505,2537,489,490,522,522,490,2602,2635,522,522,489,489,2570,4618,4618,4618,2538,2570,4650,4617,
6632,8615,14791,20998,23045,23012,22980,22915,22883,20835,18755,18723,16675,16707,16675,16675,16675,16675,16675,16675,16675,16708,18788,18821,
20901,20934,25160,29386,31500,31597,31662,33775,40178,46516,52823,57048,57049,54936,48694,42420,38260,27792,19341,17230,15214,15214,15215,13167,
11022,11054,13167,13167,15247,15247,13167,11087,9039,9039,9007,6926,6893,8974,11055,11022,11022,13102,15150,17199,19247,19278,19277,23405,
25485,25453,23405,23438,21357,19278,17230,15182,13102,13135,13167,13102,11054,11086,11054,11054,11086,11087,11087,8974,6862,9007,11087,9039,
9007,9039,9072,9072,9072,6991,6959,9104,11185,11152,11184,11184,11119,11119,11152,13168,11119,11119,11119,11119,9070,13232,13232,13231,
15312,17393,19505,17392,17392,17457,15344,15312,17424,17424,19440,19440,19440,19473,17392,17425,17425,17392,17457,15344,15344,19505,19537,19537,
19538,19537,19537,17425,17457,19537,19506,19538,19506,21586,21651,21651,21618,21618,21618,19537,19570,21650,21650,21650,21683,23763,25844,25876,
25876,25908,27989,25941,25908,25909,25941,23860,23828,23828,25908,25908,25940,25941,25908,25876,27956,27923,29971,29939,34099,38228,36115,34002,
33969,31856,29743,31791,33872,31726,29581,27435,25322,23209,23177,21031,18918,21031,23112,23112,23080,23112,20999,18918,18918,18886,16805,14725,
14661,12581,12548,12548,12548,10500,10467,10499,10467,10434,10434,10434,10466,10466,10466,10465,12545,14625,16706,18753,18753,20801,22881,22880,
24961,24961,24961,24961,22913,20833,20833,20833,20802,18722,16674,14594,14595,8354,6274,6274,2439,2440,2472,2472,2505,4553,2505,2473,
2505,2473,2440,2473,2504,2472,2471,2439,4519,6600,6600,6599,6599,6599,6600,6600,6632,6600,6600,6600,6600,4552,4520,4519,
2439,2472,2472,2505,2505,4585,4553,2440,2440,2472,2440,392,2505,2537,2537,490,522,490,554,2635,554,554,522,490,
490,2538,4618,4618,4586,2570,4650,4650,6633,8648,10663,16838,20965,20965,23012,22947,22915,22883,20835,18755,16675,16675,16675,16675,
16675,16675,16675,16675,16676,16676,16708,18788,18821,20901,23047,27241,29419,29484,31630,33775,35985,46517,50775,54968,54968,57080,57080,50807,
46613,38227,25583,19310,19310,17262,15214,13166,11021,11054,13134,13167,15247,13167,11086,11054,11087,11120,9039,9007,8974,6926,8974,11054,
13134,13134,13102,13070,15118,17198,17229,21357,23406,21357,21357,21358,19310,19343,15215,13102,13102,13102,13135,13135,11054,11054,11086,11087,
11086,11054,11087,11087,6894,6926,9007,9039,9072,11120,9039,6959,6991,6991,6991,9072,9104,9071,11152,11184,11152,13232,11119,11087,
11151,13232,11152,9071,9038,11151,13199,13199,13231,15312,17392,17392,17424,17425,15312,15311,17424,19440,19440,19472,19473,19505,19473,17424,
19505,17424,17424,17392,17392,17424,17456,19537,19537,19537,19537,19537,19537,21618,19538,19505,19506,21618,21650,21651,21650,21618,21618,21618,
21650,21683,21683,21650,21651,21683,23731,25844,25876,25876,25876,25876,23828,23828,23860,23860,23860,23860,25940,25908,25908,25940,25940,25876,
25875,25875,27891,29971,32051,34132,34099,31986,29874,31954,29841,29841,31889,31824,31856,31791,31727,29613,27468,23242,21129,25290,25322,27370,
27402,27370,27337,27336,27336,25191,23110,20998,18853,18821,16773,16741,14628,14627,14627,12547,12514,12514,12514,12513,12545,12545,12545,14593,
16673,18721,18753,20833,20833,22913,24961,24993,24993,24960,24961,24961,22881,20833,20833,20833,20834,18754,16674,16675,12547,8321,6273,6274,
2440,4520,4520,2440,2472,2473,2473,2473,2473,2473,2473,4553,2505,2472,2471,4519,4552,6600,8712,10760,8712,8680,6632,6632,
8681,6632,6600,6632,6665,6600,4520,4520,4520,2472,2440,2505,4585,2505,4585,2504,2472,2472,2472,2473,2473,2505,2505,489,
490,490,522,554,554,554,554,522,490,489,2537,4586,4586,2538,2570,2569,4617,6632,8647,12742,18918,20998,23013,22980,
22947,22915,20835,20835,18755,16707,16675,16707,16707,16675,16675,16676,16676,16676,16676,18788,20837,20902,23015,23080,27307,27372,29550,33776,
33872,44502,52953,52952,50742,52887,57112,57080,52855,46581,36082,25615,23470,19374,17294,15214,13134,11053,13166,13199,13166,13134,11086,11054,
11055,9007,9007,9007,11055,8974,8942,11054,13134,13134,11021,13038,13038,15118,17230,19277,19310,21391,19310,17197,15182,15214,15214,13135,
11055,11087,11087,13167,11119,11119,11119,11119,11119,11087,11055,11087,9006,8974,6926,9039,11120,11120,9071,9039,6959,6991,9072,9104,
9072,9071,9071,11152,11119,11152,11119,9039,11119,13232,13264,11119,9071,11119,11151,11119,13199,15312,15344,17424,17392,15344,17392,15311,
17360,19440,19440,17392,17392,17424,19505,19505,19473,17424,17392,15344,17424,17424,17424,17456,19537,21618,21617,21618,23763,23731,19570,19570,
19537,19570,21651,21650,21618,19538,19570,21650,21715,21683,19570,21650,23763,23763,23763,23763,25844,25844,25875,25908,23828,23828,23860,23860,
23860,23827,25908,23859,23827,23827,25876,25875,23795,25843,25811,27891,30036,32084,29971,27858,29971,29971,29906,29906,29906,31954,34068,34067,
34002,33970,29743,27598,27533,29613,29613,29613,31661,33740,33708,35787,37868,33674,29416,27270,25126,23013,22981,20933,18820,18787,16707,16674,
14626,16706,14625,14625,14625,14625,16673,16673,18753,20801,22913,24993,24993,24993,24992,24992,24992,24960,24993,22913,22881,22881,20833,20834,
20834,18722,18754,16707,14595,8322,6273,6274,2440,2472,2472,2440,2440,2440,2473,2473,2473,2473,2440,2505,2472,2439,2471,4552,
6600,8680,10792,12872,12873,10792,10760,8680,8680,6600,6568,6568,6633,6601,4520,4520,4520,2472,2472,2472,2505,4585,2505,4553,
2472,2472,4553,4585,2505,2505,2537,489,490,522,522,522,554,554,554,522,490,489,489,2538,2538,2538,2570,2537,
4617,4617,6632,10727,16902,18950,23045,25060,22947,22915,22915,20868,18787,16707,16707,16675,16675,16675,16675,16676,16676,16676,18756,18788,
18789,20869,20902,23016,25226,25291,27469,31727,29679,40276,48760,48727,44532,48693,55000,57112,57048,52855,44500,36114,29840,23534,19373,19439,
19407,15246,13198,15246,13166,11054,11054,11087,11087,8974,8974,8974,11055,8974,8974,11054,11086,11086,11021,13070,13070,13070,15149,17229,
17230,19311,17262,15182,13134,13134,13134,11087,11055,11087,13167,11119,13199,11151,11119,11119,11119,11119,11087,9006,11119,11151,9039,9007,
9007,9039,11119,9039,9007,6959,9072,9104,9104,9071,9039,9071,11119,11119,13232,11119,9038,11119,11151,11184,11151,11151,11151,11119,
13199,15279,15279,15344,17392,17392,15311,17360,17392,19440,17359,15279,15311,19472,19473,19505,17424,17392,19473,17392,17392,17392,17424,19472,
19537,21650,23730,25843,25843,23730,19570,21651,19602,19570,19602,21650,19570,21618,21650,19570,19570,19570,19570,21683,21715,23763,23763,23763,
23763,23763,25875,25940,23859,23827,23860,23860,23860,23860,23859,23827,23827,23795,23763,23795,23763,23762,25810,25778,27858,27891,27923,25810,
27890,27923,27923,27891,29939,29972,32052,34133,34132,34100,31954,31889,31921,33937,31823,31791,35951,40111,42158,42158,44238,44205,40012,37802,
33544,29319,25125,23013,23013,20900,18787,18786,18786,20866,20866,18786,18818,18785,18785,20833,22881,22913,24993,27073,25025,25024,25024,24992,
24992,24993,24993,22913,22913,22913,22881,20833,20802,18754,18754,16707,14627,10402,6274,6274,2439,2440,2440,2407,391,2440,2472,2473,
2473,2440,2440,2472,2472,2439,2471,4519,6599,10728,12808,14920,17001,19081,14921,12840,8680,6600,4520,4520,4520,6600,4552,4520,
4520,4520,4552,4520,4520,4553,4585,4585,2505,2472,2505,2505,2537,4586,4586,2538,2570,2570,522,522,554,554,522,522,
522,490,522,490,490,2570,2570,2537,2537,4617,6665,8680,12807,18983,23046,25061,24996,24963,22916,22916,20836,18755,18755,16675,
16675,16675,16675,16676,16676,16708,16708,16676,18756,18789,18854,20935,23080,25258,27404,27534,27566,36083,42486,40373,40372,44533,50839,55033,
57081,57048,52887,46581,42420,38162,31921,29873,25680,19439,15278,15214,13166,11054,11054,11087,11087,8974,11054,11054,11054,13167,13200,13167,
11054,11054,11086,13134,13134,13102,15149,15182,15150,15150,15182,15215,13166,13134,11086,11087,11087,11119,11119,11087,11119,11119,11119,11119,
11119,11119,9039,9038,9071,11152,11119,11087,11087,9039,11087,9039,9039,9071,11152,11184,9104,9071,9071,9071,9071,11119,13264,13232,
11119,11119,9071,11151,13232,13199,11119,11086,13167,15279,15279,15279,17392,17392,17359,19440,19473,19440,17359,17360,17359,17392,19440,17392,
19472,19473,19505,19505,19472,17391,19472,19504,21617,23698,25843,32150,25811,21650,21683,19570,19570,19570,19570,19602,19602,19570,21651,21618,
19538,19570,19570,21650,21683,21683,23763,23763,23763,23762,23794,25907,25940,25940,23827,21746,25940,25972,23827,23827,25908,23795,21714,23763,
23763,23763,23762,25810,25810,25778,25843,25810,23730,23762,25842,25843,25811,27924,27924,30037,30037,29972,29939,29939,32019,34067,34001,36048,
38129,44305,46385,46449,46416,48496,50576,48464,44174,37803,31463,27205,27173,25028,22915,22947,22946,22946,25027,22946,22946,22978,22946,24994,
24993,24993,27073,27105,27073,27072,27072,24992,24992,24992,22912,22912,22913,22913,22881,20833,20801,18754,18754,16707,12547,8386,6274,6306,
2439,2439,2439,359,359,359,392,2472,2472,2440,2472,2472,2472,2471,4519,6567,8647,10727,14855,21129,25355,25322,21161,14921,
8680,6632,6633,4552,4520,4520,6601,6633,4520,4520,4520,4520,4553,4553,2505,2505,2505,2505,424,424,2505,2506,2506,2506,
4618,2570,2570,2602,554,554,522,554,554,522,523,490,490,522,2570,2538,2505,4585,4585,8681,10728,14856,20999,25061,
25028,24963,24963,22948,20868,20835,18787,18723,16675,16675,16675,16676,16708,16708,16676,16675,16708,16741,18822,20902,20967,23112,25291,25388,
25518,31922,38326,38358,36245,36245,40405,48727,52952,57080,57080,55000,52887,52887,48694,36049,25582,21487,19407,15247,13166,13134,11086,11086,
11087,11054,11086,13167,13134,13167,13200,13167,11087,11054,11054,13134,15215,15182,15215,17263,15215,15182,13102,15215,13167,11086,11086,11119,
11119,11119,11152,11119,11087,11087,11119,11151,11151,9071,9039,6991,9071,9071,9039,11087,11087,11087,11086,11086,11119,11152,9072,11152,
9071,9071,11152,11152,9104,7023,6991,11119,11119,9039,11151,11119,13167,13199,13167,13167,13231,15279,15279,15278,17359,19407,19440,19440,
19440,17360,17327,19440,19440,17360,17392,17392,17392,19473,19505,19505,19472,17391,19439,21585,23698,23698,23730,23763,21650,21650,21683,19602,
19570,19570,17522,19570,19602,19570,19538,19538,17457,19538,19538,21618,21650,21650,21714,23795,23763,21714,21714,23827,28020,25907,21714,23794,
25907,25939,23827,23795,25876,23795,21682,21715,23795,25876,25843,25843,25811,25811,25843,25843,25843,23762,23762,25843,25843,25876,27924,30037,
27957,27892,25811,25779,27858,31986,36114,38162,40242,44403,46515,50675,50675,52754,52754,52657,52658,50512,44141,37769,33479,29253,25027,25027,
24994,24993,25026,25027,25027,25026,25026,25026,25025,25025,27073,27105,27105,27104,27072,27072,24992,24992,24960,22912,22912,22913,20833,20833,
20801,18754,18755,16707,12547,8387,6306,6306,2472,2439,2407,2407,391,359,392,2473,2440,2408,2440,2440,2440,4519,4519,6567,
8647,12807,19016,27403,40015,37902,27435,19081,10759,8680,8713,6632,6600,4552,6601,6633,4553,4520,4520,2472,4520,4553,2473,2472,
2505,2569,456,456,2505,2505,2538,2538,2538,2570,2570,522,554,554,522,554,555,555,523,522,490,522,2602,2570,
2537,4586,4618,6665,8680,12775,18919,23046,25061,24996,22915,22948,22948,20868,20836,18755,18723,18723,16707,16708,16708,16708,16708,16708,
16708,16741,18821,20902,20934,21032,23210,23308,23406,27761,36278,38455,34262,34229,36212,38227,42420,48758,55000,55000,55000,55033,55033,42453,
29808,23567,19407,17294,15247,15215,13134,11086,13167,13167,13167,13167,13135,13135,13102,11022,11022,11022,11054,11086,13134,13134,15215,17295,
17295,15215,13134,13134,13167,11086,11054,11087,11087,11119,11152,13232,13199,13167,13199,13199,11151,9071,9039,9039,9071,9039,9039,9006,
9006,11087,11119,11118,11151,11184,9072,9071,9071,9039,11120,11152,11184,9103,6991,9039,11119,11119,11119,11086,11086,13167,13199,15279,
15280,15279,15279,15278,17327,19439,19440,19440,17360,17360,17360,17327,17360,17360,17360,17392,17392,17424,19505,17425,19504,19504,17424,19505,
21585,21650,23731,21682,21682,19570,17457,19603,19602,19602,19570,17489,17490,19570,21618,19506,19538,19570,19570,21650,21650,21618,21682,23763,
23763,21714,21714,23827,25875,23795,21714,23827,23794,21714,23859,25875,23795,25875,23795,23762,23763,23795,23763,25811,25811,25811,25843,25843,
25811,25843,27923,25843,25844,27925,27924,27924,30005,27892,27860,27860,29907,31955,36115,38195,38162,40275,44435,46548,50708,52755,50642,52722,
56851,54738,52560,48366,41995,35624,29252,25026,24993,24993,24994,25026,25058,25058,25058,25025,25025,25025,25024,27073,27104,27104,27105,27072,
24992,24992,24960,22912,22912,22913,20833,20833,20801,18754,18787,16707,12579,10467,8354,6306,4520,2439,2407,359,391,2440,2440,392,
2440,2408,2472,2472,2440,4520,4487,6567,8615,14856,21128,31563,46353,46385,31627,21161,12807,10760,10793,8712,6600,4552,4552,6601,
6633,4553,4520,2440,2472,4521,2473,2473,2505,4585,2537,2537,2537,2537,2570,4618,2538,2538,2570,522,522,522,522,522,
554,555,523,523,522,554,554,2570,2570,4618,4650,6666,8681,10696,16871,20998,25061,25028,24996,22948,22948,22948,20868,18787,
18755,18755,16707,16707,16708,16708,16740,16708,16708,16708,18789,18821,20902,18919,21097,25388,23438,25681,32117,36408,34295,32182,34197,34099,
36178,42485,50871,50839,48693,50839,52985,50872,40373,29873,23535,19374,17327,15247,15247,13167,13134,13167,13167,13135,13134,13167,13135,13070,
11021,11054,11086,11054,11054,13166,15247,15247,15215,13135,13135,13135,11087,11087,11054,9006,9039,11119,11119,11119,13232,13200,11119,11119,
13232,11151,9071,11119,11120,11152,11152,9006,8974,11087,11119,11119,11119,11151,11184,11184,11152,11120,11119,9039,9071,11152,9071,9038,
11119,13232,13199,11087,11086,13167,15280,15279,13199,15279,15279,15247,17327,19439,17327,17359,17360,17360,17392,17392,15311,17392,17392,17392,
17424,17424,17392,17424,17424,17392,15344,19537,19570,21650,23763,23763,21715,21683,19570,19602,19602,19570,19570,17489,17489,19505,21651,21618,
21651,21651,21618,21651,23731,21683,21682,23763,23763,23763,23762,21714,23795,23827,23795,25875,23795,21714,23827,27956,25876,25875,25875,23763,
23762,23763,25843,25843,27924,27891,25778,25811,27891,25843,30037,30037,27925,25844,25779,25811,29973,29973,30005,32053,29908,29875,34035,36148,
38196,38195,40275,42355,46515,48627,50642,54835,56883,54737,54704,54704,48398,46189,35656,25059,22945,22945,22945,22945,22945,25026,25025,25025,
25025,25025,24992,25024,27072,27104,27105,27072,24992,24992,24992,22912,22912,22912,20833,20833,20834,20834,18787,16675,12515,8354,6306,6274,
2439,2440,2439,359,359,2440,392,360,2440,2408,2440,4521,4553,6601,6600,8648,10663,14856,21129,27337,33707,35788,27402,21193,
14920,10760,8679,8680,6632,6601,6601,6601,4553,4553,4553,4521,4520,4521,2473,4553,6666,4617,2537,2569,2570,2537,2538,2570,
2570,2570,522,522,554,522,522,554,555,555,555,555,555,554,554,554,2570,2570,4618,4618,6666,10728,14823,18951,
23046,25061,25028,24996,24996,22948,20868,20835,18787,18755,18755,18755,16708,16708,18788,16708,16708,16708,16740,18789,18853,18886,18984,23276,
23438,23601,27924,30102,30134,27989,32085,34197,38325,42550,48823,50903,48790,46678,50872,53017,48824,36147,25615,23567,21487,19407,19407,17327,
15214,13166,13166,13134,13167,13134,15150,15150,13102,13102,11054,11086,13166,13199,15280,15280,15248,13135,13167,13167,11054,11087,11119,11119,
11120,11120,11087,11087,11119,13199,11119,11119,13199,13200,11119,11119,11152,13200,13167,11087,11054,11086,11119,13232,11119,9006,11119,13200,
13232,13168,11119,11087,9038,9071,9039,9006,9006,13199,15280,13167,13167,13167,13167,13167,15279,15279,15279,15279,15279,15279,13166,15246,
15279,15247,15279,15344,15344,17392,17424,15344,15344,15312,17392,17424,15376,15376,17457,17489,19602,19602,21683,19603,19570,19570,19602,21651,
19602,19602,19570,17490,17489,17425,19505,19538,19538,21618,21651,21651,21651,21650,21650,23763,23731,23730,23763,23763,23762,23795,23795,23795,
25876,23795,25843,27988,27988,25843,25843,25843,25843,25875,27923,27923,27924,27924,27924,34198,34262,30036,30037,30037,27925,25812,25779,27860,
27860,27892,32086,29973,29940,31988,31988,34067,36116,38196,38228,40275,44434,48594,52754,54834,56882,56817,56817,56882,52656,50447,44075,31397,
22977,20864,22945,22913,22945,22945,22977,22977,25025,25025,24992,24992,27072,27072,27072,27072,27072,24992,24992,22944,22912,22912,20864,20833,
20833,20834,18786,14627,10466,8354,6274,6274,2439,2439,2439,2439,359,392,392,2440,2440,359,2440,2472,6633,6633,6600,8648,
10728,12808,16968,23176,25256,23208,21128,19081,14921,8679,6599,6600,6600,6601,6601,6633,6633,6633,6633,4553,4521,4520,2473,4553,
4585,2505,2504,4617,4682,2570,457,457,2570,2570,522,522,554,554,522,522,554,555,587,587,554,586,586,554,
2634,2602,2570,4586,6666,8713,12808,16903,20998,23045,25061,25028,25028,25028,22948,20835,20835,18787,18788,18788,16708,16708,18788,18788,
16708,16708,16740,18788,18821,16805,16871,19082,21293,23602,25812,25877,27989,30070,30004,29971,38325,44696,48856,48823,46710,44565,48791,52985,
53017,46679,34002,27695,25647,23567,21519,23568,21487,19375,17294,15214,15214,15214,15182,17263,15215,13134,11054,11054,13166,13167,15279,15279,
15247,13167,13167,13167,11086,11086,11119,11152,11152,11119,11119,11119,11119,13199,13200,11119,11087,13167,13199,13167,11119,11087,11087,13135,
11086,11086,11119,13232,11119,9006,11119,13200,13200,13167,11087,13167,11119,11087,9006,9006,11087,13167,13167,13135,15215,15215,13134,11054,
13167,15247,13166,13166,13166,15246,17359,17360,15311,13199,13231,15344,15344,15344,15344,15344,15344,15344,17424,17457,17490,17457,15409,15377,
17490,19603,19570,17490,17490,19570,21651,21651,19570,19570,19570,19538,17457,19505,19505,19506,21618,19538,19538,21586,21650,21650,23731,25811,
23730,23730,23763,25843,23763,23763,25843,25876,25875,25843,25843,27924,30036,30004,27923,27923,27956,30036,30036,30004,29972,32084,32084,34197,
32149,30068,27923,25811,25843,25844,25779,25811,29972,29972,32085,32052,29972,32020,32019,31987,34067,36148,40276,42355,44435,50675,54835,56882,
56849,56816,56881,58995,56915,52592,44172,37768,27235,20896,20864,20865,20897,22945,22945,22977,25025,25024,25024,25024,27072,27072,27040,27040,
24992,24992,24992,24992,22944,22912,22912,20833,20833,18786,18786,14659,12514,8354,6274,6306,2439,2439,2439,2439,359,359,392,392,
2440,2440,2440,2440,4520,4520,6568,8648,10761,12809,12808,16968,19048,19015,16968,14920,10793,8680,6665,6632,4520,4520,4520,4520,
6633,6666,6633,4553,4553,4520,2472,4521,2472,2439,391,2504,2537,457,2570,2602,489,490,522,522,521,522,522,522,
554,554,555,587,586,586,587,554,2602,2602,2570,2538,4586,6633,10729,14856,18983,23046,25093,25060,25028,25028,22980,20868,
20835,20835,18788,18788,18788,16740,18788,18788,16740,16740,16740,16708,16740,16773,16839,19018,21261,23569,25748,25812,25844,27957,27924,27859,
36213,44664,46776,46743,44598,38259,44565,50904,50937,50937,44598,34001,27695,27728,27760,27760,27761,25680,23568,19375,17294,17295,17262,17262,
17295,15247,13166,13134,13166,13166,13167,13199,13167,13167,13167,13167,13167,11119,11087,11119,11119,11151,11119,11119,13199,13232,13200,11086,
11054,11086,13135,13167,13135,13134,13166,13166,13134,13134,13167,11119,11119,11119,11119,11087,13167,15248,13168,15248,13200,11055,11087,13167,
13167,13167,13135,13102,15215,17328,15247,13102,13134,13199,13199,13134,13134,15246,17359,19472,17392,15312,15344,15344,17424,15376,15376,17425,
15376,17425,17424,17457,19570,15409,15377,17457,17490,17490,15377,15377,15377,17457,19570,21651,19570,19570,19538,19570,19538,19505,19538,21586,
19538,19538,19537,19505,19537,21618,23731,25811,25778,25811,23763,25843,25843,25843,25843,27924,27924,27891,25811,25811,27923,30004,30004,30036,
30004,32052,32084,32052,30004,32084,32084,32116,30035,30003,30003,29971,30004,29972,27891,30004,34165,34132,34132,34132,32051,34099,34099,34099,
36147,40308,42388,42355,46482,52755,54802,56849,56816,56816,58962,58994,59060,58996,52592,42027,31461,25057,20864,20865,22945,22945,22945,22977,
22977,22976,24992,25024,25024,25024,27040,27040,24992,24992,24992,24992,22944,22944,22912,20865,20833,18786,18786,14627,12514,8354,6273,6274,
2407,2439,2439,2439,359,359,392,392,2472,2440,2440,2440,4488,4521,6600,6600,8680,10760,12841,14921,14920,14888,14888,10728,
6632,6600,6633,6633,4520,4520,4520,4520,6633,6633,6633,4553,4521,4553,4553,4553,4520,2472,2472,2472,2472,2537,4683,2570,
490,522,522,522,522,522,554,554,555,554,554,554,587,586,554,554,554,2602,2570,2538,4586,6633,8681,12776,
16936,21031,25126,25093,25060,25028,25028,22948,20868,20835,18788,18788,18788,18788,18788,18788,18788,18788,16740,16707,16740,16773,16806,16937,
21294,23570,25715,25812,23797,25844,27925,32053,32052,38358,44664,42582,44630,38324,36178,42517,50904,53017,52985,46646,40340,38227,38227,36146,
31921,29840,27760,25616,21487,19375,19375,19375,17327,17327,15279,15247,13199,13166,13167,13167,15247,13167,13134,13166,13199,13167,11087,11087,
13199,13232,13199,11119,11151,13232,13167,11087,11086,11054,11022,13102,15183,15182,15182,15214,13133,13134,13166,13167,11119,13167,13199,13167,
13167,15248,15248,17329,15248,13135,13167,13167,11087,13135,13135,13135,15215,15215,15215,13134,13166,15247,15279,15247,15246,15247,17359,17392,
17392,15344,15312,13263,15344,17457,17457,17457,17424,17425,17425,17425,17425,15377,15377,17457,17457,17425,17425,15344,17457,17457,19505,21618,
19570,19538,17457,17457,17457,17425,17425,19506,21650,23731,23731,23730,23730,23730,23730,23698,25778,27923,27956,25843,25843,25843,25810,25843,
27956,30004,27891,25810,27890,27891,30004,32084,32052,32052,34100,34100,32019,32019,29970,32051,32083,34131,32051,34131,34132,34164,36213,36212,
36180,34131,34099,38292,40373,40372,40339,38226,40307,46581,44435,46482,50674,54770,54768,56881,56816,58929,58961,56946,59060,59093,56850,42027,
31396,25057,22945,22946,22946,22977,22945,22945,22945,22944,22944,24992,25024,25024,27040,27040,24992,24992,24992,25024,24992,22944,22944,20864,
20833,18785,16706,14626,12546,8386,6274,6274,2407,2407,2439,2439,391,392,2440,2472,2440,2472,4520,2472,4520,6601,6600,6600,
6600,8712,8713,10793,12840,12808,12840,8679,6567,4552,4553,6633,6633,4520,4520,6601,6633,6633,4585,4553,4520,4553,4586,4585,
4553,4552,4552,4553,4520,4585,4586,2505,522,522,522,522,554,522,554,554,555,554,522,554,586,554,554,522,
554,2602,2570,2570,4618,6666,8713,12809,16936,21031,23110,25125,25093,25028,25028,24996,22948,20868,20868,18820,18788,18788,18788,18788,
18788,18788,18788,16739,18788,16773,14758,16905,21294,23570,23635,23700,23797,25845,27893,32053,32052,34165,42583,44663,42550,40437,34098,40372,
48792,52985,53017,53017,55033,52952,50839,50839,46613,40307,36115,34034,27761,23535,21455,19407,19440,19440,17360,17360,15247,15247,15247,13199,
15247,15247,15279,15247,15247,13167,13167,13167,13167,13232,15312,11151,11119,11119,11055,11087,13167,15215,13070,13070,15150,15150,15214,17294,
15214,15214,13134,13134,13167,13167,13167,15280,15215,13102,13135,15248,15216,13135,13135,11087,11087,13135,15215,15183,15150,15182,15214,13166,
13166,13167,15247,15247,15247,15246,15279,15279,15279,15312,15312,15344,13264,13263,15344,17425,17425,17425,17425,17392,15344,17425,17457,15344,
17424,17457,17425,17425,19538,19506,19506,19570,19570,19538,17457,17457,17457,17457,19505,21586,21650,23730,23763,25811,25811,27923,25843,25810,
27891,27923,27923,25875,25875,25810,25778,25810,27923,30036,32085,30003,27923,29939,29971,34132,36181,38261,38261,36148,34068,34067,31986,34067,
38292,40373,40340,38260,36147,38227,42421,40340,38227,40307,40274,46613,50807,50774,50806,46580,48628,52821,52788,52755,54834,54736,56816,58929,
56816,58928,56880,56914,59060,59061,56883,44107,29316,22977,22945,22946,22946,22978,22978,22945,22945,22944,22944,22944,24992,24992,24992,27040,
24992,24992,24992,25024,25024,22944,22912,20864,20833,18785,16706,14626,12546,8386,6274,6274,2472,2439,2440,2440,391,392,2472,2472,
2440,2440,4521,4521,4520,4521,6600,6600,6632,6632,6633,8680,8680,8647,8647,6567,6567,4552,4552,4520,4520,4520,4520,6601,
6666,6633,4553,4553,4553,4553,4553,4520,4520,4520,4520,6633,4553,4521,4553,2538,2602,522,522,522,554,554,587,554,
522,554,522,554,554,554,554,522,522,2570,2538,2570,4650,6698,8746,10793,14856,18983,23111,25126,27173,27109,25061,25028,
25028,22948,20900,20868,18820,18820,18788,18788,18788,18820,18820,18788,18788,16741,14726,14857,21326,25650,23667,21651,23764,25877,27958,32085,
29972,29939,34164,42551,42551,40405,38260,42486,48791,46711,50904,55065,55065,57145,57112,55064,55000,52920,48792,46711,42454,29874,23567,21487,
21488,19440,19440,17360,15280,15280,15280,15280,15247,15279,15279,15279,17360,15279,15279,15279,13134,11086,11151,11119,11086,11119,11087,11022,
13103,15215,15183,13070,15117,15182,19342,19374,17294,15246,15214,13134,13134,13167,13167,15248,13167,11022,13070,13135,15215,13135,11054,11054,
13135,15248,15248,15150,15150,15182,15215,15214,13102,13134,13134,15247,15279,15246,13166,13167,13199,15312,13264,15344,15344,15312,19506,21651,
17457,15344,17392,17392,17425,17425,17425,15344,15312,17425,17425,19505,19473,19473,19538,21618,21650,19570,19538,19570,19570,19570,21618,21586,
21585,21617,21650,23730,23730,25778,25842,27891,25810,25810,25810,25842,27923,27923,25810,27890,27890,29971,32052,30003,29971,29971,32019,36180,
38261,40342,40341,38229,40341,40373,42421,42421,42421,44534,48727,48727,46581,46581,48694,48694,48726,50774,50806,54967,57047,57015,57014,54933,
54901,56981,59061,58996,54769,54704,58897,58929,56815,58896,54735,54801,59028,59028,54737,41995,29284,25058,25026,25027,25027,22978,22978,22945,
22945,22945,22912,22912,24992,24992,24992,24992,24992,24992,25024,25024,24992,22944,22944,22913,20865,18786,16674,14626,12546,10434,8322,6274,
4552,2472,2440,2440,392,2440,2440,2440,2440,2440,2472,2472,4521,4521,4553,6665,4585,4585,6633,6633,6632,6600,6567,4519,
4520,4552,4520,2471,2472,4520,4553,4520,6633,8714,4521,4553,4553,4553,2472,4520,4521,6601,6633,6634,6634,4553,4553,4586,
4651,2570,2570,2602,554,554,587,2635,522,522,522,522,522,554,554,554,522,2570,2570,4650,6731,6698,8714,10793,
12841,16937,21064,25158,27173,27141,27108,25060,25028,22948,22948,20868,20868,18788,18788,18788,18788,18788,18820,18820,18788,16741,14758,16970,
21359,23634,21587,21619,25812,25877,30070,34199,30005,25779,27826,36212,38357,38260,38260,42518,46711,42485,46646,52953,55066,57145,57145,57112,
54999,55000,55065,55065,55033,46646,34066,25680,23600,21520,19440,17392,17360,17392,17360,17360,17360,17327,15279,15279,17392,19473,17360,15247,
13166,11086,9037,11118,11119,11087,11087,11054,11054,13167,13167,13134,15182,17262,21455,23535,21422,19374,17327,15214,13134,13167,15247,15247,
13070,13038,13070,13102,15215,13167,11086,11087,13135,13167,15215,13070,13102,15182,15182,15214,15182,13134,13166,13166,15247,15279,13167,13167,
13199,15312,13232,13231,15312,15344,17457,19538,19505,15344,15312,15344,15344,15344,17392,15344,15344,17424,19505,19537,17457,19505,19538,21618,
21651,19570,19570,19570,19570,21618,21618,19537,21585,21617,21617,21617,23697,23729,25778,25778,23697,25810,23729,25778,25810,27891,27891,29971,
29971,29971,32052,32084,34164,34132,38261,40341,40308,42421,44534,46614,48759,50840,50808,50775,48694,52855,55000,57048,57048,55000,55000,55032,
57113,57080,59160,59128,59095,59127,59094,59093,59028,58994,56881,54736,52623,54736,58962,56816,56751,58863,54637,54735,58994,58962,52493,37703,
29219,27107,25059,25028,25060,25059,25059,25026,22945,22945,22912,22944,22912,22912,24992,24992,24992,27072,27072,27104,25024,22944,22945,22945,
20866,18786,16706,14594,12514,10434,8354,8354,4520,2439,359,392,392,2440,360,2440,2472,2472,2440,2440,4553,4553,2472,4553,
4553,4553,4553,6601,6633,6633,4552,4520,4520,4552,4520,2439,2472,2472,4520,4520,6633,6666,4553,4521,4521,4553,4521,6601,
8714,8714,8714,6666,6666,4586,4553,4586,4586,4586,2570,2603,2635,2635,554,587,554,554,555,554,554,522,554,555,
2603,2603,2602,4650,6731,8811,8778,8746,10826,14889,18984,25159,27174,29189,27108,27108,25060,25028,22980,22948,20868,20868,20836,18788,
18788,16740,18820,18821,18788,18821,16839,19115,21424,21554,21554,21586,23732,25844,27925,27925,29973,25779,25746,29970,34132,34067,36147,40373,
42518,44599,44598,48760,55066,57146,59193,59160,57080,57080,57113,55032,57112,57112,46678,34066,27793,25713,21553,21553,19473,19472,19472,19472,
21520,19440,19472,17392,15279,17360,17392,17327,15279,15247,11086,11118,11118,11086,11055,11087,13135,13167,13167,17295,17295,17229,25681,31987,
25680,21487,17261,15182,15215,13199,13134,13134,13134,13070,13070,13102,13135,13135,11086,11054,11087,13135,15215,15215,15215,15182,15215,13134,
15182,15247,15247,13167,13167,13200,13232,13199,13199,13232,13232,15312,15312,15312,15344,17425,19505,17457,15312,15312,15312,15344,15344,15344,
15344,17424,17457,17457,19537,19570,21618,19538,21618,21618,19569,19537,19537,21617,21617,21617,21617,23697,23697,21617,23697,23697,23697,25777,
25810,27923,25842,27891,30004,29971,29971,32084,34132,34132,36244,38292,38324,38292,40372,42388,44468,48694,50839,52918,55032,57112,57048,57048,
57080,59128,59128,59160,59160,57111,57112,57112,57112,59128,59160,59127,59095,59061,59028,56947,56882,56815,52622,50510,52655,56914,59027,54736,
56750,56750,50411,52557,58928,58896,48266,33444,31235,29155,27108,25060,25060,25060,25059,25027,22946,22945,22945,22944,22912,22912,22945,24992,
25024,27072,27104,27104,27104,25025,22977,22946,20866,18787,18787,14627,12482,10402,8354,8354,4520,2472,2439,392,2472,392,359,2440,
2472,2472,2472,2440,2472,4553,2472,4521,4553,4553,4553,6633,6633,6633,4553,4520,2439,2440,2472,2472,2472,2472,2472,4552,
6633,4585,4520,4521,4553,4553,4553,6634,10859,17166,15053,8746,6634,4585,4586,4586,4586,2538,2570,2602,2635,2635,554,554,
554,555,555,555,555,522,522,554,2635,2635,2602,4650,6763,8844,8811,8779,10826,12874,16968,23111,27207,27174,27141,27109,
25060,25060,22980,22948,20900,20868,20836,20836,18788,16740,18788,18820,20869,18854,14791,19148,21456,21522,19506,21554,21619,25812,25876,27892,
25779,25746,27826,32019,34067,31954,34067,38293,40373,42518,44631,44599,50840,55065,59161,59160,59128,57080,57113,57113,57080,57080,52985,44566,
34034,27761,25713,23633,23601,23600,23600,23632,23600,23633,21552,21553,19472,15279,15279,15279,17360,15312,13166,11086,13167,13167,11086,13135,
15215,15215,15182,15215,17294,17262,23568,31987,23567,19342,17229,15182,15214,13166,13134,13102,15215,15215,15183,13167,13167,11086,9006,11054,
11054,13135,13167,15215,15215,13167,15214,13134,13166,13167,13199,13199,13199,13199,13232,15312,13232,13200,15312,17424,17392,17392,17425,17425,
17425,19505,17392,15312,15312,15312,15344,15344,15344,17392,17424,17457,19569,21618,21618,19538,21586,21618,21617,19537,19504,21585,21617,21617,
21585,21617,23697,21617,23730,25778,23729,25778,25810,27923,30035,30003,32083,32083,32051,36179,36179,38259,40339,40339,42451,44500,46580,48693,
52855,54999,54966,54999,57111,59160,59160,59160,59160,59127,59095,59127,59159,59159,57111,57079,57079,57046,59127,59094,59029,54802,52689,50511,
48397,50477,50510,52655,56882,59028,59027,56784,56783,56750,50379,50444,58896,56783,46121,31331,29154,29155,27108,25060,25092,27140,25060,25059,
22978,25025,22945,22944,22912,22912,24992,25024,25024,27072,27072,27072,25025,25025,22978,20898,20899,18819,16707,14562,12514,10434,8354,8322,
4520,4520,2472,392,2440,2440,2440,2440,2440,2472,4521,2472,2440,2472,4553,4553,4521,4553,4553,4585,4585,4585,4585,2472,
2440,2472,2504,2440,2440,2504,4553,4585,6665,6665,4553,2472,2473,4553,6634,10827,12972,23505,21392,12940,8746,6634,6634,4586,
4586,4618,2602,2602,2602,554,555,554,554,555,555,587,587,554,522,554,2635,2635,2603,4683,6763,8844,8843,8811,
8779,10826,16969,21096,25191,27206,27173,27141,27141,25061,25061,22981,22949,20901,20868,20836,18820,18788,16740,18820,18821,18854,16839,19148,
21456,19473,21554,21586,21618,23763,25812,25812,23698,21585,27859,29939,27794,29842,34035,36180,36245,38325,42518,42486,46614,52888,57048,57080,
59160,57080,57080,57113,59160,57080,57113,55033,44533,34001,27696,25616,23535,25615,29841,31954,29873,27761,25680,23633,23633,19473,15279,15247,
17360,17360,13199,13134,13167,15247,15215,13135,15215,15247,15214,15149,15149,17229,19309,21422,19309,17229,17262,17295,15214,13166,15214,13134,
13134,15215,15247,15248,13167,11086,9039,11119,11087,11119,13167,13167,13167,15247,13199,13199,11118,11086,13167,13200,13232,13199,13232,13232,
11151,11151,15312,17457,17457,15344,17392,17424,17392,17392,15344,15312,15312,15279,15279,15312,15312,17392,17392,17424,19505,19505,19505,21585,
21585,19504,19504,21553,19504,19504,21552,21585,21585,21585,21585,21584,23697,25778,25810,27890,27890,27890,30003,30003,32018,34066,36146,40339,
44500,46613,50774,52886,52917,52885,54966,57047,59127,59095,59127,59127,59159,59159,59126,59094,59094,59094,57014,59094,59127,57046,57046,57045,
57045,59093,59061,58996,54737,48430,46284,46251,44203,46317,52623,56882,59060,59060,58995,58897,58864,54637,50379,52557,58928,54670,44041,33412,
29154,29122,27107,25060,25092,27140,25091,25059,25026,25026,25025,22945,22944,22944,24992,25024,25024,27072,27072,27072,25025,25026,25026,22946,
20899,18787,14594,12514,14595,12515,8354,8322,4519,2472,2440,2440,392,2440,2472,2472,2440,2440,2440,4521,4521,2440,2472,4521,
2472,2472,2473,4553,4585,4585,4585,2505,2505,4585,4618,4585,2472,2472,4585,4585,6665,6698,6633,4552,2473,2473,6633,10794,
15053,21359,19278,15053,10827,6666,6666,4586,4618,6731,4683,2570,522,554,555,587,554,522,522,555,2635,2635,2602,2602,
2635,2635,2635,2634,6795,8844,6763,6763,8811,10826,12873,19049,23176,27238,29254,29253,27173,27141,25061,25061,22981,20901,20869,20868,
18820,18820,18788,18820,18821,18854,16904,21261,23504,19408,19473,19505,21586,23699,23698,21618,21585,21585,25779,25746,23568,27729,31923,31955,
34132,38325,38357,38325,44566,50840,54968,55000,57080,55000,52887,52887,57112,59160,57112,57113,55000,46581,36049,31824,36050,38195,40308,42485,
42453,36115,29841,25680,23600,23633,19505,19441,17393,15312,17392,17392,15279,15247,15279,15215,15247,17294,15214,15214,15181,17261,19342,19277,
19277,19309,17229,19343,19375,17327,17359,15214,13134,15215,15215,13167,11119,9038,9039,11152,13232,11119,11119,13167,13167,13199,15312,15280,
13199,11119,13199,13232,13232,13199,13232,13232,9038,11119,13231,17392,17424,15312,15311,17392,17424,17392,15311,15312,15311,15247,15279,15279,
17359,17424,17392,19472,19472,19472,19504,21553,19472,19439,19439,19472,21519,21520,21552,21552,21552,23633,23665,25745,25745,25745,25777,27890,
27890,27889,32018,34131,36178,40371,44532,50773,54966,57014,57014,54933,52819,52819,57013,59094,59127,59127,59159,59126,59126,59093,57012,56980,
56980,56980,56981,59062,59094,59094,59094,59125,59125,59060,56915,52624,48398,46284,46251,48364,50477,50575,56882,59060,59093,56947,56849,56849,
58896,52524,50379,54637,58928,56750,48202,37637,31267,29122,27107,25059,25092,25091,25059,25059,25059,25058,25058,25025,25025,24993,25024,25024,
25024,27072,27104,27104,27105,25058,25027,22947,18754,16674,16674,16707,14595,10434,8354,8321,2407,2472,2472,2472,2440,2440,2440,2440,
2440,2440,2440,2472,4521,2472,2440,2472,4553,4553,2440,2440,4553,4586,2505,2505,4585,2505,2505,4585,4553,2472,4552,4552,
4585,6633,6633,4585,4585,4553,6601,8682,10859,15053,12972,12940,10859,8746,6698,6666,6699,6764,4683,2602,554,554,554,554,
554,522,522,2635,2635,2603,2602,2602,2634,2635,2635,2634,6796,6795,6763,6731,8811,10859,12874,19081,25321,31496,29318,29285,
29253,27173,27141,25093,25062,22981,20901,20900,20901,18820,18820,18853,18821,18854,19017,23374,21456,19376,19441,21553,21586,21618,21585,21585,
19472,23633,25779,25746,23536,25649,31955,29907,32051,38358,38390,38293,44566,52920,55000,52887,50807,50807,48694,46548,52854,57080,57080,57080,
57048,54968,50775,44469,46581,52920,52920,50807,50871,46678,38227,29841,25648,23600,23633,21585,19473,19440,19504,19505,17392,15279,13198,13198,
15278,17327,17294,17294,17262,17262,19342,19310,19309,19310,17230,17262,19343,19407,19407,15181,15214,15214,13166,13167,11119,11119,11151,11151,
13199,11151,11119,11087,13167,13167,13231,13232,13232,13231,11151,13199,13232,13199,13199,13232,13232,13199,13199,15279,17359,17360,17392,17360,
17392,19473,17392,17360,15311,15247,15247,15246,15279,17359,19440,19472,19504,21553,21520,21520,21520,19439,19439,21519,23600,23600,23568,25680,
25713,25713,27826,27826,29906,29905,27825,29970,32018,32017,36178,42452,46645,50837,52917,57046,59093,56981,54835,50641,50706,56981,59126,59094,
59126,59159,59159,59094,59061,56980,56979,56947,56980,56980,56981,59094,59094,59094,59093,59092,59027,54769,48462,46220,46220,46251,46219,48331,
50542,54834,59060,59093,56948,52689,54736,58897,58864,52492,50379,54605,58831,56750,52460,43944,37541,29122,27074,25059,25091,25091,25091,25059,
25059,25058,25058,25058,25025,25025,25024,25024,27104,27104,27104,27104,25025,25058,22946,20834,16706,16674,16707,16675,12482,10369,10402,8322,
2407,2472,2472,2473,2440,2408,2408,2440,2472,2472,2472,2472,2472,2472,2472,4521,4553,4585,4553,4553,4553,4585,2505,2472,
2473,2505,2472,2472,2472,2472,2472,6633,8746,6665,4552,4553,4585,4553,4521,4521,6633,8746,8746,8746,6698,6666,6666,4651,
4651,4651,2602,522,554,554,587,587,2667,2667,554,2603,2635,2602,2602,2635,2635,4715,2635,2634,4715,6763,6763,6763,
8811,10891,15019,21194,33773,44141,33511,29285,29253,29220,27141,27141,27142,25061,22981,22949,20901,18853,18853,18853,18853,16806,19017,23374,
21424,19376,21521,21586,21586,21585,21585,21585,19472,23633,25779,25714,23536,25649,27762,31987,29939,32084,38423,38326,42518,50905,50872,46613,
42420,40340,40307,40275,48694,55032,57047,57015,54967,57048,57113,55000,52887,57048,55000,54967,54999,52952,48759,38259,29841,25680,23567,21487,
21487,21520,19439,17391,17359,15278,15278,15278,15246,15278,17326,17294,17294,17261,19309,21390,19310,19310,19310,19310,17263,19375,17327,15214,
15214,15214,15247,15247,13232,13264,13264,13232,13199,13199,13167,13167,13167,13199,13231,13232,13232,13231,13232,13231,13231,15280,13199,13199,
15312,15312,17360,17359,17327,15279,17360,19440,17359,17359,19440,17360,15279,15279,15279,15246,15246,17327,19439,21552,21553,21552,21552,21487,
21487,21520,23600,25680,25680,25648,25648,25680,25713,27793,27793,29873,31986,31985,31985,36178,38259,40371,46644,50838,54998,57046,57013,56948,
54802,52722,52722,54835,56981,59126,57045,59094,59126,59126,59093,59061,57012,56947,54866,54834,59028,59093,59094,59126,59093,59028,58995,58994,
54801,50510,44139,44139,44171,44106,44106,46251,52623,58995,59061,56915,52656,50543,54704,58929,54670,48298,48265,50379,52492,50379,50315,43911,
41831,37573,27106,25090,25091,25091,25059,25059,25091,25059,25058,25058,25057,25057,25024,25056,27104,27104,27104,27072,27105,25026,22914,20866,
20899,16706,14594,14594,12482,8321,8321,8354,2407,2440,2472,4553,2473,2440,2408,2440,2472,2472,2472,2472,2440,2472,4585,6666,
4585,4553,4553,4585,6666,4553,2505,2472,2440,2472,2505,2472,4553,4520,4520,6633,10858,10858,6633,4520,4553,4585,4553,4553,
4553,6633,6666,6698,6698,4618,4618,4650,2570,522,522,522,522,554,554,554,2635,2635,554,2602,2603,2635,4715,4715,
4715,4715,2634,2634,4683,6763,6795,6763,8812,12939,15019,23372,44370,50480,37736,33446,31300,29220,29221,29189,27141,25061,25029,22949,
20901,18853,18853,18853,20901,18854,19017,23406,21424,21456,21489,21554,23634,21586,23634,21585,21520,23633,23665,23600,21455,23568,25648,27826,
29971,30003,36310,36278,38325,46745,48825,40405,34002,29841,29841,34034,42421,50839,57080,57048,57048,59161,59161,57080,57080,57080,57047,57079,
57047,55000,55065,46646,34034,29840,25615,21454,21422,21486,21487,19406,17326,17359,17391,17359,15278,17327,17327,19375,19375,19374,19342,21422,
21422,21422,21391,19343,19311,19343,19375,15214,15214,15247,15247,15279,15279,15279,13231,15312,15312,15279,15279,15280,15247,13199,13231,13231,
15279,15279,15279,13199,13166,15247,15279,15279,15215,15214,17327,17359,19407,17327,17327,19440,19440,19408,19408,17359,17327,17359,17359,17359,
17326,17327,19407,21520,21520,21520,21520,21520,23600,23600,25713,25680,25615,25648,27728,27728,27728,27760,29840,31888,31952,34000,36113,40273,
44532,52886,57079,57078,59094,56980,52722,50608,50608,52721,54867,57013,59093,59093,57013,59094,59126,59125,57012,56947,54834,52720,52721,54834,
59029,59126,59126,59061,59028,58963,56850,52656,50510,46252,42027,44107,42027,39946,44171,50542,56849,59028,56947,50543,46317,48365,50478,56784,
56751,48266,46153,46153,46185,46120,41862,41798,48106,50219,37703,27170,25090,25091,27139,27139,27139,27139,25058,25058,25058,25057,25056,25056,
27104,27104,27104,27104,27105,25026,22946,22947,20899,16707,16675,14594,10434,8321,8321,10434,2407,2439,2472,4553,2473,2440,2440,2408,
2440,2408,2440,2440,2472,2472,4585,6634,6634,4585,4553,4553,6666,4553,2472,4553,2472,2472,4553,4585,4585,6633,6632,8713,
10859,10826,8714,6601,6633,6666,4618,4586,4618,6666,4618,6698,6731,6731,4683,2570,521,554,2634,586,554,522,522,2603,
2603,522,554,2635,2603,4683,6796,6796,4715,2667,2634,2634,4683,6795,8844,8844,10924,12972,17099,29645,48596,48398,39881,37671,
33413,31333,31301,29221,29189,27141,25061,22981,20933,20933,18885,20901,20933,18886,19050,23407,21424,21456,21489,23602,23634,23633,21553,21520,
21520,23633,23600,21519,21454,21455,23535,25713,29971,30004,34230,34198,32084,36277,38358,34099,27760,27793,29874,29841,36114,46614,52887,57048,
57080,57080,57080,57048,57080,59128,57079,57079,57079,57047,55032,52952,42420,34033,31888,27695,29808,34034,31954,25615,21455,19407,19407,17327,
17326,19407,19407,19407,19375,19374,19342,21423,21423,21390,19343,21391,19343,19343,19375,19375,17327,17327,15247,15279,15279,15279,15247,15279,
17360,15280,15279,15279,15247,15247,15279,13199,15279,15280,15279,15247,15215,15214,15214,15247,17327,17294,17295,17327,19407,19407,19440,19440,
21488,21488,21488,21520,19440,19440,19440,19440,19407,19407,21488,23600,23601,21520,21487,21487,23568,25648,25648,25615,25615,25615,27696,27728,
29808,29840,34001,34001,36048,38128,40273,48628,52886,57046,59094,59061,54834,50575,50543,50511,50608,56915,59093,59061,59061,59093,59126,59126,
59093,59060,54834,54768,52687,52721,54867,59061,59094,59094,59094,59028,58995,54769,50510,46285,46252,44140,41994,39881,39914,42059,48398,54801,
59027,58995,52688,46285,39978,42026,44139,52558,56783,52492,50379,46120,44072,46120,39750,41798,48105,48138,46090,37702,27203,25090,27139,27171,
27171,27139,25058,25058,25058,25057,25057,27104,27136,27136,27104,27137,27105,25026,25059,22947,18819,16739,14627,12514,12482,10434,10434,10466,
2439,4520,2472,4521,4521,2440,2440,2408,2408,2407,2440,2473,2472,2472,4553,4586,6634,6634,6633,4553,4585,4585,2472,4553,
2472,2472,4552,4585,6633,6633,8681,10793,12906,10826,8713,6633,6666,6666,4586,4618,4650,6699,4650,4683,4683,4683,4683,2602,
521,554,586,586,522,522,2603,2603,2603,2570,2602,2635,2635,4683,6796,6796,4715,2635,2635,2634,4683,6796,8844,10924,
12972,15052,19179,33871,46450,44140,41929,41896,35558,33380,33381,31301,29221,29222,27141,25061,23013,20965,20965,20933,20933,18886,21130,23406,
21424,21424,23570,25682,23601,23601,23601,21553,21520,23601,23600,21519,21454,21422,21487,23632,25778,27891,34230,34198,27891,27826,27826,25712,
23600,25746,32020,29906,31954,40372,48694,52887,54967,57048,57080,57080,57080,59160,59160,57047,57047,57079,57112,57113,52919,46613,42388,42420,
48726,48727,44534,36115,27728,23535,21487,21455,19342,19375,21423,19375,19374,19374,21422,21455,23503,21422,21423,21423,21423,19310,19343,21488,
21488,17327,17327,17359,17359,17327,17327,17327,17327,17327,15247,17295,15247,15279,17360,15279,15247,15311,17360,15279,17327,15247,15246,17327,
19375,19375,19375,19375,19375,19407,19407,21488,21520,21488,21488,21488,19440,19440,19407,19439,21488,21488,21520,21520,23600,23600,23600,23600,
23567,25680,25648,25647,27728,27728,27727,29808,31920,36081,38194,40306,44467,48628,52854,57047,57046,57013,59028,56882,50511,48397,46317,46285,
52689,56980,59093,59094,59094,59094,59093,59028,56947,54833,52720,52654,52687,56947,59094,59126,59094,59061,56948,56883,54737,48397,44107,39946,
44107,46220,41995,39914,42060,50511,54802,59028,58995,52656,46284,42059,39946,41993,44106,54670,56751,54637,50379,44007,41894,44040,39782,37605,
39653,37605,41896,48332,35655,25091,27139,27171,27171,27139,27139,27138,25058,25057,27105,27137,27137,27136,27136,27105,27105,25058,25059,22979,
20867,16739,14594,12514,14595,12514,12514,12515,6633,6601,4520,4521,6633,2440,2408,2408,2408,2408,2440,4521,2473,2472,4553,6666,
6634,4585,4585,2472,2472,4553,4553,4585,4553,4520,4520,6568,6600,8680,10728,12841,14986,12874,10793,8778,6697,4617,4617,6730,
4683,4651,4683,4683,2570,2570,4683,2635,554,522,554,554,522,555,2635,523,2603,2603,2602,2635,2635,2635,4747,6796,
4715,2635,4715,4683,4682,8875,8843,10956,12972,15019,21291,38031,44270,41993,44041,46121,41895,37573,33413,33381,31302,29221,29189,27109,
25029,22980,20932,20933,20934,18919,21163,21358,19343,21456,23601,23602,23569,23569,23633,23633,21520,21520,23568,21487,21454,21421,23534,23599,
25712,25778,32150,32182,25811,25746,25714,23665,23633,25713,29972,34100,31987,38228,46646,48726,52854,57015,59128,59129,57048,57080,59128,57014,
54934,54967,57047,57080,57112,57048,54967,57048,57080,57048,52855,50774,42355,31888,27695,25583,23503,21423,21423,21423,19310,21390,23470,23437,
23470,25583,23503,21423,21423,19342,19342,21455,21455,21423,21455,21455,19407,19407,19407,17327,17327,17327,17327,17327,17327,17295,17295,17294,
15214,15278,17391,17359,17327,17327,17327,17359,17359,17359,19407,21455,21455,21488,23536,21456,23536,23536,21488,21455,21455,21455,21487,21487,
21455,23567,23600,23568,23600,23600,25648,27728,25648,25647,25680,27759,27759,31920,34000,36081,40274,42387,46579,50773,54967,57048,59128,59128,
59094,59028,56818,50446,50413,48333,44107,46317,56915,59093,59093,59094,59094,59029,58996,58930,52655,48429,48493,52719,54897,59125,59127,59094,
59061,56915,54801,54704,50478,44172,39914,37769,39882,42027,39978,42059,46350,54802,59060,59061,54770,48366,42059,41993,41993,41960,48266,56751,
54605,52459,48201,41862,39749,41862,41895,39750,35557,29219,33510,52591,48366,29317,27139,27171,29219,27171,27171,27139,25090,25057,27137,27137,
27137,27137,27137,27137,27137,27138,27106,25026,20899,18786,16673,14593,16707,14594,12481,12514,6601,4520,4520,4553,6601,4520,2408,360,
2408,359,2408,2440,4521,2441,2473,6666,6666,4553,4553,2505,2472,2440,2472,2472,2504,4552,6600,8681,12906,12841,12808,14922,
19115,14954,12873,10858,8777,6697,6730,6730,6731,4683,4651,2570,2538,2570,2602,2602,554,522,554,554,555,555,555,522,
2636,2635,2602,2603,2603,2635,4683,4715,4715,4715,4683,4683,6763,8875,10923,13004,13004,15019,23372,42224,46350,41993,44040,46153,
46120,41830,35493,33413,33382,31301,29221,29189,25061,23012,23012,22981,20934,21032,23276,21359,19343,21456,23569,23601,23569,21520,23601,23601,
21456,21456,21488,21455,23502,21421,23534,23599,23632,27891,30037,30005,25811,23666,23666,23666,23634,23633,27826,29939,31987,34099,44534,48693,
50774,57015,59128,59129,54935,54935,54967,54967,54967,54967,54967,54967,57047,59128,59128,59128,57047,57015,54903,54935,52887,46581,34001,27663,
27663,27663,25583,25551,23470,23437,25517,27565,27598,27663,27663,25551,23470,23470,23438,23470,23502,23534,25582,23534,23470,21422,21454,19407,
19407,19374,19374,19374,19375,19375,17262,17294,17294,15246,17326,17327,17327,17327,19407,19439,19440,19440,21520,23568,23536,23536,23536,23536,
25616,23568,23535,23535,23568,23535,23568,23567,23535,23567,25648,25648,25648,25648,25648,27728,27760,25679,25679,27759,31887,36113,42387,46580,
50774,52886,54966,57079,59128,59096,59128,59095,59094,54802,48365,46220,46187,44074,44140,48464,56948,59093,59093,59093,59061,58996,58930,54704,
48332,44170,50542,54866,59124,59158,59127,59094,59028,56849,54736,52591,46284,42027,37769,37801,39947,42059,42091,42124,50576,56980,59061,54835,
48431,44140,42026,41961,41960,46153,52492,56685,52492,48201,46088,43944,39750,37669,39782,44009,37703,25058,29284,50446,56818,39881,29251,29219,
29219,27171,27171,27171,27138,27137,27137,27137,27105,27137,27137,27137,27137,27138,27106,25026,20898,18786,16705,18722,18787,16674,12513,12514,
4520,4488,4520,4553,4520,4521,2440,2408,2408,2408,2408,2440,2473,2473,4521,6634,4586,2505,2472,2505,2472,2440,2472,2472,
2471,4584,6632,17100,29745,21261,16969,19082,21195,19082,14922,12874,10793,8778,6730,4618,4618,4618,4618,2538,2538,2602,2570,554,
554,554,554,2635,2635,2635,2603,522,2603,2635,2602,2635,2635,2635,2635,2634,2634,4715,4715,4682,6795,8875,10923,13004,
15052,17132,27565,44336,44237,41960,46088,48200,48233,46088,39686,33413,33414,31334,29253,29222,27173,25093,23012,23013,20966,21065,23309,21359,
19343,19407,23568,23601,23569,23569,23601,23569,21488,21456,21487,23503,23470,21421,21421,23502,23600,30004,27859,23698,25779,23699,23666,23634,
23634,23634,25714,29907,32020,34100,42453,46614,48694,54968,57081,55000,52855,50774,48628,52854,57080,57080,57047,54999,54934,57015,59128,57048,
57047,54935,52822,57015,57048,52920,46581,38129,33936,29743,29711,29679,31792,31792,31791,35952,33871,29646,29678,31791,29679,27566,27597,27597,
29678,29710,29742,29774,29774,29775,27662,23535,23535,21487,21455,19374,19374,19407,19375,19407,19407,19375,19407,19439,21487,21487,21487,21520,
23568,23568,21520,23568,23568,23568,23536,23535,25615,25616,25616,25615,25648,25647,25648,25648,25648,25648,25648,25648,25648,25680,27728,25679,
25679,27760,29872,34000,38226,46612,52854,57047,59127,59159,59159,59159,59127,59127,59095,59094,56948,46350,42059,44075,39848,37768,42092,52722,
59094,59126,59093,59092,59059,56881,54736,52590,46317,46317,52722,59061,59158,59159,59126,56981,54802,52656,50478,46220,42059,37833,33576,37737,
41995,44140,46285,48463,56882,59092,56915,48431,42091,41994,44074,44074,41960,50379,56718,52492,50346,46120,48169,46089,37637,35524,35557,37703,
35591,25059,25091,44140,58931,50479,35622,29218,29218,27171,27171,27171,27170,27170,27138,27105,27105,27137,29185,27137,27137,27138,27106,25058,
22979,18785,18754,18786,18787,18787,14594,12481,6633,4520,4520,4520,4488,4520,2440,2408,2408,2408,2440,2440,2440,4553,6634,4586,
2473,2473,2472,2472,2472,4553,4585,2536,4584,6697,8745,17099,25486,21227,19082,23210,27436,23243,19049,17002,12906,10826,8779,6731,
6699,4618,4618,2570,490,522,554,554,2635,554,2635,2635,2603,2635,2603,523,522,2603,2635,2635,4683,4683,4683,2634,
2634,4715,4747,4682,6795,8843,10923,13004,15020,19180,33839,46417,42091,41927,46119,48199,48200,50249,46024,37574,33414,31366,31334,29254,
29253,27173,25093,23013,20966,21065,23309,21358,19343,21423,21488,23569,23536,23568,23601,23601,21488,21488,21455,21423,21422,21389,21389,21421,
23567,27859,27892,23666,23699,25779,23699,21586,21586,23634,21553,27859,32053,34133,38260,42421,46646,52920,52920,48662,46614,48694,42355,48661,
55000,57048,57048,57047,52854,52854,54967,57047,54935,54902,54902,54967,54967,54935,52919,50807,46516,36016,33838,31758,35984,46484,46452,46419,
46451,37999,31693,35951,38032,33838,33838,35950,40209,44402,44402,44434,46547,46579,38161,29743,25583,25615,23535,23502,21455,19374,19374,21454,
19374,19374,21487,21520,23568,23600,23568,23568,25680,25681,23600,23568,23568,23535,25583,25615,25583,27663,27695,27695,27695,27727,27728,27696,
27728,27728,27728,27728,25647,25615,27727,27760,29840,31920,34033,40306,48725,57079,59094,54868,54836,56980,59093,59094,59093,59062,59094,58996,
50608,42091,39946,39882,35655,35688,46350,56948,59126,59125,59093,59059,56913,52719,48461,46284,46317,50576,56948,59127,59127,59126,59093,56947,
52656,46317,42059,39913,39914,35721,35688,37802,39914,42059,46318,54769,58995,56914,52624,44172,39946,39881,41929,41961,46186,56718,54638,46121,
48201,50314,48202,46056,39750,37669,37637,35557,35591,29285,27171,41994,56882,56850,46186,31363,29218,29250,27170,27203,27203,27202,27170,27137,
27137,29185,29185,29185,27137,27138,27138,27106,22979,18753,18753,20867,18787,18755,16675,12482,6633,4553,4488,4520,4488,4520,2440,2440,
2408,2408,2440,2440,2440,4553,4586,4521,2440,2440,2472,2472,2472,4553,4553,2536,4616,6696,8745,12841,14888,16969,23210,25322,
29516,27435,25387,21227,14953,12874,10859,8779,8779,4618,4618,4651,522,490,554,554,2635,554,2635,2602,522,2602,2602,2602,
522,2603,2634,4683,4715,4683,2634,4682,4683,4715,4747,6763,8875,8842,10923,13003,15019,21292,38032,46384,42026,41958,46086,48199,
48167,50249,50282,43912,35526,33446,31398,31333,29253,29253,27173,23046,20998,21130,23341,21358,19343,21423,21488,23536,23536,21488,23569,23601,
23568,21488,23503,21423,21422,23437,21357,21389,23535,25713,27860,25780,23731,23764,23732,21619,21554,21586,21554,23601,27827,32053,36180,40406,
44534,48695,46647,42356,42389,46614,42388,46548,52919,54967,54967,54999,54967,52822,52854,54967,52854,50709,52822,54967,57048,54967,54967,54934,
54967,50676,42257,42192,44370,52790,56983,54870,54902,50612,40079,40111,42257,44305,46417,50643,52821,57014,57047,57046,54933,54933,50740,38129,
27597,27663,27695,23502,23535,21422,21421,21486,21454,21454,21487,23535,23535,23599,25648,23567,25680,27761,27729,27728,25648,27695,29776,31856,
31888,33936,33968,33969,33968,34001,33969,29808,27727,27695,27728,29776,27695,27662,27759,34065,34065,34000,40274,48660,57014,59094,56883,52657,
50511,52656,58995,58996,58996,59061,59061,52689,42091,37833,35689,35656,33543,42060,54803,59061,59094,59093,59060,56914,50574,46380,44203,42123,
46350,52754,59094,59127,59094,56948,54802,50576,46318,42059,39914,37834,37801,35721,39914,39947,42027,48366,52624,58963,58995,54736,46285,39946,
37800,37735,37703,44041,54606,58831,50412,44008,46088,50314,48202,41863,43975,44008,41863,35590,37672,33511,31430,39882,56817,58930,54606,39814,
31297,29217,27202,27203,27203,29251,27202,27137,27137,29185,29185,29185,29185,29186,29186,27138,25027,20866,18721,20834,20835,18755,18755,16675,
4456,4488,4488,2440,2440,2472,2440,2440,2408,2440,2440,2440,2473,4521,4521,2473,2472,2440,2472,2473,2472,2472,2472,2504,
4616,6664,10825,12873,14888,16935,23242,31628,37934,42128,33806,23307,17033,14954,10827,8811,8812,6699,2538,2602,2570,489,522,554,
2635,2635,554,554,2635,2603,2602,2602,2602,4683,4683,4683,4715,4682,2634,2634,4682,4747,4714,6763,8907,10923,10955,13003,
17099,23372,40144,46350,41993,44006,46086,48199,50247,50280,52395,50315,39784,33478,33478,31365,31333,29285,27206,25126,21031,21162,21293,21358,
21391,19375,21455,23536,23568,23536,21488,23568,23601,21455,21423,21422,23470,21357,21356,21357,23502,23600,29972,30005,23732,23732,23732,21619,
21586,21586,21586,21521,23633,29940,34100,38261,38228,42421,42454,38195,36147,42421,40307,42387,52919,52919,52919,54967,57047,54934,52854,54967,
52855,48693,44467,48693,55000,54935,54935,52821,52821,56982,52756,54869,57015,57015,59095,59095,57014,56982,50708,46449,44336,48562,54868,56982,
56981,57014,59127,59094,57013,57013,57046,50709,36016,29710,29743,27630,25550,25615,25582,25614,25615,23534,23534,23534,23534,23534,25647,25679,
27728,27760,29808,27727,29807,33968,36048,38161,38161,38129,42355,42355,40241,36048,36048,33968,31855,29775,31823,29743,31823,31855,31919,34032,
36080,38128,46579,54934,59061,54802,50511,50510,48397,52623,56850,58963,59028,59028,56883,46285,35720,31527,31495,31463,35721,50544,59029,59094,
59126,59093,56947,52720,46316,42123,40043,44269,52721,59061,59125,57013,56948,54770,48462,42091,42059,44140,42027,37769,35689,37769,37801,39947,
46285,52559,56850,58995,58995,50543,42026,37768,37735,37767,41961,50412,56751,52492,46153,44040,44008,44008,46056,41863,48202,50347,41928,37671,
35592,31430,31398,33607,52624,58897,56718,50346,35555,29217,27201,27202,27202,27202,27202,27170,27137,29185,29185,29186,29186,29186,29218,27138,
25026,22914,18720,18721,20835,20835,18755,16675,2407,4488,4553,4521,2407,359,2439,2407,2408,2440,2440,2408,2440,4521,4521,2473,
2473,2440,2440,2472,2440,4553,4553,4585,4617,6664,8745,14986,17001,19048,25354,33773,46353,48531,35886,25387,19147,14986,12940,13005,
8844,6731,4618,2570,2570,522,522,554,2635,2602,522,2603,4747,4715,2635,2635,4683,6796,4715,4682,2634,2602,2634,4682,
4715,4747,4714,6827,10956,10955,13003,15051,17099,27565,44337,44205,39879,44006,46118,48231,50311,50313,52427,52493,48300,37768,33542,33510,
31365,31333,29286,27207,23176,21227,21325,21358,19343,19342,21423,21423,23536,23568,21455,21455,23536,21423,21390,21422,23438,23405,21324,21356,
23470,23535,27891,27925,23732,23764,23732,21619,21619,21586,21586,21553,23601,25714,29907,34100,36147,40373,42487,36148,36148,40373,40307,42388,
50839,55000,55000,54967,54935,54934,52854,52887,52887,50807,42387,40274,50806,54935,52822,54902,52789,56982,56982,56982,59095,59095,59095,59127,
59094,59094,57014,52788,50674,52787,56948,56981,56949,56981,59094,59126,57013,54900,56981,57047,50741,38096,31790,29710,29710,29743,27695,27662,
29775,25646,25614,27727,27694,23501,25614,27727,27727,25614,29742,33967,42322,46547,46547,48627,50675,50676,52821,52821,50741,46515,40176,38096,
33903,31822,33935,33935,33935,38128,40273,40240,40240,46547,54934,59094,56915,48430,46252,46219,50477,56816,56818,58996,59061,59028,52656,37866,
31527,27302,27270,33576,46286,56884,59061,59126,59094,56947,54769,52623,42091,33672,42124,50673,57012,59093,59060,56947,54833,52656,48430,42026,
37801,37802,35689,31495,33575,35721,35721,39914,46252,50511,56849,58962,54736,44139,37768,35655,37800,41961,48267,56784,56751,48299,46121,46121,
44008,39750,37669,43976,52460,54606,44074,35623,33479,27205,25124,27268,48332,58896,56685,56619,46054,33409,29217,27202,27202,27202,27170,27170,
27169,29185,29218,29218,29218,29218,29250,29218,27106,22945,20801,18720,22882,22948,20835,18723,4488,4520,6601,4553,2407,359,2439,2440,
2472,4521,2473,2408,2440,4521,2473,4521,2473,2472,2472,4553,4553,4553,2472,2504,4617,8810,8713,10793,14921,19081,25355,33708,
40046,37966,31661,25388,21228,15052,12972,10892,8812,6731,4683,2603,2602,2602,522,554,2635,554,522,2635,6828,6828,4683,2634,
2634,6796,6796,4683,4682,4682,2634,4682,4715,6795,4715,8908,10956,10922,13003,15083,19179,31791,44369,44204,41959,46087,48167,48199,
50344,52426,52459,54606,56784,46252,35654,35590,35526,33414,31366,27239,23209,23372,23438,21391,21391,19342,21423,19342,23504,23536,21423,21423,
21455,21423,21423,23470,23437,23437,23372,21324,21389,23535,27859,30005,25812,23732,23732,21619,21619,23666,23634,23634,23601,23601,27827,36181,
36181,40406,44632,36180,34067,38292,38227,42388,48759,52952,55000,54967,52854,52821,50741,50742,50807,50872,44565,36113,44533,52920,52855,54967,
54934,56982,56982,54869,54901,57014,57014,57014,59094,59094,59094,56981,56981,59060,56947,56948,56948,59061,59094,59094,56948,52755,56981,59094,
59095,50708,38096,31790,29742,29710,29710,29742,31887,33968,36081,40274,36081,29742,27629,29677,31822,31822,36048,48627,54933,57014,57046,59061,
59061,59029,54868,54836,56949,54902,52789,48596,40209,38095,40208,44434,46514,48627,50805,50772,50740,57013,59094,59028,54769,48365,44138,44170,
52590,56817,54770,59029,59093,56947,46318,35753,31463,27270,29383,42060,54770,56948,59061,59094,59061,54769,52558,46284,35752,31526,44269,54899,
57012,56979,56979,54833,52654,50510,48397,44075,37801,33544,31431,29350,31463,35720,37833,42026,44172,52655,58962,58962,50478,37833,33510,35623,
39881,39848,48300,58896,52525,46153,46121,46153,41960,35589,37702,50379,50379,48267,44107,33511,27205,25157,27237,25188,39913,56783,56685,56619,
52393,39715,31297,29250,27202,29250,27202,27170,29218,29218,29218,31298,31298,31298,31266,29250,29219,27106,22945,20800,20801,25028,25028,18755,
4520,4488,2408,2407,2440,2407,2407,2440,2472,4521,4521,2440,2440,2473,2440,2440,2473,2472,2472,2504,4585,4585,4553,2505,
4617,8811,8746,10761,12842,14921,21194,27467,33741,31661,27468,21228,19148,12940,8778,6698,6731,4651,4650,4651,2602,2602,554,2635,
2667,2603,2635,2667,6828,4715,2634,2634,4683,6795,4715,4715,4715,4683,4682,4715,6795,6795,6795,8907,10955,10954,15083,15083,
19244,33904,46385,44171,41991,46119,48167,48199,52425,54538,52491,54605,58897,54703,41993,35654,35590,33446,31367,27271,23241,23404,23470,21391,
19343,19310,19342,21423,23536,21455,21423,23503,21455,21423,21422,23470,23437,23404,23372,21324,21357,21454,25746,30037,25844,23732,23732,21619,
21586,21586,21554,23601,23601,23601,27827,38326,40374,42487,44632,38326,34067,34067,36147,40307,48694,50839,52887,54967,52854,52854,50742,48661,
48694,48759,42485,34001,36082,46614,50807,52854,52854,54935,54902,54837,56949,59062,59062,56982,59062,59094,59094,59061,56981,59028,56882,54834,
56883,56916,59029,59029,54835,50641,54867,59061,59062,56982,50709,40210,31823,31790,31790,35984,42323,48629,50774,52887,48693,42290,35951,35950,
40209,46516,52821,57013,57013,56948,56980,59028,56882,52690,52690,48464,44206,42191,50611,56983,52854,48628,50740,54966,57013,57013,57013,54965,
57045,59093,59028,54769,50543,46252,44138,48364,54703,56882,58996,59094,56981,50576,37898,31528,29351,27270,31496,48464,56916,59029,59093,59094,
56915,54704,50445,39912,29380,35785,52721,56980,56980,57012,56947,54768,50477,48332,46187,41962,41962,35689,31431,33544,35656,37801,39913,42026,
46252,56848,58994,54736,44139,33574,31461,37736,39849,39880,50445,56751,48299,44073,44073,44040,39814,37734,48267,54605,46186,44106,44139,33543,
25124,27270,29415,29414,37799,54573,58732,56554,54441,43941,33442,29249,29250,29251,27202,29250,29218,29218,31266,31298,31331,31298,31298,31298,
29251,29251,27106,24993,20801,20834,22947,20867,4520,2407,2407,2407,2440,2407,359,2439,2440,2440,4521,2440,2440,2440,2440,2408,
2472,2473,2472,2472,4553,6634,6666,4618,4585,4553,6633,10762,12842,12874,14921,19081,25355,25355,23275,17035,12874,10795,8779,6698,
4618,4618,4618,4683,4683,2603,2635,2635,4683,2602,2635,2635,4683,2602,2602,4715,4715,4715,4715,6795,6795,4715,4714,6763,
6795,6795,8875,8875,10955,13035,15115,15083,23404,40209,46449,44139,44039,46120,48199,48231,52425,54538,52491,54638,56848,58961,50542,39913,
35590,37606,33446,29319,25289,25453,23470,21423,21391,19310,19310,23504,23504,21423,23504,23536,23503,21423,23470,23470,23437,21324,23371,23404,
23436,21422,23600,29972,27892,23731,23732,21619,21586,21554,21553,21520,21520,23601,27827,36246,40407,42487,44632,40406,34100,34067,36146,38227,
46581,48694,50806,52854,52854,52854,50774,48661,48694,46679,40437,31954,29808,38195,46614,46581,48597,54935,54934,52756,54869,56949,59029,56948,
56948,59061,59093,59061,58996,56914,54800,52688,54769,54802,56850,54802,54770,52656,52721,56915,56948,59094,57047,50742,40177,38097,42258,44403,
50709,54935,57047,59127,57079,54901,48595,46483,52790,57016,59062,56981,59028,59028,58995,54769,52624,52624,50544,46351,39980,40013,48498,57014,
59159,57046,54933,54932,54900,56980,56980,57012,59061,59093,54802,50478,48397,46219,46218,50509,54736,58995,59061,58997,52690,42092,33641,29383,
27270,27270,35754,52625,56916,59061,59093,59061,54802,50542,48364,39912,31525,44236,54866,57012,57044,57011,56946,54768,50478,46219,41994,41994,
39850,33544,33576,35689,35689,35720,39913,44139,50478,58930,56817,46285,37767,33542,35590,35623,35622,44073,54638,50444,44073,44041,41960,39847,
35621,41928,54638,54702,46218,44074,39881,29350,25157,31528,33641,35721,35687,48234,58765,54505,52327,48134,39683,31329,29250,29250,29250,29282,
29250,29250,31298,31298,31331,31330,31298,31299,31299,29251,29218,27106,24994,22881,20769,18722,4520,2375,2407,2407,2439,2407,359,2472,
2472,2440,2472,2440,2440,2440,2440,2440,2440,4521,2504,2504,2504,2505,4585,6666,6666,4553,6633,6665,8746,10826,10761,12841,
19147,19179,17067,12906,10794,8746,8779,6731,4618,4618,4651,6764,4683,2570,2635,4716,4683,2602,4683,4683,2602,2602,2602,4715,
4715,4683,4682,6795,6795,4747,6795,6795,6795,8843,8843,10923,10955,13036,15148,17163,25548,44434,48496,42058,44039,48168,48199,50279,
52425,52490,52492,54638,56848,58994,59026,48430,37767,37670,35558,31399,27370,25453,23471,23503,21423,21390,21391,23504,23504,23504,23536,23504,
23503,23471,23470,23438,23404,23371,23371,23403,23436,23469,23535,29940,32086,23731,23731,21618,21554,21554,21521,21488,21520,23633,25746,29939,
36213,40439,42520,38293,34067,34099,36179,36146,40307,46613,50806,52855,52855,52887,52855,50775,48759,46711,38325,31953,29840,34034,44534,44533,
44468,50774,52854,50708,52755,56883,58963,56915,56883,56915,56948,59028,56914,56848,54768,54768,54768,56817,56817,54737,50511,48398,50543,54768,
56882,56980,57013,54902,48595,52790,57016,54903,54902,59062,59093,59093,57012,57013,57014,59095,59096,59064,56982,59028,59028,59027,56849,50511,
50512,46351,40012,44271,44304,48498,54869,59094,59093,57012,54899,54866,56980,59060,59028,59028,59060,59060,54737,50479,48300,44074,48299,52623,
56915,59061,58996,54737,44205,33673,29447,25221,27270,27270,39915,52657,58995,59028,59028,58995,54769,48397,44138,39945,42027,48495,54899,59125,
57044,54866,54801,54736,52591,48332,44107,41994,35656,29382,33576,35721,35721,35688,39913,46285,56817,58931,48398,39882,33575,35623,35623,33509,
37735,50412,54638,46154,41961,44009,37734,35621,37702,46218,56815,56848,50412,44075,35688,27269,29415,35819,37835,37866,31429,39848,56685,52457,
48165,48101,43876,37538,31330,29282,29282,31331,29250,29250,31298,31330,33378,33379,33379,33347,31299,31266,31266,29186,29154,29155,24994,20801,
4520,2407,2375,2407,2407,2407,359,2472,2472,2472,2440,2407,2440,2407,2407,2440,2440,2472,4552,2504,2472,2473,2505,4585,
6666,6698,4585,4585,6698,8746,6632,8713,14987,15019,12906,10890,10858,6698,6698,6698,6699,4651,4618,4651,2603,2571,4684,4716,
4683,2602,4683,4683,4683,2602,2602,4683,4715,4682,4682,6763,6795,6795,6795,8875,8876,8843,8843,10891,13003,15116,17196,19275,
31887,48627,46350,41993,44040,48168,48200,50312,52425,52490,54604,54670,54767,56880,59091,56881,46219,37734,35590,33543,29482,25453,23470,23503,
21423,21423,21423,23504,23472,23504,23504,23471,23503,23503,23470,23405,21324,23371,23371,21323,21355,21389,21422,25713,30005,25812,23731,21618,
21554,21553,21488,21488,21520,23632,25745,29939,34133,40407,40407,36180,34067,36147,36178,36178,42453,48726,48727,50839,52887,52920,52888,46646,
42420,44566,36212,29841,29841,31954,40341,42421,44468,48694,50741,50708,52722,52690,56882,56882,54802,54802,54834,58963,56849,54735,54735,54735,
56849,56849,56849,56850,50511,44172,46316,50510,52656,54802,56948,59062,56949,59063,59128,59095,59062,59061,59092,56979,56946,54899,56981,59095,
59096,59096,59094,59029,56915,52656,48430,48431,46286,37835,33674,46384,54836,56982,59062,56980,54867,54866,56979,56979,59060,59060,59028,59028,
59060,59028,54770,52527,46155,44041,48332,54736,59061,59061,54802,44172,35721,31561,27367,23109,25221,31463,46254,56818,59027,59027,58962,56849,
52623,44203,37799,39945,46285,54802,59092,59093,56979,52753,48495,46317,48332,44139,44107,39914,31494,29382,29414,33575,37769,39881,42026,50543,
58963,54705,44108,33575,31462,35591,33542,35590,46186,54703,52493,41961,41928,39848,37670,35621,41993,54670,56816,54670,50445,44139,33575,29447,
37867,37932,37899,42028,27204,31397,52492,52457,46053,45988,45956,39651,33410,31362,31330,31298,31298,31298,31330,33410,33410,33411,33379,33346,
33346,31266,31266,31266,31266,31267,29187,27107,4488,2407,2375,2407,2407,2407,2407,2440,4520,2472,2440,2407,2407,2407,2440,2407,
2440,4521,4553,4553,2473,2505,4553,4553,4618,6731,4618,4585,4585,4585,4552,6632,8778,10858,6697,6697,6698,6698,4618,4618,
6731,4651,4618,4651,2570,2570,2603,2603,2570,2602,4683,4683,4683,4683,2602,4683,4683,4682,4682,4714,4714,6795,6795,6795,
8875,8843,8843,10923,13003,15116,17196,21356,40241,48562,42123,41992,46120,48168,50280,50312,52425,52458,54604,56783,56815,54799,59058,59026,
54702,46153,37734,33575,29547,25485,23470,21423,21423,23471,23503,23504,21423,21423,23472,23471,23503,23471,21358,23405,23372,23339,23338,23338,
23371,23436,21389,23535,27892,27925,23731,21651,21554,21521,21456,21423,21488,23633,25746,29971,32052,36278,38326,34100,31986,34066,34098,36211,
40437,46711,48760,46614,44501,48695,50840,46679,38228,38325,34132,27793,27760,29873,38228,40341,44501,48694,48629,48595,50642,50577,54737,54769,
56850,54769,52688,56882,56849,54767,52654,50509,56816,58929,56849,56849,52623,48364,46284,46284,48397,54736,58995,59061,59061,59029,59062,59094,
59061,56948,56980,59027,56979,56979,56981,59095,59095,59095,59062,56916,50576,46317,46285,44140,35689,31528,39980,52755,59094,59062,56948,54835,
50640,52753,56947,56946,56947,56979,59061,59093,59060,56882,50543,46252,41994,44074,48365,56850,59061,58996,48398,37801,33576,31528,29383,25157,
25157,31527,48398,56850,58994,58962,58962,56849,50478,41993,33605,39977,50608,59060,59125,59093,56947,50543,42091,39913,41961,41961,44107,39914,
31494,31495,31527,33575,35720,42026,48365,54769,58963,48366,37801,33542,33510,33543,33510,39848,52558,54671,44106,39848,39815,37735,37702,41928,
52558,58929,54702,46251,44074,37801,31495,35786,40045,37931,42157,44173,27237,27139,46186,52458,43972,43843,43844,41732,35458,33410,31330,31330,
31330,31330,33410,33410,35459,35459,35426,33378,33346,33346,33346,33346,33346,31299,31299,29187,4455,4455,2375,2407,2375,2407,2407,2440,
2440,2440,2440,2407,327,2407,2407,2407,4488,4553,4521,4521,2473,2473,4586,4618,4618,4618,2570,2537,2538,2537,4585,2504,
6698,8810,6665,4617,2537,4617,4618,4586,4650,4618,4618,4650,2570,2570,2603,2602,2570,2602,2602,4683,4683,4683,4683,4682,
4682,4715,6763,4714,4682,6795,6795,6795,8875,8843,8843,10923,12972,15084,17163,25516,44434,46415,42026,44072,46120,48200,50280,50312,
52457,52458,54604,56783,56847,56880,59025,59058,58992,54701,44105,33672,29611,25518,23471,21455,21423,23471,23471,23471,23472,23472,23504,23471,
21423,21390,21357,23405,23404,23371,23370,23338,23370,23404,23437,21422,27795,30005,25780,21618,21554,21521,21456,19375,21487,23632,25746,29971,
29972,34133,34165,32019,29906,31953,31985,31985,34066,38325,42518,38228,36115,40341,42454,42454,38261,36180,32051,27826,25680,25680,34100,40341,
40373,46647,46581,44434,48562,48464,52624,52656,56817,54769,50543,52656,56816,56815,52621,48396,54703,58897,56816,54704,52591,48332,44171,46219,
48332,52623,58930,58995,56915,54802,56915,59028,59028,58996,56947,59027,56979,59028,59093,59094,59094,59061,56883,52657,48398,46318,44172,35689,
29415,31496,44206,56948,56949,52722,52722,56915,54834,54801,50639,52753,54866,56947,59061,59094,59028,52721,48364,44138,41961,44074,50446,58964,
59061,54738,42027,35689,33609,29416,25190,23109,25189,33575,50446,58897,58929,58896,56816,54704,46220,35687,35686,44236,54866,59125,59093,59028,
50544,42027,37833,37800,39913,44074,46220,37801,31430,31463,35721,35688,35688,42027,52591,58930,54736,42027,33575,31430,33510,33510,35590,44074,
54639,52493,41961,37735,37703,37702,39848,50412,58929,58961,50445,39880,35655,33543,35721,37932,33770,35851,48464,42125,29285,25026,41896,52459,
43973,39682,41699,41732,37571,33410,33410,33410,31330,33378,33410,35458,35458,35458,35426,35426,35426,33378,33346,35426,33378,33378,33379,31298,
2407,2407,2375,2375,2375,2407,2408,2407,2440,2440,2440,2408,327,2375,2407,2408,4488,2440,4520,2473,2473,4554,4586,4586,
4618,2505,2537,2570,2538,4586,4586,4553,6698,8779,6698,4617,2537,2537,4586,2538,2570,2570,2570,2570,2538,2570,2603,4651,
2602,489,2570,4683,4650,4683,4683,4683,4715,6763,6795,4682,4682,6795,6795,6794,8875,10923,10923,10891,13004,17164,19243,29710,
46514,44270,41993,44072,48168,50249,50280,50312,52425,52458,54604,56782,56847,56912,56977,59057,59024,58960,52589,37898,31659,25517,23503,21455,
23503,23471,21423,21391,23504,25585,25584,23471,21391,21390,23437,23405,25451,25418,25418,25417,25418,25451,23436,23437,25649,29940,25779,21553,
21521,21488,21456,19375,21455,23600,25746,29971,29972,32052,32020,29906,29873,29873,29873,27760,27793,31986,34099,31954,29842,31954,34068,36148,
36148,36181,29939,27793,25648,23535,31955,38293,40373,42486,42421,44402,48562,48463,50543,52591,54736,56849,52623,52591,54703,54703,52557,44170,
50445,56784,54703,54671,52558,44171,41993,44074,48332,52558,56784,58930,56881,54736,54736,56882,58995,59028,59060,58994,54833,54834,59028,59093,
59061,56916,50576,48399,48366,44140,37834,33609,31496,31464,39980,54738,52657,50543,54769,59027,59027,54800,50607,54800,54801,56914,59061,59029,
56915,48462,46251,44138,41961,44107,52592,58997,59029,48431,37802,35689,33545,27303,23077,23045,25189,35656,50445,56816,56815,56751,56751,54606,
44107,35687,42026,52688,57044,59125,59061,52690,39947,35655,35687,35687,41993,48333,44140,33575,29350,29382,31495,33575,35720,44171,54704,56849,
46285,33607,31462,31430,31397,33478,37736,50413,54606,46187,39848,37735,35654,37735,46219,56816,58994,54736,44106,33574,31430,31462,37834,35819,
29545,37932,50609,44173,29285,24994,33509,50379,46119,39683,39586,41700,41732,35491,33410,33411,33378,33378,33378,33378,35458,35458,35458,35426,
35426,35425,35425,35458,35426,33378,33346,31298,2407,2407,2407,295,294,2375,2407,2407,2408,2440,2440,2407,2375,2375,2407,2440,
4520,2440,2440,4521,2440,2473,2506,2506,2538,2538,2570,2570,2538,2506,2473,4553,6666,6731,6698,4585,4585,4618,2537,2537,
2538,2570,4650,2570,2570,2570,2602,2602,2570,457,2570,4650,2602,4683,4683,4683,4715,6763,6763,4682,6762,6795,6794,6794,
8875,10955,10955,10923,15052,17164,23436,38096,52755,48398,44073,46120,48168,48200,50280,52392,52425,54538,56717,56782,54766,56912,56977,59089,
59056,58960,56815,42124,29643,25549,23503,21423,21423,23503,21390,21358,23471,25584,23504,23439,23470,23438,23405,25452,25419,27466,27497,29545,
27497,27498,25452,25485,23503,27827,27827,21488,21488,21456,21423,19374,19374,21520,25745,29971,29939,29971,29939,27794,27793,27792,29840,27760,
27793,29906,27826,25713,25648,25681,31955,31988,34100,36246,32020,25714,23600,21423,25681,36213,40374,38261,38163,38129,44369,48464,50543,52591,
54704,54769,50543,50510,52590,52590,52557,42057,46219,54703,54703,52591,50510,44139,39881,41962,44107,50478,54671,56783,56816,56816,52655,54736,
58962,58995,58995,56913,54833,54801,56947,59028,56883,50608,48431,48399,44173,37834,33641,31496,27271,29351,37867,50512,48398,50543,56882,58994,
56946,56913,56881,54833,54800,56947,59029,58996,52689,44204,42058,41961,39849,46253,54706,59029,56884,44173,37769,33577,31464,25191,20997,20997,
25157,35688,50445,56751,56750,54670,56719,48332,37767,35687,46284,56946,59125,59125,56948,48366,35656,31462,33575,39913,50445,50446,39914,31430,
29350,31462,29382,31527,39913,50478,58930,50511,37768,31430,31462,31430,31365,33478,44042,56687,52494,44042,37735,37735,37735,39912,52558,58962,
56849,50478,37833,29382,29350,31495,37867,33771,29577,42190,54803,44173,29253,24995,31364,48266,48266,39716,37506,37539,41732,37571,33410,35523,
35491,33378,33378,33378,35426,35458,37506,35458,35458,35425,35426,35458,35458,35426,33346,31266,2407,2407,2407,327,327,2375,2407,2407,
2407,2408,2440,2407,2407,2407,2407,2407,2440,2440,2440,2440,2440,2440,2505,4586,2506,4619,4651,2538,2538,2505,2473,2473,
6666,6731,6699,4553,4586,6731,2570,2537,2537,2538,2538,2570,2570,2538,2570,2570,2538,489,2570,2602,2602,4683,4683,4715,
4715,4682,6763,6762,6762,6795,6794,6794,8875,10955,13036,15116,17164,19244,23404,40176,52755,46317,44073,46120,48167,48168,50312,52392,
52425,54571,56717,56782,54767,56912,56944,59057,59089,58992,52654,37931,29611,27630,23503,21423,21390,23471,21390,21358,23471,23504,23471,23439,
23471,23470,23437,25452,27466,29545,31593,31593,31593,29546,27499,25484,25518,27697,31955,25648,21455,21455,21423,19342,19342,19439,23665,27859,
29939,27858,27826,27793,27793,27793,27760,27728,27761,27794,25681,23600,23633,25681,27827,29875,32020,38327,36246,27794,23600,21455,23568,34068,
38293,36180,34002,33936,40176,46416,50544,52591,52656,52656,50510,50510,52623,50510,50510,42025,42025,50510,52591,54672,52559,44172,39882,39849,
39881,44107,50445,54671,56816,56849,54736,54736,56882,56882,56881,54735,54768,56914,58996,58963,52689,48463,50479,46222,39915,35754,31561,25223,
23077,27271,35722,46253,48398,52655,56849,56881,56913,58994,54833,52687,56881,59059,59028,56915,48463,39946,37800,35655,39816,48334,54771,59029,
54770,42060,35689,31464,29384,23111,20965,23045,23076,37769,54639,56751,52524,56718,56686,44041,33509,37800,50477,56946,59125,59060,50576,39979,
33576,31430,37768,46252,54671,48301,35623,29350,31463,31495,29382,33640,42059,54704,56817,42027,31462,31430,31398,31366,31365,35591,48300,54606,
46154,39815,37735,39880,41993,48364,56849,58995,52624,44140,35688,27334,29382,33674,35819,33771,33803,46448,56884,44141,29253,27076,31365,48267,
52524,39814,35491,35426,37539,37572,35491,39749,39782,35491,33378,33378,35426,35458,37506,37506,35458,35458,35458,35458,35394,33346,33314,29154,
2407,359,359,359,327,327,2407,2407,2408,2408,2407,2375,2407,2440,2407,2375,2407,2440,2440,2440,2440,2473,2473,2473,
2473,4586,2505,2505,2506,2505,2473,2473,4618,6764,6699,4586,2537,4618,2570,2538,2538,457,2538,2538,489,2538,2602,2570,
2570,2570,2602,2570,2602,4683,4683,4682,4682,4682,6763,6730,4682,6795,8875,8875,10955,10955,13036,15116,17229,19243,23403,40208,
50642,46252,44073,46120,48168,48200,50312,52392,50344,54538,56717,56815,56848,56944,56944,57009,59057,58992,50476,33705,29644,27598,23471,23471,
21390,23470,21390,21358,23471,23471,23471,23471,23503,23503,25517,27499,29547,33706,33673,33640,31592,31593,29546,27499,25484,27598,31889,27761,
23503,21423,21423,19342,19374,19407,23600,27858,29939,29939,27826,25681,27761,29841,29808,27728,27696,25648,23568,23601,23633,25714,27794,27794,
31987,36214,34134,27762,23568,21455,25648,29874,34100,36180,31922,31856,35982,42223,48463,50511,50543,52623,50543,48430,52590,52623,50478,39946,
37832,46285,52591,52624,48399,44141,37802,37769,37769,39914,46252,48365,52558,56849,56849,58963,56915,54769,54768,52655,56849,58995,56850,54738,
50512,46318,46253,39915,35689,33641,27368,23110,23110,27271,31496,42027,50511,54736,54736,56849,58962,56881,52623,52655,54801,59027,59060,54802,
42124,35720,33575,33478,37736,48366,56916,59028,50512,37834,31496,29351,25191,20998,18885,20997,23077,37801,50477,50412,48331,56686,52525,39815,
33542,42026,52623,58995,59060,54802,40012,31528,29415,31495,37801,48332,52591,42027,29317,29350,31430,29382,31495,37801,46285,56849,50510,37769,
31430,29318,31398,29317,31365,39816,50413,48267,39847,39815,39847,44073,50445,56816,58995,56882,48398,37833,31495,27334,31561,37900,33739,33803,
38029,52755,58997,44076,29221,29188,33445,46219,54703,44008,35524,33379,33379,35459,33443,43975,48201,37637,33410,33346,33378,35458,35491,37538,
35459,35458,35459,35459,33346,33315,31235,29155,359,359,359,359,327,327,359,2439,2440,2407,2375,2375,2407,2408,2408,2375,
2375,2407,4520,2472,2440,2440,2441,2473,4554,2473,2473,4586,2538,457,2537,2570,4683,6763,2570,2537,2537,2537,457,2538,
2538,2538,2570,2538,2538,2570,2570,2538,2570,2602,2602,2570,2602,4650,4682,4682,4682,4682,6763,6763,4682,6795,8875,8875,
10955,13035,13068,15181,17228,19243,27628,46514,52721,44139,44073,48201,48200,50280,52392,50344,50344,52490,56717,56815,56848,58993,56977,59058,
58993,56847,46283,33672,29611,25550,23438,23471,23439,23471,23470,21358,23439,23471,23439,21390,23471,23470,25517,27531,31659,35786,35753,35688,
33640,31560,29513,29547,27532,27565,27630,29744,25583,21390,21390,19342,19374,19407,21520,27826,29971,29972,29907,27761,27761,27760,29808,27696,
25583,25583,23535,23600,23633,25713,25746,27794,29907,34101,34101,27794,23568,21422,23567,27793,31987,34100,31922,29743,31725,35917,46351,48431,
48430,50543,50543,48397,48398,52624,52592,39914,33607,42059,48366,48399,46286,42028,37770,35657,35690,37801,42027,46219,46284,56817,58931,58996,
56915,54736,54735,56848,58962,56882,52625,50512,44205,42059,37834,35689,31528,27335,23142,20997,23143,27304,29415,39947,52591,56816,56849,56849,
56849,50542,48397,50575,56881,58994,58995,50575,37833,33575,33543,33478,37737,50479,58996,56915,46286,33608,29415,27271,23111,18853,16772,20997,
23109,33607,44106,44106,48266,56718,50380,35622,35655,46220,54736,59027,56883,46285,31527,27269,25189,29350,37768,48333,52558,39849,27237,29350,
27269,29350,31495,39946,52655,56816,46251,33575,29318,29318,29286,29317,31398,44042,50380,41928,39814,41960,44106,52525,56849,58994,59027,54769,
42027,33543,29415,29481,37932,40045,33771,33803,40142,54900,56916,41963,31269,31269,35558,48332,58928,46218,35524,33379,33379,33411,33410,41862,
48234,41863,35492,33346,33378,35459,37539,37539,35491,35458,35459,35427,33347,33315,31235,29123,359,2439,2439,359,327,294,327,2407,
2407,2375,2407,2407,2407,2408,2408,2407,2408,2408,2440,2440,2439,2440,2440,2473,2505,2473,2505,4586,2538,457,457,2570,
4683,4683,2537,457,489,2537,457,457,457,490,2570,2570,2570,2570,2570,2538,2570,4650,4683,2602,2602,4682,4683,4682,
4650,2570,6763,6795,6763,8875,8875,8875,10987,13036,15116,15148,15115,23436,36015,52755,50608,42026,46121,48233,48168,50280,52393,50311,
52425,52490,54636,56847,56881,58993,56977,59026,58993,56847,44203,31592,29612,27566,23471,23471,23438,23471,23438,21358,23439,23471,23438,23438,
23438,25518,27564,29611,35819,37898,37800,35720,35688,31560,29513,29514,29547,29612,27565,27630,25551,23438,21358,21390,19342,19375,21487,27794,
29939,27859,27826,27794,27761,27728,27728,25583,25550,25583,23535,23536,23600,25713,27794,29907,32020,34133,34133,25682,21455,19375,21487,25649,
29842,34003,31922,31791,31757,33837,44303,48431,46349,50543,52623,50510,46317,48398,54705,39946,29414,37866,46319,48399,44141,39915,35722,33577,
33609,35721,35721,39914,46252,50511,56882,58996,54769,52655,56848,58929,56849,52656,50447,46286,39915,35721,33608,33576,29383,27335,23142,20997,
23175,25255,29416,35754,46317,56816,56816,54703,52590,46284,46251,50510,56849,56881,56881,48365,33607,31462,29350,31398,37769,50511,58996,52721,
40012,31495,29383,27271,23111,16772,14691,20965,23109,31495,39849,41961,48266,54605,46153,35622,37800,48430,56914,59027,50511,33640,25221,25189,
27237,29349,39881,50478,48365,33543,27237,27237,27205,29349,33607,44204,56849,52623,42058,31495,29318,29286,27205,29285,33478,46188,48300,41927,
41960,44073,48331,56815,58994,58994,58962,48398,35721,31495,27335,31659,42223,40143,31723,31755,42255,56982,58997,44109,33414,33414,41898,54704,
59027,50510,35622,31331,33379,33379,33410,37636,46122,44009,35557,33379,33379,35459,37539,37571,35491,33378,33379,33379,33348,31236,29123,27043,
327,359,359,359,327,327,327,359,359,2407,2440,2407,2407,2407,2407,2407,4520,4488,2407,2407,2407,2472,2440,2440,
2473,2472,2473,2473,425,457,424,457,2538,4683,2570,457,489,2570,2537,2537,489,489,2570,2570,2570,2602,2602,2602,
4651,4683,4683,2602,2570,2602,4682,4650,4650,2602,6763,8843,8811,8875,8875,8875,10955,13035,15116,17197,19244,29677,42256,54836,
50479,44074,46153,48233,48199,50280,52393,52360,52425,52490,52556,54767,56913,59026,56945,58993,58993,56880,44236,31625,29612,27599,25519,23471,
23471,23438,23438,21390,23471,23471,23438,23406,25485,27565,29645,33739,37899,39946,37833,37768,35688,33641,31561,29481,29514,29579,29580,27533,
27534,25454,23406,21390,21390,21423,21487,25745,29971,27826,25746,25745,27761,27728,27728,27663,23502,25583,25583,23535,23568,25681,27761,29874,
34100,36278,34166,23601,17262,17295,19407,23568,29809,34002,33970,31791,31757,33804,42190,48431,46317,50510,52623,50510,46317,44172,50479,42028,
27302,33641,44206,48433,42126,37803,33642,33577,31496,33577,33609,35721,39947,46285,54769,56850,52623,54735,58929,56849,54704,52624,52560,44108,
35690,33545,31464,31432,29383,29384,23142,21029,23175,25255,27368,31561,39980,50479,50510,50542,48364,42058,44106,50445,54703,56784,56816,44107,
29382,27270,27237,29349,37768,52591,58996,46351,31528,27335,27303,25223,23111,16772,16739,20932,23077,31463,35656,41896,50347,54572,44072,37766,
42026,54769,59060,56882,44140,27301,23076,25157,27269,33543,42026,50445,44139,31462,27205,25156,27237,29349,37800,50542,56881,48429,37800,29350,
27205,27205,27205,29317,37736,48301,46154,41927,44073,46250,52622,58928,58994,58993,52655,39978,31528,29415,27401,35950,42289,40208,31788,31788,
42288,57047,59030,46222,33446,35494,46124,58899,59061,52689,37800,31363,31299,33379,33378,37636,46187,46156,35591,31332,33379,37507,37539,37538,
35459,33411,33379,31267,29187,29123,29124,29189,327,327,327,327,327,327,294,327,2407,2440,2407,2407,2407,2407,2375,2440,
4488,2407,2407,2407,2440,2472,2440,2472,2472,2440,2473,2472,424,457,457,424,2505,4650,2537,457,2537,2537,2537,489,
489,2537,2538,2570,4650,4683,4683,4683,4683,4651,2602,2602,2570,2570,2602,4682,4682,4682,6795,8843,6762,8843,10988,10987,
10987,13035,15116,17196,21389,36016,48530,52690,46252,41961,46121,48168,48167,50280,52425,52392,52425,52490,50508,52686,56945,59026,56945,58993,
58960,56847,44203,29544,29612,27631,25519,25519,25519,23470,23438,23439,23471,23471,23438,23438,25485,27565,29612,33739,37899,39946,39913,39913,
35721,33641,33641,31528,31561,29546,29547,27467,25452,23405,23406,21358,21422,21423,21487,25681,29907,27826,25713,25681,25713,27728,27728,27696,
25583,25583,25616,23536,23568,25648,25681,27793,31987,36278,34165,21520,17294,17262,19342,23568,29809,33970,36050,33903,33805,33772,37964,44205,
44204,48397,50510,48462,48430,42091,44140,42060,27270,29383,39948,44207,42062,35723,33610,31529,31496,29416,31496,33609,35721,42027,44140,46285,
50510,56848,56881,54704,52591,52656,50480,39915,33609,31497,31464,29319,29351,27303,21029,21030,25255,25256,27336,29417,35722,42028,42091,44204,
44139,39945,44139,48397,50510,54735,56784,41994,29349,27237,27237,29349,37833,54704,54738,37899,29415,27335,25255,25191,23111,16804,16740,18884,
23109,29415,35623,41928,50380,52492,44072,37799,44171,56882,58995,54704,39914,25157,23044,23077,27205,33543,41962,46220,42026,31495,27237,27205,
27270,29382,42026,54736,52655,42091,29414,25156,25092,25125,27205,31398,41962,46188,41896,41992,44106,48397,56817,58962,58994,56816,42091,31527,
27367,25255,33772,42289,42321,40208,33868,31788,42353,57080,59095,48367,35527,37575,46092,56754,58996,56850,41994,31364,31298,33347,33378,35556,
46155,50350,37704,31300,33379,37539,37571,37571,35394,33346,31299,29219,27107,29221,33479,33544,294,327,2407,327,294,295,294,327,
2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2440,2472,2440,392,392,2472,2472,2473,2505,2505,425,
2537,4650,2537,457,2537,2537,457,457,489,2570,2570,2570,4683,4683,2635,4683,4651,2570,2570,2602,2570,489,2602,4682,
4682,4682,6795,8844,8811,8843,10956,10987,13068,15116,17196,17163,27662,44467,50675,48463,42026,41993,46121,48168,48135,50280,52425,52425,
52425,52490,52556,54767,58994,59026,56913,58992,56879,52653,37929,27432,29612,29679,25519,25551,25551,23470,23438,23471,23471,23471,23438,23437,
25485,27565,29579,33739,37899,37833,39913,39946,37833,37801,35689,33575,31528,31593,31594,29547,27500,25453,23405,21358,21390,21422,21455,23568,
27826,27826,25713,23632,25648,25680,27760,25647,25583,25615,23568,23535,23568,23600,25680,25713,29874,34165,32052,21488,17294,17262,19310,23503,
25616,31857,33970,33903,33805,33771,35786,39947,44204,48365,48398,50543,48463,44172,39914,39915,29382,25157,33675,42062,39982,35724,31530,31497,
29417,29384,29384,31464,33609,37802,39914,44139,50510,56816,56816,52623,50478,48366,42060,35722,33610,31497,29384,29319,27271,23078,18884,21030,
23208,23176,25256,25256,31529,35722,35721,37833,39946,37833,42027,46252,48397,56816,56784,39913,27269,25157,25157,29349,39914,54769,48463,33674,
29480,27335,23142,23143,21031,16805,16772,16803,20997,27334,35655,44041,52460,50379,44041,41961,48365,54736,54703,50446,33576,23044,20964,23044,
25125,33543,41994,44139,39913,31462,27205,25157,27237,29382,42027,52623,48397,37833,27268,23043,23044,25092,27205,35624,46156,44042,41960,46186,
48299,52656,58963,58995,58962,48365,33608,29415,25223,25289,40145,42386,40240,38127,33869,31788,44466,57079,59095,50513,35591,35494,41865,52430,
58866,58866,46155,33412,31299,31299,33379,35493,46123,50382,39785,31332,33379,37572,39652,37571,35394,33282,31267,29219,31366,35690,37868,33674,
294,294,2375,2407,327,294,327,327,2375,2407,2407,2440,2407,2407,2407,2407,2407,2407,2407,2407,2407,2440,2440,2440,
2440,2473,2472,2440,392,425,425,425,2505,2570,2537,2505,489,457,457,457,489,2570,2570,490,2602,2603,2603,4683,
4683,2602,2570,2570,489,2569,2602,4682,4682,4682,8843,10924,10924,8875,10955,10955,13035,15116,17163,19243,36080,52820,50609,44139,
41961,44073,46153,48200,48135,50280,52425,52425,52425,52491,52589,54800,59026,59026,56945,58992,56879,50540,35816,27464,27564,27631,25551,25551,
25551,23438,23438,23471,23471,23470,23438,23405,25517,29645,31660,35819,37866,37833,39913,39945,39946,39946,37801,35655,33607,33639,31593,31594,
29547,25452,23373,23405,21390,21422,21422,23568,27794,27794,23633,23600,23600,25680,27760,25647,23502,23535,23535,21455,21487,23568,25648,25713,
27826,34165,32020,21455,17262,17262,19310,19310,23471,29777,31857,31790,33805,33739,35819,37866,46253,46285,44204,48430,48430,46253,39947,37802,
29383,23044,29417,37869,39983,35725,31531,29450,27369,27336,27336,29384,31529,37802,41995,46253,50510,54703,54736,54704,48398,39947,35722,33642,
33643,29450,29384,29352,25191,20965,16804,20998,23144,23144,23176,23176,27369,33610,31561,33608,35753,33608,33608,39946,50510,58929,54704,39881,
25157,25157,25125,29349,41994,54770,46318,31561,29448,25287,23142,23143,21063,16805,14724,14723,18916,27270,33574,41961,50380,48235,41896,41993,
52558,52590,50477,44107,27269,20964,20932,20932,23044,33543,41995,39914,33575,27269,25157,25157,27238,31463,44172,50543,42091,31494,25156,23044,
23012,23011,29317,39850,44042,39848,44073,50412,52590,58963,58996,58963,52591,37833,31495,29448,23175,29613,44468,42386,38160,36047,31820,33901,
46579,57047,59095,54738,39785,35494,39687,46026,54510,56656,48171,35493,31299,31299,33379,35493,46124,50350,39785,31364,33412,39685,39717,37572,
35395,33347,31364,35656,44141,46287,40013,33772,294,294,327,2407,2375,294,327,2375,2375,2375,2407,2440,2407,2407,2407,2407,
2407,2407,2407,2407,2407,2407,2407,2408,2440,2440,2472,392,392,392,392,392,457,2537,2505,457,457,457,457,457,
489,489,489,489,490,2570,2603,4651,4651,2602,2602,2602,521,2602,4715,4682,4682,6762,8811,10924,10956,10924,10955,10987,
13067,15115,17163,25516,44499,54900,48463,41993,41960,44073,48201,48200,48135,50312,52425,52425,52425,52491,54702,56880,59026,59026,58992,58992,
56846,48428,35816,27432,27532,27598,25551,25583,25550,23438,23438,23471,23471,23470,23437,25485,27597,29677,31724,35819,37899,39946,42026,42026,
42026,39945,39912,39847,37734,35686,33639,31593,29546,27499,25420,23405,23438,21390,21390,25616,27794,27794,23600,21520,23632,25681,25680,25647,
23534,23535,23535,21487,21487,23568,23600,25713,29906,34100,27826,19374,17229,17229,19277,19277,23470,29777,31889,31758,31692,33739,37900,42060,
44173,48366,44172,44205,46350,46253,41995,37802,29350,23045,27304,35724,37838,31596,29483,27402,25289,25224,27304,27336,31497,35690,39915,41995,
44172,50510,54704,54737,46286,35754,33674,31530,29482,27369,27336,27304,23078,18853,16772,18918,21064,21064,21096,23177,25289,29450,29449,29448,
33641,29415,29415,35721,48365,58897,52591,35688,25157,23077,23077,27269,42059,54770,44206,29448,25288,23175,23143,23143,18950,14756,14724,14723,
18917,25222,31494,39848,50348,48235,39783,44074,54671,52591,50445,37768,23044,20964,20964,18851,23012,31463,37801,35656,29349,25157,25125,25157,
29351,33608,48399,52624,37833,27269,23076,23076,23012,25092,31430,41930,41929,39848,46186,52557,56849,58996,58995,54737,44139,31494,29415,25255,
23208,36017,46581,40274,38160,36014,31820,33934,46580,57047,59062,56851,41930,35494,37606,41767,48107,54413,48171,37573,31299,31299,33347,35493,
46092,50383,41931,33445,35525,39782,39782,35588,35525,35558,37801,50512,52755,46385,37998,33837,326,2375,327,2375,2375,294,2375,2407,
2375,2375,2375,2407,2439,2407,2407,2439,2407,2407,2407,2375,2375,2375,2407,2407,359,359,360,360,360,392,360,392,
425,2505,2505,457,457,457,457,457,489,489,489,489,490,490,490,2570,4683,4683,2602,2635,2602,2602,4715,4714,
4682,6763,8843,10956,13004,10956,10955,10987,15115,17163,21323,36015,50740,54867,48430,39880,44040,46153,48201,48168,48167,50312,52425,52392,
50345,50443,54766,56945,59058,56977,56912,58959,56814,48396,37865,27465,27532,25519,25519,25582,25518,23438,23470,23471,23503,23470,23405,25485,
27565,29645,33772,37900,40012,42059,42026,42026,42025,41993,41992,41927,39845,39813,37766,33607,31593,29515,25419,23404,23437,23438,21390,23536,
27794,27794,23600,21520,23600,25713,25680,23535,23502,23503,23535,23535,23535,23568,23568,25680,29874,31987,27762,19342,17196,17229,19245,19277,
23470,27696,31857,31791,29580,29547,35820,42061,42060,46286,48398,42124,44205,46253,39947,37770,31431,25125,25223,31563,33677,31564,27403,27370,
25257,23176,25256,27337,29385,33610,35690,33609,42027,50511,56818,54705,39980,33641,33642,29449,25289,25256,27304,27304,23111,18885,16773,18951,
21064,21064,21064,21096,23177,25289,27336,27335,29416,27302,29383,33608,46253,58930,48365,31463,23077,23045,23044,27269,42092,50545,35754,25255,
25224,23143,21095,21063,16869,14724,14724,14723,18884,25222,29414,39913,52494,52461,39848,48300,56817,52559,44106,31462,23044,20932,18884,18851,
20932,31463,35689,33543,29350,25125,25125,25158,29351,35754,50512,46350,33608,25157,23044,23077,23044,25124,33543,41962,41994,44106,48332,56751,
58897,56881,56882,50510,35688,29414,27335,23174,25418,40243,44468,38193,36047,33966,31853,33934,48628,59127,59062,58932,44075,33413,35461,37574,
41767,45994,45993,39654,33380,31299,33347,35461,43979,52496,44076,35591,35622,39815,39879,41928,41994,44172,50609,54900,48530,40111,37998,35950,
2374,2374,2374,2374,2375,2374,2407,2407,2375,2375,2374,2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2407,2439,2408,
359,2440,2440,360,2473,392,360,392,424,425,457,457,457,457,457,457,489,489,490,490,490,522,490,522,
4683,2635,2634,4715,4715,4715,4715,6795,6763,6763,8843,10956,10956,10923,10923,10987,15115,19211,23404,42289,54900,54770,46219,41928,
44072,46121,48201,48200,48167,50312,52425,50344,50344,52523,54767,56945,59058,56977,56879,58927,56846,50509,37930,27466,27533,25519,23470,25550,
25550,25518,23503,23503,23503,23470,23437,25485,27565,29645,33772,37932,42060,42059,42026,42025,39944,44073,44039,43974,41893,41925,41926,35687,
31560,29514,25387,25420,23405,23406,21390,23503,27762,27794,23633,21520,21487,23600,25648,23535,23470,23502,23503,23503,21455,23536,23568,25648,
27794,29842,25648,19309,17164,17164,17164,19244,21390,25584,27696,29679,29580,29547,31627,37836,37835,42061,48399,46285,42060,44141,42028,37802,
29383,23045,23078,29450,31596,31597,29484,25322,25257,23176,25224,27337,27337,29449,31497,33610,39980,44205,50544,48432,37835,33642,31594,27369,
23175,23176,25289,27337,25224,18918,14725,18918,21031,18983,18983,21096,23177,23176,23176,25256,27336,25223,27303,31528,46285,56817,44172,29382,
23045,20997,20964,25157,39947,46286,29449,25256,23176,23143,23143,21063,16837,12644,14724,14692,18917,23109,27269,37801,50445,48299,39847,50445,
56785,46220,33542,27237,23045,20965,16804,14691,18851,27302,33641,31495,27269,23077,23077,25190,29351,37867,48398,42059,31495,25157,23045,23044,
23012,25124,33575,39881,41961,48300,54638,56816,56783,56816,52655,39978,29446,27335,23174,21127,31757,42355,42387,36112,33999,36014,33933,35982,
48628,59127,59062,58932,46189,33414,33381,35494,39654,41768,43880,39654,35461,33380,33380,35493,44011,52528,46189,35687,39848,46251,50445,50510,
50544,54835,57013,48626,42255,40143,40078,37966,2374,2375,2342,2342,2375,2375,2375,2375,2342,2342,2375,2407,2407,2407,2407,2407,
2407,2407,2407,2375,2407,2407,2440,2408,359,2440,2440,2440,2441,360,360,424,2473,2505,2505,457,457,489,457,457,
489,489,522,522,522,523,490,522,2602,2602,2635,2667,4747,4715,4715,6795,6763,6763,10891,10924,10924,10891,10923,13035,
15115,21324,27597,44370,57013,52657,44073,44008,46121,46121,48201,48200,48199,50312,52425,52392,50345,52556,54767,56945,59091,59025,56879,58927,
58927,52590,37898,27466,27566,25519,23438,25518,25550,25551,25551,23503,23503,23470,25485,25485,27564,29644,33772,39980,42092,42091,42058,42025,
39944,44137,44039,43973,43972,43972,43973,37766,33607,31561,27434,25388,23405,23406,21390,21455,25682,27795,25681,23568,21487,23535,23535,25583,
23470,23502,23503,23503,23503,23536,23536,25616,27794,27761,23535,19277,17132,17164,19244,17164,21325,25583,27632,27598,27565,27500,29483,31562,
35690,37835,44173,46286,42092,42027,41995,37802,31431,23012,20932,27305,31564,31597,29516,25322,23209,23177,23144,25224,25224,27337,29417,31530,
33610,35722,42125,42093,33642,31529,29449,25224,23143,25224,25289,25289,25256,18918,14725,16838,18951,18983,18983,21064,21096,21096,21127,23175,
25223,25191,23110,27302,44173,52624,42027,29350,23045,20965,20964,23108,33608,37867,31529,25224,23111,23111,23112,21031,16838,14724,14724,14692,
18917,20997,25157,35656,46187,44041,39880,52558,52592,37834,27269,25157,23077,18917,12643,12610,16771,27270,33641,29447,25189,23077,23045,25157,
29415,42060,46318,37898,29414,25190,23045,20932,20963,25156,35688,37800,39880,48332,56816,58897,56783,56783,46284,31559,27367,23207,18982,23306,
38097,42388,40306,36080,36047,36047,35982,36014,46547,57047,59029,58931,46189,33447,33414,33413,35493,37607,39687,39687,37574,35461,35460,37605,
44043,52527,46285,44172,52591,56849,52687,50543,54802,59094,52819,42319,40207,40175,40078,37998,2375,2375,2342,2342,2375,2342,2375,2342,
262,294,2342,2407,2407,2375,2407,2407,2407,2407,4488,4456,2407,2407,4488,2440,2407,359,2440,360,2408,2440,2440,2440,
2473,2505,2538,2537,489,2537,457,457,489,489,490,490,490,490,2571,2571,2602,2634,2634,2635,4715,4715,4715,6795,
6763,6730,10891,10924,10924,10891,10955,13068,17195,21324,29710,44402,56949,50511,41961,44008,46121,48169,48201,48168,48199,52360,52424,52392,
50377,52556,54767,56945,59091,59058,56879,56814,58927,50509,33705,29547,27566,25487,23439,23470,23470,23470,23503,23503,25551,23438,23405,25485,
27564,29644,33772,40012,44172,44171,44171,42057,41992,44104,44038,46053,46052,46019,43973,39781,35655,31561,27434,25387,25453,23438,21390,21423,
25681,25746,23601,23568,23600,23568,23503,23502,25518,23470,23503,23503,23503,25616,25616,25648,27729,27729,23503,19277,17164,17131,19244,19244,
21325,25551,27664,25551,25485,25388,25290,27337,33610,35690,39915,44141,42060,39882,37769,35689,33544,23044,18819,23111,27370,27403,27403,25290,
23176,23144,23112,23144,25225,27305,27337,27337,27336,33642,37868,35787,31529,29449,27336,23143,23143,25224,23208,25289,23176,16870,14725,16805,
18919,21031,18983,21063,21096,21128,21095,21062,21062,23078,20964,27270,42060,50511,39915,27270,20964,20965,20964,23044,29382,35754,29449,23143,
21031,21031,21064,21031,18918,14757,14692,14692,18885,20997,23045,31462,44042,41927,44105,54704,50479,33641,27238,23077,23078,18917,12611,10530,
14723,23109,29415,25222,23109,23077,23076,23109,33609,48431,48431,37865,27334,25157,20964,20931,20963,29350,35688,37800,44106,54638,58929,56816,
56751,50444,37800,29414,25255,19015,19081,29646,40243,44468,40307,36080,36047,36014,35982,38062,46515,57014,56917,56851,48302,35528,33414,31301,
31332,33413,35525,39687,39687,37542,37573,37605,44042,52560,54737,56882,58994,54768,48494,52656,59029,57013,48561,40207,40207,40142,40111,38030,
2374,2374,294,2342,2342,2374,2342,294,294,294,2342,2375,2375,2375,2375,2375,327,2407,2407,2407,2407,2407,2407,2440,
2440,2408,392,2440,2440,2472,2472,2473,2473,2473,2505,2505,489,522,489,489,490,522,522,2635,4683,2571,2570,2635,
2635,4715,2634,2634,2634,4682,6763,8843,8811,8811,10923,10923,10956,10955,10955,13068,17196,21291,29645,42256,54803,48398,41961,46121,
46121,46121,48201,48200,48199,52392,52425,52392,50377,52556,54735,56913,59091,59058,56879,56846,56815,46317,33706,27499,27534,25486,25518,25550,
23438,23438,23470,25518,25518,23438,23405,25485,29612,31692,37900,42060,44172,44139,44171,44138,44105,44072,44038,46052,46019,46052,46053,39781,
35622,31528,27402,25420,25453,23438,21390,21423,25649,25714,23601,23568,23568,23535,23503,23470,25519,23470,23471,23503,25584,25616,25648,27696,
27697,27697,21422,19244,17131,17098,19179,19212,21292,25519,27632,25519,25453,25388,23242,23176,29449,35723,35690,37834,42028,42028,39883,35657,
31463,25158,18820,20965,25257,25290,25290,23241,23177,21096,23144,25225,25225,25225,25225,25225,25256,29481,29481,27368,25255,25255,25223,23111,
23111,23144,23177,23210,23209,18951,14725,16805,18951,21063,18982,18982,21063,21096,21063,21030,21030,20965,20997,27302,39914,46285,37834,27238,
20932,18884,18884,18884,25254,31594,27336,23111,21031,21031,21064,21063,18951,14757,12644,14692,16805,23078,23077,31463,41994,41928,48267,56784,
48334,33577,27271,25191,23110,16836,10562,10530,14723,20965,23110,23077,20996,21028,23076,23108,35754,50544,46318,33640,25189,23077,20932,18883,
23076,31495,33608,39882,50446,58930,58897,56718,52493,41961,31462,27334,21062,19015,25420,31791,40275,44468,40307,36079,36047,36014,35982,38095,
48596,54902,54804,56884,48400,33480,29254,29253,31300,31300,35493,39719,39687,39686,37638,37735,50414,56850,58962,56849,52654,50542,50608,56916,
59062,54836,44368,42255,42255,40175,40143,38030,2374,2374,294,294,294,294,262,262,294,294,2343,2375,2375,4456,2375,294,
327,2407,327,2407,2440,2440,2407,359,2440,2472,392,2472,2472,2472,2472,2440,392,2505,2538,2537,489,522,521,489,
489,522,555,2635,4716,2603,2570,4682,4683,4715,4715,2634,4682,6763,6763,6763,8811,8843,10923,10923,10955,10955,10955,13035,
15083,21291,29644,40077,50544,48333,44074,46154,46121,46121,48201,48168,48199,52392,52425,52425,52426,52557,52687,56914,59091,59090,56911,56845,
52621,42092,33707,27500,27533,25518,25518,25550,25518,23438,25518,27599,25518,23405,23405,25485,29612,33740,39980,42092,44172,44172,44139,44139,
44138,44072,46118,48133,46052,48101,43973,39780,35653,31527,27434,27468,25453,23437,21422,21423,25681,27794,25649,23568,23568,25616,23503,23470,
23471,23471,23438,23471,23503,25616,27696,27697,29745,27697,21390,19212,17131,19146,19179,21227,21260,23406,27600,25519,23373,23340,23242,21096,
27337,33643,33642,33577,37802,42028,39915,35689,29351,27239,20933,18885,23145,23242,23243,23210,23209,23177,23177,23177,23144,23145,25225,25225,
25224,25256,25288,25288,25223,23111,23111,23111,23111,21064,21097,23178,25290,21097,16806,16806,18918,18983,16902,18982,21063,21063,18983,21031,
20998,18917,20997,27270,39882,48334,37802,25158,20900,18852,18852,18884,25223,27369,25224,23111,21031,21031,18983,18983,16902,12677,12612,12611,
14692,20966,23077,31463,39882,41929,50412,56784,44108,29383,25191,23143,18949,14691,10562,10530,14691,18884,20997,21029,20996,21028,23108,23141,
37899,44238,37867,29447,23077,20964,18883,18915,25221,31528,35656,44075,54640,58897,58864,54605,46218,33607,29414,27303,18982,21161,29645,33969,
42388,42420,38225,36079,36047,33934,35982,38095,48628,56950,54837,56918,50514,31432,27173,29221,31300,31300,35493,39719,37671,37639,39848,48333,
56818,56850,54704,52622,50541,50542,52754,59094,57014,48562,42255,44336,44368,42287,42223,38062,2374,2374,326,294,294,262,262,262,
294,295,294,2375,4455,4455,2375,327,2375,2407,2407,2375,2407,4488,2407,359,2440,2440,392,2440,2472,2472,2472,392,
2440,4586,4683,2602,489,489,522,521,489,490,522,522,2603,4683,2602,2602,4682,4682,4715,4682,4682,6795,6763,6730,
8811,8843,10923,10923,10923,10955,10955,13035,17130,23403,33870,44303,48398,44139,44074,44041,46121,46121,46121,48167,48199,50312,52425,52393,
52459,54637,54768,56946,59091,59058,56911,56845,50541,40011,31627,29581,27533,27566,25518,25518,23438,23438,25486,25518,23405,21293,23405,27565,
29612,33772,39980,44172,46220,44139,44139,44139,44138,46185,46151,48133,48101,46021,43972,41893,37766,33608,29482,27500,25453,23437,21422,21423,
25649,27794,27762,25616,23536,25616,25583,23470,23471,23471,23438,23470,23503,25616,27697,29777,29777,27697,21390,19211,17099,19146,19146,21227,
21228,23373,25519,25519,23373,23340,23275,21096,25224,31497,31529,31464,31496,33609,35690,35690,29351,25126,20933,18885,21064,23242,23242,23210,
23209,23209,21129,21096,21064,23177,25225,25225,23144,23175,25223,25255,25223,23111,23111,21031,20999,20999,21064,21097,23178,23145,18919,14757,
16837,18950,16902,16902,18983,18950,18950,18951,18950,20965,20997,25157,37834,50447,37802,23077,18852,18852,16804,18917,27368,25288,23110,20998,
21031,21031,18951,18951,16870,12677,12644,12579,14660,18885,23045,29383,37769,39848,52493,58833,44108,27270,23142,23111,18917,14723,10562,10530,
14691,18917,20997,21029,21029,23109,20996,25222,37900,37867,31528,25222,23077,20932,18851,20996,27334,33608,37768,50413,58832,58799,56719,50412,
39848,31494,29415,23142,19015,25387,29710,36082,40340,40339,38192,36046,36046,33934,35982,38095,48628,57015,59031,59030,48368,29319,27173,31301,
31300,31332,35525,37671,37703,41962,50446,56817,56850,54672,50478,50509,52655,52688,56948,59127,52820,44368,42255,44368,44368,42255,40110,38095,
2374,2374,2374,2374,294,294,2342,2342,2342,2375,2375,2375,2375,2343,2375,2407,2407,2440,4520,2407,2407,2440,2407,2408,
2440,2472,2472,392,2472,392,2440,2440,2472,2537,4618,2570,2538,490,522,489,457,458,490,490,2570,4650,2570,4650,
4715,4715,4715,4715,4715,4714,6730,6730,8811,8843,10891,10955,10955,10955,13035,15083,19243,27564,38030,46384,46253,41993,44041,46121,
46121,46121,46120,48200,48199,50279,50344,52393,52523,54702,56848,59027,59091,59058,56879,56878,50573,35818,29546,29581,27566,25486,27566,25518,
23405,23406,25486,25486,23405,23373,23405,27565,29612,35852,42093,44173,44140,44139,46220,44171,46219,46218,46119,46053,46020,43940,46053,41925,
37767,33640,29481,27500,27501,23405,23470,21422,23568,27794,27794,25616,23503,23503,25551,23470,25519,23471,23406,21390,23503,25616,27696,29810,
31890,27696,21357,19212,17098,19114,19146,21227,21260,21293,25486,25519,25486,25420,23242,19015,21031,25256,29417,31464,29383,31497,31497,33578,
29384,25126,16772,16804,21031,21129,21161,21162,23210,21161,21129,19016,18983,21064,23145,23144,21063,21062,23110,23110,23110,23111,23111,20998,
18918,18951,21064,21097,21097,21097,18984,14757,14757,16902,16870,14821,16870,18918,18950,18950,18918,20998,20997,25157,37802,48367,33609,20965,
18820,16772,16772,21030,29482,23175,21062,20998,20999,18951,18918,16870,16870,14757,12644,10531,12612,18885,20997,27303,33543,39848,50413,56752,
42027,27335,25223,23111,18917,12643,10562,10562,14723,18885,20997,20997,21029,21029,20996,25255,31594,29448,27303,23109,20997,20932,18884,23077,
29383,33575,41962,54638,58864,56718,54638,48299,35687,29415,25255,18981,21128,25452,29743,36114,38227,38227,36080,33966,33966,36014,36015,38095,
48628,59095,59095,58998,44142,29286,29253,31365,33445,33445,35590,41929,48268,54640,56784,54704,52559,50478,50510,50575,52720,54834,59094,57014,
48562,44335,44367,44400,44335,40174,38062,40175,2342,2374,2374,2375,2374,2407,2375,2342,2375,2407,2407,2375,2375,2375,2407,2407,
2407,4520,4520,2407,2375,2440,2440,2472,2472,2473,2473,392,392,392,392,2440,2472,4586,2537,2505,2538,2570,2570,2602,
2570,490,490,2538,2570,4651,4650,4683,6763,4715,6795,6795,4714,4682,6762,8843,8843,8843,10891,13035,13003,10954,13003,15083,
21323,29644,40111,46351,42027,39881,44041,46121,44073,44040,48201,50280,50247,50312,52425,52425,54604,56782,56913,59059,59123,59058,54830,56878,
50574,33737,29514,29613,27533,25453,27566,27599,25454,25454,25486,25486,25454,25454,25485,27533,31660,35852,42060,44172,46220,46252,46220,44139,
46219,48266,46119,46021,43940,43940,46053,41926,37766,33640,29482,29548,27500,23372,23470,23470,25616,27762,27762,25649,23535,23470,23470,23438,
25518,23438,23438,21358,23470,25583,25616,29809,31890,27697,21357,19211,19146,19114,21194,21227,21227,21260,23406,25518,25486,25421,23242,19016,
21031,23111,27304,31497,29384,29417,31498,31465,29352,25191,18853,16772,18950,19016,21129,21162,21194,21161,21129,19016,18983,18983,21064,21064,
21063,21030,21030,21030,20998,21030,21030,18918,18886,18951,21064,21097,21065,21065,19016,14790,14725,16870,16869,14789,16870,18950,18918,18918,
18918,18917,20965,25223,35755,42029,29416,20965,18852,16772,14724,21031,25289,23143,21031,18950,20999,18951,16870,16870,16870,14757,12644,10499,
12611,16805,20966,25190,29350,39816,50413,52559,39882,27302,23142,21030,18885,12611,10530,10530,14691,18852,20965,20965,20997,20996,20997,25223,
27336,23110,23110,23077,20965,18884,20964,25158,29383,31495,44106,54638,58830,56718,52557,41993,31494,27335,21029,18982,23274,25484,31824,38195,
38227,38226,34000,33966,33966,33966,35982,38096,48628,59095,59095,56917,39948,29285,29317,31365,33510,39849,48268,52559,56785,56817,52623,50510,
48397,50478,52656,52688,52721,56980,59094,52788,46416,44335,46448,46448,42255,40174,40175,44369,2342,2374,2375,2407,2374,2407,2407,2375,
2375,2407,2407,2375,2375,2375,2375,2407,2407,2440,2440,2407,2407,2407,2472,2440,2440,2472,2440,392,392,2472,2440,2440,
2473,2505,2505,2505,2537,489,2570,4715,4683,2570,2538,2570,4651,4683,4683,6731,4715,4682,6795,6795,4682,4682,6762,8843,
10891,8843,8842,13035,13035,13002,13002,17130,21290,33805,46384,46286,39914,41961,44009,44041,44073,46121,48233,48200,52360,52425,54505,52458,
54604,54735,56913,59091,59124,59058,54831,56879,48494,31657,29547,29613,25453,25420,27566,29647,25486,25453,25453,27534,27566,27566,25485,27532,
31692,37900,42060,44140,46220,48333,46219,44139,46251,48331,48233,46086,43940,43973,46086,41926,37767,33640,31562,29548,27468,25420,23437,23470,
25648,27761,27762,27761,25583,23470,23438,23405,23406,23438,23438,23438,23438,23503,25583,27664,29809,29777,23438,19179,19114,19114,21162,21194,
21195,21260,23373,25486,25486,25421,21194,19016,18983,21031,23143,27304,27304,29385,31498,29417,29353,25192,18885,14692,16870,19015,19048,21129,
21162,21161,19048,18983,19016,18983,19015,21063,21063,18950,20998,21031,20998,18917,18918,18918,18918,18951,19016,19016,19016,19016,19017,16903,
14758,14789,16837,16837,16902,18950,16870,18918,18918,18917,20965,25191,31529,31529,25191,18885,16773,14692,14725,18950,23143,21030,18950,18918,
18950,18950,16870,16870,16871,14758,12612,10499,12579,14725,20966,25158,27269,37736,50381,50446,33640,23109,20997,18917,16837,12611,8449,10530,
12643,16804,18917,20965,18917,18917,18917,23110,23142,21029,20997,20997,20997,18916,20965,25190,29351,33543,46219,56718,56717,54670,50444,35655,
27334,23175,16901,19047,23339,25485,31856,36147,38259,38226,34032,36047,33966,33934,35982,38095,48628,59095,59063,54837,37867,29317,31462,35655,
44107,54639,56817,54704,52559,50511,52590,52590,50510,50543,52721,52689,52722,59094,57013,48561,44303,44367,46480,44400,42255,40142,40175,46514,
2375,2375,2407,2407,326,2407,2407,2375,2375,2407,2375,2342,2343,2375,2375,2407,2375,327,2375,2407,2407,2407,2440,2472,
2472,2440,392,360,359,2440,2440,2473,2505,2473,425,2505,2537,2537,2537,2570,4650,4651,2570,4619,4651,4650,4683,4683,
4683,4650,4682,4715,6795,6795,6762,8843,10891,8842,8842,13003,15083,13035,15083,17162,23370,40143,48497,42060,39914,44042,44041,44041,
46121,46185,48233,50280,54473,54505,52425,52458,54604,54735,56881,59092,59124,59058,56912,54799,44268,31625,29579,29613,25453,25485,27566,27566,
25486,25453,25421,27501,27566,27598,27565,29612,33772,39980,42060,44107,46220,48332,46219,44171,48332,50412,50345,50247,48134,46053,46118,44006,
39847,35688,31562,29515,27500,25452,23405,23470,25583,27729,27761,27729,25551,23438,25485,25453,23405,23405,23405,23437,23470,23503,23503,25551,
27664,27696,21357,19179,19114,19113,21162,21194,21194,21227,21292,23406,27566,25453,19114,16936,16870,18918,23111,25192,27272,27305,29386,29385,
29385,25192,18885,12579,14757,16935,19048,19049,19081,21161,19049,16903,19016,18983,18983,21064,21031,18918,18918,20998,18918,18885,18885,18918,
18950,18951,18984,19016,16936,18984,19017,18984,14790,14757,14789,14757,16869,16902,16870,18886,18886,18918,20965,23078,27304,27336,23143,18917,
14724,14693,14725,18918,21063,18950,18918,18918,18950,16870,16838,16838,16871,14758,10532,10499,10531,14725,18918,23077,27270,37769,50413,48333,
33576,23109,16836,16804,16805,12611,8450,10563,14691,16804,18885,18949,18917,18916,18917,20997,21030,20997,20997,20997,20965,18916,20997,25190,
29351,35655,48332,56750,54637,52525,42058,31494,27335,21095,16902,21161,23372,25517,31888,36147,40339,38194,33999,36047,33966,33934,35982,38096,
50676,59063,59030,54836,42092,39881,46188,50478,56753,56785,52591,50445,48397,48397,52558,52590,52655,54736,52688,50609,54868,59094,52819,46448,
44335,46448,46448,44367,42254,40142,40175,48627,2375,2375,2407,2375,327,2407,2407,2375,2407,2407,2375,2342,2375,4455,2375,2375,
326,359,2407,2375,2407,2375,2407,2472,2440,2440,2440,392,2440,2440,2440,2440,2440,2473,2473,2505,2537,2538,2570,2570,
2570,4651,4651,4651,2570,2570,4683,6763,6763,4650,2569,4682,6795,6795,8842,8843,10923,8843,10890,13002,15082,15115,17195,21290,
29644,48530,48431,39914,41962,44042,44041,44041,46153,46185,48265,52393,56554,54506,52425,52426,54604,54767,56913,59092,59124,59058,56944,52687,
42124,31626,29547,27533,27501,27533,27566,27534,25453,25453,25453,27500,27533,27565,29645,31660,35819,39979,42059,44107,46220,48331,46218,46219,
48364,50411,52426,52425,52360,50279,46119,44006,39879,35720,29481,29515,27499,25420,23405,23438,25583,27696,27729,27664,23470,23405,25453,25453,
25421,23373,23373,23405,23470,23503,23503,23503,25584,25584,21325,19179,19082,21129,23210,23242,21194,19147,21260,23373,25486,25453,21194,16936,
14823,16870,21031,23112,25160,25224,27305,27305,27337,25224,18886,12579,14725,16903,19016,19017,19049,21130,21129,16903,19016,18983,18950,19015,
18983,18918,18918,18918,18885,18885,18918,18918,16870,18951,18983,18984,16904,16936,19017,18984,14758,14757,16837,16837,16869,16870,16870,16837,
18853,18885,18885,23046,25191,25224,23111,18918,14693,14693,12644,16838,18982,18918,18918,18918,18951,16870,14758,14790,16870,14758,10564,10499,
10531,14757,18918,20997,27237,37801,50414,44140,29382,20964,16804,16805,14724,10530,8418,10563,12644,14724,18852,18917,18917,18916,18917,18917,
18949,20998,21030,20998,20997,20997,21029,25222,29383,35688,48300,52557,50412,44139,33640,27334,25255,18982,16935,21194,23404,27630,34001,38227,
38259,36113,33967,33967,33966,33935,36015,40208,50741,57015,56950,56884,52625,54705,56818,56785,54704,52591,50445,50445,52558,50477,52558,52622,
54735,52688,50608,52755,57014,54901,48593,44366,44367,46480,44367,44367,42222,40142,42288,52821,2342,2342,2375,294,2375,2375,2375,2407,
2407,2407,2407,2375,2375,4455,2407,2375,327,2440,2407,2375,2407,2375,2407,2440,2440,2472,2440,360,2440,2440,2440,2408,
360,2473,2473,2505,2538,2537,2570,4650,2570,2570,4651,4651,2537,2537,4650,4715,6796,4682,2602,4682,6795,6795,8842,8843,
10923,10923,10922,13002,13002,15115,19275,23371,35983,52723,46253,39882,41962,41961,44041,46153,46153,46185,50314,52426,54506,56586,54506,52426,
54604,54767,56914,59091,59124,59058,56913,52687,40043,31626,29548,29613,25453,25453,27565,25485,25453,25485,25484,25420,25451,27564,29644,31659,
35819,42027,44107,46187,48299,48331,46218,46283,48364,50411,54571,54538,52393,50312,48199,44039,39879,35688,29448,29515,27467,25387,23373,23438,
27631,27729,27696,25583,23438,23405,23372,25420,25421,23372,23373,23405,23470,23470,23471,25583,25616,25583,19244,19147,19081,21129,23210,23242,
21194,21195,21259,23340,23405,23373,21195,16969,14855,16870,18919,21031,23079,25192,25224,27305,27337,27273,20966,14692,14757,16903,19016,19017,
19017,21130,21129,18984,19016,18983,16870,18950,18951,20999,18918,18886,18885,18918,18918,16838,16838,16903,18983,18984,16903,16936,19017,18985,
14790,14757,14757,16805,16869,16902,16837,16805,16805,18885,18885,20998,23111,23111,21031,18918,14693,12613,12645,16838,18983,16870,16838,16870,
18951,16870,14758,14758,16838,14758,12644,8451,8450,12676,18917,20997,25189,39881,48333,39914,25189,18884,16805,16837,14724,10530,8450,10563,
12643,14692,16804,18885,18917,16868,18916,18917,18949,21030,21030,20997,18949,20997,21029,25190,29415,35688,41994,46187,44107,37769,29415,25255,
21062,16902,19048,23307,25452,29743,36114,38259,38226,34000,33967,36047,36047,36047,38128,42321,52822,59095,56949,54836,56851,56818,54705,54704,
52591,52558,50445,50509,52590,52557,52590,54703,52655,50543,52721,57014,57047,50739,46479,44366,46447,46480,44367,42254,40174,40142,44401,54934,
2342,2342,2375,294,327,327,2375,2407,2407,2375,2375,4455,2407,2407,2407,2375,327,2407,2407,2375,2375,2407,2440,2440,
2440,2440,360,360,2440,2440,2472,2473,2473,2505,2473,2505,2570,4650,2602,4650,2602,2570,2602,4651,2538,457,2602,4683,
4683,4682,4650,4682,6763,6795,8842,8842,10923,10923,10890,13002,13002,17163,21356,25451,38063,50577,42060,39882,41994,44041,46121,46186,
46186,46185,50313,52426,54473,54506,54538,52490,54636,56815,56946,57011,59091,59026,56880,50607,40011,31594,29580,27533,25420,25485,27565,25485,
25453,25484,25452,25419,25451,29612,31725,33739,37867,42027,44107,46251,48331,48331,48331,48363,48396,50444,54571,54538,50312,48231,48199,44071,
39847,35688,31529,29514,27467,25355,25420,25518,25583,27728,25616,25551,23438,23405,23340,25388,25421,23373,23405,23405,23438,23470,23470,23535,
25616,23503,19212,19146,19114,21161,21129,23210,21194,21226,21259,21292,23341,23340,21227,17001,16903,16870,16838,20967,23079,23144,25224,25224,
27305,25225,18918,14693,12677,14822,16936,19017,19017,21129,21129,18984,18951,16870,16838,16838,18918,18919,18886,18886,18885,18918,18918,16837,
16838,16903,16935,16936,16871,16904,19017,19017,16871,14758,14724,14724,16837,16837,16837,16805,16805,18853,18885,20966,20999,23111,21031,16838,
14693,12580,12612,14758,18950,16837,16805,16838,16902,16870,14790,14758,14758,14726,12612,10499,10499,12644,16837,18916,27269,39946,48333,37801,
25189,18917,16838,16838,14724,10531,8482,10563,12643,14692,16804,18917,18917,16836,18917,18917,18949,21030,20998,18917,18916,18916,20996,23141,
29415,33608,37801,41962,39914,33576,29415,23175,18950,19015,21194,23372,27565,31856,36146,38226,38193,33999,36047,38128,36047,38128,40208,42289,
52854,59095,56917,56884,56883,54672,52591,54704,54704,52558,50445,52557,54670,54670,54703,56816,52655,50607,54867,59095,54901,48594,44399,44398,
46479,46447,44334,42222,40142,42222,48627,57047,2375,2342,294,294,327,2375,2375,327,2375,2375,2375,2407,2407,2407,2375,327,
294,327,2375,2375,2375,2408,2440,2440,2440,2440,360,360,2440,2440,2472,2440,2440,2473,2473,2473,2538,4650,2570,2537,
2570,2570,2570,4683,2570,489,2569,2635,2634,4682,4682,4682,6762,8843,8843,8842,10891,10923,10890,13002,15083,17195,23436,27564,
40143,46318,41994,41962,44042,44042,46154,46186,48266,48266,50377,52425,52425,54505,54505,52491,54669,56848,56946,59059,59091,59026,54800,48494,
37931,31659,31661,27500,25388,25453,27565,25485,23372,25420,25452,25452,27500,31693,33805,35787,39947,44139,46187,48299,48299,48331,50411,50411,
50444,50443,52491,50410,50377,48231,48167,46120,39879,33640,31561,29514,27435,27403,25420,25485,25582,27695,25615,25583,25518,25453,23372,23372,
25421,25453,23405,21357,23438,23438,23470,25551,25583,25551,21292,19146,21162,21161,21161,23210,23242,21194,21226,23307,21292,21260,23340,19082,
14855,16871,16838,18918,21031,23111,23144,23144,25225,25225,18951,12612,12612,14790,16935,19017,19017,19049,19049,18984,16871,16838,16805,16805,
18886,18918,18886,16838,16870,18918,18950,16870,16838,16870,16903,16904,16872,14824,16937,16969,16904,14758,12644,14724,14757,14757,16805,16805,
16773,16773,18886,18886,18886,21031,18951,16805,14725,12580,12580,14693,16805,16805,16838,16838,16870,16870,14757,14757,14757,14725,12612,8451,
8450,10531,16837,18916,27303,39947,44173,33608,23077,18917,18950,16869,12611,10498,8450,10531,12611,12643,16804,16804,16836,16837,18917,18918,
18950,18950,21030,18917,18916,20965,21029,25190,29415,33608,35656,37737,35656,29383,25255,21062,16934,19048,23307,25485,27630,31856,36146,38226,
36113,33999,36047,38128,36047,38128,38160,40208,50709,57015,54868,56883,54770,52591,50510,52655,54703,52590,50477,52589,56783,54735,54671,52624,
52624,52722,56981,57013,52755,48561,46447,46479,48527,44366,42254,40141,40142,44336,52820,59160,2375,2375,2375,2375,2375,2375,2375,294,
294,327,2375,2375,2407,2407,2407,327,294,2375,2375,2375,2408,2408,4488,4520,2440,2473,4553,2440,2440,2440,2440,392,
2440,392,2472,2537,4618,4650,2537,457,2570,2570,2570,2570,489,489,489,522,4715,4715,4682,4682,4682,8843,8843,8842,
10891,10923,10922,13002,15115,19243,25516,33837,42190,42059,39913,44042,44042,44074,46187,46186,48266,50378,50378,52457,52425,54505,54505,52491,
52621,56880,59026,59059,59059,56946,52719,46381,33705,29546,31661,27500,25388,25452,27533,27532,25420,25452,25485,25453,27532,31660,33772,37835,
42027,48300,48331,48331,48331,48331,50411,50411,52524,52524,50410,48330,50377,48232,46119,46087,39847,35688,31593,31595,29515,29483,27468,25485,
25582,25615,25583,27599,27599,27566,25453,23372,23372,25453,23373,21357,23438,21390,23470,25551,25583,27599,23372,21194,21162,21161,23209,23241,
23209,21161,23274,23307,21259,21259,23340,19114,14823,14823,16838,18919,21031,21031,21064,23144,23177,23177,18952,12644,10564,14757,16935,16968,
16968,19049,19049,19017,16904,16838,16805,16805,18886,18918,16838,16838,16870,16870,16870,16870,14790,16870,16871,16871,16872,14824,16937,19017,
16969,14790,12645,14692,14725,14757,14757,14725,14725,16773,18886,18886,18886,21031,18918,14757,14725,12580,12580,14725,16773,16805,16806,16805,
16838,16838,14789,14757,14757,14757,12644,8451,8418,10531,14756,18917,29384,39948,39948,29415,23045,18949,18982,16869,12611,10530,10498,10530,
12611,12643,14724,14724,16804,16837,16837,18918,18982,18950,18950,18949,18917,20997,23077,25190,29383,33576,33576,33576,31463,27302,23142,18949,
16935,19113,23340,27598,27630,31888,38227,38226,36080,33967,36047,38128,36047,38160,40208,40176,48596,56982,56949,56916,52656,50510,50510,52623,
52590,50509,50509,54670,54703,52622,50511,50511,52657,56916,59062,54868,48561,46448,46415,46479,46447,44334,42221,40141,42223,46449,54934,59192,
2374,2375,2375,2375,2375,2407,2407,327,327,295,327,2375,2407,2407,2375,327,295,2407,2408,2408,2375,2375,2408,2440,
2408,2472,4553,2440,2408,2407,2408,2472,2505,2472,2472,2537,4650,6731,2570,2537,2538,2537,489,489,489,2570,489,489,
4683,4747,4682,4682,6730,6763,8843,8843,8843,10923,13003,12970,17163,19275,25483,40144,46384,39946,41994,44074,41961,46154,46187,46186,
48299,50378,50378,50377,52392,52457,52490,52491,52621,56880,59026,59059,59059,56913,52687,44301,31657,29546,29580,27500,25420,25452,27532,27532,
25452,27533,27533,27533,27532,31660,35788,37867,44107,48300,48331,50411,48331,48331,48331,50411,52524,50443,48330,48330,50345,48200,48167,46087,
41928,35688,31561,29514,29515,29483,27467,27532,27630,27663,25550,25518,27566,27598,25453,23340,25420,23373,21324,21357,23438,21390,23438,25551,
25583,25486,23308,21162,21129,21161,23241,23241,23209,21161,21193,23307,21259,21194,21259,19114,14887,16903,16838,18886,20967,21031,21064,23145,
25225,23145,21032,14725,12612,12709,14822,16903,16936,19049,19049,19049,18984,16838,14757,16773,16805,16838,16805,14757,16805,16838,16838,16838,
14790,14822,16871,16903,14856,14824,14856,16969,19017,14823,14757,12644,12676,14757,14757,14725,14725,16805,18854,18886,18918,18918,18918,16806,
14725,12580,12580,14693,16773,16773,16805,16805,16805,16838,16838,16837,14757,14757,12644,8451,8418,10499,14692,18885,29384,39916,37803,27270,
23077,20997,18950,16837,12611,10498,8450,10530,12643,14724,14724,14756,16804,16837,18917,18918,18950,18950,18918,18949,18917,20997,23109,25157,
27270,29415,31463,31495,29383,25222,21062,16902,19048,21226,25452,27630,29743,31953,38227,36145,34031,33967,36112,38128,36047,38160,40208,40176,
48563,54869,56949,56915,52656,52591,52591,52590,52558,52558,52622,56783,54671,52558,50511,50544,54802,59094,56981,50674,46480,46415,46414,48527,
46414,42221,40141,42190,44335,48594,57046,59160,2374,2374,2375,2375,327,2407,2407,2375,2375,2375,2375,2375,2375,2375,2375,327,
294,2375,2408,2375,295,2407,2440,2440,2440,2440,2440,2472,2440,2408,2440,2472,2473,2505,2505,2505,2537,4650,2538,2537,
2537,457,489,489,457,489,489,489,2634,4747,4715,4682,6795,6763,8843,10923,10891,10923,13003,12970,17195,23436,27564,46449,
48431,41994,44042,44042,41961,44106,46186,46186,48266,50378,50345,50312,50312,52425,52457,50442,52621,56848,58994,59059,59059,56946,50607,40075,
31657,29579,29580,27501,25452,25452,27532,27500,25452,25452,27533,27532,29580,31660,35820,39948,44107,48300,50379,50443,48330,48298,50411,50443,
50443,50411,48331,50411,48265,48200,48232,46055,39847,35688,31528,29481,29482,29482,27467,27532,27598,27695,25583,23438,25486,27566,25453,23372,
25453,23373,21292,21357,21390,21390,21390,25551,25551,23405,23307,19081,19049,21129,23209,23209,23209,21161,23241,21226,21226,21194,21227,21194,
16968,16871,14758,16773,18854,18918,21064,23145,23177,21097,21064,16838,10596,12676,14790,14855,16968,19049,19049,19017,18984,16838,14725,14725,
16773,16805,16805,14757,16805,16838,16838,14790,14790,14790,16871,16871,14823,14823,14856,16969,16969,16903,14790,12677,12644,14725,14725,14725,
14693,16741,16805,18886,18918,18886,18886,16806,14693,12580,12579,14692,14693,16773,16838,16805,16805,16838,16838,16838,14757,14725,12612,8451,
8418,8451,14692,18917,27304,33642,33609,27270,23077,21030,18918,16837,12644,8418,8450,10531,12643,14724,14756,16804,16805,16837,18918,18950,
18950,18950,18918,18917,18949,20997,23109,23109,25190,27302,29383,29383,27302,23175,18982,16934,19113,23339,25452,27662,29807,34066,38259,36145,
33999,33967,36080,38128,36047,38160,38127,40176,48531,54837,56916,56883,52623,52591,54671,52590,54671,54703,54703,54736,54671,52591,50576,50642,
56948,59061,52787,50641,46448,46447,48527,48527,46414,42221,42189,44302,46481,52820,59159,59159,2375,2375,2375,2375,327,2407,2408,2407,
2375,2375,2375,2375,2375,2375,327,327,294,2375,2375,327,327,327,327,2408,2440,360,2440,2472,2472,2440,360,2440,
2473,2505,2505,2505,2505,2505,2537,2537,489,457,489,489,489,489,457,489,2602,4682,4715,6795,6795,6763,8843,10923,
10923,10890,13003,13002,17130,23403,31724,44336,44205,41962,41961,41961,41993,44106,46186,46218,48266,50378,50345,48264,50344,52457,52458,50443,
54702,56848,56946,59059,59059,56946,48527,33802,27497,29579,27532,25452,25452,27532,27500,27500,25419,25452,27532,29613,31660,33708,37868,42027,
44107,48299,50411,50443,48330,48298,50411,50411,48363,50411,48330,48330,48265,48200,50281,46088,41895,35687,31496,29449,29482,29482,29547,27500,
27565,29743,27663,25518,25486,25486,25518,25453,25453,23373,21292,21357,21357,21390,21390,25551,25551,23405,23275,21129,19049,21129,23209,23209,
23209,23241,23241,21193,21194,21194,21226,21194,19048,14823,14757,14725,16805,16838,18951,21097,23177,21097,21064,16871,10564,12644,12709,14823,
16936,19017,19017,16936,16904,16838,14725,14693,14725,16805,16805,14725,16805,16838,14790,14790,14757,14757,14790,14823,14823,14823,14856,16969,
16969,16936,14823,12709,10596,12676,14725,14725,14725,14693,16773,18886,18919,18918,16838,14725,14692,12580,12579,14660,14693,16773,16838,16805,
16805,16838,16870,16838,14725,12677,10564,8419,8418,8451,14692,20998,27336,29384,29416,27303,23110,20998,18918,16805,14692,8418,8450,10531,
12643,14724,14724,16804,16805,18885,18917,18918,18950,18950,18950,18917,20997,21030,23110,23109,25190,25222,27302,25222,25222,21095,16902,16967,
21226,23404,25517,27694,31920,36146,38258,36145,33999,33966,36047,36079,38095,38127,38095,40143,46450,52788,56948,56883,50543,52558,54671,54671,
56751,56784,54703,54703,52591,50511,52690,54868,59094,54900,50674,48528,46447,46447,48527,46446,44301,42188,42189,44335,48594,57014,59127,57079,
294,2343,2375,327,327,2407,2408,2407,327,2375,2375,295,327,2375,2375,295,2375,2375,2375,2375,2407,327,327,327,
359,359,392,392,2440,392,392,2472,2473,2505,2505,2505,2472,2472,2538,2538,489,489,2537,2538,489,489,489,489,
2602,2602,4682,6763,6763,8843,10923,10955,10923,10923,13003,12969,15017,23370,38030,46384,42059,41961,41896,41961,44074,46186,44106,46218,
48331,50378,48297,50313,50345,52457,52490,50443,54702,56880,58994,59059,59059,56946,48495,33770,27497,27499,27500,25452,25452,27500,27500,27500,
27468,27467,27500,31660,31660,33707,37867,42027,44106,48299,50411,50411,48330,48330,48330,48298,48330,50411,48298,48266,46185,48200,48232,48168,
41895,35655,31495,31529,29481,29482,29515,29548,27533,31791,29744,25583,25518,23437,25518,25453,25453,23373,21292,21357,21357,21390,23438,25551,
25518,23372,21227,21161,21129,23177,23177,23176,23209,23241,21193,21161,21193,21194,21194,21226,19081,14823,14757,14725,16805,16806,16870,21064,
23145,21097,21064,16871,10564,10564,12709,14823,18984,16936,19016,16936,18984,16838,14725,14725,14692,14725,14725,14725,14725,16805,14757,16838,
14757,12677,12677,14790,14791,14823,14856,16937,16969,16969,14856,12742,10628,12676,14725,14725,14693,14693,16773,18886,18918,18918,16806,14725,
12612,12579,12579,14692,14692,14692,16805,16805,14757,16838,16870,16838,14757,12677,12612,8451,8419,10499,14692,21030,27337,27271,27303,27271,
23110,20998,18950,16837,14692,8418,8450,10531,12611,12644,14724,16804,16837,18885,16837,18918,21031,21030,18950,18917,20997,21030,23077,23110,
25222,25190,25222,23142,23142,21063,16934,19048,21259,25516,27629,27694,31887,36145,38226,36145,33999,33935,33967,36047,36047,38127,38095,38095,
44370,52756,56948,56883,50543,50510,52590,54671,56751,56784,54704,52623,50511,50576,52787,57013,57045,52787,48561,48495,46414,48495,48527,46414,
44301,42189,42221,44368,50707,57046,57046,57046,294,294,294,295,2375,2407,2408,2407,327,2375,2375,2375,2375,2375,295,2375,
2375,2407,2407,2407,2407,2407,2407,359,359,359,359,392,2440,2440,2440,2472,2472,2473,4585,4586,2505,2472,2537,2538,
2537,489,489,489,489,489,489,489,2602,2634,4683,4714,6730,8843,10955,10955,10923,10955,13002,15017,17097,29676,46449,46351,
39914,41929,41929,41961,44106,46187,44106,46186,48299,48266,48265,50313,50312,50377,50442,50443,54702,56880,58994,59059,59059,56914,46414,33803,
27530,27499,25420,25420,25420,27532,27500,27468,27468,27499,29547,31627,33675,35754,37866,42026,46154,50411,52524,50443,50443,48330,48330,48330,
50443,48362,48330,46217,46152,48232,50280,48199,44007,37735,33575,31561,29481,27401,29482,29547,29548,29678,29744,27663,25550,23437,25486,25485,
25485,23405,21324,21357,21357,21357,23470,25551,25518,23340,21194,21129,21129,23177,23209,23176,23176,23241,23241,23241,21193,21193,21194,21194,
19081,14823,14757,14725,16805,16838,16838,21064,21097,21065,19016,16903,12645,10564,12677,14790,16936,16936,19016,16936,18984,16871,14758,14725,
14692,14693,14725,14692,14725,14725,14757,14757,14725,12677,12677,12677,14790,14791,14824,16905,16937,16969,16936,12775,12709,12677,14757,14725,
14693,14693,16773,16806,18886,16806,16773,14693,12612,10531,12579,14660,14692,14692,16773,14725,14757,16870,16870,14758,14725,12677,12644,10499,
8451,10499,14692,20998,27304,25191,25158,23078,21030,21030,18950,16837,12644,8418,8418,10531,10563,12644,14756,16805,16837,18885,18918,18950,
21031,18982,18917,18917,21030,21030,23077,25158,25222,25190,23142,23110,21062,21127,19015,19081,23404,25581,27662,29742,31952,38193,38193,38161,
36047,33935,36015,36047,38095,38127,38127,38095,44369,52756,56916,56883,50543,50477,52558,52590,54703,56784,54704,50576,48463,50609,54900,57046,
54900,50674,48560,48495,46414,48527,48495,46414,44301,42221,44334,46480,52852,57078,57046,57045,294,2375,295,294,327,2407,359,2408,
2408,2375,2375,2375,2375,2375,295,2408,2408,2408,2408,2408,2407,2408,2440,2440,359,359,392,2472,2472,2472,2472,2472,
2472,2472,4553,4586,4553,2505,2537,2537,2537,2537,489,489,489,489,489,489,2602,4683,4715,4682,6762,8875,10923,10955,
10923,13003,15083,17098,23370,40176,50610,40012,37801,41929,41929,41961,44074,46186,44106,46186,48266,46185,48233,48265,50313,50377,50410,50443,
54702,56880,58994,59027,59059,56914,46415,33836,27531,25419,25420,25420,25420,27500,27500,29516,29515,29547,29514,31594,33674,35754,39913,44106,
46250,50443,52524,50443,50443,48363,48362,50411,50410,50410,48330,46217,46152,50312,50279,48199,46087,39815,33607,33609,29481,29449,29482,29483,
27468,29613,29679,27696,25583,23470,25486,25453,25453,23373,21325,21357,21325,21358,23470,25551,25519,23340,21162,21129,21128,23177,23209,23176,
23176,23208,23241,23241,21161,21193,21193,21194,19081,16903,14757,14692,16805,16806,16838,21032,21097,18984,18984,18984,12677,10532,10564,12709,
16871,16936,16936,16936,19016,16871,14758,14725,14692,14692,16773,14692,14693,14693,14725,14693,12644,12644,12644,12645,14758,14759,14791,14824,
16905,16937,16936,14855,12742,14789,14790,14757,14693,14693,16773,16806,18886,16806,14725,14693,12612,12579,12579,12612,14692,14692,14725,14725,
14757,16870,16870,14758,14725,12645,12644,10532,8451,10499,14692,20998,25192,25159,23046,20998,20998,18950,18950,14757,12579,8418,8450,10531,
10563,12676,14757,16805,16837,18918,18950,18950,18950,18950,18918,18950,20997,21029,23077,25190,27270,25157,23109,23110,21095,21128,19048,21193,
25484,27662,27694,29774,34000,38193,38161,36080,36047,33935,36015,36047,38095,38127,40175,40143,46418,52756,56916,56882,52623,50509,52558,52590,
52623,54704,52624,50543,48496,52755,59094,57046,52787,50641,48527,48494,48462,48527,46414,44269,44269,44301,46448,50674,57046,57078,57013,57045,
327,327,295,295,2407,2408,327,359,2408,2375,2343,2375,2376,295,295,2408,2408,2408,2408,2408,2440,2440,2440,2408,
359,392,2440,2472,2472,2472,2472,2472,2472,2472,2472,2505,2473,2505,2537,2537,2538,2570,2570,489,489,489,489,489,
2602,4715,4682,4682,6763,8843,10923,10955,10923,13002,15082,19210,27564,42256,46383,37834,37801,41929,41962,44042,44074,44106,44106,46186,
46218,46185,48265,50345,50345,50377,50410,50475,54702,56880,58994,59059,59027,54834,46415,33804,27531,25420,27501,27501,25420,27435,27435,29483,
29515,31627,31594,31562,33674,39914,42026,46219,50443,50475,50443,50444,50411,50411,50411,50410,48362,50443,48330,46217,48297,50376,50311,48166,
46054,39814,35687,33608,29448,29481,29482,29482,29515,29581,27598,27663,25648,25583,25486,25453,23405,23373,21324,21325,21357,23438,25519,25551,
25551,23372,21162,21129,21129,23176,23208,25256,23208,23208,23208,23241,21161,21161,23241,21193,19081,16935,14757,14692,14725,16805,16806,18952,
19016,18984,19016,19016,14758,10532,10564,12677,14822,16936,16936,16936,19016,16903,14758,14725,14692,14692,14692,14692,14692,14692,14693,12644,
12612,12612,12644,12612,12645,14726,14759,14792,16905,16937,16937,14888,14823,14822,14822,14757,14725,14693,14693,16806,18854,18854,16805,14693,
12612,10499,10499,12580,14660,14660,14693,14725,14757,16870,16870,14757,12645,12645,12644,10532,8451,10499,14660,20998,23111,23079,23078,20998,
20966,18918,18918,14725,10531,8386,8450,10531,12611,14724,16805,16837,16837,18886,18950,18982,18950,18950,18950,18950,18949,20997,23109,25190,
27270,23109,23109,23142,18981,18982,19080,23339,27597,29775,29775,31887,36112,38193,38128,36015,36015,35983,33935,36015,36047,38127,38127,40143,
46418,54836,56916,56882,52655,50509,50510,52558,52591,52623,50543,48463,50609,56948,59126,57013,52754,50608,48527,48462,48462,48495,46382,42189,
44269,46414,48561,54932,57078,54998,54933,56980,295,2375,327,327,2407,2408,2407,359,2408,2408,2375,2375,2375,2375,2408,2408,
2408,2408,2408,2408,2407,2407,2408,2440,2440,2440,2472,2472,2473,2472,2472,2472,2472,2440,2472,2473,2472,2505,2538,2537,
2570,4683,4683,2602,489,489,489,2570,4682,4682,4682,4650,6730,6763,8843,10923,10923,12971,15050,19210,29676,42256,44238,37834,
39881,41962,44042,44074,44106,46186,44074,44073,46186,48266,48265,50345,50377,50377,50410,52556,56815,58993,59026,59027,58995,52753,42189,29611,
27499,27468,27500,27500,27468,27467,29515,29515,31531,31562,31562,33641,35753,42026,44171,46250,48363,50475,52523,50476,50443,50443,50411,50410,
50442,50442,50442,50442,52490,52489,52424,50279,46119,41927,37735,33608,29448,29449,29482,29482,29515,29581,27566,27631,27696,25583,25486,23405,
23373,23373,21325,21325,21357,23470,25551,25518,25518,23372,21161,21161,23209,23176,23208,25256,23208,23208,23208,23241,21160,21128,21193,23242,
21161,16935,14789,14724,14725,16773,16806,18951,18984,19016,19049,16936,14758,10596,10564,12677,14790,16903,16936,16936,18984,18951,16838,14725,
14692,14692,14692,14660,14692,14692,14692,12644,12612,12612,12644,12580,12612,14694,14726,14759,16872,16937,16937,14888,14855,14822,14822,14790,
14757,14725,16773,16805,18854,18886,16806,14725,12612,10499,10499,12579,14692,14692,14692,14725,14757,16870,16870,14757,12645,12645,12645,10532,
8419,10499,14692,20998,23111,23079,23078,20998,18918,18918,16870,14757,12579,8418,8418,10531,12612,14724,16805,16837,16837,18886,18918,18951,
18950,18950,18950,18917,20965,21029,23109,25157,25157,23076,23109,23109,16804,14756,19047,27500,31758,31855,31887,33967,38192,40273,40208,38095,
38063,35950,35950,35982,38062,38095,40143,40143,46450,54869,56948,56850,52623,50509,50509,52558,52623,52623,48430,48463,52755,57046,59094,54900,
50674,50576,48494,48462,48462,48462,46382,44269,44302,46447,50674,57046,57078,54965,54965,54932,294,2375,2407,327,295,327,2408,2408,
2408,2407,2375,327,327,2407,4488,2408,2407,2440,2440,2440,2440,2440,2440,2440,2440,2440,2472,2472,2473,2505,2473,2472,
4521,2472,2440,2505,2537,2505,2538,2538,2570,2635,4715,2635,521,521,521,2602,4682,4682,4682,4650,6730,6763,8843,10955,
10923,12971,15051,17130,27563,42224,42125,37802,39882,41962,44074,44106,46186,46186,44073,44073,48233,48265,48265,50345,50377,52458,50442,52589,
54767,58993,59026,59059,59027,54834,42158,29547,27467,27467,27468,27500,27500,29547,29514,31563,31530,31529,33609,35688,37833,44106,48298,48330,
50410,50443,52523,50475,50475,50475,50443,50410,50442,50410,52555,52555,52522,52489,52424,52424,50312,43974,37735,33575,31464,31497,31497,31530,
29515,29548,27565,27598,27664,27664,25551,23406,23374,23373,21357,21357,21357,23470,23502,25518,25518,23372,21193,21129,21128,23176,23208,25256,
23176,23175,23175,23240,23208,21127,21160,23241,21161,18983,16838,14692,14693,16773,16805,18951,18952,18984,19017,16936,14790,12677,10564,10597,
14790,16903,16936,16936,18984,18951,16838,14725,14692,14692,14660,14692,14692,14692,14692,12612,12612,12612,12612,12612,12612,14692,14693,14758,
16871,16936,16937,14888,14856,14823,14822,14790,14757,16805,16805,16805,18854,18886,16806,14694,14661,12580,10467,12547,14660,12612,14692,14725,
14757,16838,16870,14757,12645,12645,12644,10532,8451,10499,14692,18950,21063,23111,21062,21030,18950,16869,16869,14756,10563,8450,8450,10531,
12644,14724,14757,16837,16869,16870,18918,18983,18951,18951,18918,18885,20965,23045,25125,25189,25157,25157,25222,23141,23109,23175,27433,33740,
37999,38063,38095,40175,40175,42287,42287,44336,40110,38030,40111,40078,38029,40110,40142,42223,48562,56916,56916,54770,50510,50477,50477,52558,
52590,50510,48430,50576,54900,57046,57013,54836,50641,48495,48462,48462,48462,46382,44302,44302,46415,48528,54868,59126,57046,54933,57012,56980,
294,2375,2375,2375,327,295,2375,2408,2408,2407,2375,2375,2407,2408,2408,2408,2407,2408,2408,2440,2440,2472,2472,2440,
2472,2473,425,392,424,2505,2472,2440,2472,2472,2472,2505,2537,2537,490,2538,2570,2602,2635,2634,522,554,2602,4682,
6762,6762,6730,6730,6730,6730,8810,10923,10923,10923,15083,19210,27564,40110,39980,37802,39882,41962,44074,44106,44106,46154,46121,46153,
50313,50345,50345,50345,50377,52490,52523,52621,54768,56881,58994,59027,59059,56915,42158,29547,27435,27467,27499,27499,29579,29547,29482,31562,
31562,33609,33609,33607,39880,46186,48298,50378,50443,50443,52523,52556,52556,50475,50410,48362,50442,50442,54635,52587,52521,52489,50376,52424,
50311,43974,39782,35655,31496,31497,31497,31530,29515,29548,27533,27566,25518,27664,25584,23470,23374,21325,21358,21357,21390,23502,25550,25550,
25518,23372,21194,21128,23208,23209,25256,25224,23175,23143,23143,23175,23207,23208,23208,23241,23209,21064,16838,14692,14692,14725,16773,16838,
18951,18984,19017,16936,14823,12677,10564,10597,12710,14823,16936,16903,16904,18951,16838,14725,14692,14660,14627,14660,14692,14692,14660,12579,
12612,12612,12612,12580,12580,14660,14692,14725,14790,16936,16936,14888,14856,14823,14822,14790,14757,14757,16805,16805,16773,18886,16806,14726,
14693,12580,10467,12547,12579,12579,14692,14725,14757,16838,16838,14757,12644,12644,12612,10532,10532,10531,14724,18950,21063,21063,21062,21030,
18949,16869,16901,14756,10563,8450,8450,10531,12644,14724,14757,16837,16869,18950,18982,21031,18950,18918,18917,18884,20997,25190,31463,37802,
39915,37867,39979,40012,44206,46416,48497,48498,50610,48530,46449,46416,46415,46415,44302,44270,40077,40013,40046,40045,37965,40078,40142,40143,
48529,54868,54836,52690,50511,50510,52558,52558,52591,50478,48463,52755,57014,57046,54933,52786,50608,48462,48430,48462,48462,44269,42189,44334,
48528,50674,57013,57046,54933,54933,57013,56947,294,2343,2375,2407,2407,2375,2375,2408,2408,2407,2375,2407,2408,4488,2408,2408,
2408,2440,2440,2407,2407,2440,2473,2473,2505,2537,2537,457,425,425,424,392,2439,2440,2440,2472,457,457,2538,490,
2570,2602,2635,2634,522,554,554,4682,6763,6762,6730,6763,8811,6763,8810,8875,10923,10923,17131,23436,35982,40110,37899,39914,
41962,44042,44041,44073,44073,44105,46153,48233,50346,52426,50345,50377,50377,52490,54636,54702,56880,56881,56913,59027,59027,54835,40078,29547,
27435,27467,27499,27499,29579,29546,29481,31529,33609,33609,33608,33607,39880,46218,48298,48330,50443,52523,52555,52588,54668,52555,50410,48361,
50442,52555,54635,52554,52521,52489,52456,52424,48231,44006,41927,37768,33544,31496,31497,31530,31562,29547,27532,27533,25485,25551,27663,25583,
23406,21325,21357,21357,21390,23470,25550,25550,27598,25452,23241,21128,23208,25288,25256,23175,23175,23175,23175,23175,23239,23208,23240,25321,
23241,21096,16870,14692,12612,14692,14725,14758,18919,16903,16936,18984,16903,12678,10564,12645,12710,14823,16936,16904,16904,18952,16871,14757,
14692,14659,12579,14659,14660,14660,14660,12579,12580,12612,12580,12579,12580,12612,12612,14693,14758,16903,16936,16936,14856,14855,14823,14790,
14757,16805,16805,16773,16773,16806,16806,14726,14661,10499,10467,10499,10499,12579,14692,14725,14757,16838,16838,14725,12644,12644,10564,10532,
10532,10532,14724,18918,21063,21063,21030,18950,18949,16869,16869,14757,10531,8418,10499,10531,12644,14724,14756,16836,18916,18949,18981,21030,
21062,18949,18916,21061,29448,35818,46318,52624,52624,52656,54737,54769,54770,56883,54835,52690,50577,50577,50544,48463,48430,48431,46350,44238,
44238,44206,40045,40046,40078,40111,42223,42223,44304,46417,46384,48464,48463,48430,48431,50511,50511,48431,48464,54868,57046,57014,54932,50673,
48495,46382,48462,48462,46349,44269,44269,44334,48528,52820,59158,57046,54932,54965,57044,56947,294,295,294,2375,2375,327,327,2375,
2407,2375,2375,2375,2407,4488,4488,2408,2408,2440,4488,2439,359,392,2472,2505,2537,4682,4683,2570,457,424,392,391,
391,391,392,424,424,457,489,490,2570,2634,4715,2635,2602,521,521,2602,4682,6763,6763,6730,8811,8843,8843,10923,
10923,12971,17131,23436,38063,37997,37866,39914,41962,44073,44073,41993,44073,46185,46185,48265,50345,50378,50345,50377,50377,52490,54636,54734,
56880,56913,56881,59027,59028,48561,35852,29547,27435,27499,27499,27499,29547,29546,29513,33642,33642,33609,35688,35687,39912,48331,50378,48330,
50442,50443,50442,50474,54700,54668,50474,50409,50441,52554,52586,52554,52521,52520,52488,52456,50343,46119,46120,39880,35656,33576,31496,31529,
31562,29515,29548,27533,27565,25518,27631,27631,25518,21358,21357,21357,21390,23438,25518,25518,27566,27500,23241,21128,23208,23208,23175,23143,
23175,25223,23175,23207,25288,23208,23240,25321,25289,21095,18918,14724,12612,12612,14693,14758,16839,16871,16871,18984,16936,12710,10564,12645,
12710,14790,16904,16936,16904,16904,16903,14758,14692,14627,12579,12579,12579,14660,12611,12579,12612,14660,12579,12547,12579,12579,12580,14692,
14758,16871,16936,16936,14856,14823,14823,14790,16805,16805,16805,16773,16773,16805,16774,14693,12580,10467,10434,10467,10499,12579,14692,14725,
14757,16838,16838,14757,12645,12644,10564,10531,8451,10532,14725,18918,21063,21031,20998,18950,18949,16869,16869,14757,10531,8418,10499,10531,
12643,14724,16804,16836,18916,18948,21028,23174,27400,29481,31626,37965,46416,52690,56883,56882,56817,56817,56882,54769,54769,54736,50543,48398,
46286,46286,48398,46317,44205,44172,42092,42060,42061,42061,37900,37900,37901,37933,40078,38030,37998,35885,33804,35884,37932,40044,42158,44271,
44271,44271,48498,54902,57047,54934,52820,48560,46382,46381,48462,48429,44268,44269,46350,46415,48561,57013,59158,57045,54965,57013,57012,54898,
295,2375,294,295,295,327,2375,2375,2407,2407,2407,2375,2407,2408,4488,2408,2408,2440,2407,2407,2407,2440,2472,2537,
4682,8909,6861,2635,2570,489,424,392,392,392,391,392,424,489,489,2570,2634,4683,4715,2635,522,489,522,2634,
6762,6762,4682,6698,6763,8875,10923,10890,10890,13002,17130,25484,38062,37932,37833,41962,41994,44073,44105,41993,44105,46218,48233,48265,
50345,52425,50345,50377,50377,52490,54669,56847,56912,58961,58994,56946,56948,46449,33772,29547,29547,29547,27499,29579,29579,29546,31561,33609,
33641,35689,35720,37800,44073,48298,48330,50410,50442,52522,50442,50441,54667,56748,52554,50442,50442,52522,52554,52553,52520,52520,52520,52488,
50375,48231,46120,39847,37767,35656,31496,31529,31530,29515,29548,29580,27565,27565,27598,27630,25551,23438,21357,21390,21390,23438,23437,25485,
27566,27500,23241,21128,23208,23176,25223,25223,23175,25255,25256,25287,25320,25288,25288,25321,25321,23176,18950,14724,12611,12612,14692,14725,
14758,14791,14823,16936,16936,14791,10597,10597,12677,12710,14823,16904,16871,16903,18952,16838,14693,14627,12547,12579,12579,12579,12579,12579,
14660,14692,12579,10499,12547,12579,12579,12644,14725,16870,16904,16904,14856,14823,14790,14790,16838,16805,16805,16773,16773,16805,16773,14693,
12580,10467,8386,10466,10499,12579,14692,14725,14725,14758,16838,14757,12677,12644,10532,8451,8451,10532,14725,18918,21063,21030,18950,18950,
18950,16869,16869,14757,10531,8418,10499,10563,12643,14723,16803,18915,18948,23141,27399,33738,37997,44304,48562,54869,59061,59061,56947,58963,
58962,58930,58930,56849,54704,48397,48365,46285,46220,44140,42027,37833,35721,35721,35721,33641,31528,29448,29448,27368,29449,31595,31627,29579,
29579,31692,31692,31692,31659,31659,33740,35852,37933,37966,42224,48596,52822,52822,50707,48528,46382,46382,46382,44269,44237,46349,46382,46415,
50707,57078,57078,54997,57013,57012,56979,54866,2375,2375,295,295,295,2375,2375,2375,2375,2375,2408,2375,2375,2408,2408,2408,
2408,2408,2407,2407,359,392,2472,2537,4715,13199,17425,11119,4683,2537,457,2472,2472,2472,392,392,457,2537,489,2570,
2634,4683,4715,2602,521,489,522,4682,6795,6730,6730,6730,6730,8843,10891,10923,10922,15050,17097,27563,44368,42125,39914,44042,
41961,44073,44073,44073,46186,46186,48265,48265,50345,52425,50377,50376,50377,50410,54636,56847,58960,58993,59026,56915,54835,44304,31660,29580,
29547,27499,27499,29579,29546,29514,31561,33609,35689,35688,37768,39880,46186,50378,50410,50410,52490,52522,52554,52522,54667,54700,52554,50442,
52522,52521,52554,52521,52520,54601,54568,54568,52456,48263,46119,39814,37735,35655,33544,31529,31529,31562,31595,29580,29613,29613,27565,27565,
27631,23470,21357,21358,21390,23438,23437,25485,27566,27500,23209,23176,23176,25223,25255,25255,25223,25223,25288,25288,25287,25320,25320,27368,
27401,25288,21030,14756,12643,12611,12612,14692,14758,14758,14823,16872,16904,14823,10597,10564,12645,12710,14823,16904,16904,16903,16903,16871,
14725,12579,12546,12579,12579,12547,12579,12579,14660,14692,12579,10467,12547,12579,12579,12612,14692,14757,14823,16903,16903,14823,14823,16870,
16838,16805,16805,16805,16773,16773,14725,14693,12580,10467,8386,10466,10467,12579,14692,14725,14725,14757,14758,14757,12645,12612,10532,8451,
8451,10564,14725,18918,21031,21030,18918,18918,16869,16837,16837,14757,10531,8418,8418,10531,12643,16803,18884,20996,23141,29480,35787,40077,
46417,52756,56950,59062,59061,58996,58963,58963,58963,56850,52623,50478,48365,44139,42027,39914,39882,37801,35689,35689,35689,33608,31528,31529,
29448,27336,27336,27336,27369,29450,27402,27435,27467,27499,29580,29580,29612,29580,29580,29547,29548,29580,31693,33838,35984,40177,42256,42255,
42255,42254,42222,42189,44238,44302,46415,48528,52852,57079,57078,54965,54932,54899,56979,54865,2375,2375,2375,2375,327,327,327,2407,
2375,2375,2408,2375,2375,2408,2440,4521,2440,2407,2407,2407,2407,2440,392,2505,4683,17361,21619,13199,4683,457,424,424,
2472,2472,2440,2472,2505,457,489,489,2602,2634,4683,2602,522,489,522,4682,4682,6763,6763,6730,6730,8842,10890,10922,
10922,15050,19145,29676,50642,42124,39881,41962,41929,41961,41992,44105,46186,46185,48265,48265,50345,52425,52425,52425,50377,50378,50475,56879,
58993,58993,58994,56947,52723,35918,27467,27499,29547,27467,27467,29579,29514,29481,31561,35690,37769,37736,37768,44073,50379,50411,52490,50442,
50442,52522,52554,52586,54667,54635,52522,52522,52554,52521,52554,52521,52521,54633,54601,54569,54569,50376,46119,41894,39814,37702,33575,33576,
31561,31562,31595,31596,31660,29612,29612,27565,25550,23470,21357,21357,21357,23406,23405,25453,27565,27500,23209,23176,25224,25256,25255,25255,
25255,25255,25288,27368,25287,25287,27368,27368,25288,27369,23175,16837,12643,12611,12611,12644,14725,14758,14791,14791,14823,16872,12677,10564,
10596,12677,14823,16904,16904,14823,16904,16871,14693,12579,12546,12547,12579,12547,12579,12579,12579,12612,12579,10467,10466,12579,12579,12579,
12612,14725,14790,16871,16903,16871,16871,16870,16838,16805,16805,16805,16773,16773,14693,14660,12580,10467,8386,10467,10467,12547,12612,14693,
14725,14725,14757,14725,12644,12612,10564,10532,8451,10532,14725,18918,18950,20998,18918,18917,16837,14757,14757,14724,10563,8450,10498,10530,
14691,16803,21028,27334,31593,37899,42158,50578,56917,59030,58998,56916,56883,56883,58963,56850,54704,50478,48333,46220,44139,44075,41995,41963,
41995,42028,39948,37835,33641,31496,29416,29448,31562,31562,33675,33675,31595,31596,29548,31629,29613,29613,29581,27500,27500,27500,27533,29581,
27501,27500,27501,27533,27533,27565,31692,33805,35917,35917,37997,40110,42223,44336,46416,48594,54966,57046,54933,54900,54900,54899,56946,56913,
2375,2375,2375,2375,2375,327,2375,2407,2375,2375,2407,2407,2407,2408,2440,4553,2408,2407,2408,2407,359,359,392,2505,
2602,11021,11054,4748,2602,489,2505,424,2440,2472,2440,2472,2473,2505,489,489,2570,2602,2634,2602,522,490,2602,2602,
4650,6763,8843,8811,6730,8810,10890,10890,10890,15050,21290,35982,50642,42092,39849,41929,41929,41961,41993,44106,46186,46185,48232,48264,
50345,50344,52424,52457,52490,50410,50475,56880,58993,58993,58994,56948,48530,29580,25387,27467,27500,27467,29547,31660,29514,29481,33609,35722,
37802,37768,39848,46218,52492,50410,50410,52522,52522,52522,54635,52586,52554,52522,52522,52522,54635,52554,52522,52489,52521,54602,54601,54601,
54569,52424,46118,44006,41926,39782,33607,33608,33609,31561,31562,31595,31628,29580,29612,27532,25518,23438,21357,21357,21357,21357,23405,25453,
27565,29548,25257,25224,25256,25256,25255,25255,25255,25255,27336,25320,25287,25287,27335,27367,25287,27336,25288,21030,14724,14691,12611,12612,
12677,12678,12710,14791,14791,16904,14791,10564,10564,10597,14791,16904,14856,14823,16904,16871,14693,12579,12546,12546,12547,12547,12547,12547,
12579,12612,12580,10467,10434,12579,12579,12579,12611,14692,14757,14790,16903,16903,16871,16838,16838,16806,16773,16773,16773,14724,14692,14660,
12580,10467,8354,10434,10499,12547,12580,14693,14725,14725,14757,14725,12644,12644,10564,10532,8451,10532,14725,18918,18950,20998,18918,18917,
16837,14757,14757,14724,12611,10531,10531,12643,18917,25223,31561,35786,37899,44238,50610,58997,59030,59030,56917,56884,56883,56882,56850,54671,
48365,48332,46219,44139,46220,46220,46221,48302,46221,42029,35754,31529,31496,31561,31561,31529,29481,29481,29514,31595,31595,31596,29548,27468,
27469,27501,27501,25388,25388,25420,25420,27501,27501,27502,27534,27501,25420,25388,25420,25452,27564,29613,31725,35918,38063,42256,42256,50708,
59127,57046,52788,52820,56980,56947,56946,56881,2375,2375,2343,295,2375,2375,2407,2375,2375,2375,2375,2407,2407,2408,2440,4488,
2408,2408,2408,2407,359,359,392,424,2537,6763,4715,2602,2538,2537,2537,2472,2440,2440,2440,2472,2473,2537,489,489,
489,2569,2634,2602,490,522,2602,2602,4650,4682,6763,8811,6730,6730,10890,10890,12970,17098,23370,44401,48464,39914,41929,41930,
43978,41962,44074,44106,46186,46153,46152,48264,50344,50344,50344,52457,52490,50410,50475,58927,58993,58993,58994,56948,44336,27499,25387,27467,
27467,27435,29547,31628,29482,31530,33610,35690,37769,39848,44074,48299,50411,50377,50409,54603,52522,52522,52586,52554,52521,52522,52522,52522,
54603,54603,52554,52522,54602,54602,54602,54602,56682,52424,46118,46086,44006,41895,37768,33608,33609,31529,31530,31595,31628,29580,29579,27532,
25485,23438,21357,21325,21325,21357,23405,25485,27565,27500,25289,25257,25288,25256,25223,25255,25255,25255,27336,27368,27367,27367,27335,27367,
27335,25287,25288,23175,16836,14692,14692,12612,12645,12677,12678,14759,14791,16904,16904,12677,10532,10597,12710,14824,14824,14823,16904,16904,
14758,14660,12579,12546,12547,12546,12547,12547,12579,12612,12579,12547,10466,12547,12579,12579,12611,14692,14725,14758,16903,16903,16871,16838,
16838,16806,16773,14692,16772,16773,14692,12612,12579,10467,8354,8386,10499,12547,12580,14693,14725,14725,14757,14725,12645,12612,10564,8451,
8419,10532,14725,18918,18950,18950,18918,18918,16869,16837,14757,12676,10531,10531,14724,18917,25288,31561,31561,35754,44238,54803,56949,56949,
56917,56949,58997,58964,58898,56784,52591,50444,48332,48332,46219,46219,48333,48366,48366,46254,39915,33609,29416,29416,31561,33642,33674,31561,
29448,29448,29481,29547,29483,25322,23210,23178,21130,21130,21163,21195,23275,23275,23275,23308,23308,23309,23341,25389,23341,23341,23373,25421,
25453,27533,27565,29677,33806,35918,38031,46515,54934,54901,52788,54868,56948,56914,54833,52719,2375,2376,2375,2375,2375,2375,2407,2407,
2407,2407,2375,2375,2375,2375,2408,2408,2407,2408,2408,2440,2407,2408,2440,2472,2505,4618,4683,4650,2537,2537,4585,2472,
2440,2472,4521,2472,2473,2505,2537,2537,2537,2570,2634,2602,489,522,2602,2602,4682,4682,6730,6763,6730,6729,10858,12938,
15018,19178,25450,42256,42190,35688,39849,41930,44010,41961,44074,44074,44073,46153,48232,48264,50312,50344,50312,52425,52458,50378,52589,58928,
58960,56913,56946,52722,37998,27467,25387,25387,25354,27435,29515,31595,29482,31562,33609,35689,37769,39881,48299,50411,50411,50410,52490,54603,
52522,52490,54602,52586,52554,52554,52554,54602,52554,52554,54635,52554,52522,56682,54602,54602,56715,54537,48198,46118,44006,41927,37800,33640,
33609,33577,31529,31594,31627,31627,29579,27532,25485,23437,21357,21325,21325,21325,23405,25517,27565,27467,25289,27336,27336,25223,25223,25255,
27335,25255,27336,27368,27367,27367,27367,29415,27335,27335,25287,25255,18949,14723,14659,12579,12612,12645,12678,14791,14823,16904,18985,14790,
10564,10564,12710,14823,14823,14823,14824,16904,14791,14692,12579,12546,12546,12547,12547,12547,12579,12579,12579,12547,12547,12547,12579,12579,
14659,14692,14692,14757,16838,16871,16838,16838,16806,16806,16773,14692,14692,16772,14692,12612,12579,10434,8354,10434,10467,10499,12580,14693,
14725,14725,14757,14757,12677,12612,10532,8451,8419,10532,14725,18918,18951,18950,18918,18950,18918,16837,14757,12643,10530,12643,14756,18917,
23110,25255,37867,50545,56883,54803,52689,52690,54770,56883,58931,56850,56816,52558,48364,50412,50412,48332,48332,48365,50479,50447,46222,39915,
33577,29383,29383,29448,31529,33642,33642,29481,27368,27368,29514,27435,23209,19016,16904,14823,14824,16905,16969,19082,21195,21227,21195,21195,
21195,21196,21228,23308,23309,23308,23309,23341,25421,25454,27566,27565,27533,29613,31726,35983,42290,44402,50676,54901,54867,54834,54768,52655,
2375,2408,2408,2375,2375,2375,2375,2407,2408,4488,2408,2407,2408,2407,2440,2440,2440,2440,4520,4553,4521,2407,2440,4521,
4521,4585,6699,6698,4553,4553,6634,4553,2440,4520,4521,2472,2505,2473,425,2537,2570,2602,2602,522,490,2570,2603,4650,
4682,4650,4682,6763,6762,8810,10890,12938,15018,21258,33869,44369,42190,33641,37801,41929,41962,44010,44042,44073,44073,46153,48233,48265,
48264,48264,48264,50345,50378,50444,56782,56815,58961,58994,54802,46384,33772,27435,25355,25354,27434,27434,29482,29514,31594,33642,33609,35721,
37801,39880,48299,48331,50410,52523,52490,52522,52490,52490,54602,52554,52554,52522,52554,52554,52522,52554,54602,54602,52521,56715,56682,54569,
56683,54569,48231,48199,46054,41894,37767,33640,33609,33641,33642,33642,31594,31594,29579,27532,25484,23405,21357,21325,21325,21325,23404,27533,
29580,29515,27337,27304,27303,27270,25222,25255,27336,27303,27335,29383,27335,29415,29416,29416,29415,27334,27334,27335,23110,16771,14659,12579,
12580,12645,14725,14790,14823,14823,18985,16871,12645,10564,12677,12742,14823,14823,14823,14824,14791,14726,14660,12547,12514,12547,12547,12547,
12579,12579,12579,12579,12547,12547,14627,14627,14627,14660,14692,14724,14757,16806,16806,16806,16806,16805,16773,14692,14692,14692,12612,12612,
12547,10434,8354,10434,10467,10499,12580,14693,14693,14725,14758,14758,12677,10597,10532,8451,8419,10532,14725,16870,18951,18950,18918,18918,
18918,16837,14725,12611,12643,14724,10530,12578,21062,37965,52691,50609,46318,46317,50510,56817,58930,56850,56817,56817,54703,48364,50444,52525,
46219,44106,52526,54607,48366,44077,37771,31497,27271,29351,33610,35755,33675,31562,25256,23143,27370,27370,23177,19049,16969,14856,14824,16937,
19050,19083,19083,19083,19115,19147,19114,19114,21195,21195,19148,19148,21196,21260,23309,21260,23308,23341,25421,25453,25453,25453,25452,27533,
31727,35951,44402,50707,52754,54834,52720,52655,2375,2375,2408,2408,2407,2375,2375,2408,2408,2408,2407,2407,2408,4488,4521,2440,
2440,2440,2440,4521,4521,2440,2408,2440,2440,2472,4553,4553,2472,2472,4553,4520,2440,2472,2472,2473,2505,2505,2505,457,
2537,2570,522,489,2570,2602,2603,4651,4683,4682,4682,6763,6762,8810,10890,15018,15018,21258,31789,48563,48497,35786,37834,39882,
41961,41961,44042,46154,46154,46153,48265,48265,48264,48232,48265,50345,48298,52557,56815,56815,58994,58994,50608,40077,29547,25354,25354,25386,
27435,27402,29482,29514,31561,31561,33608,35688,37800,41960,46218,50411,50443,52523,52490,52522,52522,54570,54602,52554,52554,52522,52554,52554,
52554,54602,54602,54602,54602,56747,56715,54602,56682,52489,50311,48231,46118,41926,37799,35688,33608,31561,33641,33642,31594,31594,29579,27532,
23404,23405,21357,21357,21325,23405,25453,27532,29612,27435,27337,27336,27303,27302,27303,25255,25287,25255,27335,29383,27335,29383,29416,29448,
29447,27366,27334,27367,25223,16803,14691,12611,12580,12613,12645,14758,14823,16871,18985,16903,12677,10564,10597,12709,14790,14823,14823,14823,
14791,14758,14692,14595,14562,14595,12547,12547,12547,12546,12578,12546,12546,12546,14595,12546,14627,14659,14692,14724,14725,16805,16805,16805,
16773,16773,16773,14692,14692,14692,12612,12580,12547,10434,10402,10434,10467,12547,12580,12645,14725,14725,14758,12678,12677,10597,10564,8451,
8451,12612,14725,16870,18950,18950,18918,18918,18918,18886,16805,14692,12611,12611,14724,25288,40046,44239,35786,31527,39947,48431,56818,56850,
56817,56817,56817,56784,54671,52558,52526,48267,44041,50381,52526,46188,41995,37770,31464,27238,29384,35723,35788,33643,27304,23143,23144,25290,
25290,21064,18984,16937,16969,16969,19050,19082,21163,19083,19082,17002,17002,17002,17034,17034,17034,17034,19115,19147,19148,21228,21260,21260,
23341,23341,23341,23341,23341,25453,27533,25453,27533,29645,33903,42288,48562,52755,52689,50543,2407,2375,2407,2408,2408,2407,2408,4488,
2408,2375,2375,2375,2408,2440,2440,2440,2408,2408,2440,2440,2440,2440,2440,2408,2440,2472,2440,2472,2472,2472,4520,2440,
2440,2440,2440,2472,4553,2537,2505,457,456,457,457,2570,2602,4651,4651,4651,6731,4682,4650,6762,8843,8810,10890,12970,
15050,19145,23337,42224,44304,33674,37801,37768,37735,39816,41961,46154,46154,46153,48233,48265,48232,48232,48233,46152,46185,52557,58928,58928,
58994,56914,46382,37932,29514,25354,25355,27435,27434,27402,29515,31594,31497,29447,33608,35688,37735,42025,48331,52524,52556,52522,52522,54602,
54603,54602,54602,52554,52554,52522,52554,52522,54602,54634,54602,54602,54635,56748,56747,54602,54602,52489,50343,50311,48198,44006,39847,35688,
33608,31528,33609,33642,31594,29546,29579,27532,25452,23405,23405,21357,21357,23438,25485,27532,29580,27434,27337,27336,27303,27302,27335,25255,
25255,25255,27335,29415,29415,29415,29416,29448,29447,29415,27334,27334,25222,18884,14691,12611,12580,12612,12645,14757,14790,16871,18984,16903,
12709,10564,10596,12709,14790,14790,14791,14791,14823,14758,14693,14627,14594,14627,12579,12547,12547,12546,12546,12514,12514,14594,14627,14594,
14594,14627,14691,14692,14692,16773,16805,16805,16773,16741,16741,14692,14660,12612,12612,12579,12547,10434,10402,10434,12547,12547,12580,12644,
14725,14725,14758,12678,12645,12645,10564,8451,10499,12612,14725,16838,18918,18950,18950,18918,18918,18886,16805,14725,12611,16805,25321,35821,
33740,21061,20996,33640,46318,56819,56883,56818,56818,58898,56817,54639,54639,52526,48268,44074,50445,52558,44042,41962,39850,31431,23044,27303,
35788,35756,27369,23110,16804,21030,25322,23177,18983,16903,14823,14823,16937,16969,16937,16937,16969,16969,14856,12808,14889,14889,14922,16970,
16970,17035,17035,19115,19148,19148,19148,21196,21228,23309,23341,23308,23308,23308,25389,27566,25485,25452,27564,33838,40144,44304,46351,48495,
4456,2408,2408,2408,4488,4456,2407,2407,2408,2375,2407,2408,2440,4488,2440,2440,2408,2440,2440,2440,2440,2440,2440,2440,
2472,2473,2440,2472,2504,2472,4520,2472,4520,4553,2472,2440,2473,2505,457,424,424,424,457,2538,2602,4650,4651,4651,
6731,4650,4682,6730,6762,8778,10890,12970,17130,19178,21193,31692,40078,35787,33640,33607,33574,35622,37735,41928,41961,44008,44040,46120,
46120,46152,46153,46153,48298,52557,56783,56815,56848,54768,44269,37932,29514,25354,25354,27434,27402,27434,31595,31562,31496,31495,33575,35687,
39880,46186,50443,52556,54636,52522,54603,56715,54635,54634,52554,52554,52554,52522,52554,54634,54602,54634,52521,54602,56715,56748,56715,54602,
56682,54569,50375,50311,48198,44006,39815,37736,35688,33608,33609,33641,31594,29546,29579,27532,25452,23405,23405,23438,23438,23438,25485,27532,
27500,29515,27401,27336,27303,27270,25255,25255,25255,25255,27335,29447,29415,29415,29383,29415,29415,29414,27334,27302,25254,20997,14691,12579,
12580,12580,12612,12677,14758,14791,16904,16904,14758,10564,10564,12709,14790,14790,14791,14823,14823,14790,14725,14659,14626,14626,12547,12547,
12546,12546,12514,12514,14594,14627,16675,14627,14594,14627,14691,14691,14692,16740,16772,16772,16772,16740,16772,14692,14692,14660,12612,12579,
12547,10434,8354,10434,12547,10499,12580,12645,14725,14725,14758,12677,12645,12645,10564,8451,10531,12612,14693,16838,18918,18950,18950,18918,
18918,18886,16805,14725,12644,16870,23176,21062,14755,18883,31593,44205,50577,54771,54771,58931,58963,56817,54671,54639,50381,44075,46155,52494,
52526,41961,41961,41962,33543,25156,29383,35755,33707,25256,18982,16869,16837,21128,23209,16935,14823,12710,10629,10630,12710,14823,14823,12775,
12775,10663,10630,10662,12776,17002,19115,21196,21261,23309,21261,21261,21261,21228,19148,21196,19115,19147,19147,19115,19115,21195,21227,23308,
25453,25420,23306,25386,31661,35853,37965,46383,2375,2375,2408,4456,4521,4488,2408,2407,2408,2375,2407,2408,2408,4520,2440,2407,
2472,4521,2472,4521,2440,2440,2440,2440,2440,2440,2472,2472,2472,2472,2472,2472,4521,4585,4553,2440,2472,424,2505,2537,
457,2505,2505,2537,2538,4618,4650,6698,6698,4650,4650,6730,6762,8810,10858,10889,15018,19178,23273,25354,33772,33707,31560,35688,
35688,37768,37735,37735,39783,39815,41895,41895,43975,44040,46153,46185,48331,52524,54637,56783,54735,52623,48463,37964,25321,25354,25354,25354,
27402,29482,29514,29449,31496,33575,35655,37800,44073,46218,50443,52523,54603,54603,54635,56715,54602,54602,54602,52554,54634,52521,52553,54634,
54634,54602,52521,54634,56715,56747,54634,54634,56682,54601,52423,50310,48230,46118,39847,37768,37768,33608,33608,33641,33642,29546,29579,27532,
25485,23405,23405,23470,23438,23405,25453,25452,27467,29547,29449,27336,27303,27302,27335,25287,25255,25255,27335,29415,29383,29383,29383,29415,
29414,29446,27333,27301,27335,23142,14691,12578,12579,12612,12612,14725,14757,14790,16871,16904,14790,10597,10532,12645,12710,14790,14791,14791,
16871,14791,14725,14659,14626,14594,14594,12546,12546,14594,14562,14562,14562,14594,16642,14594,14626,14626,14659,14659,14691,16740,16740,16740,
16740,16740,16740,16740,14692,14659,12611,12579,12547,10434,10402,10434,10499,10499,12580,14693,14725,14757,14758,12677,12677,12645,10564,8451,
10531,12644,14693,16838,18918,18950,18950,18918,18885,16837,16805,14724,12644,12643,12611,10530,12610,23109,39980,52657,52689,54738,56884,58931,
56817,54670,54670,50380,41928,46155,52527,52526,44042,39848,44074,35655,27204,31495,37867,33674,27368,18982,14789,14789,19015,19016,16903,12709,
10597,10597,8516,10629,12710,12742,12775,12710,10597,8550,12743,14888,19082,21228,21229,21229,21229,21229,19116,17035,17035,17036,19148,21196,
19148,19147,19115,19082,19082,19115,19115,19115,21259,23307,21227,21161,23274,29580,33740,35851,2375,2375,2407,2375,2407,2408,2408,2408,
2408,2375,2408,2408,2408,2408,4488,2472,4521,4521,2472,4520,2472,4553,4521,2472,2440,2440,2473,2472,2440,2472,2472,2440,
4520,4553,4553,2472,2505,2473,2505,2537,2505,2537,2505,2505,2537,4618,4650,4650,4618,4618,4650,4682,6730,8810,8809,10857,
12937,19178,23306,23273,31659,31594,31560,35721,37768,39881,41962,41929,39848,41896,41928,41895,41927,41927,41895,44040,46186,48298,50444,52589,
54735,54768,48463,35851,25320,25354,25354,25354,27402,27402,29481,31497,31495,33575,37767,39912,44072,48265,52491,52523,54635,54635,54635,56715,
54634,52554,52554,52554,54634,52521,52521,54634,54634,54601,52521,54634,56715,56715,54634,54602,56682,54601,52456,50343,48198,46118,41927,39848,
37800,35688,33640,33673,33674,31594,29579,27500,25485,25486,23438,25519,23438,23405,25453,25452,27467,29514,27401,27367,27334,27302,27335,25287,
25255,25255,29415,29415,29415,29383,29383,29415,31462,29446,29381,27301,27334,25254,18883,14626,12579,12612,12612,14692,14757,14790,14791,16871,
16871,12645,10532,10564,12677,14758,14790,14791,14823,14790,14725,14691,16674,14626,14626,14627,14626,14562,14561,14594,16642,16642,16642,16642,
16674,14626,14658,14659,16739,16739,16739,16707,16707,16707,16707,16740,14659,12611,12579,12579,12547,10466,10402,10434,10467,10467,12580,12645,
14725,14758,14758,14758,12677,12645,10564,8451,10531,12644,14693,16837,16870,18950,18950,18917,16837,16837,16805,14725,12644,10530,10498,10530,
18884,35787,50577,54738,52656,56850,56882,56784,54703,54670,52524,41927,41960,48300,48332,41929,37735,39848,37735,29349,29317,35722,33642,27400,
21062,14756,12676,19015,19015,14822,10597,8484,10597,8517,8517,10630,10630,12710,10630,8517,8549,14856,19082,19147,19115,19083,14922,14890,
14890,12777,10664,10696,10697,12777,14890,14922,17003,17035,17034,17002,17034,17002,19082,19082,19082,19081,21194,23242,21129,23242,29547,29546,
2375,2407,2408,2408,2375,2407,2408,4488,4456,2408,2408,4488,4488,2440,4520,4521,2440,2440,2472,2440,4521,6666,4553,2472,
2472,2440,2472,2472,2472,2472,2472,2440,2440,4521,2472,2472,4553,2505,2538,2537,2505,2505,2505,2505,2537,4585,4618,4618,
4617,4618,4650,6730,6730,8778,8777,10857,14986,19146,23274,25386,33740,31626,33641,35753,37769,39881,41962,44009,41961,41928,44008,44041,
44008,41928,39847,41928,41992,44073,46218,48363,52590,54801,46382,31626,25353,25354,25354,27402,27402,29450,29481,31496,31495,33607,39848,41993,
41959,48265,54604,54636,54635,54635,56715,54634,54634,52522,52521,52554,54634,54602,52553,54634,54634,52521,54601,54634,56714,54634,54601,54634,
56714,56681,52488,50375,50310,48198,41959,39880,37800,35688,33640,35689,33674,31594,27466,25419,25485,25486,23438,23438,23438,23405,25452,27532,
29547,29514,27400,27335,27302,27302,27334,25254,25286,27335,29447,29447,29382,29383,29415,31463,29414,31461,29413,27333,27302,27302,21029,16771,
14627,14660,12611,12644,14757,14758,14758,16871,16871,12645,10532,10564,12677,14758,14758,14790,14791,14790,14725,16739,16738,16706,16706,16674,
14594,14561,16609,16642,16642,16642,16642,16674,16674,14626,14626,16739,16739,16707,16707,16707,16707,16739,16707,14659,14659,14659,12611,12579,
12579,12515,10402,10434,10434,10467,10499,12644,14725,14757,14790,14758,12677,12645,10532,8451,10531,12612,12644,14757,16838,18918,18918,18917,
16837,16805,16805,14692,12611,10498,10497,14723,33674,50545,50544,48431,54737,54736,54703,54702,54637,54605,44040,41959,48267,41993,37734,31428,
33509,33509,31397,27236,33609,33642,29449,23143,14724,14724,18983,18983,14790,10596,8451,8516,8517,8484,10630,12711,10630,8517,8485,10598,
12775,14856,14921,16969,14857,12777,12776,12776,12777,14890,14922,17035,17035,17003,17003,17035,17035,17035,17035,19083,17035,19082,17002,14889,
14824,14856,16968,21129,21129,19016,21096,25289,2375,2375,2408,2408,2408,4488,4488,2408,2408,2408,2375,2375,2440,4488,2440,4521,
4553,4553,4521,4520,4521,4585,4521,2440,2472,2440,2440,2472,2472,2472,2440,2440,2440,2472,2473,2473,2505,2537,4618,2538,
2505,2505,4553,4553,4553,4585,4585,4618,4618,4650,4650,4682,6730,8810,10857,12937,15018,21194,23273,33773,40111,33706,33673,35753,
37801,39849,39849,41928,41960,41928,44009,46121,44041,44008,44040,44073,44105,44105,48298,52589,54703,54768,46382,31593,25321,25354,25355,27402,
27402,29482,31529,31496,33543,37735,41928,44072,44007,50378,56716,56716,54602,56715,56747,54634,54634,52553,52521,52553,54602,54601,52521,54634,
54634,52553,54633,56682,54634,54633,54633,54634,56682,54569,52488,52424,50311,48231,44039,39911,37800,35720,35688,35689,33641,31594,29515,27467,
25453,25486,23406,23406,23438,23405,25452,27532,29580,27433,27368,27335,27301,27301,27334,25254,25254,27334,29415,29447,29414,29383,31463,31463,
31462,29381,29413,31462,29414,27302,25189,18852,14659,14659,12611,12611,14724,14725,14758,16839,16871,14758,10564,10532,12645,12678,14758,14790,
14791,14790,14757,16771,18786,18786,16738,16674,14594,16609,16609,16609,16642,16642,16642,16642,16674,16674,16674,16738,16739,16706,16674,16706,
16674,16707,16739,16739,14659,14659,12611,12579,12579,12547,10402,10402,10467,10467,10499,12612,14725,14758,16838,14758,12678,10597,10532,10531,
10532,10564,12644,14757,16838,18918,18918,16837,16805,16805,14725,14692,12579,10498,12578,25255,42158,44206,42060,48398,54671,52590,54702,54702,
52525,46153,41895,48267,41993,33541,29347,29315,29283,31429,27236,31430,33608,29448,27369,18885,14724,18950,16902,14790,10564,8451,8484,8484,
8484,10597,12710,12710,10597,8549,10598,12711,12743,12743,12776,12776,12776,14857,14922,16970,17035,19116,19116,19148,19148,19148,19148,19149,
19180,19148,19148,19115,17035,17034,17002,14889,14824,12742,14822,16903,16935,18984,18984,16903,2375,2375,2375,2375,2408,4488,2408,2407,
2408,2407,2375,2375,2407,2440,4553,4553,4521,4521,4553,4521,2440,2472,2440,2440,4520,2440,2440,2472,4552,2472,2472,2440,
2440,2472,2472,2473,2505,2537,2538,2538,2505,4553,4585,4585,4585,4585,4585,4617,4650,4650,4682,6762,6762,8778,10825,12938,
17066,21193,27434,42225,42224,31626,33641,35721,37801,39849,39849,39848,41895,41895,44009,46121,44041,44041,44041,44073,44105,46218,50476,54702,
56816,54768,44269,29480,23240,25322,25354,25354,27402,29450,29449,31463,33575,39848,41960,44007,48265,52491,56684,54635,52522,56715,56747,54634,
54602,52554,52553,54601,54601,54601,54602,56714,56714,54601,56682,56714,54634,54633,54601,54633,56681,54568,52488,52456,50343,48231,44071,39911,
37800,35720,35721,35689,33641,31594,29515,27467,25453,25453,23406,23406,23438,25486,25452,27532,29579,27401,27335,29382,27301,27301,27334,27302,
27302,27334,29382,29414,29415,29415,31463,31463,31462,29413,31461,31494,31527,29382,25222,18852,14659,12579,12579,12611,12644,14725,14758,14759,
16839,16839,12645,10532,10564,12677,14758,14758,14791,14823,16837,16804,18786,18786,18786,16674,16641,16609,16609,16577,16609,18690,16642,16642,
16674,16674,16674,16706,16738,16674,16674,16706,16706,16707,16739,16739,14659,14659,12579,12579,12579,12515,10402,10402,10467,10467,10499,12612,
14693,14758,14758,14758,12677,10597,10532,10531,10532,12612,12644,14757,16837,16837,16869,18917,16837,16805,14725,14692,12579,10498,18917,37900,
42093,37834,44173,52591,54671,54670,52589,50444,46153,39815,44041,44074,33541,31428,27234,29283,31364,31429,29317,29350,25189,27336,23111,16805,
18918,18950,14789,12612,8451,8451,10532,8451,10564,12710,12678,8517,10598,10663,10630,10663,12743,12776,12776,12776,14857,14922,17002,17002,
17002,17002,17002,17003,17035,17035,17035,14922,14890,14922,14922,14890,14889,14889,14889,14889,16904,14791,14790,14790,14790,14822,16903,16871,
2375,2375,4456,4456,2408,2375,2408,2408,2408,2375,2375,2408,2408,4488,6633,4521,2408,6633,6634,4488,4488,4521,2440,4521,
4553,4521,2440,4521,4553,4521,4520,4520,2440,2440,2440,2472,2473,2505,2505,4553,4585,4585,4586,4585,4585,4585,4585,4618,
4650,4650,4682,6730,6730,8778,8777,12938,17098,21225,31724,44337,37932,31593,33673,35721,37801,39849,39849,39847,39815,39847,44040,44041,
44073,44073,44073,44073,44105,46218,50476,52622,52655,56882,42157,25319,23240,23273,25354,27401,27401,29449,29448,31495,33575,41961,41960,44007,
50410,54603,54635,54603,52554,56715,56747,54634,54602,52554,54602,54634,54634,56714,56747,58795,58827,56714,56682,56714,56714,54634,52553,54601,
56714,54601,54569,52488,50343,48231,44071,41991,39880,37801,35721,35721,33641,33642,31562,29515,25452,25453,23438,23438,23438,25486,25453,27500,
29547,29449,29415,29383,29381,29349,29381,27301,27301,29382,29414,29414,29415,29415,31462,31462,31462,31461,31494,31494,31462,29382,27334,20964,
14691,14659,12611,12612,12612,14725,14726,14759,16839,16871,14758,10532,10532,12677,12710,14758,14823,16871,16837,16804,18786,18786,18786,18721,
16641,16641,16641,16609,18657,18689,18689,18722,18722,16674,16673,16706,16738,16673,16673,18754,16706,16706,16739,16739,14659,14659,12579,12579,
12579,12515,10402,10402,10434,10467,10499,12612,14725,14725,14758,14758,12677,10564,10532,8483,10564,12612,12612,14757,14757,16805,16837,18918,
16837,16805,14725,14692,12611,14691,29482,40078,33706,35754,48398,52591,56751,52557,46217,41959,39815,41928,46187,37735,31428,31396,27234,33508,
35654,33510,31430,23044,23077,25223,18917,18950,16870,16838,14725,8419,8451,10531,8451,10532,12677,10597,10565,10597,10630,10598,10630,12743,
14824,12808,12809,12809,12776,12808,14857,14857,14889,12809,10696,10664,8583,8583,10664,8584,8584,10696,12777,12809,14889,14889,12776,12776,
14824,14856,14790,12677,12709,14758,14758,16871,295,4488,6569,2376,4456,4456,2408,2408,2408,2408,2408,4489,4488,4488,4520,4488,
4488,6633,6666,4488,4488,4521,2440,4521,4553,6601,4521,4489,4521,4521,4521,4488,4488,4488,4521,2472,2472,2472,4521,4553,
4585,4585,4618,4585,2505,4585,4585,2570,2570,2537,4617,6730,8811,8778,8778,12938,19211,27564,40143,42191,33674,33673,37802,37834,
39882,39881,39849,39848,39847,41927,44073,44073,46153,44073,44041,44041,46154,46218,50476,52654,52654,54801,40044,25287,23240,25321,25321,27401,
29481,29481,29448,31495,35687,41993,44040,48265,52523,54603,54635,54602,54602,56715,54634,54602,54635,54634,54602,54634,54635,56747,56747,58827,
58860,58795,56747,58795,56715,54634,52521,54602,56682,54601,54601,52488,50343,48263,46184,41992,39848,37769,35721,33641,33641,33642,33642,31594,
27499,25452,25518,25551,23471,25485,27500,29547,29547,29481,29415,29383,29382,29349,29381,29349,29349,29382,29414,29447,29415,29414,31462,31462,
31462,31462,33575,33607,33575,31495,29447,25189,16771,14659,14660,12612,12612,14693,14725,14758,16839,16839,16839,10565,10532,12645,12677,14758,
14790,16870,16837,16772,18818,18786,18785,18753,18721,18721,18721,18689,18689,18722,18721,18721,18722,18721,16673,16706,18786,16673,16705,18786,
18786,16706,16739,16739,16740,14659,12579,12579,12547,12515,10402,10402,10402,10434,12547,12612,14725,14725,14757,14757,14725,10564,10532,8451,
10532,10564,12613,14758,16838,16838,16870,18918,16838,16805,14725,14692,12643,16869,29547,29481,25222,33673,46285,50510,52558,46218,39847,37766,
39847,44106,44106,33541,33541,31460,29315,37767,37767,31461,25156,20964,25223,21030,18950,18950,16838,14757,10499,6338,10499,10531,10531,12677,
10596,10564,10597,10596,10596,10629,10630,10630,12743,12776,10728,10695,10663,10695,10696,10663,8551,8551,8551,8583,10632,12777,14890,16971,
17003,17036,17003,14922,14890,14889,12744,12712,14825,14857,14824,12711,12710,12678,12645,14758,2408,4488,4488,4456,4456,4489,4489,4489,
2408,4489,4521,4488,2408,2407,4488,4520,4520,6601,8714,4521,4488,4488,2408,4521,4521,6601,4521,4521,4520,4488,4488,4521,
4521,4521,4521,2440,2472,4553,4553,4553,4585,4585,4585,4585,2505,4585,4585,2537,2537,2537,4617,6730,8811,8778,8745,12905,
19210,29644,42256,37997,33642,35722,37802,37834,37801,39849,39849,39880,39879,39879,41960,44073,46186,46154,44041,44041,46154,46251,50477,54735,
54768,50576,33738,25288,25288,25321,27401,27433,29449,29480,29448,31495,37800,41993,44039,50378,54604,54636,54635,54635,54635,54667,54602,54602,
54634,54602,54602,54634,54634,56715,56747,58828,58860,58795,58827,58827,56715,54634,52553,54601,54634,54601,54601,54536,50343,50311,48264,41991,
37800,37801,35721,33641,33641,33673,33642,31594,27499,25452,25518,25551,25518,25485,27500,29547,29547,29449,29415,29415,29382,31430,31429,31429,
29349,29382,29414,31495,29415,29382,31462,31462,31461,33542,35622,35687,35687,33607,31560,27334,18884,14659,12579,12612,12612,12644,14725,14726,
14758,16839,16871,14758,10532,10564,12677,14758,14758,16838,16837,16804,18819,20866,20833,20833,20801,18721,20769,20801,20769,20769,20802,20802,
18754,18721,16673,16705,18753,18786,18786,18786,18786,16706,16707,16739,16740,14659,14627,12579,12579,12547,12482,10402,10402,10466,12579,12612,
14692,14725,14757,14725,12645,10564,10532,8451,10532,10564,12645,14758,16838,16838,16870,16870,16838,16805,14725,14692,12643,18982,25289,21029,
23109,33673,48365,50478,48332,39911,35653,37766,44040,48331,41960,31395,33541,33540,33541,41960,35654,27268,20963,23077,23143,18950,16837,14757,
12676,10532,8386,8419,8483,10532,10564,10564,10532,10596,10597,10596,10596,10597,10597,12678,12711,10663,8583,8582,8550,8550,6469,6437,
6470,8583,12777,14890,14923,14923,16971,17004,17036,17036,14922,12810,12809,14890,14890,14857,12745,12745,14825,14792,14791,12710,12677,14725,
4520,4488,2375,4456,4489,4488,2408,4521,6634,4488,4456,2408,2408,2408,4520,4521,4521,6666,8746,6633,4488,2408,2408,4488,
2440,4488,4488,4520,4520,2440,2440,4520,4520,4520,4553,2472,2472,2505,4521,4553,4553,4553,4553,2505,4553,4585,4585,2537,
2538,4617,6665,6730,8778,8778,10826,12905,17098,27563,35917,33707,31561,35689,35754,35721,37769,37801,39849,41928,41960,41960,41960,44073,
46218,46154,44073,44073,44073,46219,52557,54768,56881,46383,29513,25288,25289,25353,27434,27401,29449,29480,29447,33575,39881,44106,44072,48265,
54636,56716,54635,56715,54635,54635,54634,52554,54602,54635,56715,56747,56715,56715,56747,58828,58860,56747,56714,56747,56747,56747,54634,54634,
56714,56681,56682,54568,50375,50310,46183,41991,39847,37801,35689,33608,31560,33641,33642,31594,29579,27500,25485,25485,27533,27500,29547,29547,
29514,29449,29415,31463,31430,31430,31430,31429,31430,31494,31462,29382,29415,31462,31462,31462,33542,33574,33574,35654,33574,33607,31527,27334,
20964,14691,14627,12579,12612,12612,14692,14725,14758,14790,16871,14758,10564,12645,12677,12710,14790,14790,16837,16804,18851,20866,20865,20865,
20833,20801,20801,20834,20833,20801,20834,20802,20801,18753,16673,16673,16705,18786,18786,18754,18754,16706,16706,16707,16707,14659,12579,12579,
12579,12547,12515,10434,10434,10467,12579,12612,14692,14725,14757,14725,12644,12612,10532,8451,10532,10564,12645,14758,14790,16838,16837,16837,
16838,16805,14725,14692,14725,21030,21062,16803,23109,40011,48398,46284,44137,37733,37733,44040,48330,50444,39846,31427,35621,33540,37766,39912,
29381,25155,18882,21029,20997,18917,12644,12611,10531,8419,6338,8451,8451,10564,8483,8451,10597,10564,10564,8516,8483,10564,10597,10630,
10630,8550,6469,6469,6437,4356,4356,6470,8583,10663,10696,10696,10697,10729,12778,12810,12810,12810,12842,12842,12842,12842,12810,12777,
12745,10664,10664,12712,12711,14791,14758,12645,4488,4488,4456,4456,4488,4489,2408,4521,6634,6602,4521,4488,2408,4488,4521,4521,
4520,6601,6633,6633,4520,4488,4520,4520,2407,2408,2440,2440,4488,2440,4488,4521,4521,4553,6633,4553,2505,2472,4553,4585,
4553,4553,4553,4553,4585,4585,4553,2537,2538,4585,6698,8778,8778,8778,10826,12905,19145,27531,31691,31626,31561,33641,35721,35721,
37801,39849,39849,41928,41960,41960,44041,46154,46186,46154,44073,44073,44041,46218,52558,56849,54802,40077,27401,25321,25321,27402,27434,27400,
29481,29480,31495,35688,39913,46186,44104,48265,54636,56716,54635,56715,54635,54634,52554,52522,52554,56715,56747,56747,56747,56715,56715,58795,
58860,56747,54602,56714,58795,58859,56746,56714,56714,56682,56714,54568,52423,50342,46183,41958,39847,37800,35688,33608,33608,33673,33642,31594,
31627,29579,27500,27500,27532,29547,29546,31594,29514,29449,29415,31463,29382,31430,31430,31461,31462,31494,31462,29382,29382,31462,31494,33542,
35654,35654,35654,35654,33574,33607,31527,29414,25157,16739,14626,12578,12579,12612,14692,14725,14725,14758,14758,14758,12645,10565,12645,12677,
14758,14790,16837,16804,18851,18850,20898,20865,22913,20833,20833,22913,22881,20801,22881,20801,20801,18753,18721,16673,16673,18753,18786,18786,
18786,18754,16706,16707,14659,14627,12579,12547,12547,12547,12515,10434,10434,10467,12579,12580,12612,14725,14757,14725,12645,12644,10532,8451,
10532,10564,12645,14758,14758,16805,16837,16837,16838,16837,14725,14692,14757,18918,16804,14723,27335,46318,44172,37832,37831,37734,46185,50444,
52524,52524,41959,39814,41959,39846,41960,35687,25219,20995,18915,18948,16868,14756,10531,10499,10499,6338,6338,8451,10532,10532,8451,10597,
10564,8451,8451,8483,8516,10597,10564,10565,8517,6436,6404,4356,4324,4356,6469,8550,8583,8583,8583,8583,10697,12810,12810,12843,
14923,12875,12843,10762,8649,8617,6504,8584,10664,10664,10664,12744,12711,12711,12710,10597,2407,4488,4488,2408,4488,4488,4521,4488,
6601,8747,4521,4488,4488,4488,4488,4520,4520,4553,6601,6601,4520,4488,4521,4521,4488,4488,2408,4488,4488,4520,4520,6569,
4521,4521,4553,4520,4585,2505,4553,6666,4553,4521,4553,4553,4585,4585,4553,4585,4586,4585,6698,8778,8746,8778,10793,12872,
17064,23272,27465,31626,31561,33641,35689,35721,37801,37800,39849,41928,41928,41993,44073,46186,46186,46154,44073,44040,44073,46251,50510,56849,
52689,33739,25320,25321,25322,27402,27401,27400,29480,31528,33575,37768,41961,44040,44040,48265,52491,54603,54635,56715,54602,52554,52554,52554,
54602,54667,56715,54667,56715,54634,56715,58795,58828,56747,56682,56714,56747,58859,58827,56714,56681,56681,56713,54568,52455,50342,48230,41958,
39846,39848,35688,33575,33640,33641,33641,31594,31594,31627,29547,29547,29579,31627,31594,31562,29514,29448,29415,29415,29382,29381,33510,33542,
31462,31462,31430,31430,29382,31430,33542,35622,35654,35654,35653,35653,35654,33606,33574,31462,27302,20931,14658,12546,12578,12611,14692,14725,
14725,14726,14758,14758,12646,10565,10564,12645,12677,14757,16837,16804,16771,18818,20898,22945,22913,22913,22913,22913,22913,22849,20801,20801,
20769,18753,18753,16673,16673,16705,18754,18786,18754,18754,16739,16707,14659,14627,14627,12579,12547,12547,10467,10434,10434,10467,12547,12580,
14692,14692,14725,14725,12644,12612,10532,8451,10532,10564,12645,14758,14758,16805,16805,16837,16837,16837,14725,14692,12644,14724,12643,16836,
33706,46319,35752,33606,35719,39912,50509,52590,50444,52524,46185,44040,46153,41927,39879,31494,23075,20963,20996,18916,14756,12611,10466,10499,
8386,6338,8451,10532,10532,10532,10597,10564,6338,4258,6370,8483,10596,8484,8451,8452,6371,6339,4323,4356,6436,6469,6469,6469,
8550,8550,8583,10696,12810,14923,14923,12843,12843,10762,10762,8681,8682,10762,10762,10730,12778,12778,12778,14858,14889,12776,12711,12711,
2407,2407,2407,2407,2408,4488,4488,4488,4488,4488,4488,4521,6601,4488,4488,6601,6601,6601,6633,4521,4521,4521,4520,4488,
4488,4520,4520,4520,4520,4520,4488,4521,4520,4520,4520,4520,4585,2472,2473,4553,4553,4553,4553,4553,4585,4553,4553,4553,
4586,6665,8714,8746,8713,8745,10825,12872,17032,23272,29545,33706,33609,35689,35721,35720,35720,37736,37768,39847,39880,41961,44073,46154,
46154,46153,44073,44040,46153,48332,52591,56882,48496,29545,25288,25322,25289,27401,27401,29448,29448,31527,33607,37800,41960,41959,44072,48265,
50410,52522,54635,54635,52522,52554,52554,52554,54635,54635,54667,54635,56715,54634,56715,58795,56747,56747,56714,56682,56714,56747,58860,58827,
56714,56714,56713,56649,52487,50374,46149,41957,39846,39848,35687,33575,33640,33641,33641,33642,33642,31594,31594,29546,31626,31626,31593,31594,
29514,29448,29415,29415,29382,29349,33510,33542,33542,31462,31430,31462,31463,31462,31462,33542,35622,35654,35654,35653,35621,33541,31494,29381,
29382,23076,16738,12578,12578,12611,14692,14692,14692,14725,14758,16807,14759,12645,10532,12645,12677,14757,14789,16805,16804,18818,20866,22946,
22913,22912,22945,22913,22913,22880,20800,20801,20769,20801,18753,18721,16673,16673,18753,18754,18754,18786,18787,16707,14627,14627,14627,14627,
12579,12547,12547,10434,10434,10467,12547,14660,14692,14692,14725,14757,14725,12644,10532,10499,10532,12613,12645,14758,16838,16870,16838,16837,
16838,16805,14725,14692,12612,14692,14724,21062,35820,37899,25220,31494,37864,46284,56817,56784,54638,50444,48299,48266,44073,37734,37734,31461,
23075,20995,20996,18916,14724,10530,10498,10499,8386,8451,10532,10564,10532,12645,10532,4258,2177,6338,8451,8451,6371,6370,6338,6338,
6338,4291,6371,6436,6436,6436,6469,8517,8582,10663,10728,14922,14987,14923,12810,10730,10730,10795,12907,12940,15021,14988,14988,14988,
17069,17069,17068,17036,14923,14889,12776,12743,4488,2407,2407,2407,4521,4520,4488,4521,4488,4488,2440,4488,4521,4488,4488,4552,
6601,6633,6633,4520,6601,4521,4520,6601,6633,4488,4520,4520,4520,4488,4488,6569,4520,4488,4520,4553,2472,2472,4553,2473,
2473,4585,4585,4553,4553,4553,4521,4553,4586,6665,8713,8746,8746,10793,12905,14952,19145,27531,33804,33706,33609,35689,35721,35720,
35720,35687,37703,39815,41928,41961,44106,46186,44073,44073,44073,44072,46153,48332,52623,52656,40044,27400,23240,25322,25289,25321,29481,29480,
31528,33575,35655,39847,41960,44040,46152,48297,50442,52522,54635,54635,52522,52554,54602,52554,54602,54634,56715,54635,54634,54634,56715,58795,
56715,56747,58795,56682,56714,56747,58860,58827,58794,56714,56649,56648,54567,52422,46149,44037,41926,39815,35655,33575,33640,33673,33674,33642,
33642,33642,31594,31594,31593,33641,33641,31594,31562,29449,29415,29415,29382,31429,31429,33509,33510,31461,31462,31462,31495,31462,31430,33542,
35622,37702,37702,35622,35621,33541,33541,31429,27269,23076,18818,14626,12578,12611,14691,14692,12644,14725,14758,16839,16839,14726,12612,12645,
12677,14757,14790,16837,16804,18819,18818,20898,22913,22912,24992,24960,22880,22880,22848,20768,20801,20801,20801,18721,18721,18753,18753,18754,
18786,18754,16706,16707,14627,14627,14627,14627,12547,12547,12547,10466,10434,10466,12547,14660,14692,14692,14724,14725,14725,12644,10532,10499,
10531,12612,12645,14758,16870,16870,16838,16838,16838,16805,14725,14692,12644,12644,14724,23143,31594,27302,20995,31527,44140,52657,58964,58931,
54704,50445,48300,48267,44041,33541,33541,29382,20963,20996,20996,18916,14691,10498,10498,10499,10499,10564,10564,10564,10597,8484,4258,2145,
4225,6371,6403,6338,4290,6338,6306,6306,4258,4258,6371,8484,6436,8484,8517,8549,10663,10696,12777,14922,12810,10697,10730,12810,
12842,12875,12908,12940,12940,12940,14988,17134,19149,17069,17068,14987,12842,12809,12777,12777,4488,4488,2407,2407,4520,4488,2408,4456,
4488,4520,4488,2407,4488,4488,4487,4520,6601,6633,6601,4520,4553,4520,4553,6666,8714,4488,2407,4520,4521,4488,4488,6601,
4520,4456,4520,4520,2440,4585,4585,2473,2473,4553,4585,4553,4553,4521,4521,4585,4586,6665,8713,8745,10826,10825,12905,17065,
21225,29611,37965,33674,31528,35689,35721,35720,35720,37768,37735,39848,41993,41961,44106,46186,46153,44073,44040,44073,46186,48364,52590,42091,
29512,25287,23241,25322,25289,25288,27400,29448,31528,33607,35655,39847,44008,44040,46185,50378,52490,52522,54603,54635,52554,52554,52554,52554,
52554,52554,54635,54634,54602,54634,56715,58827,56747,56715,58827,56715,56714,56747,58860,58827,56746,56714,56649,56713,56681,54567,48261,46117,
41958,39815,35687,33607,33640,35753,33641,33609,33609,33642,33642,31593,31592,33641,33641,31594,31594,29481,29415,29383,29382,31397,31397,31429,
33509,31461,31429,31430,31462,31462,31430,33542,37703,37735,37702,35622,35590,35622,33541,31429,27268,25156,20931,14658,12578,12579,14691,14724,
14724,14693,14726,14758,14759,14726,12613,12645,12677,12710,14790,16838,16805,18820,20867,20865,20832,22912,24993,24993,24960,22881,22848,20768,
20801,20801,20802,18721,18721,18753,18753,18754,18786,18786,18755,16707,14627,14627,14627,14627,12579,12547,10499,10466,10434,10434,12547,14660,
14692,14692,14724,14724,14692,12612,10531,10499,10531,12612,12645,14758,16838,16838,16838,16838,16838,16805,14725,14692,14692,12644,14691,21030,
25288,20996,23044,33641,48399,56884,56884,48432,44140,46220,48300,46155,41961,33509,31429,29318,20963,23044,20996,16804,12610,10498,10466,10499,
10532,10564,10564,10564,8484,4226,2145,4225,6338,6371,6338,6338,4289,4257,4258,6306,6306,6338,6371,6403,8484,8484,8517,8485,
8550,10631,10696,10696,10697,10665,10665,10697,8649,10730,10795,10827,10827,12908,14988,14988,14988,12875,12875,10762,10729,10729,12810,17035,
2407,2407,2407,2407,2407,2407,4456,4456,4456,2440,2408,2407,4488,4488,4487,4520,4520,8713,6665,6601,6601,6601,6633,6601,
4520,2407,2407,4488,4488,4488,6569,6601,6569,4520,4520,4520,4520,4585,4553,4521,2473,4521,4585,4553,4553,4521,4521,6634,
4585,6665,8713,8713,10793,10825,12937,17065,23370,35917,37965,31593,33609,35689,35689,35688,35720,37768,37768,39880,41993,41993,44073,46153,
44073,44073,44040,44073,46185,50444,50510,37898,27399,23207,25289,25354,25321,25288,27368,29447,31528,33608,35687,39847,44040,46152,48297,52523,
52491,52490,54635,54635,52554,52554,52554,52554,52554,52522,54602,54634,54634,54635,56715,56747,56747,56714,58827,56747,56715,56747,58828,58827,
56746,54601,56649,56681,58762,56649,50374,48198,44006,39847,35687,33607,33640,35721,33641,33577,33609,33642,33641,31561,31592,33640,33641,31594,
31595,29482,29415,29383,29382,29349,31397,33477,33509,31461,31429,31430,31430,33510,33510,33542,37671,37703,37703,37671,35622,35621,33541,31461,
29349,27236,20931,16738,14658,12579,14659,14692,14724,14693,14693,14726,14759,14758,12645,12612,12645,12677,14790,16838,16805,18820,18819,20833,
20832,22912,24992,24993,24993,24961,22881,22816,20801,20801,20802,18722,18721,18753,18753,18754,18786,18786,18755,16707,16707,14627,14627,14627,
12579,12547,10499,10466,10434,10434,12515,14660,14660,14692,14724,14725,14725,12644,10532,10499,10499,12612,12645,14758,14758,16838,16837,16837,
16838,16805,14725,14692,14724,14692,14692,18950,23111,18884,23077,35754,52626,59030,54805,44174,33608,35656,46188,46188,39881,31429,27236,27237,
23012,23045,20997,16771,12611,10498,8418,8419,10532,10565,12645,10564,4258,2145,2145,4226,6338,6339,6338,6338,4257,4225,4257,6306,
6338,6338,6338,6371,8451,8452,8484,8517,10598,10631,8551,8551,6471,6471,8551,8584,10697,10729,10730,10762,12875,12908,12908,12875,
10794,12842,10794,10729,10729,10762,14923,17036,2407,2407,2407,2407,2407,4456,4488,4456,4488,2408,2407,2407,6601,4520,4488,4487,
6568,10826,8746,6601,6633,8778,6633,4520,2439,2439,2439,4488,4488,4488,6568,6601,6601,6601,4520,4520,2472,2472,2472,2472,
2440,2472,4553,4553,4521,4521,6569,6602,4553,6633,8713,8713,8745,10825,12937,14984,27531,44337,42126,31529,33609,33641,35688,35688,
35688,37768,39848,39880,41961,44041,44041,46121,46121,46121,44072,44072,44105,48364,48397,35785,25287,23240,25321,27401,27369,27368,29415,29447,
33576,35688,37767,41959,46152,46184,50378,54636,52523,52490,54635,54635,52554,52554,54602,54602,52554,52554,54602,54635,54635,54667,54634,56715,
56715,56715,58795,56714,56714,56747,58827,58827,56714,56649,56649,56681,58762,56681,54535,50343,46151,41926,37734,35687,35687,35721,33641,33576,
33609,35690,33641,31561,31560,33673,33673,31626,31594,29513,29416,29383,31463,31429,31429,33509,33541,33509,31429,31430,31462,33510,33510,33542,
35622,37703,37703,37702,37670,35589,33508,33476,31428,27204,23011,18818,16706,14627,12579,14691,14692,14692,14693,14726,14758,16806,12677,12612,
12645,12677,14758,16838,18854,18820,18787,20833,22912,22944,24992,24992,25024,24993,22881,22849,22849,20801,20802,20802,18721,18753,18753,18754,
18786,18786,16706,16707,16707,14627,14659,14627,12579,12547,12515,10467,10434,10434,12515,12579,14692,14660,14692,14724,14724,14692,12612,10531,
10499,12612,12645,14725,14757,16837,16837,16837,16837,16805,14724,14692,14692,14692,14692,16837,18950,18885,25223,42127,56885,54805,50579,48433,
39915,29318,37737,44108,37769,29317,27204,25158,23013,23078,20964,16739,14659,12579,10466,10499,12613,12677,12677,8452,4226,2113,2145,4226,
6306,6338,6306,4257,4225,4225,6306,6338,6306,6338,6338,6371,6403,8484,10565,10597,10597,8517,6437,6405,6405,8486,8518,8550,
8583,8616,8649,10794,10795,10794,12843,12842,12842,12842,12810,10697,10729,14955,17068,14956,4488,2407,2407,2407,2407,4456,4455,4488,
4488,2408,4488,4520,4520,4488,4520,6600,8681,10761,8648,6600,6600,8713,6632,4519,4520,6600,4520,4487,6568,6601,6568,4520,
4488,4520,6601,6601,2440,2440,4520,4521,4520,2472,4521,4521,4521,4521,6601,6602,6601,6634,8713,8713,8745,8777,12937,14984,
25450,42256,40013,29480,31560,33640,35688,35720,35688,37767,37800,39880,41993,44009,44040,46088,46121,46121,44072,44072,44072,48331,48397,37899,
25319,23240,27401,27401,29416,29416,29415,31463,33575,37735,37767,41959,46184,46216,48329,54603,52555,52522,54635,54635,54602,52554,54602,54602,
52554,52554,54602,54635,54667,56715,54635,54635,56715,56747,56715,54634,56681,56746,58795,58795,56714,56682,56649,56682,56714,56649,54568,54504,
50344,44038,39846,37735,35655,35688,35656,33576,35689,35690,33610,31529,31528,33673,33673,31625,31594,29481,29448,29383,31463,33510,33509,33509,
35589,33509,31429,31462,31462,31462,33510,35590,35622,37702,37702,37702,37669,35588,35555,33475,33476,31364,27171,20899,16706,14627,14627,14659,
14692,14692,14693,16806,16806,16806,14758,12645,12645,12677,16806,18886,18854,18853,18787,18785,22913,24992,25024,25024,25024,25025,24961,22849,
22849,22882,22882,20834,20801,18753,18753,18754,18786,16738,16706,16706,16707,16707,16707,14659,14627,12547,12515,10467,10434,10434,12547,12579,
14660,14692,14692,14692,14724,14692,12612,10531,10499,12612,12644,14724,14757,16837,16837,16805,16837,16805,14724,14692,14692,12644,14724,14757,
16837,16837,27369,50547,54805,44175,42029,48400,48368,31497,27270,37770,35689,27237,25124,23078,20933,23046,18852,18819,16739,14659,10466,10499,
10564,12677,8484,4258,2145,2145,4226,4226,6306,6274,6273,4225,4257,4225,6306,6338,4258,4258,6338,6371,8451,10564,10565,8517,
8484,6372,6372,6405,8485,6437,8485,8518,8550,8616,10729,12842,10762,10730,12810,12842,12810,12809,10697,10697,12874,17036,14955,12875,
4488,2407,2375,2375,2375,2407,4455,4455,2407,4456,4521,4488,4488,4487,6568,12939,12907,10729,8648,6600,6600,8681,6633,6600,
6633,6633,6568,6600,8681,10827,8714,4520,4520,4520,6601,6633,6666,4521,4521,4553,4553,4521,2472,2472,4521,6601,6601,6634,
6601,6633,8681,6665,8745,10825,12970,17065,23370,35852,33674,29480,33641,33640,35688,37768,37768,37767,39848,39880,41993,44041,44008,44008,
46088,46121,44072,44072,44072,48364,48397,37899,25352,23240,27401,29449,29416,29383,29415,33543,37704,37735,37767,41959,48265,46217,48297,50442,
52522,52522,54635,54667,54603,52554,52554,54602,54634,54602,52554,54635,56715,56715,56715,54634,54634,56747,56715,54602,56681,56746,58794,56714,
56714,56650,56649,56682,56681,54568,56649,54536,52457,46151,41926,39815,35655,35656,33608,33608,35721,35722,33642,31529,31528,31592,33673,31658,
31594,29481,27400,27335,29350,33510,33541,35589,35589,33509,31429,31430,33510,33510,33510,35590,35622,37702,37669,37669,37637,35556,35555,35555,
35556,31364,27203,22979,16674,14626,14627,14659,14659,14692,14693,16774,16806,16838,14790,12677,10565,12645,16806,18887,18854,18853,18787,18785,
22913,25024,25024,27104,27104,27073,24993,24961,22849,22881,22914,22914,20801,18721,18721,18753,18786,18786,16706,16706,18787,16707,16707,14659,
14627,12547,12515,12515,10466,10466,12547,12579,14660,14692,14692,14692,14724,12644,12611,10531,10499,12612,12644,14724,14757,16837,16837,16805,
16805,16805,14724,14692,14692,14692,14692,14757,16805,18918,35789,52725,46353,37836,37868,44174,48368,39949,25158,29351,33577,25157,23045,23078,
20966,20965,18885,18852,16772,12546,10434,10499,12645,10565,6339,4226,2145,4226,4258,4258,4226,6274,6273,4225,4225,4257,6306,6306,
4258,4258,6306,6371,8484,8484,8452,6404,6372,4291,4292,6405,6372,6340,8485,8582,10631,8616,8616,10697,10697,10697,10729,12777,
10729,10696,10697,12842,14987,14923,12842,12810,2440,2440,2440,2440,2407,2375,2375,2375,2407,2407,4456,4455,6568,6568,6568,10793,
17035,14955,8681,8616,8648,8681,6600,6600,8713,6600,6601,8714,10794,8714,6601,6568,6601,6568,4520,6601,6634,4553,4521,4521,
4553,4585,4521,2472,4521,6601,6634,6634,6601,6633,6633,6633,8745,8778,12938,19146,25386,29514,29480,31593,35722,33641,35720,37768,
37768,39848,39848,41928,41960,44008,43976,43976,46056,46088,46088,46121,46186,50477,48397,33738,25320,25287,27369,29448,29416,31496,31463,33543,
37736,39848,39847,44040,46217,48297,48329,48361,50442,52522,54635,54635,54603,52554,52522,54602,54635,54602,52522,54635,54667,56715,56747,56715,
54602,56715,56683,54602,56682,58794,56746,56682,56682,54602,54601,56649,54601,54600,56681,54536,52424,46151,41927,39847,37735,35688,33607,35688,
35689,35690,33642,33609,31560,31592,33673,33706,31626,29481,29448,29415,29382,31430,33509,35621,35621,33509,33477,31430,33478,33478,33510,35622,
37702,37702,37669,37669,37636,35555,35523,35523,35523,33443,29251,25059,16705,14626,14659,14659,14659,14692,14692,14693,14758,14758,14790,12678,
12645,12645,14758,16838,18886,18853,18819,18785,22945,25057,27104,27104,27104,27105,27073,27041,22881,22849,24962,22914,20833,20769,18721,18753,
18786,18786,16706,16706,18787,18787,16707,16707,14659,14595,12515,12515,10466,10466,12547,12579,14660,14692,14692,14692,14756,14692,12612,10531,
10531,12612,12644,14724,14757,16837,16837,16805,16805,16805,14724,14692,14692,14692,14692,14724,14724,21063,44273,52692,42159,33642,27303,31561,
39981,44174,31497,25158,29384,25158,23013,23078,20998,20998,20965,18885,14691,10498,10467,12612,12645,8452,4226,4226,2145,4226,4258,4226,
4225,6273,4225,4225,4225,6305,4257,4225,4258,6338,6338,6371,8451,6371,6339,4290,4258,4259,4291,4292,4292,6405,8550,8518,
6470,6470,6503,8583,8584,10664,10697,10664,8583,8616,10729,12842,14923,12842,12842,12810,2407,2407,4488,4520,4520,2408,2375,4456,
4520,4455,2407,4487,6568,6535,6535,8648,12841,17035,14922,10761,12874,10793,8681,8713,8681,6600,6568,10729,14987,10761,8649,8681,
8681,6568,4520,6601,6601,6634,4521,4521,4521,4553,4553,4553,4585,6634,6634,6633,6633,6633,6633,6633,8746,8778,12906,19178,
23338,29514,33674,33674,35754,35753,35720,35687,37768,41961,41961,41928,41928,41895,41895,43975,46056,46056,46088,46089,50347,52558,44205,29480,
25255,25288,29449,31496,31495,35657,33576,35591,39816,41928,41927,44104,46217,48329,48362,48329,50409,52522,54635,54667,54635,52522,52522,54635,
54667,54634,52554,54667,54667,56715,56748,56715,54667,54635,54602,54602,56714,56714,56714,56682,56682,54602,54569,56649,54601,56649,56713,56681,
50376,46151,44039,39879,37767,35687,33575,33608,35689,35689,33641,33641,33641,33640,33673,33706,31593,29481,29481,29415,29383,31430,33509,35589,
37669,35589,33509,33510,33510,33510,35590,37670,37702,39782,37701,37668,35587,35555,35555,35523,37603,37604,33411,27138,20866,16706,14626,14659,
14659,14692,14692,14693,14726,14758,14758,14758,12645,12645,14725,14758,16838,18853,18852,18818,22978,27138,27104,27104,27104,29185,29153,27073,
24961,24929,24961,24961,22914,20801,18721,18753,18786,18786,18786,18786,18786,18787,16739,16739,16707,14595,12515,12515,10434,10434,10499,12579,
14660,14692,14692,14724,14724,12644,12612,12612,10532,12612,12644,14692,14725,16837,16837,16805,16805,16805,14724,14692,14692,14692,14692,14724,
16772,29450,50579,48498,42160,31594,23077,23109,31529,39981,37836,25191,25158,23111,23046,20999,20998,20998,20998,18885,12579,10466,10531,12612,
10532,6339,4226,4194,4194,4226,4258,4226,4225,4225,4225,4225,4225,6306,4225,4225,6306,6338,6339,6338,6338,6339,4258,4226,
4226,4259,4259,2179,6340,6405,6404,4292,4292,6470,8551,8551,8583,10664,10664,6471,6503,10664,10729,12810,12842,12842,10762,10729,
2407,2407,2440,4488,4520,4488,4488,4488,4520,6601,6568,4487,6535,6535,8648,10696,12776,14921,12841,12809,17067,17100,10761,8681,
6568,6535,6536,8616,10762,10729,8681,8681,8681,6601,6601,6601,8714,8714,4553,4553,4553,2473,4585,4585,4586,6666,6633,4585,
4553,6633,6633,6633,6665,8778,12906,17065,23306,31627,33706,33641,33641,35721,35688,37736,39848,41961,41961,41928,41895,41895,43943,43975,
46023,46056,46056,46089,50380,50478,40011,27367,25255,27368,29448,31463,31430,33543,35591,39783,44041,44041,44040,46185,46217,48329,50442,48362,
50410,52555,54668,56716,54635,52554,52554,56716,54667,54603,54603,54668,54667,54667,56748,54667,54667,54634,52521,54602,54634,54634,54602,56682,
56682,56650,54569,54601,56649,56681,56681,54601,50376,46151,44039,39879,37799,37767,33607,33607,35721,35721,33641,33609,33641,31592,33673,33705,
31593,29481,29448,29415,29383,31430,33509,35621,37669,37669,35589,35558,35558,35590,35590,37670,37702,39749,39749,37668,37635,37635,37603,37603,
37635,39716,35556,29250,22946,18753,14626,14626,14659,14659,14692,14693,14725,14758,14758,12677,12645,12645,12677,14758,16806,16805,18852,18818,
22946,27138,27104,27104,29152,29152,29153,29153,27073,27041,24993,24993,24994,22881,20801,18721,18753,18786,18786,18786,18754,18786,18787,16739,
16707,14595,12546,12514,10466,10466,10467,12579,14660,14660,14692,14692,14724,12644,12612,12612,12612,12612,12644,14725,14725,16805,16805,16837,
16805,16772,14724,14692,14659,14691,14691,18917,29450,46353,50612,44273,42127,31594,23110,20964,27303,39981,39982,27271,23046,20999,20999,20999,
20999,20999,18950,18886,10499,10498,10531,10499,8419,6306,4226,4194,4226,4258,4225,4225,4225,4225,4225,4225,4225,4225,4225,4225,
6306,6338,4258,4258,6338,6338,4226,4258,4258,4226,4226,4259,6340,4292,4292,4292,6405,8550,6438,6438,8518,6470,6438,6438,
8551,10664,10697,12777,12777,10729,10697,10697,2440,2440,4520,4520,2407,4520,6601,4488,4520,6633,6601,6600,10761,12809,12809,14921,
12776,14889,17034,12841,12841,14922,12841,10728,10729,8616,8584,8616,8616,8615,8616,8649,8649,8681,8681,6633,6633,6633,6601,4553,
4553,4585,6666,4585,4553,4586,4553,4585,4585,6633,6665,6633,6665,8778,12873,14953,25386,33740,33673,31560,33641,35688,35688,37736,
39816,41896,41929,41896,41895,43976,43976,43943,46023,48137,46089,48201,52493,48365,35786,27335,25255,27336,31529,31463,31397,31429,33509,39783,
44041,46121,44040,46153,48298,50410,52523,50443,52523,52555,54668,54668,52555,52555,52555,54667,54667,52555,54635,54668,54635,54667,56748,54635,
54635,54635,52521,52554,54602,54602,54602,54602,54602,54602,54602,54601,56681,56649,54601,54569,50409,48264,44104,41959,39880,37800,35687,33607,
35688,35689,33641,33609,33640,33640,33705,33706,31593,29481,29448,29416,29383,31430,33509,37669,39749,39782,37637,35557,35557,37638,35589,37670,
39749,39749,39749,39749,37668,37636,37635,37635,39683,41764,39684,33411,25058,18753,14625,14658,14659,14659,14692,14693,14693,14758,14758,14726,
12677,12645,12645,14757,14757,16804,16771,18818,22946,27106,27105,29152,29184,29152,29152,29185,29185,27073,27073,27073,27073,24961,22881,20801,
18753,18786,18786,18786,18754,18786,18787,18787,16739,14627,12514,12514,10466,10466,10466,12579,14660,14660,14660,14692,14724,14692,12612,12612,
12612,12612,14692,14725,14725,16805,16805,16805,16805,16805,14692,14692,16805,20998,29450,42160,52692,52692,44305,42192,40046,29514,25223,20997,
27271,44239,46288,27304,20998,20999,20999,20999,20999,18951,18919,16838,12644,12579,10531,10467,8386,6306,4226,4226,6306,6306,4225,4225,
4225,4225,6305,4225,4225,4225,4225,6306,4258,4226,4226,4258,4258,4226,4226,4226,4226,4226,4259,6339,6339,4291,4291,4324,
8485,6437,6405,6405,6373,4325,6438,8583,10664,10664,10697,10664,8584,8584,8616,6503,4520,2407,4520,6666,4521,6634,8714,4520,
4520,6568,6568,8681,17035,14922,12777,17002,19082,17001,17001,14889,12808,12873,14986,12841,12873,10729,10664,8616,8616,8616,8648,8681,
8681,8681,8681,6601,4520,4521,6633,4553,4553,4553,2537,4585,4618,4618,4585,4585,4585,6665,8713,6633,6665,10826,12905,17000,
23306,40046,40012,31560,33608,33607,35655,37735,39816,41896,41896,41895,43976,46056,46024,43943,46024,46089,46089,50315,52526,44172,31528,25222,
25255,27335,31495,33543,35590,35622,35590,39782,44040,46121,44072,46185,50411,52523,52556,52523,52523,52522,52555,54635,52554,52522,52522,52554,
52555,52587,54668,54635,52555,54635,56716,54602,54635,54635,52554,52554,54602,54602,54602,54602,54570,54602,56682,56650,54601,54601,56650,54634,
52457,48297,46185,41960,39880,37800,35687,33607,35688,33640,33641,33641,33640,33672,35753,33705,31593,29481,29448,29416,29383,31430,33509,37669,
39782,41862,39717,37605,37637,37669,37637,37669,39749,39749,39781,39749,37668,37668,39684,39683,39682,41763,39715,35523,27138,20833,16673,14658,
14658,14659,16740,16773,14693,14726,16807,14758,14726,12645,12644,12677,14757,14757,14691,18786,22946,25058,27105,29184,29184,31232,31232,31232,
31265,29152,27105,27073,27073,27073,22881,20800,18753,18754,20834,20835,18786,18754,18786,18787,16739,14627,14594,12514,10466,10466,10466,12579,
14660,14660,14692,14692,14724,14692,12644,12644,12612,12612,14692,14725,16805,16805,16805,16805,16805,16772,18918,25257,35789,46354,52660,52725,
50579,44240,46385,42192,37933,31562,25191,23110,25157,33674,44174,31497,23046,20999,20968,18951,18952,18984,18951,16871,14790,12645,10531,8386,
6305,4225,4225,4226,6306,6306,4226,4225,4225,4225,4225,4225,4225,4193,4193,6306,4258,4225,4226,4226,4226,4226,4226,4226,
4226,4226,4259,4291,4226,4226,4259,6373,8453,6372,6372,6340,4324,6437,8518,8551,8551,10664,10664,8584,8583,8584,8584,8551,
4520,2440,6633,6666,4553,6633,6601,4520,4521,4520,6568,6568,12841,17003,16937,16969,16969,19050,23308,21194,17001,14921,14921,10728,
12841,14922,12777,10663,8615,10696,10728,10729,10729,8681,6600,4520,4520,4553,4553,4553,4585,4553,2505,2537,6698,6698,4585,4585,
4553,6633,6665,6665,8778,10858,12937,17032,25418,42223,42093,31560,31528,33575,35655,37736,39848,41896,41896,41895,43976,46056,43975,43943,
46056,46056,44008,48267,50445,37866,27302,25222,25287,29416,31463,33510,37703,37703,37702,41927,46153,46121,44104,46217,50443,52556,52523,52491,
52523,52522,52555,52555,50442,50442,52522,50474,50474,52554,56748,54636,52555,54667,56748,54635,54635,54635,54602,54602,54634,52554,54602,54602,
54602,54602,56682,54601,54601,54601,54602,54602,50410,48297,46153,41992,39913,37800,35687,33607,33640,33640,33609,33641,33673,35785,35785,33705,
31561,29448,29448,29416,29383,29382,33509,37669,39782,41862,39717,37637,37637,37637,37636,37637,39717,39749,39781,39749,39716,39716,39716,39683,
39650,39682,39683,35523,29251,22913,16705,16706,16739,14659,14692,16773,14693,14726,16806,14758,14726,12677,12645,12709,14757,14757,16771,18818,
20898,25026,29185,31264,31232,31264,31232,31232,31264,29184,29185,29185,29153,27105,24961,20800,18721,18754,20834,20835,18787,18786,18786,18787,
16707,16675,14627,12546,10466,10466,10466,12579,12612,14660,14692,14692,14724,14724,12644,12644,12612,12612,14692,14725,16805,18886,18918,23111,
27402,35789,42129,46419,48467,44241,39982,42127,50547,54805,48530,40046,37933,33708,33708,37868,31496,29383,42061,33610,23111,20999,21000,18952,
18952,18984,18984,16903,14758,12644,10499,8386,6305,4225,4225,4225,4226,6274,6274,4193,6274,4225,4193,4193,4225,4193,4193,4226,
4258,4226,4226,4226,4226,4226,4226,4194,4226,4226,4259,4258,4226,4226,4227,4260,4260,6307,6340,6340,6405,8518,8550,6470,
8551,8583,8551,8551,8584,8552,8552,10697,4520,4520,4553,4553,4553,4553,4521,6633,8714,6600,6568,8616,10664,14890,16970,19050,
19017,19049,27500,27565,25452,17001,12808,10727,12808,14889,12776,12776,12776,12776,10729,10729,8681,6600,6568,6568,6633,6666,4553,4553,
4586,4585,4585,4585,4618,6698,4618,4553,4553,4585,6633,8713,10826,12938,12937,17032,29611,44304,37867,31495,33576,33607,35688,37736,
39848,41896,41863,41863,43975,46056,43943,43975,46056,44008,44008,46186,42026,29382,23109,25255,27335,29416,31495,33510,35590,37670,39815,46120,
46153,44073,46185,46185,50411,52523,52491,50410,50410,52523,52555,52555,50442,50442,52522,50441,50442,52522,56748,54668,52555,54635,56748,54667,
54635,54635,54602,54634,54635,52554,54602,54602,54570,54602,54634,54601,54601,56682,54602,54602,52490,48298,46153,44073,41993,39881,35687,33607,
33608,33640,33609,33673,35785,37866,35785,33673,33641,29480,29481,29448,29383,29350,33509,37669,39749,39749,39717,39717,37636,37636,37636,37636,
39749,39781,41829,41829,39716,39716,41796,41795,41730,41730,41763,37603,31363,25026,18753,16706,16739,14659,14659,14692,14693,14693,14758,14758,
14758,14757,12677,12677,14757,14757,14723,16738,20865,25026,29185,31297,31264,31296,31264,31264,31232,31265,31297,31265,31265,29185,27073,22880,
20801,20802,20834,20835,18787,18787,18786,18787,18787,16707,14627,12546,10466,10466,10466,10499,12580,14660,14660,12612,14692,14692,12644,12612,
12612,12580,14692,14725,18886,27338,37903,44274,48467,46387,44274,40015,33709,31563,35821,48434,54837,54805,44304,42159,46385,50579,52725,50514,
33642,31464,44142,33610,23111,20999,21000,21000,18952,18984,18984,18984,16903,12677,8418,8386,6305,4225,4257,6306,4225,6274,6274,4193,
4225,4225,4193,4193,4193,4225,4225,4226,4226,4225,4193,4226,4226,4226,4226,4194,4226,4226,4226,4226,4226,4226,4227,4227,
4227,4227,6307,6340,8485,6437,6438,6470,6471,8551,8551,8551,8551,8551,8552,10665,4520,6633,6601,4520,4520,4520,6568,8681,
8681,8648,10729,10729,12745,16906,19018,21098,23210,23209,27466,31725,29612,21194,16968,12808,14889,14921,12808,14888,14889,12809,12809,10729,
8648,8649,6601,6601,8714,6666,4553,4585,4585,4585,6698,4650,4618,4618,4618,4553,4521,6601,6633,6633,8745,12938,14985,19112,
33804,48496,37899,31527,33608,33608,35688,37736,39816,41864,41831,41862,46023,45991,43943,43976,46056,44008,44040,44041,35687,27270,25222,27303,
29383,31495,33543,33542,35622,37670,41863,44041,44073,44073,44105,46185,48330,50411,50378,48297,50378,52523,52555,52523,50409,50410,50410,48329,
50410,52523,56748,54667,52555,54635,56716,56748,54667,54635,54602,54635,56715,54603,54603,54635,54602,54603,54635,54570,54602,56683,56650,54602,
52490,48265,46153,44073,44073,41961,35687,33607,33608,33608,33640,35721,37833,37833,35753,33673,31593,29480,29481,29448,29383,29349,33477,37670,
39717,39717,39717,39749,37604,37636,37668,37636,39716,39749,41829,39781,39716,39748,41796,41795,41762,41730,41763,39683,35491,27138,20865,16705,
16706,14659,14692,16740,14692,14693,14726,14758,16807,14758,12645,12677,12677,14757,14724,16738,18786,22946,27138,31297,31297,33312,33312,31265,
29184,31265,31297,31265,31297,31265,29185,24993,20801,20834,20834,18787,18787,18786,18786,18787,18787,16707,14626,12514,12514,10466,10466,10499,
12579,12612,14660,14660,14692,14692,12644,12612,10564,10532,12644,14757,27403,46419,50580,44273,37870,31628,33709,35789,35790,37935,46354,50645,
52725,50644,48498,52724,54870,54805,50579,37901,27303,33578,46256,31530,23079,21031,21000,21000,21000,18985,18984,19017,18952,12645,8419,8418,
8386,6305,6306,6306,4226,4226,4226,4226,4193,4193,4193,4193,4193,4193,4226,4258,4226,4193,4193,4193,4226,4226,4226,4226,
4226,4226,4226,2146,4226,4259,4227,4227,4227,6307,6307,6340,6405,6405,6405,6438,6471,8551,8519,8519,8519,6471,8552,8584,
2439,6600,6633,4520,6600,4487,4487,6600,8713,10728,12809,14890,16938,19018,21098,23177,23176,27401,35820,37933,29547,25354,21161,16969,
17001,21195,19114,16969,14889,12777,12809,10729,8648,8681,6601,4520,6633,6666,6633,4553,4553,2505,4618,4618,2537,4585,4585,4553,
4553,6633,8681,8713,8713,12873,14985,21192,33803,46383,39980,31528,33608,33608,35656,37736,41864,41864,43911,43942,46022,45991,45992,46024,
46055,44040,46185,39913,31462,29383,31497,31496,31496,33575,33543,33542,37670,39783,41896,44008,44041,44040,41992,44105,46250,48330,48265,48265,
50378,52523,52555,50442,48329,48329,48297,48297,48330,50443,54636,54635,54603,54635,56748,56748,54667,54635,54635,56748,56716,54635,56716,56716,
54603,56683,56683,52490,54570,56683,56651,56618,52490,50345,48233,46153,46153,41993,37799,35687,33607,33640,35688,35720,37833,37833,33672,33672,
33673,31593,29481,29448,29382,29349,33477,37669,37669,37637,39717,39749,37636,37636,39716,37636,39716,39748,39748,39780,39747,39747,39715,39715,
41730,41698,41762,41763,37636,31331,25026,18785,16706,16707,14692,16740,16740,16740,14693,14726,16839,14758,12645,12678,12678,14757,14757,16771,
18818,20898,25058,29217,33345,33345,33345,31264,29184,31297,31297,31264,33344,33344,31297,27105,22881,20834,20834,18786,18786,18786,18786,18787,
18787,16707,14626,12546,12514,10466,10466,10467,12579,12612,12612,14660,14692,14692,12644,12644,10564,10564,10564,18950,42193,50579,35788,29449,
27369,29515,33742,37936,39984,44242,48565,50645,52758,54871,54903,54870,52725,46386,35853,31563,31562,40015,46354,31531,23112,21032,21032,21032,
21001,21033,18985,18985,16871,12613,10467,10467,8386,6306,6306,6306,4226,4225,4226,4258,4226,4193,4226,4226,4226,4226,4226,4226,
4226,4226,4226,4226,4226,4226,4226,4226,4226,4226,4194,2146,4226,4227,4227,4227,4227,4259,6307,4259,6340,4292,4357,6438,
6470,8519,6406,6406,6438,6439,6471,8552,2439,2407,2407,4519,10794,6600,4455,8680,12906,12873,12841,21196,19082,21130,27371,25257,
25289,31595,37933,40046,33740,27434,23241,21194,19114,19146,19114,16969,14889,12776,12777,10729,8648,8681,6633,4520,4553,6633,6633,4585,
2505,2473,2505,2505,2537,4618,4649,4585,4553,6633,8713,8746,10794,12873,14984,23273,37965,46351,39979,31560,33608,33608,35623,37736,
41864,41863,43943,43975,43942,43943,46024,46056,43975,46120,48299,35720,27302,29416,31496,31496,33608,33575,33543,35622,37703,41896,43976,44008,
44041,41992,41992,44072,44105,48298,48297,48298,50442,52523,52555,52523,48329,48329,48297,48297,48330,50410,52555,54635,54635,54667,56748,56716,
54635,54635,54668,56780,56716,54636,56748,56716,54635,56716,56715,54570,54602,56683,56651,54602,52457,50345,48233,48233,46186,41992,39880,37767,
35688,35720,35720,37801,37833,35753,33672,33640,33673,31593,29481,29448,29382,29349,31396,35589,37669,37637,39717,39782,37701,37636,37636,39684,
39716,41828,39748,39748,39747,39747,39715,39683,41731,41730,41762,41827,39748,35524,27106,20866,16706,14658,14659,14692,16740,14692,14692,14725,
14758,14758,12645,12678,12678,12710,14757,16804,18819,20898,25025,29185,33345,35425,35425,33345,31296,33345,33377,33344,35424,35457,33410,29218,
24961,20833,20834,20834,20834,18786,18787,18787,18787,16707,14626,12578,12546,10466,10467,10467,12579,12612,12611,14659,14692,14724,12644,12644,
12612,10596,10564,21031,44273,42094,29383,29384,27369,29515,33774,35888,40049,46389,50647,52792,57017,57017,56952,54871,52758,46452,40145,44306,
48500,50613,46387,31597,23177,21065,19017,18985,18952,18985,18985,18985,16871,12613,10532,10531,8419,6306,6306,6306,6306,4226,4258,4258,
4225,4225,2145,2145,4226,4226,4226,4226,4226,4194,4226,4226,4226,4226,4258,4226,4226,4226,2146,4226,6339,4259,4226,4227,
4227,4226,4226,4259,4259,4292,4325,6405,6438,6438,6438,6405,6406,6438,6471,8551,2439,2439,2407,4487,6600,8681,8680,8648,
10761,10728,14857,23275,23242,27435,31596,29482,33708,40014,44240,44272,37933,29546,27466,27499,23306,17000,16969,14889,14889,14857,12777,10696,
10729,8713,8681,4553,4520,4553,6665,4585,4618,4585,457,457,2537,4682,4682,4585,4585,4553,6633,8746,10858,12906,17065,23305,
37965,42158,35754,31528,33608,33608,35655,37736,39816,41863,43943,43975,43943,46023,46055,46055,46055,48234,42026,29414,25222,29383,31464,31463,
31463,33543,33575,35655,39816,41896,41928,41960,44073,44040,44072,44073,44104,46217,48298,48330,50443,52523,52523,52523,50410,48330,48297,48330,
50410,50443,52555,52555,54635,54668,56748,54668,54603,54603,54668,56781,56716,54636,56716,56684,56684,56716,56683,54570,54603,56683,56651,54602,
52425,50345,48265,48265,48233,41992,39880,37800,35720,37769,37801,37801,35720,35720,33640,31592,33673,31561,29448,29415,29382,29349,31396,35557,
37637,39717,39749,41830,39781,37668,37636,39684,39748,41796,39748,39780,39747,39748,39715,41763,41763,41730,41794,41827,41828,37603,29250,22946,
16706,16706,14659,14660,16740,16740,14692,14693,14758,14758,12678,12645,12678,12710,14757,16804,18852,18851,22978,27105,31297,35425,35457,35457,
33376,33376,35457,35424,35424,37537,35458,31298,27073,22881,20833,20834,20834,20835,18787,18787,18787,16707,14627,14627,12546,10466,10499,10467,
12579,12611,12611,14659,14692,14724,14692,12644,12644,12644,12644,16837,37868,37803,29350,29351,27337,29483,31695,35889,40115,48567,54873,56986,
57017,56985,56952,54872,54871,54871,54872,56952,56952,52726,44275,33743,27404,23211,21098,18985,18953,18985,18985,19017,16872,12678,12645,10564,
8451,6338,6338,6338,6338,4258,4258,4225,2177,2177,2145,97,2177,4226,4226,2178,2145,2145,4193,4193,4194,4226,4226,4226,
4258,4226,4226,4227,4227,4226,4194,4226,4226,4194,4226,4227,4227,4292,4292,4325,6437,6405,6405,6373,6405,6438,8518,8551,
2439,4487,4487,4520,4520,8713,10826,8681,12842,12841,14921,19049,21128,29547,33708,33675,42094,48466,52724,50644,42191,33772,31659,29579,
23273,19080,19081,19082,14889,12809,12809,10728,10761,8713,6568,4520,4520,4520,4585,4553,2505,2537,2505,424,2537,4650,6762,4617,
4585,4553,6633,8713,10826,12906,21226,29644,40078,40012,33641,31528,33608,33608,35688,37768,39847,41895,43943,43975,43975,46055,46055,46055,
46120,46154,35688,25221,25222,29383,31496,31495,31463,31494,33542,35687,39880,39848,39848,41928,44041,44041,44040,44105,44072,46217,50378,50378,
50443,52523,52523,52491,50410,48330,46217,48298,50411,52491,52555,52555,52555,54636,56781,54668,52555,54635,56748,56748,54636,54636,56716,56684,
56684,56716,54603,52490,54570,54603,56683,54602,52425,50377,50345,48265,48265,44105,41960,39880,37801,37801,37833,37801,35720,35720,33608,31559,
31624,31560,29448,29415,29382,29349,31396,33508,37637,39717,39749,41829,39781,39716,37635,39715,41796,39715,39715,39779,41827,41860,41828,41828,
43876,41827,43875,43907,41827,37635,31362,25026,18753,16674,14659,14659,16740,16740,14692,14692,14725,14758,12710,10597,12678,14758,14757,16837,
16804,18851,20931,25025,29185,35490,37538,35457,33376,35456,37504,37504,37504,37569,37570,33378,29154,22913,20833,20834,20866,20867,18787,18787,
18788,16707,14659,14659,12546,10466,10499,10499,12579,12579,12611,12611,14692,14724,14692,12644,12644,12644,12644,16804,35755,37802,29317,29351,
31498,29516,31663,33841,42260,52793,56986,57017,56985,56984,56920,56920,56952,56984,56984,54872,52726,46388,40017,33710,29517,25324,23179,21098,
21066,21066,19017,19017,16904,14758,14726,12612,8451,6371,6371,6371,6371,4258,4258,4226,4226,2177,2177,2177,2210,2210,4258,2178,
2145,2145,4194,4194,4226,4226,4226,4226,4226,2145,4226,4259,4194,4194,4226,4194,4194,4226,4226,2178,4259,4292,4292,6404,
6405,4292,4324,6405,6437,6437,6470,6470,4520,4552,6633,4520,4520,6600,10826,10826,10761,12841,19082,19081,21128,27466,31627,35820,
44272,54837,59063,56982,48531,40110,35852,27433,21160,21193,21162,19082,14889,14889,14922,12809,10793,8713,4520,4520,8746,6698,4585,4617,
4618,2537,2537,457,2537,4650,4650,4649,4618,4585,6633,6633,10826,12938,21226,33805,44304,42125,35754,33640,31527,33608,35688,37768,
39847,41895,41895,43975,46056,46055,43974,46055,48234,41961,33575,27335,25223,27303,31495,33608,33575,31461,33574,37768,39881,39816,39816,39880,
41960,41960,42024,44073,44073,46185,50378,50443,50443,52491,52523,52491,50410,48330,46217,48297,50378,50443,52555,54603,52555,54636,56749,54636,
52555,54668,56748,56716,54603,54603,56716,56684,54635,54603,52522,52490,52490,54603,56715,54602,52457,52425,50377,48265,46185,46153,41960,39880,
37800,37800,37833,37801,37800,35720,31559,29447,31560,29480,29448,29415,29350,29349,31396,33509,37669,39749,39781,41829,39780,39748,39715,39715,
41796,39715,39714,41827,41827,41860,43908,43941,48134,46021,43939,43939,41859,39747,35555,27171,20865,16673,14626,14659,14659,14692,14659,12611,
12645,12677,12710,12645,10597,12710,14758,14757,16837,18884,20931,25058,27105,33410,37538,37537,35456,35456,37536,37536,37504,39617,39618,35458,
31265,24992,22881,20834,20866,20867,18787,18787,18788,16739,14627,14627,12547,10498,10499,10498,12579,12579,12579,12611,14691,14692,14692,12644,
12644,12644,12644,14723,33642,39882,29317,31431,31562,29516,29615,38067,48630,54905,54904,54904,56984,56984,56984,59032,56919,54806,50613,48468,
44210,37903,33678,29485,27372,25291,23179,23179,23178,21098,21065,21065,18952,16839,14726,12612,8451,8451,6403,6371,6338,4258,4258,4258,
4258,2177,2177,129,2210,2210,2178,4226,2145,2145,4194,4226,4226,4226,4226,4226,2113,2113,4226,4226,2146,4194,4226,4194,
2146,4226,2178,2178,4259,4292,6372,6372,4291,4259,4292,6404,6404,6437,6437,6437,2440,4553,6633,4520,4520,8681,10826,10859,
10761,10728,17002,23307,25386,27466,31627,37933,44304,56918,59096,56983,52724,42224,33772,29579,27499,25387,23308,19082,14889,14954,19180,17067,
12841,8680,4519,4552,8778,8778,4585,6730,8811,4618,4650,4650,2570,2569,2569,4650,4618,6666,6633,6633,10826,12906,19113,29612,
44304,46319,39947,35753,31527,33607,35688,37735,39815,39815,41862,43975,46121,46120,43975,44040,48267,39914,33641,29448,27271,29350,31463,33575,
33542,31429,33574,39880,39848,37768,37800,39880,39879,41960,44073,44105,46153,46217,48330,50411,50411,50442,52523,50443,50378,48297,48265,48297,
48330,50443,52556,54636,54603,54668,56749,54635,52555,54636,54668,54668,54603,54603,56684,54603,54603,52522,52458,52458,52490,54570,56683,54602,
52457,52457,52457,48297,46152,46185,41992,39848,37768,37801,39881,37801,37800,35720,33607,31495,29447,29447,29447,29415,29382,29349,29316,33476,
37669,39749,41829,41861,39748,39748,41828,41828,41828,41795,41795,41827,41827,41827,41827,43941,50247,48166,43972,43939,41859,41827,39716,31331,
20865,16705,14658,14627,14659,14691,14660,12612,12644,12677,12709,12677,12645,12677,14790,14789,16837,16805,18852,23044,27138,31297,35489,37569,
37569,37536,37568,37536,37504,37536,39617,37538,33345,27072,22880,20833,20834,18787,18787,18788,18788,16740,14627,14627,12579,10499,10498,10499,
10499,12579,12579,12611,14691,14692,14692,12644,12644,12644,12644,14691,27303,35689,29318,29383,31530,29516,31695,44372,52855,54903,54870,54870,
56951,58999,56918,54773,48499,46322,42096,37870,33677,31532,29420,27307,25226,23178,23178,23179,23146,21066,21033,18985,18919,16807,14726,12613,
10532,8451,8451,6371,6338,4258,4258,4258,4258,2177,2177,2177,2210,2177,2145,2177,2145,2145,4193,4226,4226,4226,4226,2145,
2113,2145,4226,4226,2146,2146,2146,2114,2146,2178,2178,4227,4291,4291,4259,4259,4226,4259,6372,4324,4324,6436,4356,4356,
2472,2472,4520,4520,6601,6633,6633,10826,14987,12841,12808,19114,25419,27467,31660,35885,40078,50644,56950,50676,46450,42192,33773,33806,
35919,29614,23308,19082,16969,14922,14954,14922,10761,6568,4519,4520,6633,4585,4585,4585,4618,4650,4650,4650,4650,4617,4617,4650,
4585,6698,6665,8745,10826,12906,17000,27466,44304,46286,37866,33640,31495,33607,35688,35655,39848,41928,39815,41895,46088,46120,41959,44138,
46220,35721,29383,29383,29383,29383,31463,33575,33542,31429,35622,37800,37767,37768,37800,37832,39880,41960,44105,46186,46153,46185,48266,50378,
48330,50410,52523,50443,48330,48297,46217,48297,48330,50411,52556,54636,54636,54668,54668,54603,52555,54636,54668,52555,52490,52490,52490,52522,
52523,52490,50410,52490,52490,54538,54603,52490,52489,54538,52490,50345,48233,48266,44073,39848,37800,39881,39881,37800,35720,35720,33607,29447,
29479,29447,29415,29382,29382,29381,29316,31396,37637,39749,41829,41861,39748,41828,43941,43908,43908,43908,41827,41859,43907,43939,41827,43940,
48199,48198,43972,43939,43939,43908,41828,33475,22945,18753,14658,14626,14627,14691,14692,14660,14692,12677,14725,14758,14757,14725,14757,14757,
16805,16805,16804,20964,25091,29217,33409,37601,37568,37536,37536,39616,39616,37536,39617,37569,33377,29152,24960,20833,20834,18787,18787,18788,
18788,16740,14659,14659,12611,10499,10466,10499,10498,10499,12579,12579,14691,14692,14724,12676,12676,12644,12644,12643,23110,31529,27271,27335,
29417,29451,35855,50678,54903,54870,56918,56950,54837,52660,48434,44176,39983,37838,33612,29419,27307,25226,25194,23114,23114,23114,23146,23146,
21066,21033,18920,18920,18919,16807,14758,14726,10533,8451,8451,6338,4258,4258,4225,4258,4258,2145,2145,4258,4226,2145,2145,2113,
2145,2145,2145,4194,4226,4226,2145,2145,2145,2146,4226,4226,4226,4226,2146,2113,2146,2178,2179,4259,4259,4259,4226,2178,
2146,4259,6340,4292,4324,4324,4291,4324,4520,2472,4520,4520,4520,4520,6568,8713,10826,12939,12874,14889,19114,23275,25387,31660,
33805,42191,48530,42224,37966,33773,29580,27468,27468,23307,19082,19114,21195,17034,14921,12809,8648,6568,6568,4552,4585,4552,4553,4552,
4553,4618,2537,2537,2537,2537,2537,4618,4650,4617,6665,8745,10826,14954,17033,29547,44272,46318,37866,35720,31559,33607,37768,37735,
39848,44009,41895,43943,44007,41959,42024,46284,42092,29447,27302,29383,31463,31495,33543,33542,31461,35622,39848,37800,35687,35720,35720,37799,
37831,39912,44105,46153,46153,46185,48266,48298,48298,50378,52555,50442,48330,48298,48297,48298,48298,48330,52523,52555,54636,54668,54604,52555,
52555,54636,54636,50410,48329,50378,50377,52490,52490,52490,52490,52458,52490,52490,52458,50377,52490,54570,52458,48264,48233,48266,44073,39880,
37800,39913,39881,37800,35720,35720,31527,29447,31527,31495,31495,29382,29349,29349,29316,31396,35588,39781,41829,41828,41828,41828,43941,43940,
43940,43908,43875,43939,43972,46020,43939,43940,46086,46085,43972,43939,43971,43940,43908,37636,27138,18785,14625,12578,14626,14659,14692,14660,
12612,12644,14692,14757,14790,14757,14757,16805,16805,16837,18885,18884,23011,29218,33376,35521,37568,37568,37568,39616,39616,39584,39616,39617,
37537,31297,24993,22881,20834,18787,18788,18788,18788,16740,16740,14692,12611,12579,10466,10498,10498,10498,12579,12579,14691,14692,14724,12676,
12676,12677,12644,12644,25289,33707,27336,27304,27337,31563,42194,52790,54870,54838,54805,52691,48433,44143,39917,35724,33580,29387,27274,25194,
23114,23082,23082,23114,21034,21034,21066,21067,21066,21033,18920,18920,18920,18887,16807,14694,10533,8451,8419,6338,6274,4226,4225,4226,
4226,2145,2145,4226,4226,4194,4194,4193,4226,4226,4226,4226,4226,2145,2145,2145,4226,2146,2145,2146,4194,4226,4194,2146,
2146,4227,2211,2211,2179,2178,2178,2178,4226,4259,4259,4291,4324,2243,2211,4292,4553,4552,4520,4553,4520,4520,6600,6601,
6633,12907,12906,12809,14921,19114,21162,25322,27466,31692,40111,40079,33773,31661,27468,25323,23275,19082,14888,17034,21260,17034,12841,10696,
8648,8713,8713,6633,4552,4520,4520,4553,4553,4585,2505,2505,2504,2505,4585,4618,6730,4617,6665,8745,10826,12873,19146,31660,
44239,48431,46317,39978,35720,35752,35687,35687,39848,44074,43976,43943,43943,41959,44170,46285,35818,27303,27270,29382,31462,31495,33543,31461,
33509,37767,39880,37767,37768,37800,35720,37832,39912,41993,44105,46153,46186,48266,48266,48266,46217,48330,52523,50410,48297,48330,48330,48298,
46217,48298,50411,52491,54604,54636,52555,52555,52555,52523,52491,50410,50378,50377,50410,52490,52458,52458,52458,50377,52425,50377,50377,50377,
52458,52490,52426,48232,46185,46186,44106,41961,37800,39881,37833,37800,37768,35688,31495,31495,31528,31527,31495,29382,29349,29317,27235,31428,
37669,39781,41861,41860,41828,43908,46021,45988,43940,43940,43907,43940,46052,46052,46019,43940,43972,44004,44004,43972,46020,45988,43908,39716,
29283,18785,14657,14626,14626,14627,14660,14660,12612,12612,12644,14724,14757,14757,14757,16805,16805,18885,16805,18852,23011,27202,31329,35489,
37601,39681,39648,37568,39616,39616,39616,39616,37569,33377,27073,22913,20866,20835,18787,16740,18788,16740,16740,14692,12611,12579,10498,10466,
10498,12579,12579,12611,14691,14724,14724,14724,14725,14757,12677,18951,35854,44338,37934,31627,31627,40047,50645,54838,54805,54773,52627,48369,
42063,37805,33579,29418,27241,25161,25161,23081,21002,21002,21002,21034,21034,18954,21034,21034,21034,21001,18920,18888,18887,18887,16807,14726,
12613,10532,8419,6306,6306,6306,6306,4226,4225,2145,4193,4226,4194,4226,4226,4194,4226,4226,4226,4226,4193,2145,2145,4226,
4226,4226,2113,2146,4194,2146,4194,2146,2178,4227,2179,2179,2178,2178,2178,2146,4227,4259,4259,4259,4291,2243,4292,4292,
};
static const uint16_t IC_LIGHT[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65184,65184,65184,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,65184,65184,65217,65184,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65251,65487,65521,65521,65487,65251,65184,65184,65184,65184,65184,65184,
65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65318,
65521,65521,65521,65521,65521,65419,65184,65184,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65250,65521,65521,65521,65521,65521,65521,65352,65184,65184,65184,65184,65184,65184,
65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65454,65521,
65521,65521,65521,65521,65386,65184,65184,65184,65184,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65184,65218,65521,65521,65521,65521,65487,65285,65184,65184,65184,65184,65184,65184,65184,65184,
65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65251,65521,65521,
65521,65486,65217,65184,65184,65184,65184,65184,65184,65184,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65184,65218,65521,65521,65521,65217,65218,65464,65219,65360,65394,65184,65465,65288,65184,65184,
65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65487,65521,
65352,65184,65394,65255,65358,65394,65324,65219,65220,65464,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65217,65318,65184,65184,65394,65254,65358,65394,65290,65219,65219,65395,65184,65184,
65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65184,
65184,65184,65290,65358,65290,65394,65359,65218,65325,65288,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65184,65184,65184,65184,65499,65220,65359,65430,65185,65534,65217,65184,65184,
65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,
65184,65184,65184,65464,65218,65323,65359,65218,65534,65184,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65184,65184,63008,60968,65394,65395,65428,65395,63010,63040,65184,65184,
65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,
65184,65184,56576,62976,65185,65184,65184,65184,65184,58752,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65152,52288,65120,65184,65184,65120,50112,65120,65184,65184,
65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,
65184,65184,65184,65184,54400,65184,65184,48032,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65184,65184,65184,65184,65184,54464,65184,65152,50144,65184,65184,65184,65184,
65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
65520,65520,65251,65184,54464,65184,65184,48000,65184,65184,65184,65184,65184,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65351,65521,65520,65219,54432,63008,63008,50112,65152,65184,65184,65184,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
56804,52453,63049,54338,52160,56320,56320,50048,54240,52224,52256,56675,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,34168,34133,42320,44269,48395,50474,50474,46251,48428,44368,38227,34168,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
34168,52778,58981,58980,56902,56902,56902,58980,61059,61059,56902,34167,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,34168,34168,36214,40371,42449,42448,40370,34167,32119,34167,32087,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,15346,15346,17458,17458,15346,15346,13265,17491,25845,34168,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,32119,36378,38523,38523,32119,32087,27894,21652,15346,13265,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,15346,15378,19571,17458,21620,25813,30006,30007,32087,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,34200,38523,36410,30039,23733,21620,17459,15378,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,11185,11217,11217,11217,17458,21652,23733,23733,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,13200,11217,11217,11217,11217,13200,11184,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,14921,14954,14954,14921,16904,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_GARAGE[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65535,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,65535,65535,65535,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
65535,65535,65535,65535,65535,63519,63519,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,65535,65535,65535,
65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,
65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,63519,63519,63519,
63519,63519,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,63519,63519,63519,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,63519,
63519,65535,65535,63519,65535,65535,27800,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,
10997,10997,10997,10997,10997,29849,65535,65535,63519,65535,65535,63519,63519,63519,63519,63519,65535,65535,23576,10997,10997,10997,10997,10997,
10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,19415,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,
27801,27801,27801,27801,19415,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,38266,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,38266,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,19415,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,27801,
27801,27801,27801,27801,19415,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,10997,10997,10997,10997,10997,
10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,25720,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,
42459,42459,42459,42459,25720,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,38266,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,38266,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,13110,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,
15190,15190,15190,15190,13110,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,10997,10997,10997,10997,10997,
10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,34073,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,
57118,57118,57118,57118,34073,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,34073,57118,57118,57118,57118,
57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,57118,34073,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,10997,
10997,10997,10997,10997,10997,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,23576,13110,15190,15190,15190,15190,
15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,15190,13110,23576,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,65535,65535,23576,38266,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
65535,65535,65535,65535,38266,23576,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,25656,25720,42459,42459,42459,42459,
42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,42459,25720,25688,65535,65535,63519,63519,63519,63519,
63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65535,65535,65535,65535,65535,65535,65535,
65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_PLUG[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,
52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,
52825,52825,52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,52825,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,52825,52825,52825,52825,63519,63519,63519,63519,25388,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,50744,63519,63519,25388,31727,31695,25388,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,52825,50744,44373,40147,25356,31727,33808,33808,29614,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,52825,52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,44373,40147,25388,25356,31695,33808,33808,31727,
25388,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,52825,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,27469,25356,25356,25356,31727,33808,27469,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,52825,52825,52825,52825,52825,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,25388,25356,19017,
25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,52825,52825,63519,63519,63519,63519,
63519,63519,63519,25356,25356,25356,25356,25356,25356,23275,16936,21162,25356,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,52825,52825,52825,52825,52825,63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,25356,23275,16904,16936,25388,
31695,27469,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,52825,52825,52825,52825,63519,63519,
63519,25356,25356,25356,25356,25356,25356,23243,16904,16904,21162,29582,33808,31727,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,52825,52825,52825,50744,44373,63519,25356,25356,25356,25356,25356,25356,23243,16904,16904,16936,25356,27501,
33808,33808,29582,23275,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,52825,50744,44373,40147,25388,
25356,25356,25356,25356,25356,23243,16904,16904,16904,23275,25356,25356,29614,33808,31695,25356,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,42292,40147,25388,25356,25356,25356,25356,25356,23243,16904,16904,16904,21130,25356,25356,25356,
25356,31727,31727,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,
25356,25356,25356,23275,16904,16904,16904,19017,25356,25356,25356,25356,25356,29614,33808,25356,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,25356,23243,16904,16904,16904,19017,25356,25356,25356,25356,25356,
25356,27501,31727,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,
25356,23275,16904,16904,16904,19049,25356,25356,25356,25356,25356,25356,25356,27469,27469,25356,25356,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,23275,25356,25356,25356,25356,23275,16936,16904,16904,21130,25356,25356,25356,25356,25356,25356,25356,
25356,25356,23275,21162,25356,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,23275,25356,25356,23275,
16904,16904,19017,23243,25356,25356,25356,25356,25356,25356,25356,25356,25356,23275,19017,19017,29614,31727,25388,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,23275,25356,63519,16904,21162,25356,25356,25356,25356,25356,25356,25356,25356,25356,25356,
23275,19017,16904,23243,33808,33808,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,23275,25356,25356,25356,25356,25356,25356,25356,25356,25356,23275,16936,16904,19017,25356,31727,27469,21162,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,25356,25356,23275,16936,
16904,16936,25356,25356,25356,16904,23275,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,25356,25356,25356,25356,25356,19017,16904,19017,25356,25356,25356,16904,19049,23243,16904,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,23275,23275,
25356,25356,23275,16936,19017,21162,16904,21130,25356,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,25356,23275,16936,19049,25356,16936,21162,25356,25356,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,
25356,21162,23243,25356,25356,25356,25356,25356,19049,16936,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,23275,16936,21130,21162,16904,23243,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,16936,19017,23275,23275,23275,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,25356,25356,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_BELL[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,58657,58657,58658,58657,58657,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,58657,58658,58657,58658,58658,52385,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,52385,52385,50306,63519,63519,48161,41889,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,58755,60836,65060,65093,65092,58755,52451,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
65092,65125,65126,65093,65093,65093,65093,65093,65060,62948,65092,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65125,65292,65427,65459,65227,65093,65093,65093,65093,65093,58658,58690,62980,
65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65393,
65459,65459,65459,65326,65093,65093,65093,65093,65093,60803,58658,58658,62947,65093,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65293,65459,65459,65459,65459,65259,65093,65093,65093,65093,65093,62980,58658,58658,
58658,65092,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65126,65459,65459,
65459,65459,65459,65126,65093,65093,65093,65093,65093,65093,58658,58658,58658,60835,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65193,65459,65459,65459,65459,65226,65093,65093,65093,65093,65093,65093,65093,58690,58658,
58658,58690,65093,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65292,65459,65459,
65459,65459,65126,65093,65093,65093,65093,65093,65093,65093,58690,58658,58658,58658,65060,65092,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65092,65326,65459,65459,65459,65326,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,
58658,58658,62915,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65359,65459,65459,
65459,65160,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,58658,58658,60835,65093,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65093,65359,65459,65459,65459,65126,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,
58658,58658,60803,65093,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65359,65459,65459,
65393,65093,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,58658,58658,60770,65093,65092,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,65092,65093,65359,65459,65459,65226,65093,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,
58658,58658,60770,65093,65093,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65093,65360,65459,65459,
65125,65093,65093,65093,65093,65093,65093,65093,65093,65093,58690,58658,58658,58658,58722,65093,65093,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,65092,65093,65394,65459,65426,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,58722,58658,
58658,58658,58690,65093,65093,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65125,65459,65459,65192,
65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,60835,58658,58658,58658,58690,65093,65093,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,65092,65192,65459,65293,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65060,58690,
58658,58658,58658,65060,65093,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65159,65226,65093,65093,
65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,62947,58658,58658,58658,60803,65093,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,65092,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,
65028,60802,58658,58658,65060,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65126,65193,65260,65326,
65359,65359,65326,65292,65226,65159,65093,65093,65093,65093,65093,65093,65093,65093,65092,62980,65028,65093,65092,63519,63519,63519,63519,63519,
63519,63519,63519,63519,65092,65093,65192,65426,65459,65427,65426,65426,65426,65393,65360,65326,65293,65260,65093,65093,65093,65093,65093,65093,
65093,65093,65093,65093,65093,65093,65093,65092,63519,63519,63519,63519,63519,63519,63519,65092,65093,65393,65459,65326,65159,65093,65093,65093,
65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,63519,63519,63519,
63519,63519,63519,65093,65093,65126,65093,54564,37765,25125,20933,20933,20933,20901,20901,20901,33540,48227,50275,37700,18853,20901,20901,20933,
20933,20933,23045,35621,50341,65092,65093,65093,65093,65092,63519,63519,63519,63519,65092,65093,65093,29349,20933,18853,18853,18853,18853,18853,
18853,18853,18853,27237,60837,60803,58658,58658,39811,18853,18853,18853,18853,18853,18853,18853,18853,20901,25157,58789,65093,65092,63519,63519,
63519,63519,65092,65093,65093,33541,18853,18853,18853,18853,18853,18853,18853,18853,18853,23013,63115,65427,63115,58658,27237,18853,18853,18853,
18853,18853,18853,18853,18853,18853,27237,60868,65093,65093,63519,63519,63519,63519,63519,63519,65092,65092,58756,46148,35621,25125,18853,18853,
18853,18853,18853,18853,33541,58924,58856,41891,18853,18853,18853,18853,18853,18853,23013,31461,44036,54564,65092,65092,65092,63519,63519,63519,
63519,63519,63519,63519,63519,63519,65092,65092,65093,65093,62980,56645,50373,46149,41957,39845,37733,37733,37733,37733,39845,41957,46117,50341,
54565,60868,65093,65092,65093,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
65092,65092,65092,65092,65092,65093,65093,65092,65092,65092,65093,65092,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_MUSIC[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,21359,
21359,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,
21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,
21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,
21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,21359,21359,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,21359,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_STAR[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,65223,65222,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,65222,65222,65422,65322,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65421,65488,65190,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65222,65222,65222,65355,65521,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65322,65521,65388,65222,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,65222,65222,65222,65222,65289,65521,65521,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65256,65521,65521,65256,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,65222,65222,65222,65222,65222,65256,65521,65521,65389,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65190,65190,65222,65222,65222,65222,65222,65223,65521,65521,65421,65222,65222,
65190,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65190,65222,65222,65222,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65288,65388,65223,65222,65222,65222,65222,65222,65222,65222,65222,65222,65190,65190,65190,63519,63519,
63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65124,62978,62880,65189,63519,63519,63519,
63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65157,65092,62913,62880,62880,65124,63519,63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,65222,65222,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65190,62978,62880,62880,62880,62913,65190,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
62913,62880,62880,62880,62945,65190,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,62880,62880,62880,65091,65190,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65092,62978,65157,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65190,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65190,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,
65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,
65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,65222,65222,63519,63519,65222,65222,65222,65222,65222,
65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,
65222,65222,65222,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,
65222,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65222,65222,65222,65190,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,65222,65222,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,65190,65222,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,65222,65190,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_MOON[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,64961,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,65092,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65092,64960,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65060,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65026,64960,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65093,65093,64961,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65060,64960,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65093,65093,65093,64961,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65027,64960,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65093,65093,65093,65092,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65093,64993,64928,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65093,65093,65093,65093,64994,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65093,65026,64960,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,65093,65093,65093,65093,65093,65027,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65093,65093,65093,65093,65093,65027,64928,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,65093,65093,65093,65093,65093,65093,65027,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65093,65093,65093,64994,64960,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,65093,65093,65093,65093,65093,65093,65093,64993,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65093,65093,65093,65093,64960,64928,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,
65093,65093,65093,65093,65093,65093,65093,65060,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65092,65093,65093,65093,65093,65093,65093,65093,65093,64994,64960,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,
65093,65093,65093,65093,65093,65093,65092,64960,64960,63519,63519,63519,63519,63519,64960,65092,65092,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,64993,64960,63519,63519,63519,63519,
63519,63519,63519,64961,65093,65093,65093,65060,63519,63519,63519,63519,63519,63519,63519,63519,65092,65093,65093,65093,65093,65093,65093,65093,
65093,65093,65093,65093,65093,65060,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,64993,65093,65093,65093,65093,65093,65093,65093,
65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,64960,64960,63519,63519,63519,63519,63519,
63519,63519,63519,63519,64928,64961,65092,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,
65093,65093,65093,65092,64960,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,64960,64960,65059,65093,65093,65093,65093,
65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65059,64960,64928,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,64960,64993,65060,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,65093,
65092,64994,64960,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,64960,64960,64993,65027,
65092,65093,65093,65093,65093,65093,65093,65093,65093,65093,65060,64994,64960,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,64960,64960,64960,64993,65026,65027,65059,65059,65059,65027,64994,64961,64960,64960,
64928,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,64960,64960,64960,64960,64928,64960,64960,64928,64960,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_CAM[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,38263,38198,38197,38197,38197,63519,21359,21359,21359,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38491,38328,38197,38197,38197,63519,31891,29811,
29811,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,44504,46616,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,
46584,46584,46584,46584,46584,46584,46584,46584,46584,44536,63519,63519,63519,63519,46617,48925,46584,46584,46584,46584,46584,46584,46584,46584,
46584,46584,46584,46584,44536,36085,36084,36084,36084,36084,36084,38230,46584,42423,35953,35953,35953,35953,35953,42358,46779,46747,63519,63519,
63519,63519,46617,48925,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,44504,54936,65502,65469,65435,65369,65369,46583,
46584,40245,25388,10597,10597,10597,23341,38198,46779,46747,63519,63519,63519,63519,46617,48925,46584,46584,46584,46584,46584,46584,46584,46584,
46584,46584,46584,46584,44504,54936,65534,65501,65436,65369,65369,46583,46584,40245,25356,8452,8452,8452,23309,38198,46779,46747,63519,63519,
63519,63519,46617,48925,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,44536,42390,46583,46583,46583,46583,46583,42390,
46584,40245,25356,8452,8452,8452,23309,38198,46779,46747,63519,63519,63519,63519,46617,48925,46584,46584,46584,46584,46584,46584,46584,46584,
46584,46584,46584,46584,46584,46584,44536,42391,42391,44471,46584,46584,46584,42358,33906,31891,31891,31891,33971,40310,46779,46747,63519,63519,
63519,63519,46616,46812,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,46584,40245,31760,25388,25356,25388,25356,27501,35954,
42390,46584,46584,46584,46584,46584,46584,46584,46746,46714,63519,63519,63519,63519,8452,8452,8452,8452,8452,8452,8452,8452,8452,8452,
8452,8452,8484,19049,27469,35953,42325,44405,44373,44405,42292,31760,25356,14823,8452,8452,8452,8452,8452,8452,8452,8452,63519,63519,
63519,63519,8484,8484,8452,8452,8452,8452,8452,8452,8452,8452,8452,8452,23243,31727,46486,8452,0,0,0,0,0,19082,
46518,27469,19017,8452,8452,8452,8452,8452,8484,8484,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,
8452,21162,33840,29614,0,0,2308,8908,11087,6763,97,0,2113,46486,25356,16904,8452,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,12710,25356,35921,0,130,13200,32217,36443,19571,11217,11184,8876,
0,4226,48631,23275,8452,8452,8452,10597,25356,25356,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,
23243,46518,6339,65,13200,36410,38556,36443,15410,11217,11217,11217,8811,0,19017,31727,16936,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8484,25356,40147,0,6698,23830,38523,21684,9006,8973,11086,11185,11217,
11185,2276,2145,50744,25356,8452,8452,10597,25356,25356,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,10565,
25356,21130,0,11054,28023,25878,8973,8908,8908,8908,11087,11217,11217,6730,0,46518,25356,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,12710,29582,12678,0,11152,13265,11184,8908,8908,8908,8908,8973,11217,
11217,8876,0,38034,25356,8452,8452,10597,25356,25356,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,10597,
25388,16936,0,11087,11217,11184,8909,8908,8908,8908,9006,11217,11217,8811,0,44373,25356,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8484,25356,31727,0,8876,11217,11217,11152,8941,8909,8941,11184,11217,
11217,4454,32,52857,25356,8452,8452,10597,25356,25356,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,
23275,52825,2113,2243,11217,11217,11217,11217,11217,11217,11217,11217,11054,0,10565,38066,21130,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,16904,29582,23275,0,4454,11217,11217,11217,11217,11217,11217,11119,
97,0,44405,25356,8452,8452,8452,10597,25356,25356,63519,63519,63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,
8452,23243,42292,16936,0,130,6763,11217,11217,11152,4519,33,0,35921,29647,19049,8452,8452,8452,10597,25356,25356,63519,63519,
63519,63519,25356,25356,12710,8452,8452,8452,8452,8452,8452,8452,8452,10565,23275,40179,31695,32,0,0,32,0,0,6339,
40179,33808,21162,8452,8452,8452,8452,10597,25356,25356,63519,63519,63519,63519,23275,25356,10597,8452,8452,8452,8452,8452,8452,8452,
8452,8452,10597,23243,31727,42260,35921,25356,23275,27501,38066,40179,27501,19049,8484,8452,8452,8452,8452,10565,25356,23275,63519,63519,
63519,63519,29680,29777,29647,29647,29647,29647,29647,29647,29647,29647,29647,29647,29647,29680,33873,27469,25388,31695,33808,29614,25356,29614,
33873,29647,29647,29647,29647,29647,29647,29647,29745,29745,63519,63519,63519,63519,44569,46811,46584,46584,46584,46584,46584,46584,46584,46584,
46584,46584,46584,46584,46584,46584,42358,38099,36019,38132,44471,46584,46584,46584,46584,46584,46584,46584,46584,46584,46714,46714,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_AI[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,36149,38165,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,36149,38197,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,36117,38197,49542,49509,38197,38197,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,38197,38197,45641,47526,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,36117,38197,38197,36149,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38165,38165,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,38165,38165,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38197,38197,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38197,36149,36149,36149,36149,36149,
36149,36149,36149,36149,36149,38197,38197,36149,36149,36149,36149,36149,36149,36149,36149,36149,36149,36149,38197,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38165,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,38165,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,49444,49444,38100,38197,38197,38197,38197,36117,31826,
31793,33939,36117,38197,38197,38197,38197,38197,38197,38197,33939,31793,31858,36117,38197,38197,38197,38197,38197,47657,49444,63519,63519,63519,
63519,63519,49477,49477,49477,38100,38197,38197,38197,36084,27534,50744,61309,48631,29615,36117,38197,38197,38197,38197,38197,27534,44405,59229,
52858,31727,34004,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,27567,61309,65535,
65535,65535,54970,29680,38197,38197,38197,38197,33906,52857,65535,65535,65535,63390,23308,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49444,49477,49477,38100,38197,38197,38197,29582,65535,65535,65535,65535,65535,29647,38197,38197,38197,38197,21195,63422,65535,65535,
65535,65535,40212,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,29582,65535,65535,
65535,65535,65535,29647,38197,38197,38197,38197,21227,63390,65535,65535,65535,65535,38099,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49444,49477,49477,38100,38197,38197,38197,27599,59164,65535,65535,65535,48632,29745,38197,38100,38035,38197,36084,46518,65535,65535,
65535,61277,23308,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,36117,27566,42292,
52857,40147,29680,36117,38197,43756,47624,38197,38197,29713,35953,52825,44405,29615,36085,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,36117,34004,33939,36052,38197,38197,40050,49477,49477,39985,38197,38197,36084,33971,
34004,36117,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,41805,47559,47559,45641,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,38197,40278,57083,42292,59196,27534,57051,48631,38099,57116,16904,57083,50744,42292,
44503,38197,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,63519,63519,49444,49477,49477,38100,38197,38197,38197,38197,38197,63422,
65503,48599,65503,33840,61309,54938,44405,63422,23243,63390,57051,48631,63422,38197,38197,38197,38197,38197,38197,47624,49477,49477,63519,63519,
63519,63519,49477,49477,49477,38100,38197,38197,38197,38197,38197,63390,63422,46518,63422,33808,61277,52857,44373,63390,21162,61309,54970,48599,
63390,38197,38197,38197,38197,38197,38197,47624,49477,49444,63519,63519,63519,63519,63519,49477,49444,38100,38197,38197,38197,38197,38197,40310,
54971,40212,57051,27534,54938,46551,38066,55003,16936,54970,48632,42260,44471,38197,38197,38197,38197,38197,38197,47624,49444,63519,63519,63519,
63519,63519,63519,63519,63519,38165,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,38165,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,36117,38197,38197,38197,38197,38197,
38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,38197,36117,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_LOUD[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,44536,44536,46584,46584,46584,46584,44536,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,44536,44536,44536,46584,63519,63519,63519,44536,44536,46584,46584,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,44536,44536,46584,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,46584,46584,44536,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46584,44536,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46584,46584,44536,46584,46584,44536,46584,
63519,63519,63519,63519,63519,46584,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,46584,46584,44536,63519,63519,63519,63519,44536,46584,44536,63519,63519,63519,63519,46584,44536,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,44536,46584,63519,63519,63519,63519,44536,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46715,46747,46747,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46584,44536,63519,63519,63519,46584,44536,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,46748,46715,36183,36182,44634,46715,46715,63519,63519,63519,63519,46584,44536,46584,46584,46584,
63519,63519,63519,44536,46584,63519,63519,63519,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46748,21294,10664,10664,
10696,12809,38231,46716,63519,63519,63519,63519,63519,63519,63519,46584,46584,63519,63519,63519,46584,46584,63519,63519,46584,46584,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,46748,14922,10664,10664,10664,10664,10664,17067,42457,46748,63519,63519,63519,63519,63519,63519,
46584,46584,63519,63519,46584,46584,63519,63519,44536,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,46748,21294,10664,10664,
10664,10664,10664,10664,12777,31859,46715,63519,63519,63519,63519,63519,63519,46584,63519,63519,63519,46584,63519,63519,63519,46584,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,46715,34037,10664,10664,10664,10664,10664,10664,10664,10664,23407,46748,63519,63519,63519,63519,
63519,44536,44536,63519,63519,46584,63519,63519,63519,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,44700,44699,12744,10664,
10664,10664,10664,10664,10664,10664,10664,19181,46715,63519,63519,63519,63519,44536,44536,63519,63519,46584,63519,63519,63519,46584,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,38360,46845,31924,10664,10664,10664,10664,10664,10664,10664,10664,10664,17068,44635,63519,63519,
63519,44536,44536,63519,63519,46584,63519,63519,63519,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,36248,42554,44732,14890,
10664,10664,10664,12809,10696,10664,10664,10664,10664,21294,44635,63519,63519,44536,63519,63519,63519,46584,63519,63519,63519,44536,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,36247,36248,44765,42586,10696,10696,23439,46845,38360,12809,10664,10664,10664,10664,31924,46747,
63519,63519,63519,63519,46584,46584,63519,63519,44536,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,40473,36215,38328,46845,
34037,34070,36215,44764,46845,42554,10664,10664,10664,19148,21293,40344,63519,63519,63519,63519,46584,63519,63519,63519,46584,46584,63519,63519,
63519,63519,63519,63519,63519,63519,63519,36215,44700,42586,36215,40441,46845,40441,36215,38361,46845,46845,27665,10664,14889,21261,21293,23374,
44635,63519,63519,63519,46584,63519,63519,63519,46584,63519,63519,63519,63519,63519,63519,63519,63519,63519,32054,38361,46845,46845,38328,36215,
42554,46748,40409,36215,38328,40506,19181,10664,21261,21293,21293,21293,31924,46715,63519,63519,63519,63519,63519,44536,46584,63519,63519,63519,
63519,63519,63519,63519,63519,17328,23603,44764,46845,46845,44764,36248,36215,42554,46715,40441,36215,29811,10664,10696,21293,21293,21293,21293,
21326,44602,63519,63519,63519,63519,44536,46584,63519,63519,63519,63519,63519,63519,63519,63519,17328,17361,21554,40539,46845,46845,46845,44764,
38328,36215,42554,46748,40441,14955,10664,12744,21293,21293,21293,21293,21293,34004,46747,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,17328,17329,32022,34135,25780,46845,46845,40506,36215,36215,36215,36215,42522,46715,36118,14889,12744,21293,21293,21293,21293,
21293,27633,46747,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17328,17328,32022,36215,36215,25748,34200,38361,36215,36215,
36215,36215,36215,36215,38263,44635,44602,21294,21293,21293,21293,21293,21293,25519,46748,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,17328,29941,36215,36215,36215,34102,17329,32054,36215,36215,36215,36215,36215,36215,34102,31891,42554,46715,44602,25552,23374,21293,
21293,29778,46748,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17328,36215,36215,36215,34135,17361,17328,17329,34102,36215,
36215,36215,36215,36215,36215,29811,29779,34004,46748,46748,46748,44635,44635,46748,46715,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,17328,25715,36215,34135,17361,17328,17328,17328,17328,25715,36215,36215,36215,34037,29811,29779,63519,63519,63519,63519,46715,46747,
46715,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17329,32022,17361,17328,17328,17328,17328,17328,17328,
19409,25715,29843,29779,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,10924,12971,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17328,17328,17328,17328,17328,17328,17328,17328,
10924,10858,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,17329,17328,17328,17328,17328,17328,13004,10858,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17329,15248,15215,15215,13004,10858,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_MORE[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,23635,17328,63519,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,19442,17328,63519,63519,
63519,63519,36215,36215,36215,38393,40506,40506,42554,42586,40506,40473,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,38296,40506,42586,42586,42586,40506,38361,36248,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40441,42586,42586,42554,38361,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,42554,42586,42554,38328,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40506,42586,38361,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,40506,42586,36248,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40506,42554,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,38296,38296,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36248,36248,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,63423,40408,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,63422,
40441,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,65503,63390,42489,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,65503,
65503,63390,42489,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,40441,65503,65503,65503,65503,65503,63422,42489,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,59261,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,
65503,65503,65503,63422,40409,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,65503,65503,65503,65503,65503,
65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,36248,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,
65503,65503,65503,65503,65503,38296,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,65503,65503,65503,65503,65503,
65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,48763,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,38328,42521,42521,42521,42521,42521,42521,42521,42521,42521,42521,42521,42521,48795,65503,65503,65503,
65503,65503,65503,48795,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,65503,65503,65503,48795,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,65503,
65503,48795,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,65503,48795,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,65503,65503,48795,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,55068,48762,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21522,17328,63519,63519,63519,63519,25715,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,32022,17329,17328,63519,63519,
63519,63519,17328,19442,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,
21554,21554,21554,21554,21554,21554,21554,17329,17328,17328,63519,63519,63519,63519,63519,17328,17328,17328,17328,17328,17328,17328,17328,17328,
17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_BACK[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,23635,17328,63519,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,19442,17328,63519,63519,
63519,63519,36215,36215,36215,38361,40506,42554,42554,42586,42554,40473,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,38328,40506,42586,42586,42586,40506,38360,36247,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40441,42586,42586,42554,38361,36247,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,40474,42586,42554,38328,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40506,42586,38361,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,40506,42586,36248,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,40506,40506,36215,36215,36215,36215,36215,36215,36215,36248,38328,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,38296,36248,36215,36215,36215,36215,36215,36215,
36215,61310,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36248,61310,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,38296,59262,
65503,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,38328,59261,65503,65503,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,38328,59229,65503,65503,
65503,65503,65503,50875,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,38328,59229,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,
65503,65503,65503,65503,63455,38296,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,59229,65503,65503,65503,65503,
65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,38296,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,63422,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,
65503,65503,65503,65503,65503,38296,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,42521,65503,65503,65503,65503,
65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,65503,38296,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,42521,65503,65503,65503,65503,65503,65503,55036,42489,42489,42489,42489,42489,42489,42489,42489,
42489,42489,42489,42489,40408,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,42521,65503,65503,
65503,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,42521,65503,65503,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,42521,
65503,65503,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,42521,63422,65503,46682,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,44569,57149,40408,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,21554,17328,63519,63519,
63519,63519,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,21522,17328,63519,63519,63519,63519,25715,36215,36215,36215,36215,36215,36215,36215,36215,36215,
36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,36215,32022,17329,17328,63519,63519,
63519,63519,17328,19442,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,21554,
21554,21554,21554,21554,21554,21554,21554,17329,17328,17328,63519,63519,63519,63519,63519,17328,17328,17328,17328,17328,17328,17328,17328,17328,
17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,17328,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint16_t IC_GIRL[] PROGMEM = {
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22915,22915,22916,22916,22916,22916,22915,22916,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22915,22915,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22915,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,
22916,22916,22915,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,
22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,22915,22916,22916,22916,22916,22916,22916,22916,22916,22948,22948,22915,22916,22916,22916,
22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,
22916,22915,24996,25028,27076,27108,27108,27108,27108,24996,22948,22916,22916,22916,22916,22916,22916,22915,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,22948,27076,27108,27108,27108,27108,27108,27108,27108,27108,27108,27076,
22948,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,24996,
27108,27108,27108,27108,27108,27108,27141,41964,27108,27108,27108,27108,27108,22948,22916,22916,22916,22916,22915,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,24996,27108,27108,27108,25189,23270,21352,21481,34256,48820,29609,25189,27108,27108,
27108,27108,22948,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22948,27108,25189,
19530,13870,11919,11919,11919,11919,11919,11919,11919,11919,13838,19530,25189,27108,27076,22915,22916,22916,22916,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,22915,22916,22916,23174,17675,11919,11919,11919,11919,11919,13935,22160,22160,13967,11919,11919,11919,11919,
11919,17675,25222,22948,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,19304,13902,11919,11919,
13870,19562,25384,52658,65239,65239,65239,65239,61142,54965,30192,13870,11919,11919,13902,21352,22916,22916,22916,22915,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,22916,19207,11919,11919,13870,21384,27141,41932,63061,65239,65239,65239,65239,65239,65239,65239,65239,61013,
29868,13870,11919,11919,19175,22916,22916,22915,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,20964,13902,11919,13838,27367,44012,
63061,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,50674,15918,11919,13902,20964,22916,22915,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,17417,11919,13934,54836,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,
65239,65239,59094,13935,11919,17385,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,15659,13870,11919,26159,65239,65239,65239,
65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,65239,24079,11919,13870,19271,22916,63519,63519,63519,63519,
63519,63519,63519,63519,63519,11919,11919,11919,30321,65239,65239,65239,65175,37837,35691,63126,65239,65239,65239,65239,63126,35691,37836,65175,
65239,65239,65239,28273,11919,11919,11919,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,11919,11919,11919,32434,65239,65239,65239,
52562,10434,10434,48368,65239,65239,65239,65239,48368,10434,10434,54642,65239,65239,65239,30386,11919,11919,11919,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,11919,11919,11919,30321,65239,65239,65239,37836,10434,10434,35724,65239,65239,65239,65239,33643,10434,10434,42030,
65239,65239,65239,30321,11919,11919,11919,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,11919,11919,11919,28176,65239,65239,65239,
58901,12579,12547,56788,65239,65239,65239,65239,56788,12547,12579,63062,65239,65239,65239,28176,11919,11919,11919,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,11887,11919,11887,38094,65239,65207,65143,65207,63094,61013,65239,65239,65239,65239,65239,65239,61013,63094,65207,
65143,65207,65239,38094,11887,11919,13902,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,17320,21061,39819,65175,64725,64725,
64725,65143,65239,65239,65239,65239,65239,65239,65239,65239,65175,64725,64725,64725,65175,37738,21061,17352,20964,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,22915,22916,22916,25028,60948,64950,64885,64950,65207,49962,54318,65207,65239,65239,65207,54318,49963,65207,64950,
64853,64982,60949,25028,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,31400,65207,65239,
65239,65239,56528,45639,49898,56528,56528,49897,45639,58608,65239,65239,65239,65207,31400,22916,22916,22916,22916,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,22915,22916,22916,22916,22916,44077,65239,65239,65239,65239,60819,49897,45639,45639,49930,60851,65239,65239,65239,
65239,41997,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,22916,22916,50384,
65239,65239,65239,65239,65239,63029,65109,65239,65239,65239,65239,65239,48303,22916,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,22916,22916,22916,22916,22916,22916,22916,44045,63094,65239,65239,65239,65239,65239,65239,65239,65239,63094,44045,
22916,22916,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,22916,22916,22916,
22916,25028,39851,56755,65239,65239,65239,65239,58835,39851,22980,22916,22916,22916,22916,22916,22916,22916,22915,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,25061,31367,31400,25028,22916,22916,22916,22916,
22916,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,22915,22916,22916,22916,22916,22916,
22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,22916,
22916,22916,22916,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,63519,
};
static const uint8_t CLIP0[] PROGMEM = {
122,124,124,124,124,124,126,126,126,126,128,128,128,129,129,129,131,131,133,133,134,136,136,138,136,136,134,133,129,126,122,121,117,117,116,116,117,117,117,119,
121,122,124,126,128,128,129,129,131,131,133,133,134,136,136,140,143,148,150,152,150,148,145,140,133,124,116,107,102,97,93,93,93,97,100,107,112,117,122,128,
131,136,140,141,143,146,148,150,150,152,152,152,153,155,157,157,153,148,143,136,129,119,107,98,90,85,81,81,81,85,90,97,104,112,117,124,129,134,140,143,
146,150,152,153,157,157,157,157,157,157,160,164,164,160,155,148,140,129,117,104,90,80,74,71,71,74,78,85,95,104,112,119,126,131,136,143,146,152,155,157,
160,162,164,164,162,160,160,164,167,167,164,155,145,134,124,110,95,81,69,62,61,62,66,71,80,88,100,110,121,128,134,141,146,153,157,162,165,167,169,170,
170,169,165,165,169,172,172,167,155,143,129,116,100,85,68,57,52,54,57,64,73,83,95,107,117,128,134,140,145,150,155,160,164,165,169,170,172,172,170,167,
167,169,174,176,170,157,141,126,109,93,76,61,50,45,47,52,61,69,81,93,107,119,129,138,143,148,153,158,162,165,169,170,172,174,174,172,169,169,172,181,
182,177,160,140,119,100,81,64,49,38,37,44,56,68,80,90,100,112,122,131,136,140,145,152,158,164,169,172,174,174,174,174,172,167,164,167,176,182,179,164,
141,116,95,80,66,56,47,49,56,68,76,85,92,98,107,116,122,129,134,141,148,153,160,164,167,169,170,170,170,169,167,162,164,172,188,189,177,145,110,81,
66,56,52,50,56,69,83,95,98,97,93,98,107,117,126,133,141,148,157,162,165,167,169,169,169,169,165,164,157,157,169,189,201,188,150,102,71,56,57,59,
66,73,88,98,104,95,85,80,85,98,114,129,138,146,148,152,153,158,162,165,164,165,167,169,167,164,153,162,181,205,198,164,107,66,50,56,62,66,74,88,
105,109,98,80,73,81,100,117,129,140,143,148,148,150,150,155,162,167,169,165,165,162,158,153,146,165,191,218,196,143,80,56,56,64,71,78,95,107,109,86,
69,59,71,88,109,126,138,145,145,141,140,145,153,164,172,176,172,169,164,164,158,157,148,172,193,217,188,138,76,49,45,54,64,74,95,105,107,86,69,61,
71,86,110,131,148,152,148,140,138,145,153,165,172,179,176,169,160,157,152,150,145,174,208,227,181,110,56,44,59,66,78,93,117,114,93,62,50,56,74,95,
121,145,155,150,136,129,134,145,160,174,184,182,176,165,158,157,152,155,146,186,210,222,160,100,56,47,52,52,78,98,122,104,83,56,54,57,74,98,131,153,
158,146,133,131,136,150,164,181,186,186,169,160,152,157,148,153,148,169,208,218,188,107,69,44,57,49,74,98,122,112,83,61,50,56,64,90,126,152,157,146,
138,131,134,141,160,179,189,189,176,165,153,157,150,153,153,155,200,213,205,128,81,52,52,49,57,95,116,117,86,68,54,54,59,81,116,145,160,150,141,131,
133,138,155,172,189,191,181,165,150,148,152,150,153,155,160,205,210,189,107,76,54,54,47,68,110,121,107,76,64,56,50,59,88,131,152,155,148,136,133,128,
143,157,182,189,191,177,164,150,146,152,150,158,152,167,212,208,169,97,80,59,50,47,81,117,117,93,69,64,56,50,66,104,138,150,150,143,138,129,133,145,
165,182,189,186,174,160,148,146,153,150,160,150,174,218,201,158,93,83,56,42,49,90,121,107,86,73,66,47,44,71,105,133,141,150,148,140,129,131,150,169,
181,188,193,181,160,150,153,155,150,157,158,158,208,206,170,105,85,68,42,49,81,116,97,81,68,59,44,49,80,107,128,140,146,141,133,131,141,155,169,186,
189,188,176,164,153,153,155,158,157,160,143,193,213,182,121,80,74,40,40,66,116,117,93,76,66,50,38,61,98,128,140,148,150,138,126,128,145,162,176,189,
194,184,165,152,152,155,155,153,167,150,176,220,194,145,90,85,50,37,59,100,116,92,80,68,54,38,54,86,117,133,141,150,140,128,128,140,155,169,186,191,
188,174,160,150,152,153,160,157,162,145,194,220,179,128,90,80,35,37,76,110,109,93,88,69,45,40,64,93,119,136,150,148,136,126,128,138,153,170,186,193,
184,172,157,146,146,152,160,155,165,145,181,222,182,145,102,92,40,28,62,95,95,92,97,80,52,44,59,78,100,126,150,152,145,136,131,134,146,165,179,193,
193,184,170,157,150,150,155,153,165,148,176,224,188,160,121,100,44,21,54,71,81,90,104,86,61,52,56,64,85,116,143,150,150,148,140,133,140,157,174,184,
193,191,177,160,152,150,155,148,160,157,162,224,196,172,136,109,59,21,44,56,73,83,98,93,74,57,56,56,73,97,122,145,152,153,145,143,143,150,164,182,
189,193,194,177,160,155,152,148,143,160,153,150,218,188,170,150,116,64,23,44,44,54,83,100,95,81,73,59,57,71,92,121,141,155,157,153,146,140,150,160,
167,188,196,188,182,170,155,145,150,145,146,152,157,206,184,181,158,124,69,37,38,23,45,68,92,97,95,92,73,59,64,74,93,119,148,158,162,167,158,153,
158,164,179,181,191,189,177,172,155,153,141,143,146,145,201,182,177,177,128,85,47,37,18,33,57,71,95,97,100,90,76,80,76,93,109,136,145,160,165,165,
164,164,170,170,179,186,182,179,169,162,150,143,140,140,136,140,191,162,179,170,129,92,59,40,8,33,37,59,86,100,107,104,104,88,88,98,104,124,141,157,
164,174,170,176,176,172,182,176,177,176,164,158,150,143,136,136,133,138,182,152,177,170,124,105,69,40,21,32,33,50,83,92,107,116,112,104,105,105,104,126,
128,146,155,162,176,170,181,179,176,179,174,169,162,152,150,136,140,129,136,124,167,158,155,189,131,131,100,62,47,40,32,45,66,78,98,110,116,117,112,116,
107,119,121,128,145,150,160,170,176,174,184,172,176,169,153,162,136,143,138,128,133,134,121,170,143,157,179,128,143,105,81,66,54,44,52,61,71,95,98,117,
121,117,126,117,121,124,124,136,140,150,162,167,170,179,174,169,174,152,153,148,131,138,131,122,140,119,152,155,134,182,131,140,121,93,83,64,68,54,74,80,
86,107,112,110,122,112,117,117,117,128,128,138,150,153,169,172,170,179,167,172,155,158,146,136,140,126,124,126,122,116,141,121,138,140,124,134,112,110,98,93,
81,86,86,86,104,102,114,121,124,128,131,129,133,131,131,138,136,146,145,148,160,148,157,155,145,152,141,140,136,133,129,128,126,126,134,128,141,133,134,131,
119,117,102,102,92,90,92,93,97,107,110,119,124,124,133,129,134,131,136,133,136,140,140,146,145,150,148,150,148,145,143,140,134,134,131,124,131,122,133,141,
131,152,140,141,140,128,124,109,104,93,86,88,86,90,97,102,109,114,117,121,126,124,129,133,133,140,140,146,150,152,157,157,160,155,157,153,148,146,140,138,
134,131,129,133,133,133,140,136,138,134,131,126,119,110,102,95,88,86,81,81,85,85,92,97,102,109,116,121,128,133,138,145,148,153,157,162,164,164,167,164,
162,158,155,150,145,141,134,131,133,128,129,128,128,126,122,121,116,114,109,105,102,100,100,98,100,100,102,105,109,110,114,119,122,126,129,133,136,138,140,143,
143,145,145,145,145,145,145,143,141,141,140,140,138,140,138,138,138,138,136,136,133,131,128,122,119,116,112,109,107,105,104,104,104,105,107,109,110,114,117,121,
124,128,131,134,136,140,141,143,143,145,145,143,143,141,141,140,140,138,138,140,138,141,140,140,138,136,133,128,124,119,114,112,107,105,104,104,104,104,105,107,
110,112,116,119,122,126,129,131,134,138,140,141,143,145,145,145,143,141,141,140,138,138,136,136,145,141,145,143,143,141,134,129,124,117,110,105,105,104,102,102,
104,105,107,107,110,112,116,117,121,122,128,131,133,138,141,143,146,146,150,148,146,146,143,143,138,140,136,134,134,133,146,140,145,141,141,138,124,122,114,112,
105,100,104,98,100,100,104,105,104,107,110,114,119,124,126,133,134,136,143,141,148,148,150,150,148,148,143,143,141,136,136,134,131,133,128,141,146,145,143,140,
138,129,122,116,116,110,105,104,104,100,93,97,97,97,100,104,110,112,114,122,128,133,138,141,148,148,155,153,158,158,157,157,152,150,141,140,138,138,134,131,
133,141,150,148,145,138,131,124,114,110,105,100,100,92,93,88,86,90,86,90,95,100,105,114,119,126,131,134,145,150,155,158,160,169,167,162,164,158,160,153,
150,146,140,136,133,133,129,140,148,153,150,136,134,119,114,107,104,104,98,95,90,86,83,80,83,86,90,97,104,112,116,121,128,136,141,148,155,162,165,167,
167,167,165,164,158,153,146,146,145,140,138,131,134,153,165,167,152,129,117,105,100,97,97,98,90,83,73,69,66,68,73,78,90,98,109,117,121,129,136,148,
157,167,176,181,182,182,181,177,174,169,164,157,148,143,138,133,128,121,119,140,155,157,140,116,104,95,93,90,95,95,92,80,76,73,71,74,80,92,102,112,
121,128,133,136,145,155,162,174,179,181,182,177,176,169,169,160,155,148,140,136,131,126,122,117,128,150,160,155,131,109,95,88,88,93,95,93,85,76,71,68,
69,74,83,97,109,121,128,133,138,145,153,162,174,181,186,184,181,177,172,167,162,155,148,141,136,133,126,122,114,122,153,170,164,124,92,73,76,78,85,90,
92,86,80,69,64,66,74,93,110,126,134,140,141,143,146,157,169,179,188,191,189,181,176,169,164,157,152,143,136,131,128,124,116,110,104,131,158,169,140,98,
76,74,85,88,97,97,98,88,81,71,71,80,95,114,129,140,141,141,140,145,150,162,170,181,184,181,174,167,162,155,150,146,143,136,129,128,124,117,114,107,
136,165,179,145,97,71,74,92,97,100,98,102,95,80,64,62,76,97,117,131,136,138,136,138,140,150,160,176,182,186,179,172,164,158,153,150,146,141,136,131,
128,121,114,107,114,152,177,174,117,74,57,85,104,110,105,102,102,90,73,62,73,92,117,129,138,138,138,134,134,143,160,176,177,176,172,169,162,153,145,141,
145,146,141,129,117,117,114,117,110,129,160,176,148,97,68,80,116,128,119,98,98,105,104,85,68,76,104,134,145,138,129,129,136,140,141,148,158,167,170,167,
155,146,140,140,138,134,128,129,131,133,128,121,117,112,143,174,186,148,98,73,92,124,133,122,102,100,102,95,73,64,78,110,133,134,122,116,117,128,138,148,
158,169,174,172,164,153,150,150,155,148,143,134,133,129,126,117,114,110,134,167,179,146,95,69,83,116,122,117,104,109,109,100,74,66,78,112,141,145,131,121,
126,138,146,146,152,158,167,165,162,152,148,145,143,140,138,133,128,124,126,128,128,122,143,165,176,133,85,64,93,133,145,122,90,81,88,92,83,78,90,117,
140,143,129,119,121,136,150,162,167,174,169,162,148,143,141,146,148,143,134,126,124,126,124,119,112,119,155,182,174,119,73,68,105,133,133,114,105,107,107,88,
69,68,95,129,148,141,121,112,117,136,150,158,157,157,162,167,160,145,131,133,145,150,141,128,122,131,131,128,116,126,164,186,170,105,69,71,119,140,134,104,
93,104,105,88,62,62,92,131,150,141,121,110,117,131,143,152,160,172,181,174,153,133,131,145,157,157,146,138,133,124,117,107,129,167,188,157,98,62,69,109,
126,124,104,104,107,107,83,62,66,98,136,152,146,129,122,121,131,140,155,164,172,169,158,148,141,136,136,140,140,138,134,136,133,128,116,119,153,184,182,129,
81,71,104,128,129,107,97,98,104,90,69,62,85,121,146,148,128,116,114,131,145,155,164,169,172,165,155,143,136,141,146,145,140,136,133,124,116,107,124,162,
184,164,104,62,66,110,138,136,110,97,98,104,93,76,78,107,141,155,140,119,112,126,140,148,150,157,165,165,158,145,136,136,145,146,141,134,131,134,129,126,
116,133,172,196,176,110,66,68,112,136,133,105,92,90,90,73,56,62,95,131,143,131,114,112,122,136,148,157,167,177,179,169,153,148,152,155,153,146,143,143,
140,131,121,119,155,186,191,136,80,57,86,121,129,112,90,86,83,73,54,54,78,116,136,134,119,114,119,131,143,155,167,179,182,176,164,153,153,153,155,150,
150,148,146,134,124,116,138,176,196,164,104,66,73,107,121,114,93,88,88,80,57,45,62,97,124,133,126,119,121,128,136,145,160,176,184,182,172,158,153,153,
155,153,152,150,150,141,131,119,122,162,193,188,128,80,66,95,114,114,98,90,92,83,62,42,50,81,116,128,124,119,121,128,133,140,153,170,182,184,177,167,
160,157,153,152,153,153,153,143,134,122,128,164,191,184,129,83,68,95,114,114,98,90,86,76,56,42,54,81,109,121,122,119,122,126,131,140,155,170,182,182,
179,169,162,157,155,155,152,152,150,145,134,128,121,150,181,191,148,100,74,90,109,107,102,97,97,83,61,42,49,74,100,114,119,122,124,122,122,129,150,170,
184,182,177,170,165,160,155,153,152,157,155,150,138,129,124,152,182,191,150,100,80,97,114,109,97,92,97,85,59,40,49,76,100,110,112,117,122,124,122,129,
148,170,181,179,176,172,169,162,158,155,157,157,158,152,140,133,126,160,191,196,145,98,81,97,107,97,95,97,100,74,45,28,45,73,95,105,114,122,122,117,
117,133,153,174,184,188,182,177,170,165,162,162,162,162,160,152,141,133,122,158,189,189,136,93,83,97,98,86,93,100,97,66,40,33,52,73,90,104,117,124,
122,117,122,136,157,172,184,188,186,181,172,167,162,162,162,164,162,152,140,133,124,160,186,184,133,95,86,97,93,81,93,102,97,61,37,35,56,73,86,102,
119,126,122,117,122,140,158,176,184,189,189,182,174,169,167,165,162,160,157,153,140,133,119,140,179,184,153,102,93,92,95,78,90,102,102,71,44,38,49,62,
74,93,112,124,122,121,124,136,152,169,181,191,193,189,181,174,169,167,164,164,160,158,145,138,122,140,177,179,148,102,97,93,92,74,86,98,95,66,40,40,
49,59,68,90,109,122,121,124,129,141,153,169,186,194,198,193,188,181,174,172,165,162,160,157,145,133,126,124,164,170,158,119,107,100,88,76,80,97,90,73,
50,45,42,44,49,71,93,110,117,126,131,138,148,162,179,193,203,206,203,196,191,182,174,167,165,162,155,141,134,121,141,158,157,131,112,109,90,80,71,88,
85,76,61,56,49,44,49,62,81,98,116,128,133,140,146,155,167,181,193,200,201,196,193,181,176,165,162,155,155,148,138,129,121,146,153,152,131,126,114,97,
85,81,92,81,74,61,59,45,40,40,50,66,81,102,119,131,138,148,157,169,182,196,208,212,212,208,198,188,177,170,164,160,150,145,131,134,146,143,136,121,
121,100,92,80,83,78,69,61,56,52,44,42,44,57,68,85,100,119,131,143,153,165,177,188,200,210,213,213,212,205,196,182,176,167,158,146,141,129,131,134,
129,124,116,112,98,90,83,81,73,68,61,57,50,45,45,45,50,59,73,88,102,119,133,148,160,174,188,200,210,217,224,224,222,218,210,201,191,181,169,158,
145,136,133,124,119,112,107,100,92,86,81,73,66,59,54,50,47,45,45,47,52,61,71,83,97,112,126,141,155,169,181,193,201,208,213,215,215,212,206,200,
193,182,174,164,155,150,141,138,131,126,119,112,107,98,90,80,71,62,54,47,42,38,37,38,44,50,61,74,88,102,119,134,152,167,181,193,201,210,215,218,
218,215,212,205,196,186,176,164,157,148,140,134,128,122,116,110,107,100,93,85,78,71,62,57,52,49,47,47,50,57,66,76,88,102,116,131,145,160,172,184,
194,201,206,210,212,210,205,200,193,182,172,164,157,146,140,133,128,121,116,110,105,98,93,85,80,73,66,61,56,50,50,50,54,59,66,76,88,102,116,131,
146,160,174,186,196,205,210,213,215,212,208,201,193,182,172,164,153,145,138,131,124,119,114,109,104,98,92,86,80,73,68,62,57,52,50,50,52,57,62,71,
81,95,109,122,138,153,167,181,193,205,212,218,220,222,218,213,206,196,186,176,164,152,141,131,122,114,107,100,95,90,86,81,78,74,69,68,64,62,61,61,
64,66,71,78,86,97,109,121,133,146,158,170,181,189,198,201,205,206,205,201,196,189,181,172,164,155,146,138,131,124,117,114,109,104,98,95,90,86,81,76,
71,66,62,59,57,57,59,62,69,76,86,97,110,124,138,152,167,179,191,201,208,215,217,217,215,210,203,194,186,174,164,153,143,133,122,116,107,100,93,88,
81,76,73,68,64,61,59,57,59,59,62,68,73,81,90,100,112,124,136,150,162,174,184,193,200,206,208,210,210,205,201,193,186,176,167,157,146,138,129,121,
114,107,102,97,92,86,83,80,74,71,69,68,66,64,66,68,71,74,81,90,98,109,119,131,143,155,165,177,186,194,200,203,205,205,203,198,191,184,177,167,
158,150,140,131,124,117,110,104,100,95,90,86,83,80,78,74,73,69,69,69,69,71,74,80,85,92,100,110,121,131,143,155,165,176,184,193,198,201,203,203,
200,196,191,184,176,167,157,148,140,131,124,116,110,104,98,95,90,86,83,80,78,76,74,73,73,73,74,78,81,86,93,100,109,117,126,136,146,155,164,172,
179,186,189,191,193,191,189,186,181,176,169,162,155,148,141,134,128,122,117,112,107,102,98,95,90,86,83,80,78,74,74,74,74,76,80,85,90,97,104,114,
122,133,141,152,160,167,176,181,184,188,189,188,186,182,179,174,167,162,155,148,141,136,129,124,119,114,109,104,100,97,92,88,86,83,81,80,78,78,80,81,
85,88,93,98,107,114,122,131,140,148,155,164,170,176,179,182,184,184,182,179,176,172,167,162,157,150,145,140,133,128,122,117,114,109,105,100,97,93,92,88,
86,85,83,83,83,85,86,90,95,98,104,110,117,124,131,138,145,152,157,164,167,170,172,174,176,174,172,170,167,164,158,155,150,145,140,134,129,124,119,114,
109,105,100,97,93,92,88,88,86,86,88,88,92,95,98,104,109,114,121,126,133,138,143,150,153,158,160,164,165,165,165,164,162,160,157,155,152,148,143,140,
136,131,128,124,119,116,112,109,107,104,102,100,98,97,97,97,97,98,100,104,105,109,114,117,122,126,131,134,140,143,146,150,152,153,155,157,157,155,155,153,
152,150,146,145,141,138,136,133,129,126,124,121,117,116,112,110,109,107,107,105,105,107,107,109,110,112,114,117,119,122,126,128,131,133,136,138,140,141,141,141,
143,143,141,141,140,138,136,134,133,131,129,128,126,124,122,122,121,121,119,119,119,121,121,121,122,122,124,124,126,126,128,128,129,129,131,131,131,
};
static const uint8_t CLIP1[] PROGMEM = {
122,124,124,124,124,124,126,126,126,126,128,128,128,128,129,129,131,131,133,133,135,137,137,138,137,137,135,133,129,126,122,120,118,117,115,115,115,117,117,118,
120,122,124,126,128,128,129,129,131,131,133,133,135,137,137,140,144,149,151,153,151,149,146,140,133,124,115,108,100,95,91,89,91,93,98,104,109,117,122,128,
131,137,140,142,144,146,148,151,151,153,153,153,155,157,160,158,157,151,146,138,129,118,108,97,88,82,78,77,77,80,86,93,102,109,117,124,129,135,140,144,
148,151,153,155,157,158,160,158,158,160,164,166,168,164,158,151,142,131,117,102,88,77,69,66,64,66,71,78,89,98,109,118,126,133,138,144,148,151,155,158,
160,164,166,168,166,168,169,173,175,175,169,158,149,137,122,108,91,75,64,57,53,55,58,66,75,86,97,108,118,128,135,140,148,153,158,162,166,168,171,173,
173,171,171,173,177,178,177,169,158,146,131,117,98,80,64,53,48,48,53,60,68,78,91,104,115,124,131,137,144,148,155,158,162,166,169,171,173,175,173,173,
175,178,182,182,175,162,146,129,113,95,77,58,48,42,44,48,57,66,77,89,102,113,122,131,137,144,149,155,160,164,168,171,173,175,177,175,171,173,177,184,
188,184,168,149,128,108,89,71,53,40,37,40,51,62,73,84,95,104,115,122,128,133,138,146,153,160,166,169,173,175,177,177,177,173,173,178,189,191,186,166,
140,117,97,78,64,51,46,48,55,66,73,82,88,95,102,111,118,126,131,138,148,153,160,164,168,169,173,175,175,173,169,171,180,197,200,188,157,120,89,71,
60,51,49,51,64,77,89,93,91,89,93,98,109,118,128,137,146,153,160,164,168,168,169,169,171,169,168,162,158,171,191,208,197,160,109,75,55,55,57,62,
69,82,95,100,95,84,78,80,91,108,122,135,144,151,153,155,157,162,166,168,166,168,169,169,168,158,166,182,209,206,171,113,66,48,49,60,62,73,86,104,
106,97,78,71,77,95,113,128,137,144,146,149,149,151,157,162,168,168,169,168,169,160,157,148,164,193,224,208,151,88,51,53,60,69,73,91,106,109,91,68,
53,62,82,104,124,137,146,146,142,140,142,151,162,171,175,177,173,173,166,164,155,157,184,213,222,175,117,58,46,44,55,62,80,98,106,98,77,62,57,71,
91,115,135,149,153,146,140,138,148,160,171,178,178,177,171,168,157,153,148,155,191,222,226,157,89,38,46,51,68,77,104,120,109,77,48,46,57,80,102,131,
151,157,146,131,128,137,153,168,182,188,186,173,164,160,155,157,149,160,200,224,213,135,80,40,49,44,60,78,111,117,95,68,49,51,58,78,108,140,158,157,
146,131,131,140,155,173,186,193,182,171,157,160,153,157,149,160,209,228,213,122,77,38,51,42,64,89,118,118,88,62,42,48,55,80,113,146,162,153,138,128,
133,140,158,175,193,197,184,169,157,155,151,153,157,153,195,218,217,146,84,55,44,46,48,89,109,118,88,68,53,49,51,71,108,140,158,153,148,133,133,135,
153,171,189,193,186,173,160,157,151,151,158,153,200,226,218,142,82,58,44,42,48,97,118,117,84,62,51,44,46,71,109,146,157,155,142,133,131,133,155,173,
195,193,189,169,158,149,153,151,153,158,155,209,217,198,115,77,58,46,42,60,111,122,104,68,62,53,46,51,91,131,149,149,146,142,133,129,142,164,184,189,
188,180,166,157,151,158,149,160,149,186,226,209,157,89,78,46,37,44,95,118,106,82,71,58,38,40,68,108,129,148,151,149,133,128,137,155,169,184,197,195,
178,160,162,157,157,151,168,151,193,224,200,140,84,80,40,38,55,104,108,88,69,58,46,38,57,88,117,135,146,146,140,135,137,146,164,182,195,193,184,175,
158,157,157,162,157,164,146,202,224,188,122,78,71,33,31,64,115,115,89,71,62,44,33,57,97,128,140,149,149,138,124,128,146,166,180,193,198,186,166,155,
157,160,157,158,166,157,213,220,184,117,82,68,31,38,75,113,102,82,71,55,38,37,64,97,124,137,148,144,137,128,129,148,164,180,193,195,186,168,157,155,
157,160,158,169,151,195,229,191,135,86,80,37,28,64,113,113,86,82,71,38,28,57,91,111,131,153,151,133,126,128,137,151,173,189,195,189,177,158,153,155,
158,155,168,158,177,235,206,158,108,91,48,17,53,91,98,89,91,80,48,35,46,73,93,120,146,153,144,133,131,133,146,160,182,195,195,189,175,160,153,157,
158,157,169,153,198,229,188,157,113,84,29,22,51,73,86,93,95,73,57,42,48,66,95,122,142,155,153,140,133,137,149,160,178,197,198,188,173,164,158,153,
153,158,166,153,218,220,184,149,117,77,15,29,51,66,75,95,97,71,55,48,53,60,86,120,142,149,155,148,144,140,148,164,178,195,197,197,182,171,162,160,
151,158,160,164,229,202,188,155,113,62,18,31,38,55,75,98,89,77,60,49,49,58,91,117,140,155,158,151,146,144,146,164,178,191,195,195,182,166,164,157,
144,157,160,151,220,204,186,168,131,75,22,37,26,31,68,93,88,89,80,64,49,55,71,88,122,146,157,168,166,153,157,158,166,175,195,197,189,188,175,160,
157,144,158,137,173,211,173,197,153,120,58,37,24,9,37,60,75,93,95,86,80,69,71,78,100,117,146,151,166,168,160,169,166,173,177,191,184,184,178,171,
160,153,151,148,135,193,182,175,193,144,108,58,44,8,11,33,44,73,93,98,97,91,82,77,80,100,113,138,157,166,175,175,178,175,178,184,182,186,180,175,
166,160,149,144,148,124,186,169,162,200,135,117,80,44,15,13,22,31,60,88,93,108,111,98,102,98,102,113,129,140,158,166,173,180,182,182,184,186,175,180,
162,166,151,146,146,140,129,175,162,158,197,135,135,95,68,35,35,28,28,62,64,91,102,108,111,106,113,104,117,124,126,148,151,164,171,177,180,184,178,180,
168,166,157,148,149,137,142,135,140,178,142,182,168,131,131,95,57,49,40,22,48,57,64,97,104,108,124,120,117,126,120,126,133,138,151,157,168,177,175,186,
180,171,175,157,153,146,137,135,131,124,138,160,128,180,144,131,146,84,93,69,51,55,58,60,82,91,102,122,113,129,126,122,131,129,131,137,149,148,160,168,
162,173,169,155,162,144,144,137,129,133,128,129,140,149,140,169,144,146,137,117,100,91,78,64,77,69,75,97,97,106,120,117,126,120,129,126,129,135,135,148,
144,158,157,160,168,157,160,155,148,144,138,131,131,129,122,135,133,133,148,131,142,128,122,118,98,104,88,89,91,91,98,102,109,115,117,124,126,122,133,126,
133,137,133,144,142,148,148,153,149,151,146,148,142,137,138,129,133,124,137,131,131,146,128,138,131,124,118,113,108,98,100,98,95,108,106,111,124,118,129,129,
128,135,129,133,131,133,135,133,138,138,137,140,138,138,138,137,137,133,131,131,129,128,133,128,133,135,131,135,129,129,126,122,120,115,115,111,109,111,111,115,
115,118,122,124,128,129,133,135,137,138,138,140,140,138,137,135,133,131,128,126,126,124,124,124,124,126,128,129,131,133,135,135,135,135,133,131,129,126,124,122,
120,118,117,117,117,117,117,118,118,120,122,124,128,129,131,133,135,137,137,138,138,138,138,138,137,135,135,133,131,131,129,128,128,128,128,126,126,124,124,122,
120,120,118,118,118,118,118,120,120,122,122,124,126,128,129,131,133,133,135,135,135,135,135,135,135,133,133,131,129,129,128,126,126,124,124,124,124,126,126,126,
128,128,129,129,129,129,128,128,126,126,124,124,122,122,122,122,122,124,124,126,126,128,129,129,131,131,131,131,131,131,131,131,129,129,128,128,128,126,126,126,
126,126,126,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,129,129,129,129,129,129,131,131,133,133,133,135,133,133,133,131,129,128,126,124,122,122,120,120,118,118,120,120,120,120,122,122,124,126,126,128,128,129,
129,131,131,131,133,133,133,133,133,133,133,133,133,135,135,137,137,137,138,135,135,131,129,126,122,120,117,117,115,113,115,115,115,117,117,118,120,122,124,128,
128,131,133,135,137,137,138,138,138,137,135,135,131,131,131,129,129,129,133,135,137,140,138,138,137,133,129,124,122,117,115,113,111,111,111,113,113,115,117,118,
122,124,126,128,128,129,129,133,135,137,138,140,140,138,138,137,133,131,128,128,128,128,128,133,142,146,153,155,157,153,148,140,128,118,108,98,95,89,91,95,
100,104,108,111,113,115,115,117,115,118,122,128,135,140,149,155,158,162,160,158,155,149,144,138,135,131,129,128,128,129,135,140,148,149,148,142,133,122,109,98,
88,80,78,80,84,93,100,108,117,122,129,131,135,133,133,135,135,138,142,146,151,155,157,158,157,155,149,142,135,129,126,124,124,126,128,138,155,177,193,193,
178,151,118,88,60,40,26,24,35,55,82,108,126,137,140,138,133,126,120,117,118,126,137,151,164,177,186,189,189,184,175,164,151,138,128,120,115,115,117,120,
124,133,151,169,184,182,168,140,108,78,55,37,26,26,37,57,84,109,128,138,142,138,133,128,122,120,122,129,142,157,171,184,189,191,188,180,169,157,144,131,
120,115,113,115,118,124,128,142,158,178,188,177,149,115,82,55,37,26,24,31,51,78,108,129,144,148,144,137,129,124,120,120,124,135,149,164,178,189,193,191,
186,175,164,149,137,126,117,111,111,115,118,124,129,142,158,178,186,175,146,108,73,48,33,28,29,40,60,89,118,140,151,151,144,135,126,120,117,118,126,138,
153,169,182,189,193,189,180,169,155,142,131,120,113,109,111,115,120,124,129,142,158,177,184,171,144,108,75,49,37,31,33,44,64,91,118,140,151,149,142,133,
126,120,118,122,128,140,155,169,182,188,189,184,175,164,151,140,128,120,113,111,113,117,122,126,129,142,158,178,184,169,138,100,69,48,38,37,42,53,73,100,
126,144,149,146,135,126,118,117,117,122,129,142,157,171,184,189,189,182,171,160,149,138,128,120,115,113,117,120,126,128,137,149,168,182,177,153,117,82,57,44,
42,46,55,71,93,117,135,144,140,133,126,118,117,118,122,129,138,151,164,175,180,180,177,169,160,153,142,133,126,120,118,117,120,124,128,128,135,149,168,180,
175,153,117,84,60,49,48,49,57,71,93,117,135,142,138,131,122,118,117,120,124,129,140,151,164,173,177,175,171,164,157,149,140,133,126,120,118,120,124,128,
128,131,144,160,177,175,153,120,88,69,64,66,71,77,84,97,111,124,128,126,118,113,113,117,122,126,131,137,144,155,162,168,169,166,162,157,151,146,140,133,
128,124,124,126,128,128,124,128,137,149,158,151,133,109,93,89,89,93,97,102,109,120,126,124,118,111,108,109,117,128,133,137,135,131,128,129,138,148,153,153,
149,146,144,142,137,133,126,126,133,138,140,137,128,137,146,158,149,126,104,91,98,108,117,117,109,108,106,111,109,106,104,108,115,124,131,135,138,140,142,142,
144,146,148,146,144,140,135,131,129,135,137,138,131,126,124,128,129,126,137,146,155,142,118,100,97,113,124,129,124,118,117,111,109,100,102,109,120,135,137,137,
129,129,129,133,135,131,133,133,142,146,144,137,126,124,124,129,131,137,140,140,133,124,117,135,160,178,171,131,100,82,95,113,128,126,117,111,111,113,102,95,
93,109,129,146,149,138,126,118,120,124,135,146,157,160,153,137,126,120,128,133,135,131,128,128,129,126,124,115,128,149,166,166,131,104,86,98,122,137,137,128,
124,117,111,100,97,102,122,137,144,138,128,117,115,122,137,153,151,146,131,129,131,133,140,135,138,131,131,129,124,129,122,128,124,144,158,155,131,93,84,91,
124,137,135,120,111,113,115,115,106,108,118,137,151,151,138,124,117,128,135,146,146,144,140,137,137,135,135,131,126,122,120,122,128,133,135,128,122,113,133,153,
166,146,106,84,89,126,148,148,122,102,108,118,124,108,98,104,128,148,148,137,118,117,122,133,140,142,146,142,144,140,135,128,124,131,138,144,137,122,118,113,
126,128,135,157,162,166,129,97,78,88,124,148,157,138,115,98,88,95,104,117,129,142,142,135,117,106,102,120,142,157,162,151,142,131,128,129,131,137,142,142,
142,131,124,120,115,124,122,144,162,171,162,109,86,73,108,138,146,137,109,108,108,115,111,102,109,122,140,140,128,113,108,115,133,144,149,146,138,131,129,131,
137,146,146,146,137,133,126,126,126,133,135,140,157,155,149,111,86,78,98,133,146,140,117,102,98,102,108,108,109,120,135,144,144,129,118,115,124,144,153,157,
146,135,131,133,142,138,131,124,124,131,135,137,131,126,124,118,140,151,160,135,102,84,86,122,138,149,129,109,98,97,108,113,124,129,135,137,129,117,113,117,
133,144,148,146,138,138,135,135,129,128,135,144,146,137,120,115,118,131,138,133,146,151,168,148,111,86,82,126,148,160,129,102,95,102,117,109,108,104,128,142,
146,126,108,102,113,135,140,144,133,135,133,137,137,129,131,124,135,133,140,142,142,144,131,126,137,153,169,148,117,91,91,122,135,137,115,100,106,115,120,113,
106,108,115,126,131,128,126,120,124,128,138,146,151,149,144,140,137,137,135,137,137,135,137,137,133,126,117,113,142,162,171,135,89,73,86,133,149,144,115,98,
106,117,120,109,100,111,126,144,140,129,122,122,131,131,137,137,146,148,142,135,124,129,131,137,133,128,131,133,137,135,120,133,148,168,153,115,89,82,120,144,
155,133,115,104,104,108,106,111,120,138,142,138,117,109,111,124,137,137,138,133,135,137,133,131,126,129,133,135,133,129,133,138,140,133,118,135,151,175,157,118,
93,91,128,146,148,124,104,106,109,117,113,109,117,128,140,135,120,108,102,117,129,142,140,135,133,133,137,129,128,126,135,142,144,140,135,129,133,131,148,168,
175,166,122,93,82,104,133,138,129,106,97,97,102,106,98,100,106,122,135,135,124,118,122,135,146,148,151,149,157,155,149,140,138,142,146,144,138,129,122,122,
120,128,146,153,149,117,91,82,93,115,120,124,117,113,113,102,104,102,115,131,137,140,133,128,126,126,135,138,148,144,148,142,138,137,131,138,138,140,137,131,
135,137,140,131,118,140,158,182,158,113,75,68,104,128,140,118,100,93,95,106,106,109,111,120,126,129,128,128,128,129,137,142,148,149,148,144,142,140,142,142,
137,133,133,137,140,137,135,131,158,184,197,168,118,89,86,111,124,126,113,95,84,75,69,66,71,82,98,108,115,117,118,118,126,138,149,162,168,173,171,171,
168,166,164,157,155,151,151,151,146,140,126,142,166,184,169,122,88,68,82,98,109,108,93,82,71,64,60,68,78,97,109,120,124,126,128,131,140,151,166,175,
180,178,175,171,166,162,158,155,151,151,149,144,133,126,148,171,182,160,113,82,66,82,95,104,100,89,77,66,58,57,68,77,95,108,118,124,128,131,135,144,
153,166,175,182,186,180,180,173,168,162,157,158,153,151,146,137,135,151,169,175,151,113,82,69,75,84,93,91,84,69,58,49,51,60,77,91,102,115,124,131,
135,142,151,162,177,184,193,193,191,189,184,177,168,162,162,158,153,146,135,140,157,166,160,128,100,75,68,69,75,84,80,73,58,49,46,49,64,77,95,106,
118,126,133,142,149,164,173,188,195,198,202,197,195,188,180,171,166,160,157,148,140,129,138,148,155,149,122,100,75,66,62,66,75,71,66,53,44,42,46,60,
75,91,106,120,131,142,149,162,175,188,202,209,218,217,211,206,197,191,180,173,164,153,140,129,120,126,131,135,124,106,86,68,62,58,62,66,64,60,55,48,
46,53,66,78,93,108,118,131,144,155,166,178,189,200,209,213,215,213,206,198,189,180,173,164,155,144,131,129,135,138,135,117,100,82,69,64,62,66,62,60,
57,49,46,46,53,66,77,91,106,118,129,140,157,168,182,193,204,213,217,218,213,209,202,193,186,175,164,151,140,129,128,131,133,126,109,95,78,68,64,62,
66,64,64,58,53,51,51,60,69,84,97,109,120,131,142,155,166,178,191,198,206,209,211,208,200,197,188,182,173,162,155,140,137,138,138,137,120,108,93,80,
75,68,69,66,66,60,53,49,48,55,60,73,84,95,108,118,131,142,155,169,180,191,200,208,209,209,206,200,197,188,180,171,162,151,140,135,137,135,131,117,
104,93,82,77,69,71,68,68,64,58,58,55,60,66,77,86,97,108,120,129,140,153,162,177,184,195,200,204,206,202,200,193,188,180,171,164,155,148,146,144,
138,129,117,106,93,86,78,73,71,68,64,60,57,55,57,62,69,78,89,98,111,120,131,144,153,168,177,186,191,197,200,198,198,191,188,182,175,168,158,149,
148,148,146,140,128,117,104,95,88,80,78,73,73,66,60,58,57,58,60,68,78,86,98,108,117,128,138,151,162,173,182,189,197,198,200,198,195,191,186,180,
171,164,157,153,149,146,140,126,118,106,97,88,80,78,75,71,68,64,62,60,62,68,73,82,91,102,111,120,131,138,151,158,169,175,182,189,189,191,189,188,
184,178,175,169,164,160,158,155,149,140,135,124,117,106,98,93,86,82,77,73,68,64,62,62,66,68,73,80,88,97,102,113,122,131,142,151,162,169,177,182,
186,188,188,188,186,182,180,175,173,169,164,160,153,146,137,128,118,109,102,95,88,82,77,71,69,66,66,66,69,71,75,82,86,95,100,109,118,126,137,144,
153,160,166,173,177,180,182,184,184,184,182,182,178,175,171,166,160,153,146,138,131,122,115,108,100,93,88,84,78,77,73,73,73,75,77,80,86,89,95,102,
109,117,122,129,138,144,149,155,160,164,166,169,171,171,173,171,171,169,166,162,160,155,151,146,140,135,129,124,120,115,109,106,102,98,95,93,91,91,91,89,
91,93,95,97,100,104,108,111,117,120,124,129,133,137,140,144,149,151,153,155,155,157,155,155,153,151,149,146,144,140,137,133,131,128,124,122,120,117,115,113,
113,113,111,111,113,113,115,115,117,118,120,120,124,126,126,128,129,131,133,133,135,135,137,137,137,137,135,135,135,133,133,131,131,129,129,128,128,128,126,126,
124,126,124,124,124,124,124,124,124,126,126,124,126,126,128,126,128,128,128,128,128,128,128,128,128,128,129,128,128,128,129,128,128,128,128,128,128,128,126,129,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,129,128,129,128,128,128,128,128,128,128,128,126,128,128,126,128,126,128,128,128,126,128,128,128,128,128,128,128,128,128,129,128,129,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,129,126,128,128,128,128,128,128,128,128,129,
128,129,128,128,128,128,128,128,128,126,128,128,128,128,128,126,128,128,128,128,128,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,126,128,128,126,128,128,128,128,128,128,126,129,126,128,129,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,126,129,128,126,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,129,
128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,129,128,128,128,126,128,128,126,128,128,128,128,128,129,128,128,128,128,128,129,128,128,129,128,126,
129,128,128,128,128,128,128,128,128,129,128,128,126,128,129,128,128,128,129,128,129,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,129,126,129,128,128,128,126,128,128,128,128,128,128,128,128,128,126,128,129,126,128,128,128,128,128,129,128,128,126,128,128,128,128,128,129,128,128,128,129,
128,128,128,126,128,128,128,126,128,128,128,128,128,129,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,126,129,128,129,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,126,129,128,128,128,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,129,126,
128,128,128,128,128,128,126,129,128,128,128,129,126,128,128,128,128,128,128,126,128,128,126,128,128,126,128,128,128,128,128,129,126,128,128,126,129,128,128,126,
129,128,129,128,128,128,128,129,128,128,128,129,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,129,128,126,128,128,128,126,128,128,128,128,128,128,
128,128,126,129,128,128,126,129,128,128,129,128,128,128,128,128,128,128,128,128,126,128,128,128,128,126,129,126,128,128,128,128,126,128,128,128,128,128,128,128,
126,129,128,128,128,126,
};
static const uint8_t CLIP2[] PROGMEM = {
136,136,137,137,137,137,136,136,134,132,131,129,126,124,121,120,118,116,115,113,113,113,115,115,116,118,120,121,123,126,128,129,131,132,134,136,137,139,139,140,
140,140,139,137,136,134,132,131,128,126,124,123,123,121,120,120,120,120,120,120,121,121,121,123,124,124,126,128,128,129,131,131,132,132,134,134,136,137,139,139,
139,140,139,139,137,134,132,131,128,124,123,120,118,116,116,115,115,115,116,116,118,120,121,121,124,126,128,129,131,132,132,134,134,136,137,140,144,145,147,148,
147,142,139,134,129,123,120,116,112,110,110,108,110,110,110,112,112,113,115,118,118,120,124,126,128,132,136,137,140,144,144,145,148,147,152,156,155,158,158,155,
152,148,140,132,124,116,110,104,100,97,96,97,97,99,100,104,105,108,112,115,118,123,128,129,136,140,142,145,148,148,152,152,150,150,158,164,161,164,164,156,
148,140,126,113,104,94,86,84,84,86,89,94,97,100,105,108,108,113,116,120,124,129,136,142,147,152,156,160,163,163,161,161,158,160,168,166,166,168,161,152,
140,126,110,94,81,73,68,68,72,78,84,92,100,105,110,118,120,121,128,132,134,140,148,153,155,161,164,163,163,163,158,155,155,163,169,164,168,166,152,134,
118,97,78,64,52,51,56,64,73,86,97,105,113,120,121,126,131,131,136,145,150,158,168,172,174,177,176,172,168,163,156,150,161,163,155,155,153,136,115,97,
80,62,48,43,48,56,65,81,97,110,120,128,132,134,136,137,140,145,150,156,164,171,172,174,174,171,166,160,153,145,144,155,160,153,152,150,132,108,91,73,
56,43,41,48,59,72,88,102,116,124,129,134,137,136,137,142,147,152,158,166,172,176,176,174,171,166,156,150,145,144,156,163,155,148,148,131,102,80,67,49,
36,38,52,65,80,99,115,123,131,137,137,132,134,139,140,144,153,163,169,171,174,174,171,163,156,148,145,137,140,161,169,156,152,153,132,97,76,65,49,36,
43,59,73,89,105,120,128,131,132,131,128,128,131,136,142,152,161,169,174,176,172,168,163,156,145,140,137,145,168,179,164,152,152,132,88,57,52,44,32,38,
64,84,99,113,124,131,131,129,126,123,124,131,137,147,160,172,179,182,182,179,172,166,155,147,140,139,158,177,169,150,150,142,104,64,51,46,28,24,43,67,
83,97,115,124,129,132,131,128,128,132,139,147,158,171,182,187,187,184,177,169,160,148,140,136,139,160,179,169,147,142,134,94,51,40,40,28,25,46,75,92,
105,118,128,131,132,131,126,128,134,142,150,163,176,187,192,190,185,179,171,160,145,139,136,139,161,176,161,142,142,129,86,48,41,40,25,27,51,76,91,105,
118,126,129,132,132,128,129,137,147,155,166,179,188,190,187,182,177,168,155,145,140,139,155,179,177,150,137,137,107,57,36,40,32,24,40,67,86,99,112,120,
123,129,131,126,124,132,144,152,161,176,188,193,192,188,182,174,164,150,142,137,139,156,179,169,144,136,131,96,52,40,41,32,28,46,72,88,102,113,118,123,
129,129,124,126,137,147,155,166,179,188,190,188,182,176,168,158,147,140,137,142,166,185,171,142,134,124,88,49,41,41,35,35,54,76,91,104,112,115,120,124,
126,123,126,137,147,156,168,180,188,192,188,182,174,168,160,148,144,140,147,171,184,166,140,132,118,81,52,46,44,40,46,64,80,91,100,107,108,112,118,123,
123,128,140,153,163,172,182,185,185,182,174,166,161,155,147,144,142,155,180,188,161,132,123,104,70,48,48,51,54,64,76,88,99,105,104,104,112,121,124,128,
136,147,156,166,174,179,180,179,172,164,160,158,152,144,144,144,161,190,188,152,124,115,89,56,43,49,56,64,76,86,94,100,100,96,97,108,120,126,132,144,
156,164,172,177,179,180,177,169,161,160,156,148,144,144,144,168,196,179,131,112,100,68,40,41,57,73,81,89,97,102,99,94,92,100,118,131,137,145,153,160,
161,164,171,174,176,171,164,158,155,155,148,144,140,147,177,195,164,126,108,92,65,52,56,70,83,86,89,91,91,89,89,91,102,120,132,139,144,148,152,155,
161,168,171,174,172,164,158,155,155,148,144,144,144,171,200,177,128,102,89,68,54,60,76,92,96,92,91,89,86,84,91,105,121,136,142,144,142,145,152,155,
161,169,172,171,164,156,152,152,150,145,145,142,166,198,185,142,112,94,76,68,70,80,94,99,92,84,81,81,84,89,100,118,132,137,139,137,139,144,148,156,
166,171,171,164,156,155,156,155,148,148,147,180,209,184,131,91,73,72,73,78,92,107,102,89,78,70,75,84,97,112,124,137,140,136,131,132,142,156,163,169,
172,169,160,150,148,152,155,150,148,142,164,203,195,145,88,65,72,83,88,94,105,108,94,76,67,73,91,105,113,124,134,139,134,128,131,142,155,161,168,169,
166,160,150,145,148,156,153,148,139,153,196,195,148,88,56,67,89,100,102,107,110,97,75,64,75,100,118,124,128,131,137,136,126,124,136,156,168,168,161,156,
155,147,142,142,148,150,147,142,142,180,195,168,97,52,59,94,118,115,107,104,100,89,76,76,96,120,132,134,132,132,126,123,123,134,150,160,160,155,152,150,
142,136,140,152,158,150,142,136,174,211,200,126,40,27,73,129,139,118,102,97,89,70,67,91,128,147,136,118,115,129,142,136,124,126,145,164,166,156,148,145,
142,139,140,148,152,155,139,156,203,209,150,46,9,51,120,150,126,105,100,97,80,59,73,113,150,152,129,118,124,134,131,124,128,147,166,169,160,148,145,140,
132,134,145,158,155,144,129,164,206,198,121,28,17,68,134,144,120,104,100,97,76,65,86,129,158,148,124,115,126,136,134,126,129,145,163,168,161,148,137,131,
136,145,155,152,147,132,155,201,206,152,48,11,48,116,150,128,110,102,99,81,62,75,115,152,155,131,118,124,136,131,120,124,147,169,172,155,142,139,142,140,
137,144,150,152,145,129,163,206,204,131,27,8,59,134,153,128,107,99,99,73,62,84,131,161,150,124,115,129,137,129,116,126,152,169,168,153,144,142,137,136,
137,152,156,155,136,147,190,204,161,64,20,41,102,139,131,120,107,100,78,60,73,113,150,152,134,120,124,134,129,123,124,144,161,168,158,152,147,139,131,131,
144,156,158,150,129,158,200,200,134,40,20,60,118,137,123,116,112,99,68,56,81,128,156,147,126,120,131,134,126,120,131,153,164,166,155,152,150,142,136,134,
147,155,155,147,137,174,195,184,115,48,40,68,112,120,118,116,110,92,65,62,91,128,145,139,128,131,137,132,120,118,134,158,166,163,155,152,147,137,129,132,
147,155,152,137,137,174,203,195,126,56,36,64,102,115,116,118,115,96,64,56,76,112,129,132,131,134,140,132,123,121,132,152,163,168,166,164,166,156,144,140,
147,155,156,155,158,185,192,177,132,89,73,68,80,88,99,104,97,80,62,57,62,83,100,115,121,128,131,128,126,128,140,153,161,171,176,177,172,161,160,160,
166,166,163,156,168,187,192,177,142,115,92,83,75,78,86,86,81,68,57,54,57,68,78,89,104,110,120,121,129,131,139,152,161,172,177,184,184,179,176,172,
174,171,171,164,172,185,187,180,155,139,113,94,86,76,81,76,76,70,60,57,54,59,65,76,88,99,107,115,121,126,134,139,153,161,172,179,184,184,180,180,
180,179,174,174,172,187,182,182,168,153,137,115,104,86,81,73,68,65,59,57,57,59,62,68,75,86,94,104,112,118,129,136,147,156,164,172,177,180,182,180,
179,177,174,172,166,171,176,174,171,160,150,136,123,108,97,88,80,72,67,67,64,65,65,70,75,78,86,94,102,108,115,124,132,142,152,160,168,174,180,185,
187,187,185,184,180,177,177,172,171,166,158,150,139,128,116,104,92,83,75,70,68,65,67,68,72,76,81,91,94,102,108,113,121,128,136,140,148,155,158,163,
164,169,168,168,166,163,168,160,163,155,153,147,140,136,128,123,116,110,105,102,99,97,96,96,96,97,97,99,102,104,107,110,113,118,121,126,131,134,140,142,
147,148,152,152,152,153,150,152,148,147,148,144,144,139,137,136,129,131,123,121,120,115,116,113,112,112,110,112,112,115,115,116,116,121,120,124,124,124,129,126,
132,129,134,134,134,136,136,136,136,134,136,132,134,132,131,132,128,129,126,128,126,124,126,123,124,123,124,124,124,126,124,128,126,128,126,128,128,128,128,129,
128,131,128,129,129,129,129,131,126,131,128,129,128,128,128,128,128,128,128,128,126,128,124,129,124,128,126,126,126,128,126,129,126,129,126,131,124,132,124,131,
126,131,128,129,129,129,128,131,128,131,128,128,129,124,131,123,131,124,128,126,126,126,126,126,126,126,128,126,126,126,126,128,124,129,124,131,124,131,126,131,
128,131,128,132,128,131,129,131,128,132,124,132,124,132,126,129,126,128,126,128,124,129,124,128,126,126,128,124,128,126,128,126,128,128,128,131,126,131,128,129,
128,128,129,128,128,129,128,129,128,128,129,126,129,128,126,129,124,131,123,131,126,128,128,126,128,126,129,126,128,128,128,128,128,128,129,126,131,124,131,124,
131,126,129,129,126,131,124,129,128,126,129,128,126,129,126,128,128,128,128,128,128,129,128,128,129,126,129,128,124,131,124,131,126,128,128,126,128,126,128,124,
129,126,129,128,126,131,126,129,129,128,129,128,128,129,128,129,128,128,128,128,128,126,128,128,126,128,128,128,128,128,128,126,128,126,129,124,131,124,131,124,
131,128,126,131,124,131,126,129,128,129,128,128,128,128,129,124,132,124,131,126,129,128,126,129,124,129,128,126,129,124,129,126,128,128,126,128,128,128,128,128,
128,128,128,128,128,128,128,131,126,136,128,134,131,131,132,128,131,128,126,129,123,128,123,126,124,124,126,123,126,124,124,124,124,126,126,126,128,126,129,128,
128,132,126,132,128,132,129,131,132,128,132,131,136,131,137,131,134,131,129,129,124,126,123,123,124,120,124,123,121,126,121,126,124,124,126,126,124,128,124,129,
126,131,128,131,129,132,129,131,132,129,131,131,131,129,136,132,134,136,131,134,128,131,126,126,126,124,123,124,121,124,123,123,124,124,124,124,126,123,129,123,
129,126,129,129,129,131,129,129,129,131,126,129,126,128,126,126,126,128,126,128,128,128,129,128,128,128,128,128,126,128,126,128,126,129,126,129,128,129,129,131,
131,131,131,129,131,129,131,128,129,126,129,126,128,126,126,126,124,126,124,126,126,126,126,126,126,126,128,128,128,129,128,129,128,129,128,129,129,128,129,128,
128,129,128,129,128,129,128,128,129,128,129,126,129,126,128,126,128,126,128,126,128,126,128,126,126,128,126,129,126,129,128,128,128,126,128,128,126,128,129,126,
129,128,128,128,128,129,126,129,126,129,128,128,128,128,128,128,128,128,128,129,129,129,131,129,131,129,131,129,128,128,126,126,126,124,124,124,126,124,126,124,
126,126,126,128,126,126,128,126,128,128,128,129,128,129,129,128,129,129,129,131,129,132,132,134,136,136,139,137,137,137,136,134,131,129,126,123,120,118,115,113,
110,110,110,110,110,112,115,116,120,121,124,128,131,132,136,137,139,140,140,142,142,140,140,140,140,139,139,137,137,136,134,132,129,128,126,124,121,121,118,118,
116,116,115,115,116,116,118,118,120,121,123,124,124,128,129,131,132,132,134,136,136,136,136,136,136,136,137,137,137,137,137,137,137,137,134,134,131,129,126,124,
123,120,118,116,115,115,115,113,115,115,116,118,120,121,123,126,126,129,131,131,132,134,134,136,136,136,136,136,137,136,137,137,137,136,137,134,134,132,131,128,
126,124,123,121,120,120,116,118,116,116,116,118,120,120,121,121,124,124,128,128,129,131,132,132,132,134,134,134,134,134,136,136,137,137,137,136,136,134,132,129,
129,126,124,123,121,121,121,120,120,120,120,120,120,121,121,123,124,124,126,129,128,132,129,132,129,132,132,131,134,131,134,131,140,136,139,139,137,136,132,132,
128,126,124,121,120,118,118,118,118,120,120,121,121,123,124,123,128,126,128,131,128,131,129,132,131,129,131,129,131,128,131,129,131,131,136,137,140,142,142,144,
139,139,132,132,126,124,120,118,116,113,115,113,115,113,116,115,115,116,118,118,120,121,124,126,129,131,134,136,137,137,139,140,139,140,139,144,147,148,153,153,
156,152,148,140,132,121,113,105,99,97,92,96,94,100,104,105,112,112,118,116,121,126,124,132,132,137,139,142,145,145,147,145,144,144,140,140,140,140,152,152,
160,161,161,156,145,137,121,108,96,89,81,81,83,88,92,100,107,110,115,120,121,123,126,129,132,136,140,145,148,152,153,155,153,150,148,145,142,142,140,148,
160,164,172,171,168,152,136,113,92,73,59,52,52,60,68,80,91,102,110,115,121,123,126,129,132,139,142,150,158,161,168,168,171,168,166,163,158,152,150,145,
148,158,160,164,160,156,139,121,102,83,68,54,51,54,59,70,80,92,100,108,115,120,126,129,136,140,144,152,156,160,164,168,171,168,166,163,158,153,147,144,
140,137,152,156,164,164,160,150,129,108,88,70,56,49,51,56,67,80,89,99,107,112,116,120,124,129,134,140,147,152,160,163,168,172,172,172,169,166,161,155,
150,147,142,150,158,163,161,155,147,126,104,84,68,54,49,49,57,70,80,89,99,107,112,115,123,126,132,137,142,152,155,161,166,168,171,171,169,166,161,158,
153,147,144,140,150,158,163,163,155,148,128,107,86,70,57,49,51,59,70,78,89,99,104,108,115,121,126,131,137,140,148,153,158,163,166,169,171,169,168,164,
161,155,150,148,145,158,161,166,161,153,140,118,97,78,65,56,51,57,67,76,86,94,100,105,108,113,118,123,129,134,142,147,153,158,164,168,169,171,166,164,
160,158,150,148,145,152,163,166,171,161,153,136,113,94,76,64,57,56,60,68,78,86,92,99,104,108,113,118,126,132,136,144,150,156,158,163,168,168,168,166,
164,161,158,153,152,148,160,168,171,169,158,148,124,100,81,67,59,54,56,64,73,80,86,92,97,102,107,113,123,128,134,140,145,152,155,161,166,169,171,171,
169,166,164,160,156,152,152,166,171,176,168,158,140,118,94,76,62,54,51,54,62,70,76,84,89,96,102,110,118,124,131,137,144,150,158,161,168,172,174,174,
172,171,166,163,156,153,148,155,168,172,174,163,148,129,105,84,68,57,52,52,57,65,72,80,84,89,96,104,110,120,128,134,142,147,153,160,164,171,174,176,
174,172,169,166,161,158,153,156,171,179,182,171,155,136,110,86,67,54,46,44,51,59,67,75,81,88,96,104,112,121,128,136,142,148,155,161,168,172,176,177,
176,172,169,164,161,158,153,153,169,182,190,180,158,131,104,81,67,56,51,51,57,67,75,76,76,76,83,94,107,116,126,134,140,148,155,161,168,172,177,180,
179,176,171,166,163,158,155,152,168,182,188,182,163,140,118,91,70,51,41,40,44,54,64,70,73,80,88,99,108,118,128,134,142,150,158,164,169,176,179,182,
180,177,171,168,163,160,155,156,174,188,193,180,152,123,96,75,59,48,43,46,54,64,72,73,75,78,88,99,112,121,129,137,144,152,160,166,172,179,182,184,
179,176,171,166,160,158,152,160,179,190,190,172,147,123,97,73,56,43,38,41,52,64,70,73,75,83,92,104,115,124,131,139,147,155,163,168,174,179,182,184,
179,174,169,164,160,156,152,166,184,195,188,164,134,107,83,65,51,41,41,48,59,68,73,75,76,84,96,108,118,128,134,142,150,158,164,171,176,180,184,182,
177,172,168,163,156,153,153,171,185,193,182,158,129,104,80,60,44,40,41,49,62,72,75,75,78,88,99,112,121,129,136,144,152,160,166,171,177,180,184,180,
176,169,166,161,156,152,156,176,192,195,177,148,120,94,72,56,44,40,43,54,65,73,76,75,80,91,104,115,124,131,139,145,153,161,169,174,179,182,182,179,
174,169,164,158,156,150,161,180,193,192,169,139,112,86,67,52,41,41,46,59,70,76,76,78,83,94,107,118,126,132,140,148,156,163,169,174,179,182,180,177,
172,169,163,160,155,158,177,192,196,179,150,121,94,73,56,43,38,43,52,65,73,75,75,80,89,102,113,124,131,137,145,153,161,168,174,179,182,184,179,174,
171,164,161,156,155,172,190,198,185,158,128,99,76,60,46,41,43,51,64,73,75,76,76,84,99,112,121,129,134,142,152,160,166,172,177,180,182,180,176,171,
168,161,158,153,166,185,198,193,168,137,108,83,64,49,41,40,46,59,70,75,75,76,83,94,108,120,128,132,140,148,156,164,171,176,180,182,180,177,172,168,
163,160,153,160,182,195,196,174,142,113,88,68,54,43,41,46,59,70,75,76,75,80,91,105,116,124,131,139,147,155,163,169,174,179,182,182,179,174,169,164,
160,155,156,176,192,198,182,153,123,96,73,57,44,40,43,52,64,73,76,76,78,88,100,113,123,131,136,144,152,160,168,172,177,180,182,179,174,171,166,160,
156,153,169,188,200,188,161,129,102,78,62,48,41,43,51,64,73,78,75,76,84,97,110,121,128,134,142,150,158,166,171,176,179,182,180,177,171,168,161,158,
152,163,184,196,195,171,139,110,84,64,51,41,41,48,59,72,76,76,76,83,94,107,118,126,132,139,147,156,164,171,176,180,182,180,177,171,168,163,160,153,
158,179,193,198,177,147,116,89,70,54,44,41,46,57,68,76,76,76,80,89,104,115,124,131,139,145,153,163,168,174,179,180,180,179,172,169,164,160,155,153,
172,190,198,185,155,123,96,73,59,46,41,43,54,67,76,78,76,78,86,100,113,123,129,136,142,152,160,168,172,177,180,182,179,174,169,166,160,156,153,166,
187,198,192,164,131,102,78,60,48,41,43,51,64,75,78,78,76,84,96,110,121,128,136,140,148,158,164,171,177,180,182,179,176,169,166,161,158,153,160,180,
196,196,174,139,108,83,64,52,43,41,49,60,73,80,80,78,81,91,105,118,126,134,139,145,155,163,171,176,179,182,180,177,172,168,161,160,153,155,176,192,
200,182,147,115,88,67,52,44,41,48,59,72,80,80,78,80,88,102,115,124,132,139,145,153,161,169,174,179,182,182,179,172,166,163,156,155,150,156,177,193,
195,171,134,102,75,60,49,43,44,51,64,76,83,83,80,83,94,107,120,129,134,140,148,156,164,171,176,180,182,180,177,171,166,161,158,152,152,174,192,200,
180,144,108,81,62,51,41,41,48,60,75,83,84,80,80,88,102,116,126,132,139,145,155,164,171,177,180,182,184,179,172,164,160,156,153,150,156,177,193,195,
169,129,96,68,54,46,41,44,54,68,83,88,86,83,83,91,107,120,129,137,142,152,160,168,174,177,180,184,182,176,169,163,158,153,152,145,160,184,196,192,
156,113,81,57,48,41,38,46,59,76,89,91,88,83,84,96,110,123,132,139,147,156,164,172,179,182,184,185,180,174,166,161,158,155,150,145,164,187,198,184,
139,94,64,46,41,38,38,49,67,88,99,96,88,81,84,97,113,126,136,145,155,166,174,179,182,184,184,182,177,171,164,160,155,152,147,147,172,193,196,171,
120,76,51,40,38,35,40,56,78,96,100,94,81,78,84,99,116,131,140,150,160,169,176,180,184,185,185,185,179,171,163,158,153,150,145,142,164,188,195,174,
123,78,51,38,36,35,38,54,76,97,105,97,84,78,83,97,113,129,142,153,163,172,177,180,182,184,184,184,177,171,166,161,155,152,144,145,172,192,193,163,
108,68,46,36,35,35,41,60,84,100,102,91,78,75,84,99,118,132,145,156,166,174,179,180,182,185,185,185,180,171,163,156,152,147,145,137,150,179,190,179,
136,84,52,38,35,35,36,49,75,99,110,105,89,80,81,92,108,126,140,153,164,171,177,179,180,182,182,184,180,174,166,160,155,148,147,139,145,176,192,184,
147,92,57,41,35,35,36,48,70,94,108,104,89,78,78,89,105,123,139,152,163,171,176,179,180,182,182,184,182,177,168,158,153,150,145,142,136,155,182,190,
171,123,72,48,36,35,36,41,57,84,105,112,102,88,78,83,96,113,131,145,156,166,174,177,179,180,180,182,182,179,172,163,155,152,147,144,137,137,166,188,
185,155,100,59,41,35,36,38,48,68,96,112,110,97,83,78,88,104,121,137,150,163,171,174,177,177,179,180,180,180,177,168,158,152,147,142,140,136,134,163,
185,184,155,102,60,43,35,36,41,49,72,97,113,113,99,84,81,88,104,121,137,152,163,169,174,176,177,179,179,180,179,176,168,160,153,148,142,140,134,145,
176,188,177,137,83,51,38,36,38,43,56,81,105,115,108,92,80,81,92,108,128,144,156,166,171,176,176,177,177,179,180,179,176,164,155,147,142,139,137,134,
131,155,182,182,158,107,62,43,36,38,43,52,72,99,116,116,104,88,81,88,102,120,137,152,163,169,172,174,176,176,177,177,179,176,168,158,148,142,140,136,
136,129,139,171,187,172,134,81,49,40,38,41,48,62,88,112,120,112,96,83,83,94,110,128,145,158,166,171,174,174,174,176,176,177,177,172,163,153,145,140,
137,136,134,129,155,182,184,158,108,62,44,38,40,46,54,75,102,118,116,102,88,81,86,100,120,137,153,163,169,172,172,172,174,176,177,177,176,169,158,148,
142,140,136,136,131,137,171,187,172,134,83,51,41,40,43,49,65,91,113,120,110,94,81,81,94,110,128,145,158,166,171,171,171,172,174,176,177,177,172,164,
153,144,137,137,134,134,129,137,171,187,169,131,81,51,41,41,44,54,70,94,116,120,108,92,83,83,94,112,131,147,160,166,169,169,171,171,172,174,176,176,
172,163,152,140,134,134,134,132,131,128,155,184,177,145,100,60,46,44,46,52,67,88,112,121,113,99,86,83,91,105,123,142,155,163,166,168,168,169,171,172,
176,176,174,166,156,144,137,136,136,132,134,128,140,177,185,160,118,76,54,48,44,48,60,81,105,120,115,102,89,83,86,97,115,134,150,161,164,166,166,168,
169,172,174,176,176,171,161,148,139,136,136,134,134,131,132,164,188,172,134,92,62,51,49,48,56,75,96,113,115,102,88,83,84,92,108,128,145,156,161,164,
164,166,168,171,174,176,176,172,166,155,142,134,134,136,132,131,129,134,169,193,168,124,96,73,57,51,46,56,80,100,110,107,96,88,83,81,89,107,128,144,
153,160,164,168,169,174,177,179,182,180,174,166,156,145,139,137,136,131,129,124,134,171,184,150,113,97,76,54,44,49,64,83,99,105,102,97,89,83,83,97,
116,134,144,153,161,164,166,169,172,176,179,179,176,171,163,153,142,134,134,136,128,121,123,128,152,176,158,120,107,99,70,51,60,75,86,97,102,102,99,88,
78,81,92,108,124,136,145,156,160,161,169,176,177,177,180,180,172,163,156,147,139,139,139,134,126,126,124,140,163,155,126,118,113,88,68,70,78,86,92,100,
104,97,88,84,80,83,96,110,118,129,140,147,150,156,161,166,172,176,176,177,177,172,166,158,152,148,145,137,132,131,129,145,156,140,123,121,113,91,83,88,
91,89,89,92,91,88,81,78,81,88,96,102,112,121,128,134,142,152,160,166,174,177,177,179,177,174,168,164,161,155,148,144,139,132,131,140,144,134,124,121,
113,97,89,91,92,89,89,91,89,84,81,80,80,83,89,97,102,108,118,126,132,142,153,163,168,174,182,185,184,185,187,185,177,172,169,161,152,144,140,137,
132,123,116,113,108,99,94,92,91,88,88,88,89,89,89,88,91,96,99,102,108,116,120,124,131,139,144,150,156,160,161,164,168,166,166,166,164,161,156,152,
148,144,139,140,140,134,129,128,123,116,113,113,110,107,105,104,100,97,96,94,91,91,94,94,96,99,104,107,112,118,126,131,137,145,152,155,160,166,168,168,
169,171,168,163,160,156,152,148,148,144,137,132,129,123,116,115,113,108,104,102,100,97,94,94,94,92,92,94,96,97,100,107,110,113,120,128,132,137,144,150,
153,158,161,163,164,166,164,161,160,158,155,150,148,147,142,136,132,129,123,118,118,115,108,107,107,104,100,100,99,97,99,100,100,100,104,105,108,113,118,121,
126,131,136,139,144,148,152,153,155,156,158,156,156,155,153,150,148,148,147,140,137,137,134,126,124,126,120,115,115,113,108,105,105,104,100,100,102,99,100,104,
104,105,108,113,116,120,126,131,134,139,144,147,150,152,155,156,156,156,156,155,152,150,150,148,144,139,139,136,128,126,126,123,116,115,115,112,107,105,105,104,
102,102,104,104,102,105,108,110,112,116,120,123,128,131,134,137,142,144,147,148,150,152,152,152,150,150,150,148,147,144,142,140,137,132,131,129,126,123,120,118,
116,115,112,110,110,108,107,107,110,108,107,110,113,113,115,120,123,123,128,131,132,134,140,142,139,144,147,144,144,147,147,144,144,147,142,137,140,140,132,131,
134,129,123,124,124,120,116,118,116,113,112,113,112,110,112,115,113,113,116,120,120,121,126,128,128,131,134,136,136,139,140,140,140,140,140,139,139,139,139,136,
134,136,132,128,128,129,126,124,124,123,121,121,121,120,120,120,120,120,120,121,121,121,123,124,124,124,128,128,129,129,131,131,132,132,132,134,132,132,
};
static const uint8_t CLIP3[] PROGMEM = {
126,124,126,126,126,124,126,128,128,126,126,128,131,129,129,129,131,129,129,139,134,136,129,131,129,128,128,124,129,126,128,123,123,123,120,124,124,126,126,129,
129,128,129,129,131,131,131,131,131,129,131,126,126,128,132,131,131,126,128,129,136,134,128,124,124,128,124,121,121,124,126,126,123,118,120,124,131,129,126,126,
128,131,128,129,129,129,132,129,129,126,128,134,129,131,126,132,128,131,132,129,139,140,142,134,128,124,128,124,126,124,126,123,118,120,116,115,112,115,123,121,
124,123,129,131,129,128,126,129,129,136,136,142,136,132,126,124,132,136,140,137,131,134,142,140,131,123,121,123,121,118,116,116,121,123,120,112,112,116,124,128,
126,128,128,132,131,129,129,131,132,136,134,136,131,126,124,128,132,136,136,136,132,132,139,144,144,134,121,123,128,131,131,126,124,121,120,115,113,116,116,116,
115,120,123,126,128,124,128,129,131,128,131,139,139,137,137,136,132,131,131,131,137,137,148,169,168,148,112,100,107,121,126,124,124,120,113,96,89,91,107,118,
123,123,118,120,121,124,126,134,142,147,150,147,147,142,140,137,137,142,147,153,145,140,128,142,182,192,168,99,75,83,113,123,116,113,105,97,78,70,76,96,
113,120,116,115,118,121,124,128,140,153,160,160,156,155,153,152,150,150,155,158,153,142,136,128,158,190,190,140,80,62,80,112,118,118,107,97,80,65,62,80,
107,123,123,121,121,123,124,126,139,150,161,164,168,163,160,152,150,150,150,153,152,152,140,139,124,150,182,193,145,72,46,67,116,129,121,97,91,84,70,59,
72,105,129,132,118,116,123,126,126,131,147,161,169,166,163,158,150,148,147,147,153,156,160,145,134,121,148,192,203,153,70,38,64,121,137,120,94,84,83,73,
67,73,99,121,128,120,116,120,123,128,136,148,161,168,168,166,161,155,152,148,150,153,158,156,145,137,121,140,187,208,158,60,17,51,129,150,123,83,75,83,
76,62,67,102,134,134,115,102,116,132,140,137,142,161,177,180,164,155,155,161,161,152,150,152,161,150,139,120,137,187,206,160,60,16,44,121,144,118,81,76,
86,73,56,57,99,137,142,115,99,113,134,144,139,145,164,184,180,164,155,160,169,166,155,145,148,156,155,139,126,118,161,196,185,100,27,25,88,139,128,94,
75,88,84,65,51,81,126,144,126,104,110,131,144,139,139,156,179,185,174,163,160,163,163,155,152,156,161,155,139,128,118,160,196,193,108,30,17,78,136,129,
97,73,84,83,65,48,73,121,147,129,102,105,129,147,140,140,155,179,187,176,163,160,166,164,160,153,155,156,155,139,128,118,134,182,201,158,56,8,36,118,
142,118,78,75,84,78,57,59,96,137,144,118,102,116,139,147,140,145,168,188,185,168,156,161,169,164,155,148,153,158,152,132,124,118,156,192,190,110,27,11,
70,137,136,100,70,78,81,70,54,75,120,145,134,108,105,128,145,144,142,156,179,188,177,161,156,164,169,161,152,150,153,155,142,128,118,131,180,200,158,57,
11,36,115,140,113,76,73,88,81,57,54,92,139,147,120,100,115,140,148,140,145,164,188,188,172,155,158,168,169,160,147,147,150,152,136,129,116,132,174,195,
153,57,11,36,115,139,118,78,76,84,78,59,62,100,137,142,118,105,120,144,148,139,144,164,190,188,172,156,161,166,166,153,145,150,160,158,136,120,110,155,
196,193,107,24,12,78,139,129,89,67,83,89,70,49,68,120,147,132,104,105,134,152,145,136,152,179,195,182,164,158,164,168,160,152,150,153,153,144,128,121,
116,156,192,188,104,24,12,76,137,129,94,68,83,84,67,51,75,124,145,131,102,107,134,152,147,139,153,177,193,182,168,160,166,168,164,153,150,152,153,144,
128,123,120,161,193,184,94,20,16,83,137,126,91,70,81,81,67,56,81,123,142,126,107,112,136,148,145,144,160,179,185,177,168,168,171,168,160,152,147,150,
153,148,132,123,115,158,196,187,97,20,19,84,134,120,86,70,83,83,64,51,75,118,139,128,107,113,136,148,142,140,158,180,192,182,172,168,169,166,158,152,
153,156,155,145,129,120,116,161,196,184,91,19,19,84,131,116,84,72,88,83,62,44,75,120,144,128,108,115,137,147,137,139,161,188,196,184,166,166,172,172,
163,155,150,153,152,148,131,124,113,139,184,196,140,51,19,49,108,116,97,76,84,83,65,46,59,102,131,129,113,115,132,147,142,139,150,176,192,192,179,172,
169,171,163,158,155,158,155,150,134,128,115,142,188,200,139,49,19,52,107,108,89,73,83,81,57,36,51,94,123,123,113,118,136,145,140,137,153,179,195,193,
185,185,185,177,168,160,158,158,156,153,140,132,121,121,166,192,174,89,35,32,76,97,92,81,80,83,65,46,41,73,108,128,126,124,132,140,142,137,148,168,
190,196,193,185,180,179,171,164,158,156,155,156,148,136,128,113,148,187,193,137,76,57,64,75,64,72,78,83,62,40,28,41,59,81,102,121,137,144,145,145,
147,158,177,195,204,209,209,203,192,180,174,166,160,160,160,152,136,128,116,147,169,169,131,97,86,76,70,57,65,68,68,52,43,36,40,49,65,84,104,123,
136,142,144,150,163,176,187,200,209,211,208,200,193,182,174,168,169,164,153,145,132,140,168,171,152,120,110,89,75,59,62,64,57,51,40,36,30,35,46,64,
83,102,120,134,140,147,156,168,180,192,203,209,212,206,201,193,185,176,169,166,161,150,144,131,142,163,161,147,121,115,94,80,64,67,65,60,51,43,41,35,
38,46,62,81,99,116,129,139,147,155,164,179,190,198,206,209,206,200,193,185,176,169,166,161,152,145,134,148,163,160,147,126,118,96,83,67,68,65,60,52,
46,43,36,40,48,65,80,99,115,128,139,144,155,163,176,185,196,203,204,203,198,192,182,176,169,166,160,152,145,136,153,161,160,144,131,121,99,84,72,72,
65,59,51,46,41,36,40,48,62,78,96,113,126,137,145,153,164,174,185,195,201,203,201,198,192,184,174,171,163,158,148,145,132,142,158,156,148,131,128,105,
91,76,75,72,64,59,52,49,41,43,48,60,73,91,108,121,132,139,150,156,169,179,190,196,200,203,198,195,185,179,172,168,163,155,150,140,148,158,156,147,
134,126,108,94,80,76,70,64,56,52,49,44,44,49,62,73,89,105,121,131,139,148,158,168,174,187,192,196,195,195,188,182,174,169,164,158,152,147,139,140,
155,155,153,139,136,120,104,91,84,78,68,62,57,52,46,44,44,52,62,75,91,107,118,129,140,150,160,169,180,190,196,200,201,198,195,188,180,174,168,161,
153,147,137,142,150,147,142,132,129,112,100,88,84,78,72,65,62,60,56,54,57,65,75,88,100,113,124,132,140,148,158,166,174,182,187,190,192,188,187,180,
176,171,166,160,153,148,147,153,148,148,142,139,129,120,108,99,91,78,68,60,56,48,46,44,46,51,57,70,81,94,107,120,132,145,156,168,177,185,193,198,
201,203,200,196,193,187,180,171,164,156,153,153,148,145,139,136,124,116,107,99,88,76,67,59,52,46,44,44,46,49,57,68,80,92,104,118,131,144,155,168,
177,185,192,198,201,203,203,200,196,190,184,176,168,160,156,152,147,144,137,134,126,118,112,102,91,81,72,64,56,51,48,44,46,51,57,67,76,89,102,115,
128,140,152,163,172,179,187,192,196,196,196,195,192,187,180,174,168,158,155,150,145,142,139,136,131,124,120,112,102,91,81,72,62,54,49,44,43,43,49,56,
65,78,89,105,118,132,147,160,171,180,188,195,200,201,203,201,198,193,187,180,172,164,158,153,147,144,139,134,131,124,121,113,105,96,86,78,68,60,56,51,
48,49,52,59,67,76,89,100,115,128,140,152,163,171,179,185,190,193,193,193,190,187,182,177,171,163,156,153,147,144,142,137,136,131,128,123,115,107,97,89,
80,72,64,59,54,52,52,57,64,72,81,92,105,118,131,144,155,164,172,180,185,188,190,190,188,185,182,176,171,164,158,155,148,147,144,140,139,134,132,128,
121,113,105,96,86,78,68,62,56,52,51,52,57,64,73,83,96,108,121,136,147,158,169,177,185,190,193,195,195,193,188,184,177,171,163,156,152,145,142,139,
136,132,129,126,121,116,108,102,94,86,78,72,65,60,59,59,62,65,72,81,91,102,113,124,137,148,158,168,176,180,185,188,190,188,187,184,179,172,166,160,
155,150,145,142,139,137,134,132,129,124,120,112,105,97,89,80,73,65,59,56,54,56,57,64,70,80,91,104,116,129,142,153,164,176,184,190,196,198,200,200,
196,192,185,179,172,166,158,152,147,142,137,132,129,124,120,115,108,102,96,89,81,75,68,62,59,57,56,57,62,68,76,86,97,108,121,134,147,158,169,179,
187,192,196,198,198,196,193,187,180,174,168,160,153,147,140,136,131,126,121,118,113,107,102,96,91,84,78,73,68,65,62,62,64,67,72,78,86,96,107,118,
131,142,153,164,174,182,188,193,196,196,196,193,188,184,177,169,163,155,148,142,136,129,126,120,115,110,105,100,96,91,84,80,75,72,67,65,64,65,67,72,
78,84,94,104,115,126,139,150,161,172,180,188,195,200,201,201,200,195,190,184,176,168,160,152,144,136,128,123,116,112,107,102,97,94,89,84,81,78,75,72,
70,68,70,72,75,78,84,92,100,110,120,131,142,153,163,172,180,188,193,196,198,198,195,192,185,179,172,163,155,147,139,132,124,118,113,107,102,99,94,91,
88,84,83,80,76,75,75,75,75,76,80,84,89,96,104,112,121,131,142,152,161,169,177,184,188,193,195,195,192,188,184,179,171,164,156,148,140,134,128,121,
115,110,105,102,99,96,92,89,86,84,83,81,80,80,80,81,84,88,91,97,104,110,118,126,136,144,152,160,168,174,179,182,185,185,185,182,180,176,171,164,
160,153,147,140,134,129,124,118,115,110,105,102,97,94,91,88,84,83,80,80,78,80,80,83,86,91,97,104,112,118,128,136,145,153,160,168,174,179,182,184,
185,184,182,179,176,171,166,160,153,147,140,134,129,123,118,112,107,102,99,94,91,88,86,84,83,83,83,84,86,89,92,97,102,108,115,121,128,134,140,147,
153,158,163,166,169,171,171,171,169,168,164,161,158,153,148,144,140,136,131,126,121,118,113,110,107,104,100,99,97,96,94,94,94,94,96,97,99,102,107,110,
115,120,124,129,134,139,144,148,152,155,158,160,160,161,161,160,158,156,155,152,148,145,142,139,136,132,128,124,121,116,113,110,107,105,102,100,99,99,99,99,
99,100,104,105,108,112,116,120,124,129,132,137,140,145,147,150,152,153,153,155,155,153,153,152,150,147,145,144,140,137,136,132,129,126,123,121,118,115,112,110,
108,107,104,104,104,104,104,105,107,108,110,113,116,120,124,128,131,134,137,140,142,144,145,147,147,147,147,145,144,142,139,137,134,132,129,126,124,123,121,120,
118,118,118,118,118,118,120,121,121,
};
static const uint8_t CLIP4[] PROGMEM = {
145,153,165,172,167,157,141,129,122,116,112,107,107,109,112,117,119,119,117,114,110,107,104,102,102,104,107,112,119,126,131,136,140,141,141,141,141,141,141,143,
143,146,155,167,177,177,169,153,134,119,107,97,90,86,86,92,100,110,116,119,117,116,114,112,112,112,116,119,126,134,141,146,152,153,153,150,146,143,141,140,
140,141,141,146,158,172,181,177,164,141,121,104,93,86,81,80,83,92,102,110,116,116,112,109,105,104,105,107,112,119,129,138,148,155,160,162,160,157,152,146,
145,143,143,143,148,162,177,191,188,169,136,104,78,62,54,52,54,64,80,100,117,126,129,124,117,112,110,110,112,117,126,136,150,162,170,176,176,170,164,155,
148,143,140,138,136,138,148,162,179,184,174,150,121,93,74,62,56,56,59,71,88,107,119,126,124,121,116,112,112,114,117,124,134,146,158,167,172,172,170,164,
157,148,143,138,136,136,138,148,164,182,189,177,152,116,85,61,49,45,45,54,68,88,109,124,131,129,122,116,112,110,112,116,124,134,148,162,174,179,181,177,
170,160,152,146,141,138,136,140,152,167,182,182,165,134,102,76,59,52,49,52,62,76,97,114,124,126,122,117,114,114,116,119,126,134,146,157,167,172,174,172,
167,160,152,146,141,138,138,136,140,152,167,181,181,164,131,98,73,57,50,50,56,64,80,98,116,126,128,122,117,112,112,114,119,124,134,145,157,167,174,176,
174,167,160,153,148,143,141,138,140,150,165,181,179,160,129,97,76,64,64,64,68,76,88,102,112,117,117,114,110,112,116,121,126,129,136,145,153,160,165,165,
164,160,155,150,145,143,140,140,138,145,155,170,179,169,146,116,90,73,66,68,69,73,81,95,110,121,124,124,119,117,116,116,116,116,119,128,138,148,155,158,
157,157,153,152,150,146,145,145,145,150,158,165,162,145,122,104,93,93,95,97,100,105,109,109,104,100,102,109,117,122,126,128,131,134,134,136,140,146,150,150,
145,138,136,138,140,143,140,138,131,140,155,165,162,134,110,93,98,109,116,116,110,112,114,121,116,109,102,105,116,126,133,134,133,128,129,131,138,138,136,138,
136,141,141,138,134,129,133,133,136,148,155,160,143,119,98,93,107,121,128,122,119,117,117,114,107,105,114,126,138,138,129,121,117,126,133,143,145,145,140,136,
134,134,136,138,138,134,134,131,136,152,157,153,122,97,90,102,129,136,129,116,109,112,117,114,105,102,114,133,143,141,128,117,116,121,133,138,141,141,141,141,
138,131,128,128,133,141,146,143,153,153,157,133,110,97,102,126,138,140,121,109,98,102,109,112,116,117,122,126,129,124,119,116,117,128,136,143,146,145,140,138,
133,133,133,134,134,138,143,158,169,165,140,104,92,100,129,145,141,122,105,102,104,104,102,104,116,131,140,134,117,102,104,117,138,148,146,138,131,129,133,136,
138,138,136,134,136,133,145,155,162,146,114,97,95,122,141,143,129,117,119,121,114,98,93,107,131,148,143,124,105,107,117,133,138,138,140,136,134,124,117,122,
136,153,155,146,131,141,153,165,145,117,102,109,133,141,134,116,107,112,119,116,107,100,112,124,136,133,121,116,117,131,133,133,124,126,134,143,143,133,128,128,
134,145,140,140,152,158,157,122,100,86,112,138,146,133,105,98,107,121,122,110,104,116,133,150,136,122,107,116,138,150,152,134,129,128,134,133,129,129,133,141,
141,136,138,145,155,148,126,109,104,124,136,140,126,105,105,105,121,116,114,109,112,128,131,134,121,114,119,128,141,138,136,133,138,145,143,140,133,133,140,140,
157,164,160,138,100,92,95,131,148,146,126,98,97,95,112,114,117,121,122,131,126,121,109,105,117,131,148,150,143,138,128,133,134,136,143,136,148,140,155,162,
160,150,104,100,88,126,140,138,122,100,105,102,109,102,102,116,133,146,140,121,107,105,128,148,160,152,136,124,122,133,138,143,140,140,136,133,150,155,165,143,
112,93,95,129,140,146,124,112,112,112,114,93,93,100,126,145,141,128,104,98,105,122,140,148,148,145,138,140,133,133,128,131,141,148,174,169,164,121,97,93,
114,146,145,138,114,110,114,114,110,100,105,117,131,136,128,119,114,116,121,119,126,129,136,134,129,124,124,133,140,145,138,145,164,177,174,134,104,90,117,150,
160,150,116,102,97,105,110,105,109,109,121,121,121,116,117,126,131,136,134,134,138,138,140,136,138,140,143,143,136,150,153,162,138,107,90,90,122,136,145,124,
107,100,97,109,112,122,129,131,129,122,122,121,129,131,136,136,138,138,134,131,126,124,131,134,145,143,152,158,158,152,117,107,100,131,148,148,133,104,104,104,
116,112,105,107,114,129,131,128,119,116,117,124,133,140,145,141,138,133,134,138,136,138,134,148,162,167,150,110,88,83,112,134,143,133,109,102,95,107,110,119,
122,128,134,136,133,121,112,114,131,150,160,153,138,129,129,140,140,136,131,128,141,164,169,150,105,78,81,114,145,145,126,98,97,107,121,124,112,114,119,140,
148,145,129,110,112,121,145,152,150,133,122,121,133,141,140,136,128,141,160,174,167,122,93,81,110,141,152,145,116,104,95,98,98,98,109,128,143,146,134,116,
104,107,124,145,155,150,131,116,114,122,136,143,146,143,155,164,165,146,110,95,102,136,153,146,119,98,98,109,119,114,109,112,119,134,129,124,110,114,128,138,
145,133,128,122,129,133,136,140,134,141,134,155,167,170,155,117,102,97,126,145,148,131,107,95,97,105,107,110,110,117,119,122,119,117,119,119,124,128,138,141,
143,138,138,146,153,155,150,145,172,196,210,169,107,68,71,117,145,146,112,86,74,80,85,80,83,88,109,124,129,122,114,114,126,146,160,167,164,158,155,158,
164,164,162,152,162,189,208,201,138,83,47,73,114,134,128,92,71,61,66,71,76,81,95,109,119,122,121,121,124,136,146,162,167,172,167,164,165,167,169,165,
160,176,198,208,181,122,73,57,85,116,128,110,83,64,59,66,69,76,83,97,110,121,124,124,124,131,143,160,172,174,172,164,165,169,172,172,164,165,186,206,
208,164,107,64,64,90,114,117,100,80,61,54,49,56,66,83,98,105,110,114,117,124,133,143,158,167,176,177,179,182,181,181,176,177,196,215,218,188,138,98,
78,86,97,104,100,80,66,45,38,40,49,68,78,92,100,110,119,126,134,143,155,170,181,188,189,189,191,186,186,181,186,206,212,210,169,131,97,81,85,86,
92,81,68,50,38,30,33,44,59,73,86,98,107,117,126,138,148,165,179,188,196,196,200,198,198,194,189,201,210,217,200,164,129,98,86,78,80,76,68,56,
37,26,23,28,42,56,71,85,98,114,126,136,148,160,174,189,201,208,212,212,206,205,198,200,210,215,213,186,155,122,97,83,73,68,61,50,37,20,9,8,
13,26,40,59,76,92,107,124,141,160,176,194,208,220,227,229,229,224,220,215,218,222,218,203,176,145,117,95,80,69,61,49,37,23,13,8,8,18,28,44,
61,80,95,110,128,143,162,177,194,208,215,222,224,225,224,220,213,218,218,215,196,169,145,117,98,83,71,62,50,37,23,13,9,9,18,28,42,57,74,90,
109,128,145,165,179,194,206,215,224,227,229,225,220,218,224,220,213,189,165,140,116,100,83,74,61,50,37,23,14,9,16,23,35,47,62,78,95,110,128,145,
162,181,193,206,213,218,224,222,224,217,222,222,217,205,181,160,138,119,102,88,74,62,50,35,23,18,14,20,25,37,49,61,80,93,112,128,145,164,177,193,
201,213,218,220,220,218,215,217,217,215,201,181,160,138,119,102,88,78,66,56,42,30,25,20,23,30,38,52,66,81,93,109,124,141,158,174,188,196,206,210,
215,217,215,213,213,217,210,200,182,165,146,126,112,97,85,74,61,50,37,30,25,25,32,38,50,61,74,86,102,117,133,150,164,179,189,198,205,210,212,212,
210,212,215,212,203,186,170,152,133,117,102,92,80,69,57,45,38,32,30,35,38,50,61,73,86,97,112,126,141,157,169,182,189,198,205,206,210,208,212,213,
212,205,189,179,162,146,129,116,104,90,80,68,56,45,38,35,37,40,47,56,64,76,88,100,114,128,143,157,169,179,188,194,198,201,203,203,208,206,205,194,
182,170,153,140,122,112,100,90,80,68,57,50,44,42,44,49,54,61,71,80,92,104,116,129,141,153,165,176,184,189,196,198,200,205,206,206,203,196,189,176,
164,150,138,126,114,100,90,80,68,59,52,50,47,49,52,57,64,71,80,90,100,112,124,136,148,158,167,174,182,186,193,200,201,203,200,196,189,179,170,160,
150,140,128,116,105,95,85,76,69,64,59,57,59,59,62,68,73,80,88,98,109,117,128,138,146,157,165,172,179,186,191,193,194,196,193,189,184,177,170,160,
152,143,133,122,112,104,95,86,80,76,71,69,69,69,71,73,78,83,90,97,105,112,121,128,138,145,153,160,165,170,174,179,179,181,179,177,174,170,164,158,
152,145,138,131,122,117,109,104,98,93,90,86,85,85,85,85,88,90,93,98,102,107,114,119,124,131,136,141,146,150,153,157,158,160,160,158,158,157,153,150,
146,143,140,136,131,128,124,121,117,116,112,110,110,109,109,109,110,112,112,116,117,119,121,124,126,128,129,131,133,134,134,136,136,136,136,136,136,134,134,133,
133,131,129,129,128,128,126,126,124,124,124,124,124,122,124,124,124,126,124,126,126,126,126,128,128,128,128,129,129,129,129,129,129,129,129,128,129,129,128,128,
128,128,128,128,128,128,128,128,126,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,129,128,128,128,128,128,128,128,126,128,
128,128,128,128,128,128,128,128,128,128,126,128,128,126,128,128,128,128,128,128,128,129,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,128,128,128,126,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,126,128,128,128,128,129,128,128,128,128,129,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,
128,126,129,128,128,128,128,128,128,128,128,128,129,126,128,128,128,128,128,129,126,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,129,126,129,128,
128,128,128,128,126,128,128,128,128,128,126,129,128,128,128,128,129,126,128,126,128,128,126,129,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,126,128,129,128,128,128,126,128,126,128,128,128,128,128,128,128,126,129,126,128,129,126,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,
128,128,128,128,128,128,129,128,129,126,128,128,128,126,128,128,128,128,128,129,128,128,128,128,129,126,129,128,126,128,126,128,128,126,128,128,129,128,128,128,
129,128,128,128,128,128,128,128,129,126,128,128,128,128,126,128,128,128,128,128,126,128,129,128,128,128,128,128,128,126,128,128,128,128,128,126,128,128,128,128,
128,129,126,128,128,129,126,129,128,128,128,128,128,128,128,128,128,126,128,126,128,128,128,128,128,129,126,128,128,128,128,128,128,126,128,128,128,126,129,128,
128,128,128,128,128,128,126,128,128,128,126,128,128,128,128,128,128,128,129,128,128,126,128,128,128,126,128,128,128,128,128,128,129,128,128,128,126,128,126,129,
126,129,128,128,128,129,128,128,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,126,128,128,126,129,126,128,128,128,128,126,128,
128,128,126,128,128,128,128,128,129,128,128,128,128,126,128,128,128,128,128,128,128,129,128,128,128,128,126,128,128,
};
static const uint8_t CLIP5[] PROGMEM = {
136,136,136,136,135,133,129,129,128,124,122,124,122,122,120,120,120,120,120,122,124,124,126,126,126,128,129,128,129,133,131,133,133,135,136,135,138,138,149,154,
152,150,145,140,129,122,113,108,108,105,105,105,110,110,108,112,117,117,119,122,124,126,129,133,133,136,138,142,142,142,142,142,143,143,143,147,149,172,186,184,
172,149,133,105,82,69,69,76,83,92,99,103,103,99,98,99,106,113,124,133,140,145,147,149,149,154,158,158,159,158,161,159,152,154,145,166,191,189,177,142,
124,92,62,46,43,60,71,85,96,103,105,96,94,96,103,113,126,140,149,154,156,156,156,159,159,163,165,166,166,166,159,154,147,149,188,195,184,147,117,92,
53,38,36,55,75,90,101,106,101,92,85,85,98,113,131,145,154,154,154,149,150,154,161,168,172,173,170,168,161,154,149,140,177,200,195,165,117,96,55,30,
20,38,73,85,99,106,108,101,82,82,92,108,128,140,156,163,156,150,149,156,159,166,173,177,180,172,168,156,150,143,180,207,196,159,108,87,46,23,22,45,
82,96,106,106,99,87,73,76,94,113,136,156,163,159,147,145,145,152,161,172,180,180,179,173,163,152,149,140,188,210,202,156,99,80,38,20,18,50,89,99,
105,105,99,83,68,75,98,122,143,158,165,159,149,136,142,152,165,175,177,182,179,170,158,145,145,136,196,221,205,147,82,66,29,16,20,60,105,112,112,101,
92,73,59,69,98,128,150,165,168,159,145,138,140,154,168,180,189,193,186,177,159,154,140,161,212,210,191,131,99,60,20,13,25,62,85,96,101,98,82,66,
60,76,103,129,150,163,165,158,145,143,150,161,175,182,186,186,179,170,154,150,142,161,205,209,184,128,96,62,32,22,34,66,87,96,96,94,80,69,62,75,
96,120,145,156,163,159,152,149,156,165,173,182,188,188,184,175,166,163,150,173,205,207,191,147,119,76,45,25,25,48,59,68,75,76,71,60,59,66,80,99,
119,133,145,150,154,158,163,175,184,189,196,200,200,195,186,182,170,184,200,202,191,165,142,106,71,48,34,38,39,46,55,60,64,59,60,64,71,87,103,122,
135,145,154,159,163,172,179,186,189,195,196,195,189,184,180,177,191,195,191,177,163,142,108,83,60,46,38,34,36,43,46,50,50,55,59,66,75,85,99,110,
124,135,149,161,172,184,193,202,209,214,214,212,209,209,212,209,207,198,188,170,147,124,99,76,57,43,32,25,25,23,29,34,41,48,57,69,82,94,106,119,
131,147,158,172,184,195,203,210,216,219,219,221,223,221,218,210,203,191,173,158,136,115,92,71,53,38,23,15,9,8,8,13,20,27,38,50,62,76,90,106,
122,140,156,172,188,202,214,225,233,239,244,244,242,239,232,223,210,195,177,158,136,117,96,78,60,46,36,27,22,20,20,23,29,36,45,55,66,78,92,106,
120,135,149,163,175,188,196,207,214,221,225,226,225,223,216,209,198,186,172,156,140,124,108,94,80,68,57,48,43,38,36,38,39,43,48,57,66,76,87,99,
113,126,138,150,163,173,184,191,198,203,207,209,209,205,200,193,186,177,166,156,143,133,122,112,101,92,83,78,71,68,66,64,64,66,69,75,80,87,94,101,
110,117,126,135,142,149,154,159,163,166,168,170,170,170,168,165,161,158,154,149,143,138,135,129,124,120,117,113,110,108,106,105,105,105,106,106,108,110,112,115,
117,119,122,124,126,128,129,131,133,135,135,136,136,136,136,136,136,135,135,135,133,133,131,131,129,129,129,128,128,128,126,126,126,126,126,126,126,126,126,126,
126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,126,126,126,126,126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,128,129,129,129,129,129,129,129,129,
129,129,128,128,128,128,128,128,128,128,128,126,128,126,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,126,128,126,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,129,128,129,128,128,129,128,129,128,128,128,128,126,128,128,126,128,128,128,128,128,128,126,128,131,128,129,129,129,129,
128,128,128,128,128,126,128,128,126,128,126,128,126,128,126,126,126,126,126,124,126,126,126,128,126,126,129,129,129,129,131,129,133,135,133,131,133,131,129,129,
128,126,126,124,126,124,124,124,124,124,124,124,126,124,124,126,128,126,126,129,129,128,129,129,131,133,133,135,133,135,135,142,142,136,136,136,133,128,126,124,
124,119,119,117,117,117,117,119,117,119,117,119,120,120,122,122,124,128,128,129,133,135,133,136,138,140,138,143,142,154,172,168,166,165,161,147,131,122,113,105,
98,94,94,92,90,89,90,90,92,96,103,108,113,119,124,129,135,140,145,149,152,158,161,163,163,165,165,165,188,189,179,175,172,154,131,117,106,94,83,80,
78,75,75,75,75,75,78,83,89,96,105,112,119,128,135,143,150,158,161,166,170,172,173,170,173,166,186,202,188,180,173,156,128,103,92,82,71,69,69,73,
73,73,75,75,78,83,92,101,110,120,128,136,143,149,158,163,166,168,172,175,175,170,172,166,170,196,191,179,172,161,135,106,89,80,68,60,62,68,71,73,
76,80,82,85,94,101,110,119,128,136,143,150,159,165,170,170,172,175,173,170,168,166,161,186,200,182,168,161,138,99,73,64,55,48,50,60,73,78,82,87,
89,92,98,105,113,124,133,142,149,158,165,168,173,177,175,175,175,170,163,163,158,154,180,189,172,163,156,131,94,69,62,52,43,50,64,75,82,89,96,98,
99,105,112,117,126,135,142,149,158,163,170,173,173,172,173,172,165,161,161,152,168,196,186,165,165,149,106,73,62,53,41,39,57,71,78,87,96,98,99,103,
110,115,120,129,138,145,156,163,168,175,177,175,173,172,168,163,156,154,149,168,189,179,163,159,140,103,73,60,50,41,45,59,73,83,92,99,101,105,108,112,
117,126,131,138,147,156,163,168,173,175,173,170,168,165,158,152,152,150,173,193,179,163,161,138,96,66,57,46,39,43,60,76,85,90,98,101,103,106,112,117,
124,131,142,152,159,170,179,179,180,179,173,166,161,158,150,147,143,161,188,180,161,158,145,103,66,55,48,38,39,59,76,87,96,105,106,106,110,115,117,122,
133,142,149,159,168,173,179,179,175,172,166,161,156,149,149,145,165,191,186,159,152,142,98,57,46,43,32,34,55,76,89,98,106,108,106,110,113,115,120,131,
143,152,163,175,184,188,186,184,177,170,163,156,147,145,143,154,180,189,166,150,143,112,66,43,38,32,32,45,68,85,98,105,108,108,108,113,115,120,129,142,
152,161,172,182,188,188,186,179,172,165,159,150,145,143,147,172,189,173,152,145,126,80,46,39,39,34,39,60,83,94,99,106,106,103,106,112,115,124,138,150,
161,170,180,188,188,184,180,173,165,159,154,147,143,142,152,179,188,165,145,140,112,66,41,39,39,38,48,73,90,99,105,106,105,103,106,112,119,128,140,156,
165,173,182,188,186,182,177,168,161,156,150,145,143,142,159,186,182,150,140,135,94,48,41,48,43,43,62,87,99,103,103,105,103,103,105,112,122,135,147,159,
172,179,182,186,184,177,170,163,158,152,147,142,143,142,150,179,184,156,140,136,103,55,41,46,43,41,60,85,99,103,105,106,106,105,106,113,124,135,149,161,
172,179,184,184,180,175,168,159,154,150,145,140,140,140,158,182,173,143,135,128,85,48,48,55,50,55,80,98,103,105,106,105,99,101,108,113,120,136,152,159,
168,180,182,177,177,175,166,156,154,152,147,142,142,142,143,172,186,159,136,138,112,62,48,57,53,50,68,90,99,101,105,105,99,99,106,113,119,131,145,154,
161,170,175,177,175,173,170,161,154,152,152,143,142,145,147,168,191,170,140,142,122,69,48,57,55,50,66,87,96,99,101,99,92,92,99,106,117,131,143,156,
165,172,175,177,177,173,168,163,156,152,152,147,142,143,142,149,182,191,156,140,142,103,53,48,55,52,53,73,96,103,101,99,99,94,94,105,117,124,136,150,
158,163,170,172,172,173,172,165,163,156,147,145,145,138,138,140,147,175,188,156,138,140,103,57,53,59,53,59,80,96,101,105,106,101,96,96,106,113,120,135,
149,156,161,168,172,173,173,170,166,161,152,149,147,143,138,140,136,143,179,184,150,143,143,99,60,59,59,52,59,76,92,98,99,105,101,94,103,112,113,128,
142,145,154,165,166,170,175,172,170,168,161,154,150,147,143,136,136,136,140,172,182,154,143,140,103,64,60,62,55,62,80,90,96,101,101,98,96,101,112,119,
126,136,149,154,156,165,173,172,170,175,172,163,156,150,143,140,136,133,135,131,152,182,166,145,150,124,80,69,68,57,60,73,85,94,98,101,103,96,98,108,
115,120,129,142,147,150,158,165,168,170,170,170,166,158,150,149,145,143,140,140,136,142,175,182,156,149,140,101,73,68,62,60,69,80,89,94,96,98,92,92,
99,108,117,126,136,143,149,154,161,166,168,172,175,170,163,159,152,145,142,142,138,140,136,143,180,182,156,152,138,96,73,68,59,62,71,76,89,92,89,90,
92,89,98,112,120,128,136,145,150,156,165,170,173,175,175,172,166,159,150,147,147,143,140,142,136,159,186,166,149,147,115,80,73,62,55,62,69,76,82,85,
85,87,85,90,106,117,124,138,147,150,156,165,170,173,179,182,179,173,168,161,150,145,147,142,135,138,133,161,186,170,152,133,99,78,66,57,60,71,78,78,
78,76,75,78,80,92,112,126,136,142,147,150,154,163,170,173,182,184,179,173,166,158,147,142,145,143,136,138,129,142,184,182,154,126,94,75,68,62,66,78,
87,89,83,78,73,75,82,94,110,128,138,143,145,143,149,158,166,173,179,182,180,173,165,156,149,145,147,149,142,142,129,145,188,189,156,113,82,71,71,69,
75,83,89,89,78,68,64,73,85,99,117,131,140,143,142,142,145,156,168,175,179,180,177,166,159,152,147,145,145,147,140,133,124,128,168,188,170,124,83,75,
82,82,82,90,99,103,92,73,64,68,78,94,112,128,143,149,140,129,128,136,150,172,186,188,180,170,161,152,149,145,149,152,150,147,140,133,161,172,158,115,
78,71,85,96,92,89,90,92,85,68,59,66,90,113,126,131,135,138,136,138,145,159,173,180,180,177,172,165,158,152,147,143,142,142,142,135,128,115,138,172,
184,147,87,53,68,101,112,105,90,89,90,76,55,52,73,112,135,140,133,129,133,136,142,150,166,180,189,184,172,161,154,150,147,145,145,145,142,135,124,115,
112,147,180,191,149,92,60,73,99,108,106,98,98,92,73,50,48,73,108,129,133,131,133,131,131,133,147,166,184,189,184,173,163,156,149,147,145,150,152,150,
135,124,112,131,168,191,170,112,75,71,96,105,105,94,92,90,78,59,48,59,87,113,128,129,131,131,133,135,142,158,175,188,186,179,168,163,156,150,147,147,
152,152,143,131,120,120,154,186,188,142,92,71,87,101,105,98,90,90,82,62,43,45,69,101,120,126,124,126,128,131,138,152,172,186,191,184,173,166,165,159,
158,150,152,149,147,136,129,117,122,159,189,186,135,90,71,89,101,105,98,94,90,78,55,39,46,73,99,117,122,126,129,129,131,138,152,173,189,191,184,175,
168,163,158,152,152,154,154,149,136,126,119,147,179,193,158,110,80,82,99,103,99,94,92,82,59,39,39,62,89,108,117,122,126,126,126,129,149,170,189,191,
186,177,172,166,161,158,158,156,152,147,140,128,122,120,150,184,193,156,113,85,89,101,101,99,94,92,78,57,38,38,57,80,99,112,120,124,124,124,133,150,
170,184,191,189,186,177,170,166,163,158,158,154,150,142,131,122,120,158,186,189,142,105,87,96,98,92,96,99,96,68,43,32,43,57,75,94,112,122,124,120,
124,136,156,175,188,198,198,195,186,177,172,168,165,159,158,154,143,133,120,122,156,177,173,136,112,96,92,83,85,92,92,82,60,46,38,41,48,66,85,106,
119,124,128,133,145,158,175,189,200,203,202,196,188,179,172,166,161,158,154,143,135,122,126,154,166,159,131,115,99,92,80,83,89,87,75,59,50,43,43,48,
68,85,105,117,126,131,136,145,159,173,186,196,202,200,196,188,180,173,166,161,156,154,142,136,124,133,152,159,152,131,124,106,96,82,89,85,80,68,59,50,
38,38,41,55,69,89,106,120,131,138,150,161,175,188,202,209,210,209,203,195,184,175,170,163,156,147,142,129,133,145,147,138,126,120,105,94,82,85,78,71,
62,59,50,43,41,45,53,66,83,99,115,128,140,150,163,173,186,198,207,212,212,210,205,195,184,177,168,159,150,143,133,128,133,133,129,120,119,110,99,90,
87,80,71,64,59,55,48,45,45,48,52,62,76,92,106,120,135,149,161,175,188,198,207,214,218,221,218,214,207,200,191,180,170,158,149,145,140,133,126,120,
113,103,94,87,78,68,60,53,50,45,43,43,45,48,55,68,80,94,108,122,136,150,163,175,186,195,202,205,209,209,207,203,196,189,180,172,165,154,150,147,
142,136,133,128,120,113,106,99,89,80,69,62,53,48,43,41,41,43,48,57,68,80,94,108,124,140,154,168,180,191,202,209,212,216,214,212,207,200,191,182,
172,165,158,149,143,136,131,124,115,110,101,92,82,73,64,55,48,43,39,38,39,45,52,60,73,87,101,117,133,149,163,175,188,196,205,210,212,214,210,207,
202,193,184,175,165,156,149,142,136,129,126,120,115,110,103,98,89,82,75,68,60,55,52,50,48,52,57,64,75,85,98,112,128,142,156,170,182,193,200,207,
210,210,209,205,200,191,182,173,161,154,145,138,133,126,120,115,112,108,103,98,90,83,78,71,66,60,55,53,52,53,59,64,73,83,96,110,124,140,154,166,
180,191,200,207,210,212,210,207,202,195,186,175,165,158,147,140,133,126,120,113,110,105,99,94,89,83,76,71,66,60,57,55,55,55,60,66,75,85,98,110,
124,140,154,168,182,193,203,209,214,216,214,210,205,196,188,175,165,154,143,135,128,120,115,110,106,103,99,94,90,85,80,76,71,66,62,59,59,59,60,66,
73,80,90,103,115,129,143,158,170,182,193,202,209,212,214,214,210,205,196,188,177,168,158,147,138,129,122,113,108,103,98,94,90,85,82,76,73,69,66,62,
59,59,60,62,68,73,82,92,105,117,131,145,158,172,182,193,202,207,210,212,210,207,200,193,182,173,163,152,143,135,126,119,113,108,103,99,96,92,89,83,
80,76,73,69,66,64,62,62,64,68,75,82,90,101,113,126,138,152,166,179,189,200,207,212,216,216,214,209,202,193,184,173,163,152,142,131,122,113,106,101,
94,89,85,80,76,73,69,68,64,64,62,64,66,69,73,80,87,96,106,117,129,142,152,165,175,186,193,200,203,205,207,205,200,195,188,179,170,161,152,142,
135,126,119,112,106,101,98,92,89,85,82,78,75,73,69,68,66,66,68,69,73,80,85,94,105,115,128,138,150,163,173,184,193,200,203,207,207,205,202,196,
189,180,172,163,152,143,135,126,119,112,105,99,94,89,85,82,78,75,71,69,68,68,68,68,71,75,80,85,94,101,112,122,133,145,156,166,175,184,191,198,
202,203,203,202,196,191,186,177,168,159,150,142,133,126,119,112,106,101,96,92,87,83,82,78,76,75,73,73,73,75,76,80,85,90,98,105,112,122,131,140,
149,158,166,173,179,184,188,189,189,188,186,182,177,172,165,159,152,145,140,133,128,122,117,112,106,103,98,94,90,87,83,80,78,76,75,75,76,78,80,85,
90,98,105,113,122,131,140,149,158,166,173,179,184,188,188,188,188,184,180,177,170,165,158,152,145,138,133,126,120,113,108,103,99,96,92,89,85,83,82,82,
82,82,83,85,89,92,98,103,108,115,122,129,136,142,149,154,159,165,168,170,172,172,172,172,168,166,163,159,156,150,147,142,138,133,128,124,120,115,112,108,
103,101,98,96,92,90,90,90,90,92,94,96,99,105,108,113,119,124,129,135,140,145,150,154,156,159,161,161,163,161,161,159,158,156,152,149,145,143,138,135,
131,128,124,120,117,113,110,106,103,101,99,98,96,96,96,96,98,99,103,105,110,113,117,122,128,131,136,140,145,149,150,152,154,156,156,154,152,150,149,145,
142,138,135,131,128,124,120,119,117,115,113,112,112,112,112,113,115,117,117,120,122,124,126,128,129,131,131,133,133,133,135,133,135,133,133,
};
static const uint8_t CLIP6[] PROGMEM = {
136,136,136,136,135,133,129,129,128,124,122,124,122,122,120,120,120,120,120,122,124,124,126,126,126,128,129,128,129,133,131,133,133,135,136,135,138,138,149,154,
152,150,145,140,129,122,113,108,108,105,105,105,110,110,108,112,117,117,119,122,124,126,129,133,133,136,138,142,142,142,142,142,143,143,143,147,149,172,186,184,
172,149,133,105,82,69,69,76,83,92,99,103,103,99,98,99,106,113,124,133,140,145,147,149,149,154,158,158,159,158,161,159,152,154,145,166,191,189,177,142,
124,92,62,46,43,60,71,85,96,103,105,96,94,96,103,113,126,140,149,154,156,156,156,159,159,163,165,166,166,166,159,154,147,149,188,195,184,147,117,92,
53,38,36,55,75,90,101,106,101,92,85,85,98,113,131,145,154,154,154,149,150,154,161,168,172,173,170,168,161,154,149,140,177,200,195,165,117,96,55,30,
20,38,73,85,99,106,108,101,82,82,92,108,128,140,156,163,156,150,149,156,159,166,173,177,180,172,168,156,150,143,180,207,196,159,108,87,46,23,22,45,
82,96,106,106,99,87,73,76,94,113,136,156,163,159,147,145,145,152,161,172,180,180,179,173,163,152,149,140,188,210,202,156,99,80,38,20,18,50,89,99,
105,105,99,83,68,75,98,122,143,158,165,159,149,136,142,152,165,175,177,182,179,170,158,145,145,136,196,221,205,147,82,66,29,16,20,60,105,112,112,101,
92,73,59,69,98,128,150,165,168,159,145,138,140,154,168,180,189,193,186,177,159,154,140,161,212,210,191,131,99,60,20,13,25,62,85,96,101,98,82,66,
60,76,103,129,150,163,165,158,145,143,150,161,175,182,186,186,179,170,154,150,142,161,205,209,184,128,96,62,32,22,34,66,87,96,96,94,80,69,62,75,
96,120,145,156,163,159,152,149,156,165,173,182,188,188,184,175,166,163,150,173,205,207,191,147,119,76,45,25,25,48,59,68,75,76,71,60,59,66,80,99,
119,133,145,150,154,158,163,175,184,189,196,200,200,195,186,182,170,184,200,202,191,165,142,106,71,48,34,38,39,46,55,60,64,59,60,64,71,87,103,122,
135,145,154,159,163,172,179,186,189,195,196,195,189,184,180,177,191,195,191,177,163,142,108,83,60,46,38,34,36,43,46,50,50,55,59,66,75,85,99,110,
124,135,149,161,172,184,193,202,209,214,214,212,209,209,212,209,207,198,188,170,147,124,99,76,57,43,32,25,25,23,29,34,41,48,57,69,82,94,106,119,
131,147,158,172,184,195,203,210,216,219,219,221,223,221,218,210,203,191,173,158,136,115,92,71,53,38,23,15,9,8,8,13,20,27,38,50,62,76,90,106,
122,140,156,172,188,202,214,225,233,239,244,244,242,239,232,223,210,195,177,158,136,117,96,78,60,46,36,27,22,20,20,23,29,36,45,55,66,78,92,106,
120,135,149,163,175,188,196,207,214,221,225,226,225,223,216,209,198,186,172,156,140,124,108,94,80,68,57,48,43,38,36,38,39,43,48,57,66,76,87,99,
113,126,138,150,163,173,184,191,198,203,207,209,209,205,200,193,186,177,166,156,143,133,122,112,101,92,83,78,71,68,66,64,64,66,69,75,80,87,94,101,
110,117,126,135,142,149,154,159,163,166,168,170,170,170,168,165,161,158,154,149,143,138,135,129,124,120,117,113,110,108,106,105,105,105,106,106,108,110,112,115,
117,119,122,124,126,128,129,131,133,135,135,136,136,136,136,136,136,135,135,135,133,133,131,131,129,129,129,128,128,128,126,126,126,126,126,126,126,126,126,126,
126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,126,126,126,126,126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,128,129,129,129,129,129,129,129,129,
129,129,128,128,128,128,128,128,128,128,128,126,128,126,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,126,128,126,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,129,128,129,128,128,129,128,129,128,128,128,128,126,128,128,126,128,128,128,128,128,128,126,128,131,128,129,129,129,129,
128,128,128,128,128,126,128,128,126,128,126,128,126,128,126,126,126,126,126,124,126,126,126,128,126,126,129,129,129,129,131,129,133,135,133,131,133,131,129,129,
128,126,126,124,126,124,124,124,124,124,124,124,126,126,124,126,128,128,126,129,129,128,129,129,131,133,133,135,133,135,135,142,142,136,136,136,133,128,126,124,
124,119,119,117,117,117,117,117,117,119,117,119,119,120,122,122,124,128,129,129,133,133,135,136,138,138,140,143,145,165,172,168,163,163,156,140,126,119,112,103,
98,96,94,92,89,89,89,90,94,98,105,110,115,120,126,131,136,142,147,150,154,159,163,165,163,166,163,175,193,188,177,172,166,145,124,113,105,94,85,82,
78,75,73,71,71,71,75,82,89,96,105,113,120,129,136,145,154,159,163,168,173,175,173,173,173,173,198,203,186,175,170,147,117,98,90,80,69,68,69,71,
69,68,69,71,75,83,94,103,113,124,129,138,147,152,161,166,170,173,177,180,179,173,175,170,188,202,188,177,168,152,124,101,89,82,71,66,66,68,68,66,
66,69,73,80,90,101,110,120,129,138,145,152,161,166,170,172,175,179,177,172,173,168,173,205,203,179,168,159,124,87,73,68,59,53,59,66,69,69,71,75,
78,83,94,105,115,126,135,143,150,158,166,172,177,179,179,182,182,175,172,172,165,186,202,182,163,159,136,96,71,66,59,48,48,59,68,69,73,78,82,85,
94,106,117,126,136,147,152,159,166,172,175,177,175,175,177,173,165,165,159,168,200,200,168,152,145,108,60,46,53,46,38,50,69,76,76,82,89,89,94,105,
117,126,136,147,154,161,170,175,179,182,182,180,180,177,170,165,161,158,184,196,172,150,149,120,75,50,52,46,38,46,66,76,78,85,92,94,96,106,117,124,
133,147,156,159,166,175,179,179,177,173,170,168,165,156,154,150,154,188,198,168,143,140,112,60,34,43,46,38,50,75,90,90,94,98,99,101,108,117,126,135,
145,154,163,172,177,182,184,179,175,173,170,159,154,154,152,170,193,184,154,142,128,87,50,41,46,46,48,64,85,94,92,94,99,103,105,112,120,131,140,149,
159,168,173,179,180,177,173,168,163,161,154,147,147,150,177,200,180,147,140,124,73,39,41,48,45,52,73,94,99,98,98,99,101,106,110,117,129,140,149,159,
170,179,182,184,182,175,166,163,161,150,145,147,152,179,198,175,140,135,117,64,30,38,52,48,53,80,105,106,101,101,101,103,108,115,120,133,149,158,163,173,
182,184,179,175,170,161,156,154,147,140,143,143,165,196,188,142,126,124,82,27,25,48,48,46,75,106,112,108,110,105,99,106,113,113,120,142,156,163,173,188,
193,189,184,177,166,158,154,149,140,142,143,156,189,196,154,124,126,98,39,18,41,52,45,62,96,112,108,106,105,99,103,110,113,119,135,154,163,170,180,191,
191,184,179,172,163,156,154,147,142,142,150,180,195,170,138,133,113,62,29,32,41,34,45,75,98,103,106,108,105,105,108,112,115,126,142,156,166,179,188,191,
191,186,177,170,165,158,149,143,145,147,166,193,184,149,131,124,85,39,29,41,43,41,60,87,99,101,103,101,101,106,110,113,122,138,150,161,173,184,189,191,
188,182,173,165,159,154,147,143,143,149,173,191,172,142,135,117,73,38,36,41,38,45,69,90,99,105,106,103,103,106,112,115,124,140,154,165,172,182,191,189,
184,179,172,161,156,154,147,143,143,156,182,191,165,136,129,110,62,32,39,48,43,50,76,99,101,99,103,103,101,103,110,119,129,143,159,170,179,186,189,186,
180,173,166,161,156,150,147,145,145,161,188,182,149,135,131,96,48,38,50,48,43,59,87,99,98,101,105,101,99,105,113,120,133,149,161,172,180,189,189,186,
180,175,163,156,154,150,142,142,142,154,179,182,150,133,131,99,52,38,50,48,43,60,89,99,99,103,105,101,101,106,112,120,135,150,161,172,184,188,186,184,
180,172,163,154,150,149,143,138,138,142,158,177,166,138,129,122,83,46,46,53,48,50,75,94,99,101,105,105,103,103,108,117,128,140,152,165,173,180,184,184,
182,179,172,161,156,152,149,140,140,138,145,166,177,156,136,135,112,69,50,53,48,43,57,78,89,94,101,103,101,103,108,112,119,131,145,156,166,179,184,186,
188,184,177,170,163,154,150,147,142,140,140,152,175,175,150,138,131,99,62,50,50,46,48,64,82,92,96,99,99,96,99,105,110,119,133,145,158,168,177,184,
186,184,182,179,170,161,156,154,147,142,143,142,150,173,179,158,145,138,112,75,57,52,43,41,52,68,76,85,92,94,94,98,105,108,115,126,140,152,163,173,
182,188,188,188,186,179,170,163,159,154,149,145,143,152,172,179,165,149,140,119,85,64,55,52,48,52,62,73,80,82,83,85,90,98,103,112,124,136,149,159,
172,180,186,189,189,186,180,173,165,159,156,149,145,143,143,158,175,172,152,140,129,105,73,57,53,50,50,57,69,78,83,85,87,90,98,101,108,119,129,142,
154,165,175,182,188,189,186,182,177,170,161,158,154,147,143,142,149,166,175,163,147,138,120,90,66,57,53,50,53,62,75,82,85,87,90,94,99,106,113,124,
135,147,159,168,177,184,188,186,184,179,172,163,158,154,147,143,142,142,154,172,172,154,142,131,108,80,62,57,53,52,57,69,78,85,89,90,94,99,105,110,
117,128,138,150,161,170,179,184,186,184,180,177,168,159,154,149,145,142,138,138,152,170,170,154,140,131,108,82,64,57,55,55,60,73,83,89,92,96,98,101,
106,112,117,126,138,147,158,168,177,182,184,184,180,173,166,159,152,149,143,140,138,140,152,166,168,156,142,129,110,87,68,55,52,52,57,66,78,89,96,98,
103,108,112,113,117,124,133,142,154,163,173,180,184,186,186,180,173,165,158,152,147,143,140,136,143,158,165,159,149,138,126,106,85,68,57,50,48,52,62,73,
83,90,98,105,110,115,119,122,129,136,145,156,165,173,180,184,186,184,179,172,163,156,150,145,140,136,140,152,165,168,161,149,135,119,99,80,62,48,41,43,
48,57,68,78,87,94,103,108,112,117,120,128,136,149,159,170,180,189,195,196,195,191,184,175,166,159,152,145,140,145,156,163,163,152,140,124,106,87,66,46,
34,30,34,45,59,71,85,98,106,115,120,124,128,133,138,147,158,168,177,186,191,193,191,186,177,168,159,150,143,140,135,138,149,161,170,166,152,131,110,89,
68,50,34,29,34,46,64,82,96,106,115,120,124,126,126,128,131,140,150,161,173,182,188,189,189,184,175,165,154,145,140,135,133,133,143,158,173,177,165,142,
113,87,64,45,30,22,25,39,62,85,106,119,126,129,129,128,128,128,129,136,145,159,172,182,191,193,191,186,175,163,150,140,133,128,126,126,129,143,161,177,
177,163,136,105,78,55,39,29,25,32,50,76,101,120,131,135,133,131,128,126,124,126,133,145,159,173,184,191,191,188,180,170,158,143,135,129,126,126,128,140,
156,175,184,175,149,113,82,57,39,27,23,29,45,71,99,120,133,136,135,131,128,124,122,124,129,142,156,172,182,191,193,189,182,172,158,145,133,126,122,122,
124,128,142,158,177,182,170,142,105,75,52,38,30,30,39,57,83,110,129,140,140,135,129,124,122,120,124,131,143,158,172,182,188,189,184,175,165,150,140,129,
124,122,124,126,131,145,163,180,182,166,135,98,68,46,36,32,34,46,66,92,119,136,142,140,133,126,122,120,120,126,133,145,159,173,182,188,186,179,170,159,
147,136,128,122,122,124,128,135,149,166,182,179,158,122,87,62,46,43,45,50,62,82,105,124,135,135,129,124,120,119,120,124,129,138,150,163,173,180,180,177,
172,163,152,143,133,128,124,126,129,129,138,152,170,182,175,152,115,83,60,50,48,50,57,71,89,110,126,133,129,124,119,117,119,122,126,133,142,152,163,170,
173,172,168,161,156,147,140,133,129,129,131,133,133,143,156,172,177,163,135,101,76,64,62,66,68,75,85,101,115,122,124,119,115,113,115,119,124,129,136,143,
152,161,166,168,168,165,159,154,150,145,140,135,133,131,133,129,131,136,145,152,142,122,99,87,87,92,99,101,103,105,110,112,112,108,108,110,119,129,135,138,
135,135,135,140,145,150,156,156,150,142,135,133,135,136,136,135,131,131,126,135,145,156,154,135,117,101,101,103,103,105,105,112,113,117,112,110,110,119,129,135,
138,135,131,131,133,136,140,142,145,145,142,136,131,129,131,129,128,122,126,129,135,133,135,147,154,158,138,113,98,99,113,120,124,119,117,120,122,119,106,101,
106,122,136,136,129,120,120,128,133,136,135,138,140,143,143,136,133,128,129,131,133,135,133,136,135,136,152,165,166,138,106,89,98,120,136,136,122,108,106,110,
112,105,98,103,115,129,136,128,119,115,120,131,140,142,138,140,147,149,142,129,124,124,136,143,147,142,138,133,136,152,158,156,126,106,96,110,128,135,131,119,
113,108,106,99,98,103,115,126,131,129,122,119,115,120,128,140,149,149,145,138,135,135,138,138,140,138,140,142,135,131,124,122,142,156,161,135,103,87,98,122,
129,126,115,113,119,117,113,105,106,117,129,142,140,136,128,124,126,135,143,150,147,142,131,126,126,128,133,129,126,120,126,131,133,128,113,124,145,170,161,122,
87,82,117,147,161,136,113,106,112,120,113,110,110,126,138,142,129,110,108,112,133,145,149,143,131,133,135,138,133,124,128,131,147,143,136,115,112,117,136,163,
163,154,113,96,94,119,143,145,135,110,108,108,113,108,99,108,122,145,147,131,105,92,103,126,145,149,142,140,142,143,135,126,120,126,142,154,158,145,133,120,
124,131,158,168,168,135,99,87,96,129,138,138,112,106,105,110,108,99,106,112,131,129,124,108,108,115,129,143,142,145,138,147,149,145,136,122,128,136,156,158,
145,126,110,113,119,150,170,179,150,105,75,69,108,131,145,128,115,112,113,112,99,101,112,140,150,147,124,110,110,124,143,152,150,140,133,128,124,122,129,136,
143,140,131,124,128,129,133,120,131,150,172,168,126,96,78,110,136,154,142,120,112,106,110,103,105,106,119,129,133,131,120,120,113,119,124,135,145,140,140,133,
135,140,136,135,120,119,126,138,150,143,133,120,143,161,170,140,99,87,105,149,158,147,113,96,99,103,113,112,119,126,128,129,120,119,120,124,128,126,131,138,
142,140,128,120,124,136,143,140,131,124,128,135,135,135,129,149,163,170,147,108,90,98,131,154,150,131,101,96,98,108,119,113,119,117,126,128,120,119,112,120,
126,140,147,149,142,133,129,133,135,138,140,140,143,136,133,120,126,128,145,163,161,145,99,82,78,115,140,147,135,108,106,99,108,110,115,129,136,147,133,120,
108,108,128,138,149,142,136,131,131,133,131,129,129,128,131,129,133,135,135,135,129,136,159,172,168,126,96,87,110,138,140,126,103,103,108,113,110,103,110,126,
142,140,126,110,106,119,133,145,142,140,133,136,136,138,136,135,135,135,136,136,136,133,131,129,131,156,166,170,133,94,80,87,128,138,138,113,98,105,105,112,
99,101,112,131,145,136,126,115,126,142,150,147,136,135,143,152,150,136,124,119,124,128,128,126,126,126,131,129,142,152,158,145,113,98,94,120,138,140,129,112,
113,113,115,113,108,117,124,136,131,120,115,113,129,135,138,138,135,142,143,142,135,126,129,129,135,133,128,128,120,129,119,128,145,156,165,128,110,87,105,131,
142,147,126,122,113,117,120,119,128,122,129,120,119,120,119,126,122,131,131,135,129,124,122,124,131,135,143,142,143,133,133,124,131,135,140,166,168,173,135,106,
83,96,122,138,140,117,105,92,98,94,99,101,112,124,124,126,117,120,120,135,145,156,158,145,142,135,140,145,150,152,147,142,131,129,126,128,128,129,152,165,
172,136,96,73,76,115,131,133,113,98,99,98,103,94,98,113,129,145,136,128,122,128,143,149,150,147,149,150,152,150,143,136,136,136,135,133,131,135,129,129,
124,149,173,188,166,119,87,76,101,120,128,117,103,90,80,75,69,75,87,103,115,120,120,122,124,128,136,147,159,166,170,172,166,163,159,156,154,149,150,150,
149,143,135,128,152,175,188,156,106,71,68,92,115,120,105,90,75,71,68,69,76,89,105,113,120,120,122,128,133,142,152,161,170,173,173,173,166,166,161,154,
152,149,152,149,147,138,135,158,172,182,152,112,80,66,83,90,103,99,89,76,62,57,57,68,83,98,108,117,124,129,131,136,143,156,168,179,184,186,182,177,
173,168,163,159,156,156,149,145,135,136,154,168,170,142,108,82,69,75,83,92,90,85,71,59,53,55,66,80,96,108,117,124,129,138,143,150,161,170,180,188,
191,191,184,179,170,166,159,158,158,150,147,133,136,152,163,165,135,110,83,71,71,75,85,83,78,64,53,50,50,60,76,90,103,113,124,133,138,149,158,168,
180,189,196,198,196,195,186,180,172,168,165,158,150,142,133,143,154,159,145,119,94,73,66,64,69,73,71,62,53,45,45,53,64,82,96,110,120,131,142,150,
161,172,186,196,205,207,207,203,196,193,180,175,168,161,154,142,131,131,138,142,138,122,103,83,69,60,59,62,64,62,53,46,43,43,53,66,83,98,112,124,
136,149,159,173,186,200,209,214,216,214,212,205,198,188,179,168,158,149,136,129,136,136,136,119,103,87,71,68,60,64,62,62,60,52,48,48,55,64,75,89,
103,117,129,142,152,165,175,188,196,207,210,212,212,207,200,189,184,175,168,156,145,138,142,142,136,124,105,90,76,71,66,66,64,60,55,48,46,46,53,62,
73,85,96,110,120,135,145,158,172,184,195,202,207,210,209,207,200,195,189,180,172,159,150,143,147,145,140,126,112,96,82,75,69,66,64,62,57,53,48,48,
53,59,71,82,94,108,119,133,142,154,166,179,191,198,205,209,207,205,200,195,188,180,172,165,152,147,147,143,140,124,112,99,87,80,73,69,68,64,60,55,
53,53,53,60,68,78,90,103,115,126,136,147,159,170,182,193,198,203,203,202,198,193,189,182,175,168,158,158,154,152,142,128,115,101,92,82,76,73,68,64,
57,53,52,52,57,60,69,80,90,103,112,126,136,149,159,172,182,189,196,200,202,200,198,193,189,182,175,168,163,161,158,150,138,128,117,105,94,83,80,75,
69,64,59,57,52,55,57,64,73,80,94,101,112,122,133,145,156,168,177,184,191,193,195,191,191,188,184,179,172,166,165,163,158,150,140,129,119,106,98,89,
83,78,73,68,62,59,55,55,59,62,71,78,87,96,105,117,126,138,150,159,170,177,186,191,193,195,195,195,191,188,184,182,179,173,166,158,149,136,126,115,
105,98,87,82,73,66,62,59,57,57,59,64,68,75,83,90,101,110,119,129,140,150,159,168,173,179,184,186,188,188,188,186,188,184,180,175,170,163,152,145,
136,128,117,108,99,90,83,76,71,68,64,64,64,66,68,71,76,82,90,98,106,115,124,133,142,150,158,165,170,175,179,182,184,186,188,186,182,180,175,170,
163,156,149,140,131,122,115,106,99,92,85,82,78,75,73,71,73,75,78,82,87,92,99,106,113,122,128,136,143,149,156,161,165,170,173,175,177,175,175,173,
170,166,161,156,150,143,138,131,124,119,112,106,101,98,94,90,89,89,87,89,90,92,96,99,103,108,113,117,122,128,133,138,142,147,149,154,156,158,158,159,
158,158,156,154,150,147,145,140,136,133,129,126,120,119,115,112,110,108,106,106,105,106,106,108,108,110,113,115,119,120,124,126,129,131,133,135,138,138,138,140,
140,140,140,140,138,138,136,135,133,133,129,129,128,126,124,124,124,122,122,122,120,122,122,122,122,124,124,124,126,126,126,128,128,128,129,129,129,129,129,131,
129,131,131,129,129,129,129,129,129,129,129,128,129,128,128,128,128,128,126,126,128,126,126,126,126,126,126,126,128,126,128,126,128,128,126,128,128,128,128,128,
129,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,129,128,128,128,128,
128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,129,128,129,
128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,126,129,128,128,128,
128,128,128,128,128,128,128,128,128,129,126,129,128,128,128,128,128,126,128,128,129,128,128,128,129,126,129,128,128,128,128,128,128,128,126,129,126,129,128,129,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,126,128,128,128,128,128,128,128,128,128,128,126,128,128,128,129,128,128,128,129,128,128,
129,128,128,128,128,128,128,128,128,128,128,128,128,128,126,129,128,128,129,126,129,128,128,126,129,128,128,128,129,128,129,126,129,128,128,129,126,129,126,129,
126,129,128,128,128,129,126,129,128,128,128,128,126,128,128,128,126,128,128,126,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,129,
126,129,128,128,128,126,129,128,128,128,126,129,126,129,126,129,128,128,128,128,128,128,129,128,128,128,128,126,129,126,128,128,128,126,128,129,128,128,128,128,
128,128,128,126,129,128,126,128,128,128,126,129,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,129,126,128,128,128,
129,128,128,126,129,128,128,126,128,128,129,128,128,128,128,129,128,128,126,128,128,128,128,128,128,128,129,126,129,128,128,128,129,128,128,128,128,128,126,128,
126,128,126,129,128,128,128,128,128,126,129,126,128,128,128,128,126,128,128,128,128,126,129,128,128,126,128,128,128,126,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,126,129,126,128,128,128,129,126,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,126,128,128,126,
128,128,128,128,126,128,129,128,129,126,129,128,128,128,128,128,128,129,128,128,128,128,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
126,128,128,128,128,126,128,126,128,128,128,128,128,128,128,128,128,128,128,129,126,128,128,128,129,126,128,129,128,129,128,128,128,128,128,128,128,129,128,128,
128,128,128,129,128,128,128,128,128,126,128,126,129,128,128,128,129,126,
};
static const uint8_t CLIP7[] PROGMEM = {
123,123,123,123,125,125,125,125,126,126,126,128,128,129,129,129,130,132,132,133,135,135,136,136,136,135,133,133,130,129,128,125,123,122,119,117,117,116,116,116,
117,117,119,120,122,123,125,126,128,129,132,133,135,136,136,138,141,142,143,145,146,148,148,148,146,143,139,135,129,125,119,113,109,104,101,100,99,99,99,101,
103,107,110,114,120,125,129,135,139,142,145,149,151,152,154,154,155,156,156,156,156,156,155,152,149,145,138,130,122,113,104,96,88,83,78,75,75,77,80,84,
90,97,106,113,122,130,138,145,151,156,161,165,168,169,171,174,174,175,175,175,174,172,168,162,154,142,130,116,103,90,77,67,58,52,51,51,55,60,68,77,
87,99,110,120,132,142,151,159,167,172,178,181,184,187,188,190,190,188,187,184,181,174,167,156,142,129,113,99,84,70,58,49,44,42,42,47,54,61,73,83,
94,107,119,130,141,149,158,165,172,178,182,185,187,191,194,194,194,194,191,190,181,174,161,146,130,112,94,78,62,49,41,34,32,34,39,47,57,68,80,93,
106,119,130,142,152,162,171,178,185,190,194,197,200,201,201,200,197,193,188,180,169,156,141,125,106,90,74,60,48,41,36,36,39,47,55,65,77,90,103,116,
128,138,148,156,165,172,177,181,184,187,187,188,191,191,191,190,187,182,174,164,149,130,113,94,77,61,48,38,32,31,34,38,48,58,70,83,96,109,122,133,
146,156,167,175,184,191,195,200,203,204,207,207,208,206,200,194,182,171,155,135,114,91,71,52,36,25,16,13,15,21,31,42,57,71,87,103,119,133,146,158,
169,180,188,195,201,206,208,210,210,211,210,208,204,197,190,178,165,149,130,110,88,68,49,34,22,13,10,12,16,26,38,52,67,83,99,114,130,145,158,169,
181,190,198,204,210,213,214,217,217,216,213,208,203,193,181,167,148,129,106,86,65,47,31,19,10,8,9,15,23,35,49,65,83,99,116,133,148,161,174,184,
194,201,207,211,214,217,217,217,216,210,204,195,184,169,152,133,113,93,73,55,42,31,25,22,23,29,38,48,60,73,87,101,114,128,141,152,164,172,181,188,
194,198,203,206,207,210,208,207,203,194,184,169,154,133,113,91,71,54,39,29,23,21,23,31,39,51,64,78,93,107,122,135,146,158,168,177,185,191,197,200,
201,206,206,207,206,201,198,190,180,168,151,133,113,91,74,55,42,32,25,23,25,31,39,51,64,77,91,106,120,133,145,156,167,175,184,191,197,200,204,207,
208,211,208,207,200,193,181,167,148,128,106,84,65,47,34,23,19,18,22,31,41,54,68,84,99,114,128,141,154,164,174,181,190,195,198,201,204,206,204,206,
201,200,191,182,172,156,141,120,103,83,67,49,39,29,25,26,29,39,48,61,74,90,104,119,132,145,156,165,175,181,188,193,195,198,200,198,200,197,195,191,
182,177,162,151,132,116,97,78,62,48,38,29,28,29,35,45,57,70,86,100,116,130,143,155,165,174,181,185,190,193,194,194,194,194,193,190,188,181,175,165,
154,139,123,106,88,71,57,44,36,32,31,35,42,54,67,81,97,112,128,141,152,164,171,180,184,188,191,191,194,191,194,190,188,185,180,174,162,152,136,122,
103,86,68,55,44,36,34,36,41,51,62,77,93,107,122,135,148,156,167,172,178,181,184,184,184,182,184,180,182,177,177,172,165,159,148,138,120,106,88,74,
60,48,41,38,39,44,54,65,80,94,110,123,138,148,158,165,172,177,180,184,184,185,182,187,180,184,180,177,175,165,161,146,135,117,103,84,70,54,44,38,
35,39,45,57,70,86,99,116,129,142,152,159,168,172,177,178,181,181,181,180,184,178,181,178,175,174,164,159,145,132,116,99,81,65,51,42,36,35,41,48,
60,74,90,104,120,132,145,155,162,169,174,177,181,181,182,182,181,184,178,181,178,172,174,159,155,141,126,110,93,77,60,49,39,38,39,45,55,67,83,97,
112,125,136,145,155,159,165,169,172,175,178,178,181,177,184,178,180,182,171,177,162,154,143,123,109,88,73,55,47,35,35,38,42,55,68,83,99,113,125,136,
146,154,161,167,171,177,181,184,187,188,185,190,185,182,185,172,172,161,148,138,119,101,83,67,51,41,35,34,38,45,57,71,86,101,114,128,138,146,155,161,
168,171,177,181,184,185,187,184,187,185,180,185,172,171,162,146,138,114,100,78,62,48,36,32,29,36,45,58,73,90,104,119,130,141,149,156,164,168,174,180,
182,187,190,190,187,188,185,180,182,171,168,158,143,130,112,96,74,60,45,34,32,31,38,48,61,77,93,110,122,135,145,151,159,164,169,175,180,184,187,187,
190,184,187,185,174,182,169,162,159,139,129,109,91,71,55,42,31,31,31,38,52,64,83,100,113,129,139,148,156,161,167,171,177,181,184,187,190,190,184,193,
180,180,181,162,162,148,132,117,96,80,57,45,35,26,32,34,47,62,75,96,110,125,138,146,152,159,165,168,174,178,182,187,188,188,187,180,185,174,169,175,
154,152,143,122,112,91,73,55,44,34,28,35,39,52,71,87,106,122,135,146,154,158,164,167,169,174,177,181,184,185,184,184,174,178,171,158,168,151,142,139,
117,106,90,71,54,42,34,29,34,41,54,71,90,106,125,138,148,156,162,165,169,172,175,181,184,187,190,187,187,177,178,171,158,165,149,139,135,116,103,87,
70,52,42,34,29,35,42,55,74,91,110,126,141,151,158,164,167,171,172,175,180,181,184,185,184,182,174,181,167,164,167,145,142,130,112,97,80,62,47,36,
29,29,35,45,61,78,97,114,130,143,152,159,164,168,172,174,178,182,184,187,187,185,182,172,177,164,156,161,143,136,128,112,97,80,65,48,39,32,31,38,
47,64,81,99,119,135,148,158,164,168,171,174,174,175,178,180,181,181,180,177,167,172,162,154,159,145,136,130,114,99,83,67,51,39,34,32,38,47,62,81,
99,117,133,146,155,162,167,169,171,174,175,177,180,181,181,180,177,167,171,165,154,158,148,136,130,117,101,84,68,54,39,34,34,36,47,62,81,100,120,138,
152,162,169,172,174,175,174,172,174,174,174,174,172,171,164,167,164,154,158,151,139,133,120,104,88,71,55,39,31,28,28,35,49,67,86,107,128,145,158,169,
177,181,184,184,184,182,182,182,180,178,175,167,168,165,154,156,149,138,129,117,103,84,68,55,39,31,31,32,39,55,74,93,113,135,151,162,174,180,181,184,
184,182,180,180,178,177,174,172,165,165,162,154,155,148,138,133,119,106,90,71,57,41,29,23,22,28,39,55,75,96,119,139,156,172,181,188,193,194,194,191,
190,188,185,181,178,172,171,168,159,159,152,142,133,122,106,88,73,57,39,29,23,22,28,38,54,73,94,114,135,152,167,177,182,187,188,187,187,184,182,180,
178,177,171,172,171,164,165,159,151,142,130,117,100,81,67,49,36,31,25,28,36,48,65,83,103,122,139,154,165,172,178,181,182,182,182,181,180,180,178,175,
175,175,169,169,167,158,149,138,123,106,88,71,55,41,34,29,29,36,48,62,81,100,117,135,148,159,167,171,174,175,175,175,175,175,175,177,175,177,178,175,
175,172,165,156,146,133,116,100,83,67,52,42,38,36,39,48,60,73,88,103,117,130,141,149,155,159,162,165,168,169,172,175,177,178,181,184,181,182,180,175,
168,158,146,130,114,97,81,67,55,47,42,44,48,55,67,78,91,103,116,126,133,142,146,152,156,159,164,167,169,174,177,178,182,182,182,182,180,174,165,156,
143,129,114,99,83,70,61,54,51,51,55,62,71,81,93,103,113,120,128,135,139,143,148,154,158,162,168,171,175,181,185,187,190,188,185,180,172,161,148,133,
117,101,87,74,64,57,52,52,55,60,68,75,84,94,103,110,117,125,132,139,145,152,159,165,172,178,185,191,194,198,198,197,191,184,172,159,145,128,112,96,
81,70,61,57,54,55,58,64,71,78,86,94,101,109,116,123,129,138,145,152,159,165,172,180,184,188,191,193,191,187,181,172,162,149,136,122,109,97,87,78,
73,68,67,67,70,73,77,83,88,94,101,107,114,122,129,136,145,152,159,167,174,180,185,188,191,191,188,184,177,168,156,145,133,120,109,99,90,83,77,73,
71,71,71,74,77,81,86,91,97,104,112,119,126,135,143,151,158,165,171,177,181,182,185,184,182,180,174,167,158,149,141,130,120,112,103,96,90,84,80,78,
77,77,77,80,83,86,91,97,103,110,117,126,133,141,149,156,162,168,172,177,178,180,178,175,172,167,161,155,146,139,132,125,117,110,104,100,96,91,90,88,
87,88,88,91,94,99,103,107,113,119,125,130,136,142,148,152,156,161,162,164,164,164,161,158,155,151,146,141,136,130,125,120,114,110,107,103,100,99,97,97,
97,99,100,103,106,109,113,117,123,128,132,136,142,145,149,151,154,155,155,155,154,151,149,146,142,139,136,132,128,125,120,117,114,113,110,109,109,107,109,109,
110,112,113,116,119,122,125,128,130,133,136,138,141,142,143,143,143,143,143,142,141,139,138,135,133,130,128,125,123,120,119,116,114,114,113,113,113,113,114,114,
116,119,120,122,125,126,129,130,133,133,136,136,138,138,138,138,138,138,136,136,135,133,132,130,129,128,126,126,125,123,123,123,122,122,122,122,122,123,123,123,
125,125,126,126,126,128,128,129,129,129,130,130,130,130,130,130,130,130,129,129,129,129,129,128,128,128,128,128,128,126,126,126,126,126,126,126,126,126,126,126,
128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,129,128,129,129,129,129,129,129,128,129,128,128,128,128,128,128,128,128,126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,
128,128,128,128,129,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,129,129,129,129,129,128,128,128,128,126,126,128,128,128,128,128,128,
126,128,128,128,128,126,128,128,126,128,128,128,128,126,128,128,128,129,128,130,130,130,133,132,133,132,130,130,128,128,126,125,125,123,123,123,123,123,122,122,
122,122,122,122,123,123,125,125,126,129,129,130,132,132,135,135,138,143,143,146,148,148,145,142,139,132,128,122,117,112,109,107,107,107,109,110,113,114,117,119,
120,125,125,126,129,132,132,133,136,139,141,141,145,145,154,156,159,164,161,159,148,139,129,113,106,96,93,90,91,96,100,106,107,112,114,114,117,119,122,122,
126,130,132,138,141,146,149,152,155,156,155,162,169,167,168,165,159,141,128,113,93,81,73,68,71,77,87,96,106,112,116,123,122,123,126,130,132,136,145,146,
152,155,159,161,162,161,162,155,165,174,167,172,162,154,126,107,90,61,54,47,51,60,77,93,107,120,128,129,129,129,126,126,129,133,138,146,154,159,165,168,
169,169,167,162,159,154,169,164,165,167,155,142,112,99,73,49,44,39,47,57,77,93,109,120,126,130,129,130,128,130,132,136,145,151,159,165,171,172,172,171,
165,164,155,174,167,164,168,151,136,103,91,65,45,45,44,57,71,93,109,120,132,129,132,129,125,125,125,132,135,143,154,159,165,167,171,167,167,159,158,149,
155,171,156,167,159,145,119,96,81,51,44,44,47,62,80,100,113,125,133,130,130,130,126,125,129,135,141,149,158,162,168,171,171,167,165,159,154,146,169,162,
156,168,148,132,101,87,62,42,47,45,60,75,97,112,123,132,130,132,126,126,125,126,132,136,148,151,159,168,165,169,167,165,158,158,149,155,172,156,167,162,
141,119,94,78,49,42,47,52,67,87,106,116,129,129,129,129,123,122,125,128,132,139,152,155,159,168,169,167,169,167,162,154,164,175,161,172,165,143,123,100,
81,54,48,47,45,64,77,90,106,116,117,122,123,120,122,125,129,132,141,151,155,164,169,175,175,177,177,169,168,159,172,167,161,174,152,145,126,104,87,62,
54,47,42,55,61,74,88,97,109,113,120,125,125,130,132,138,145,148,159,164,168,174,175,175,174,171,165,161,172,162,167,171,152,149,129,109,91,68,58,45,
45,48,54,65,77,90,99,110,117,120,129,130,132,139,142,148,156,161,169,177,180,184,187,184,180,181,177,172,174,164,158,148,132,119,99,84,71,57,54,51,
51,57,65,74,84,94,104,113,119,126,132,135,142,146,152,158,161,169,171,174,178,174,177,181,174,178,178,171,167,155,141,125,109,90,75,62,52,51,48,52,
61,67,78,87,97,106,112,119,125,129,135,141,146,154,161,167,174,180,185,187,191,191,188,191,182,178,171,158,148,132,117,103,88,80,68,64,61,60,62,67,
73,78,86,93,99,107,112,119,126,132,139,146,152,159,165,168,174,175,180,181,181,182,180,178,171,164,155,142,132,117,106,96,86,80,74,73,71,73,75,80,
84,88,94,97,103,109,114,120,128,133,141,148,154,161,165,169,175,177,180,181,177,178,172,167,161,151,142,130,120,112,101,96,88,84,83,81,83,83,86,88,
91,96,100,104,110,114,122,128,133,141,146,152,156,161,165,168,171,172,171,172,168,165,161,154,146,138,129,122,114,107,103,97,93,91,88,88,88,88,91,93,
96,99,103,107,112,117,123,129,135,141,145,151,155,158,162,164,165,165,164,164,161,158,154,149,143,139,133,128,123,119,114,110,106,103,101,99,99,97,99,99,
101,103,106,110,113,117,122,125,129,132,136,139,141,143,143,145,145,145,145,143,142,141,139,138,135,133,130,129,128,125,123,122,122,120,120,120,120,120,120,120,
122,123,123,125,125,126,128,128,129,129,130,130,130,130,130,130,130,130,130,130,129,129,129,128,128,128,128,126,126,126,126,126,126,126,126,126,126,126,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,129,129,129,129,129,129,129,129,129,129,
128,128,128,128,128,128,128,128,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,
128,128,128,128,128,126,128,129,128,126,126,128,128,128,128,128,128,128,128,128,126,128,128,128,126,128,129,128,129,128,129,129,129,129,128,126,126,130,128,126,
129,126,129,128,126,129,129,126,126,128,130,128,126,128,129,129,126,126,128,132,128,126,128,128,128,129,129,126,126,129,128,123,128,128,128,126,126,129,126,125,
128,128,129,128,128,130,129,128,126,129,129,128,128,126,128,129,126,128,129,129,125,126,128,126,126,125,129,128,126,126,132,130,133,136,133,136,135,133,130,132,
132,128,129,128,125,123,123,122,120,120,119,120,119,119,122,120,122,122,122,126,126,126,129,132,132,132,132,138,136,138,138,146,151,146,146,145,141,138,133,132,
129,123,122,117,113,109,109,107,103,101,103,107,107,109,112,114,119,123,129,133,133,138,142,143,146,148,151,151,149,151,155,162,161,159,151,143,141,133,125,119,
113,109,103,97,94,91,88,86,87,90,97,97,104,112,114,122,126,135,138,142,152,154,158,155,158,162,158,161,161,161,169,169,169,158,149,141,130,122,114,113,
106,100,91,90,87,81,83,81,86,90,93,101,106,112,117,123,130,139,145,151,156,155,159,161,162,164,161,162,159,168,178,181,171,151,138,128,117,112,107,104,
99,86,80,71,73,70,71,77,84,91,99,107,112,122,125,135,145,154,162,164,171,168,171,168,169,169,165,164,159,172,180,178,161,138,128,114,107,103,100,97,
90,78,71,68,67,68,71,78,88,96,106,113,119,126,133,145,151,161,168,171,175,171,169,168,169,168,164,162,158,175,182,177,155,130,112,101,97,94,94,90,
80,68,64,60,60,62,71,83,90,103,114,122,126,133,142,155,164,175,178,181,180,177,175,172,172,168,165,158,159,177,180,169,139,116,99,91,88,88,87,81,
74,67,61,58,61,68,80,93,107,117,123,129,135,142,151,161,169,177,181,180,177,169,167,164,167,162,159,149,159,178,182,162,122,94,81,84,83,87,84,83,
74,64,57,55,62,75,91,107,117,125,130,133,138,146,158,169,180,182,185,182,177,169,165,165,165,158,155,145,151,168,174,155,116,90,78,83,84,86,84,84,
78,70,62,61,68,81,99,114,126,132,136,138,141,146,158,168,177,180,181,177,169,161,155,156,158,155,149,141,142,164,174,162,117,86,74,87,91,93,90,88,
84,75,65,62,68,83,100,116,129,133,135,133,135,142,155,168,175,180,180,175,167,158,151,152,155,155,148,143,132,145,164,171,141,96,71,77,97,101,96,88,
93,88,80,64,65,80,104,123,132,133,133,133,136,143,152,165,174,178,177,174,162,154,142,142,145,152,151,139,125,116,139,156,155,112,78,73,101,122,117,99,
88,94,100,97,84,81,96,125,141,141,130,128,133,142,146,146,152,156,165,164,156,146,142,139,139,139,139,136,136,130,139,159,168,149,103,75,78,112,126,123,
101,93,91,93,83,77,83,104,130,142,141,129,126,132,145,154,158,159,161,161,158,152,143,135,130,133,139,143,136,129,120,139,161,169,136,96,81,99,125,126,
113,101,103,101,93,80,77,91,112,132,138,136,130,128,126,130,139,152,162,167,161,155,146,143,136,135,138,146,148,139,130,120,142,162,172,138,101,83,103,126,
128,112,93,93,97,97,83,75,87,112,130,132,125,120,126,135,143,148,155,161,164,156,149,143,146,148,143,141,139,142,135,128,120,130,161,177,164,119,86,81,
109,126,123,104,96,100,101,88,68,67,90,123,143,138,123,116,128,141,149,151,156,167,174,171,158,145,138,143,146,148,143,136,132,128,123,143,156,156,116,84,
74,103,123,125,104,91,94,103,101,86,80,93,120,141,141,130,126,133,142,146,145,149,159,167,167,156,143,133,135,133,138,138,141,135,132,119,132,156,172,151,
104,77,86,123,135,125,96,88,94,103,88,74,77,104,135,141,128,113,117,132,151,156,156,156,162,168,164,152,143,145,149,149,142,132,128,126,129,123,135,152,
164,142,99,75,86,117,128,122,103,101,104,100,84,73,84,114,142,148,135,122,122,128,136,143,155,165,171,165,151,136,135,139,142,136,135,139,145,136,123,106,
123,159,185,161,104,68,80,120,139,125,97,90,101,109,93,71,74,106,139,146,129,113,114,129,145,151,151,155,162,165,158,149,143,145,145,145,142,142,139,133,
129,122,141,167,180,152,101,70,83,117,133,122,97,90,93,91,77,65,77,107,133,136,123,114,120,132,139,145,151,164,172,171,161,151,148,149,149,145,142,143,
146,141,130,117,136,165,185,155,103,70,81,117,128,116,93,90,94,91,71,61,74,106,130,133,123,117,122,130,138,145,156,167,172,168,161,155,154,151,148,145,
145,146,145,136,128,120,129,159,181,172,125,86,75,100,119,120,106,97,96,90,73,60,65,93,119,130,125,120,122,126,132,138,151,164,175,175,167,159,152,151,
149,151,148,146,141,138,132,126,120,132,161,181,165,119,84,80,106,119,117,103,99,97,87,67,57,68,97,120,128,126,122,123,126,132,138,152,167,177,175,167,
159,152,149,149,148,148,148,148,139,130,122,122,151,178,182,142,99,78,97,116,117,106,97,99,90,73,55,60,81,109,123,125,120,122,123,128,133,146,164,177,
180,169,159,154,155,154,154,148,148,146,145,135,126,120,125,158,181,177,129,93,81,101,114,113,104,100,100,88,70,54,62,88,112,123,122,123,122,125,128,136,
151,165,174,174,169,162,158,152,152,152,151,146,142,139,135,128,123,117,148,180,190,148,100,80,99,117,113,107,104,107,91,67,48,58,83,104,114,117,122,122,
120,119,128,145,164,174,175,171,165,159,155,155,155,154,152,149,146,138,130,126,123,155,181,184,136,96,84,101,109,101,103,106,106,81,57,47,62,84,103,112,
117,125,125,120,120,132,151,168,175,177,175,168,162,158,156,156,156,152,149,143,139,129,126,117,142,175,182,148,101,88,96,106,97,101,109,112,87,61,49,62,
81,96,107,117,126,126,119,119,129,148,162,172,175,177,172,165,159,156,158,158,155,148,142,139,133,125,122,119,154,178,175,129,96,91,101,101,94,106,113,107,
77,55,52,68,86,97,110,123,128,123,117,122,136,152,165,175,178,177,169,164,159,159,159,158,152,148,145,141,129,123,117,141,174,175,143,103,97,97,101,90,
100,110,110,84,58,54,62,77,87,103,117,129,125,120,122,133,148,161,172,180,181,177,169,164,162,161,158,155,151,146,139,132,123,122,116,146,172,168,129,100,
99,100,96,88,106,113,103,73,58,58,70,77,90,106,120,126,123,120,128,139,154,164,175,182,182,177,171,165,165,162,159,154,151,143,139,130,123,117,117,154,
168,156,117,106,103,99,87,91,107,109,90,67,62,61,64,70,86,106,119,122,125,130,136,142,154,168,181,187,188,185,180,174,168,164,161,155,148,143,138,129,
119,116,110,138,154,152,129,114,110,100,90,87,103,103,94,75,70,65,62,64,75,96,109,120,126,133,136,141,149,162,174,181,188,187,185,177,171,165,161,154,
148,145,142,135,122,122,113,133,149,152,138,120,120,103,94,88,101,99,91,80,71,65,57,57,65,81,94,107,120,130,135,141,149,161,172,182,193,197,195,191,
185,180,171,164,155,151,143,138,128,122,112,117,135,135,132,120,123,106,99,90,96,93,87,83,77,73,62,64,65,74,84,99,112,125,135,141,149,156,167,177,
185,191,194,195,190,185,177,171,161,155,146,142,132,126,119,116,126,125,128,119,122,110,104,96,96,94,87,84,80,77,70,65,64,70,75,84,96,110,122,132,
142,154,162,172,181,191,197,201,203,201,195,190,181,174,164,156,146,136,129,117,117,114,112,112,110,109,104,101,99,94,88,84,80,75,73,70,68,68,70,73,
78,88,99,109,120,132,143,154,165,175,182,190,195,198,200,198,195,190,185,177,168,159,151,141,135,132,126,123,119,117,113,109,106,101,96,90,84,78,73,68,
64,61,60,61,65,71,78,88,99,110,123,136,148,161,171,180,188,194,198,201,201,200,195,190,182,175,167,156,146,139,133,126,123,119,116,113,110,109,104,101,
97,91,87,83,78,74,70,68,67,68,73,77,84,93,103,113,125,136,148,159,169,178,185,191,194,195,195,193,188,182,175,168,159,149,143,136,129,126,120,117,
114,112,110,107,104,100,96,91,87,83,78,74,71,70,70,71,75,81,88,97,107,119,130,143,154,165,175,184,191,195,198,198,197,194,188,181,172,164,154,145,
136,129,123,117,113,109,106,104,103,100,99,96,93,88,86,83,78,75,74,74,74,77,81,87,94,103,113,125,136,148,159,171,180,188,195,200,201,201,200,195,
188,181,172,162,152,143,133,125,117,112,106,101,100,96,94,93,90,88,87,86,83,81,80,80,80,80,83,87,91,99,106,114,123,135,143,155,164,172,181,187,
191,194,194,193,190,184,178,171,162,154,145,136,129,123,117,113,109,106,103,101,100,99,96,93,91,88,86,83,80,78,78,78,80,83,87,93,100,109,119,129,
141,151,162,172,181,188,194,197,198,198,197,193,185,178,171,161,152,143,135,126,119,113,107,103,99,94,91,88,86,84,81,80,78,77,77,77,78,80,84,88,
94,101,110,119,128,138,148,158,167,175,182,187,191,194,194,193,190,185,178,171,164,155,146,139,132,125,117,113,107,103,100,97,94,93,90,88,87,84,83,83,
81,81,83,84,87,91,96,101,109,117,126,135,143,154,162,169,177,182,187,190,190,190,187,182,177,171,164,156,149,142,135,128,122,116,112,107,103,100,97,94,
93,90,87,86,84,83,81,81,81,83,86,88,93,99,106,113,122,130,139,148,156,165,172,178,184,187,188,188,187,184,180,174,168,161,154,146,139,132,126,120,
114,109,106,101,99,96,93,90,88,87,84,84,83,83,84,86,88,91,96,101,107,114,122,129,138,145,154,159,167,172,177,180,181,181,180,178,174,169,165,159,
154,148,142,136,130,126,122,117,113,110,106,103,100,97,94,93,90,88,87,87,87,87,88,91,94,99,103,109,116,123,130,138,145,151,158,164,168,171,174,175,
175,174,172,169,165,161,156,151,146,142,136,132,128,123,120,116,113,109,106,103,99,96,94,91,90,88,87,87,87,88,91,94,99,103,109,114,120,128,135,141,
148,154,159,164,168,171,172,172,172,171,169,167,164,159,155,149,145,139,135,129,125,119,114,110,106,101,99,96,94,91,91,90,90,91,93,96,99,101,106,110,
116,120,126,132,136,142,146,149,154,156,158,159,161,159,159,158,156,155,152,149,146,143,139,136,133,130,128,123,120,117,114,112,110,107,106,104,103,103,101,101,
103,103,106,107,110,112,116,119,122,126,130,133,136,141,143,145,148,149,149,151,151,151,151,149,148,146,145,142,141,138,136,133,130,128,125,122,120,117,114,112,
110,109,107,106,104,104,104,106,107,109,110,113,116,119,122,125,128,130,133,136,139,142,143,145,146,146,146,146,145,143,142,141,139,136,133,132,129,126,125,122,
120,119,117,117,116,116,116,116,117,117,119,120,122,123,125,125,126,128,129,130,130,130,132,132,132,132,132,132,
};
static const uint8_t CLIP8[] PROGMEM = {
123,123,123,123,125,125,125,125,126,126,126,128,128,129,129,129,130,132,132,133,135,135,136,136,136,135,133,133,130,129,128,125,123,122,119,118,118,116,116,116,
118,118,119,120,122,123,125,126,128,129,132,133,135,136,138,138,140,142,143,145,146,148,148,148,146,143,139,135,129,125,119,113,109,105,102,99,99,99,99,102,
103,108,110,115,120,125,129,135,139,142,146,149,150,152,153,155,156,156,158,158,158,158,156,153,149,143,136,128,119,109,100,93,86,80,76,75,76,78,82,86,
93,100,108,116,125,133,140,148,153,158,163,166,168,169,170,172,173,173,173,173,172,170,165,159,150,140,129,115,102,89,76,68,59,55,52,52,56,62,69,79,
89,100,110,122,132,142,150,159,166,172,178,182,185,186,189,190,190,190,188,186,180,173,165,153,139,125,109,95,79,66,56,48,43,42,43,49,56,65,76,86,
99,110,122,133,143,152,160,168,175,179,183,186,188,189,192,192,192,190,189,186,178,170,158,143,128,110,93,78,62,50,40,35,33,35,40,49,58,69,82,95,
108,120,132,143,153,162,170,179,185,190,193,196,200,202,200,200,198,193,186,178,168,152,136,119,102,86,70,56,48,39,38,38,42,49,58,69,82,93,106,119,
130,140,150,159,168,173,178,182,185,186,186,189,190,190,192,188,185,179,169,158,142,125,106,88,72,56,45,36,32,32,35,42,52,62,75,88,100,115,126,139,
150,160,170,179,186,193,198,200,202,205,208,208,208,203,198,190,178,165,146,126,106,83,63,46,32,22,15,15,18,25,35,48,63,78,93,109,125,139,152,163,
173,183,190,198,203,208,209,210,213,212,212,209,203,196,185,173,158,138,118,95,73,55,38,25,15,10,10,13,22,33,46,62,78,95,110,126,140,155,166,178,
188,198,205,209,213,215,218,218,216,215,209,203,195,183,169,152,132,112,89,69,49,33,20,10,8,8,12,20,32,46,63,80,98,115,132,148,160,173,185,193,
202,208,212,213,216,216,216,215,209,205,195,185,170,153,136,115,95,76,58,43,33,25,22,23,28,36,46,59,72,86,100,115,129,142,153,163,173,180,188,193,
198,200,203,205,208,206,203,200,192,183,170,155,136,116,96,76,58,43,30,25,22,23,29,38,50,65,79,95,109,123,136,149,160,169,178,185,190,196,198,200,
202,203,205,202,199,193,186,176,162,148,129,110,90,72,58,43,35,29,28,30,36,46,58,70,85,99,113,126,139,150,160,170,178,185,189,193,195,199,198,200,
199,196,195,186,180,169,155,139,120,103,83,66,52,39,32,26,29,32,42,52,68,82,98,113,128,142,152,163,172,179,185,188,192,192,193,195,192,195,189,189,
182,178,168,156,143,128,110,92,76,59,48,36,33,30,33,40,50,65,78,93,109,125,138,150,160,170,178,182,188,189,192,190,192,189,189,186,183,180,172,166,
155,145,128,113,95,79,63,50,40,35,35,36,45,55,68,83,99,115,129,142,153,163,170,176,180,183,185,185,183,185,183,183,182,179,176,170,163,153,140,126,
109,93,76,62,50,40,38,38,43,52,63,78,92,108,122,135,148,156,165,172,176,180,180,183,182,180,182,179,178,178,173,172,168,159,152,140,126,112,95,78,
63,50,40,38,36,42,50,63,78,93,109,125,139,149,160,166,172,176,179,179,180,180,178,178,178,173,178,172,172,168,159,152,138,125,108,92,73,60,48,40,
38,38,46,55,69,83,100,116,129,143,153,160,169,172,176,178,179,179,179,179,175,179,172,179,172,170,168,155,150,133,120,102,86,69,55,45,38,38,40,49,
62,76,92,109,123,138,148,158,163,168,170,172,173,175,173,175,172,170,176,168,176,172,166,169,153,148,132,115,100,80,65,50,42,38,36,43,53,66,83,99,
115,130,142,152,160,165,169,172,173,176,178,176,178,176,173,179,169,175,172,163,165,149,139,126,108,92,73,59,46,39,36,39,46,58,73,89,106,120,135,148,
155,162,168,169,172,173,173,176,175,176,173,175,178,168,178,168,162,160,142,132,116,95,79,62,48,39,36,35,45,53,69,86,102,120,135,146,155,162,166,169,
172,173,175,176,176,175,178,172,172,175,162,169,162,149,150,132,119,108,88,72,58,46,39,38,42,50,65,80,96,115,130,142,153,160,165,169,170,172,173,173,
175,176,175,175,169,166,172,156,162,158,140,143,128,112,100,82,68,52,45,36,38,43,52,68,85,102,119,135,148,158,165,168,170,173,175,175,178,179,179,179,
178,172,169,170,156,158,155,139,138,123,109,98,82,66,53,46,38,39,46,53,69,85,102,119,133,146,155,162,166,169,172,172,175,178,178,180,179,179,172,173,
172,156,165,153,139,139,120,105,92,75,58,46,39,33,38,45,56,75,90,108,126,139,149,159,163,168,172,172,175,178,180,180,182,180,179,170,170,169,152,159,
149,135,133,118,105,90,75,60,46,42,36,39,49,58,75,92,109,126,139,150,158,163,168,169,170,173,176,178,180,180,179,178,170,169,169,153,159,150,136,136,
120,106,92,76,60,48,40,35,38,46,56,73,92,109,128,140,152,160,165,169,170,172,175,175,178,179,179,178,176,169,166,168,155,153,152,138,132,122,106,93,
78,63,49,40,38,39,46,59,73,92,109,125,140,150,159,163,166,169,170,172,173,176,178,179,178,176,170,166,169,156,155,155,140,135,125,110,95,79,63,49,
39,36,38,43,56,73,90,110,128,143,155,163,169,172,173,173,172,172,172,172,173,170,170,165,168,168,156,160,155,142,136,123,108,90,73,59,43,33,32,32,
39,53,69,88,108,126,143,156,166,173,176,179,180,180,180,180,180,180,178,176,168,168,166,155,156,150,138,130,119,105,88,72,59,43,35,32,33,40,55,72,
90,110,132,148,162,172,178,180,182,182,179,178,176,173,172,169,168,160,160,160,152,153,149,139,133,122,109,93,78,62,45,35,30,28,33,43,60,79,99,120,
139,155,169,179,185,189,189,190,188,186,186,182,179,178,170,169,168,158,158,152,140,133,120,108,92,75,60,43,33,28,25,29,40,53,73,93,115,135,152,166,
176,183,188,188,188,186,183,180,179,176,173,170,169,169,162,162,158,149,142,130,116,100,85,68,52,39,32,29,30,38,50,66,83,103,122,139,153,165,172,178,
180,182,182,180,180,179,178,178,175,172,172,168,165,163,156,148,139,128,112,96,80,65,50,40,33,32,36,46,58,75,93,110,128,143,156,163,170,175,175,176,
175,175,175,173,175,173,172,176,173,172,172,166,159,149,139,125,106,92,75,59,49,40,38,39,45,55,68,83,99,112,126,138,146,153,158,162,165,166,169,170,
173,176,178,178,182,182,182,180,176,170,162,150,138,120,105,88,72,59,49,45,42,45,52,62,73,86,99,112,123,132,140,146,150,156,159,162,166,169,173,176,
178,182,183,183,185,180,176,168,159,146,130,116,100,85,72,60,53,52,52,56,62,70,82,92,102,112,120,128,133,139,145,149,153,159,163,168,173,178,182,185,
188,189,188,185,179,170,160,146,130,116,99,85,73,62,56,52,52,56,62,69,78,86,96,105,113,120,128,133,140,146,153,159,166,172,176,183,188,192,195,195,
193,189,182,173,160,146,130,115,98,85,70,62,55,52,50,55,59,66,75,83,92,100,109,118,125,132,139,145,152,159,166,172,179,185,188,192,193,193,189,185,
176,166,153,140,128,113,100,89,80,73,68,66,66,68,70,76,80,86,93,99,106,112,119,126,133,142,149,156,163,170,178,182,186,189,190,189,186,180,172,162,
150,138,126,113,102,92,83,76,72,70,69,70,73,76,80,85,90,96,103,110,118,125,133,142,149,156,165,170,176,182,185,186,186,185,180,175,168,159,149,139,
129,119,110,102,95,89,83,80,78,76,76,78,79,83,88,92,98,105,112,119,126,135,142,149,156,163,169,173,178,179,180,180,178,175,169,163,156,148,140,132,
123,116,109,102,98,92,89,86,85,85,85,86,89,92,96,102,106,112,118,125,130,138,143,149,153,158,162,163,165,166,165,163,160,156,152,148,142,136,130,126,
120,116,110,108,103,100,99,98,98,98,99,100,102,105,109,113,118,122,128,132,136,140,145,149,152,153,155,155,155,153,152,150,146,143,140,136,132,128,125,120,
118,115,112,110,108,108,108,106,108,109,110,112,115,118,120,125,128,130,133,136,139,140,142,143,143,145,145,143,142,140,139,138,135,133,130,128,125,123,120,119,
116,116,115,113,113,113,113,115,116,118,119,120,123,125,128,129,132,135,136,138,139,139,140,140,140,139,139,138,136,135,133,132,130,128,126,125,123,122,120,120,
119,119,119,119,119,120,120,122,122,123,125,126,126,128,129,129,130,130,132,132,132,132,132,132,132,132,132,130,130,129,129,128,128,128,126,126,126,126,125,126,
125,126,125,126,126,126,126,126,126,126,128,128,128,128,128,128,129,129,128,129,129,129,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,
129,129,129,129,129,129,129,129,129,128,128,128,128,126,126,126,126,126,125,125,126,125,125,126,126,126,126,128,126,128,128,128,128,128,128,128,130,130,130,132,
132,133,133,135,133,133,132,132,130,130,129,129,128,128,126,125,125,123,122,120,120,120,119,119,120,122,122,123,125,126,128,129,130,133,140,142,145,148,149,149,
145,142,138,130,126,122,118,115,112,113,112,112,113,115,115,116,119,118,119,122,122,125,126,129,130,132,138,136,139,143,143,150,156,159,163,162,160,150,142,132,
118,109,100,95,92,93,98,100,105,110,112,115,115,118,119,119,123,125,129,132,136,140,142,148,152,152,155,153,162,168,165,170,165,158,143,130,115,96,85,78,
73,73,82,89,99,105,112,119,119,122,125,123,128,128,132,138,140,148,150,156,160,159,162,162,156,165,173,168,172,166,158,132,110,96,68,56,52,53,62,76,
95,108,120,126,129,129,128,126,123,128,129,133,145,149,156,160,166,168,168,168,163,160,153,168,166,163,168,156,146,118,102,79,56,48,42,48,58,72,90,103,
118,125,128,129,129,129,129,130,136,142,150,156,163,166,172,170,170,168,163,156,172,170,165,168,156,139,108,93,70,48,46,48,56,73,93,109,120,130,132,129,
128,125,122,125,128,135,142,149,159,163,166,168,169,166,159,160,152,152,172,159,165,163,148,128,102,89,59,48,46,45,56,75,90,106,119,128,128,129,130,125,
126,130,133,139,148,155,160,165,169,169,166,166,159,159,146,169,169,156,172,152,135,105,89,66,40,48,49,56,78,99,112,122,133,132,128,129,125,122,126,129,
135,142,150,155,163,166,169,169,168,165,159,155,156,170,159,163,166,146,128,106,89,62,52,52,48,62,73,89,99,110,118,116,122,120,122,126,129,133,142,149,
153,162,166,170,173,173,172,170,165,165,176,165,170,170,150,140,118,98,75,58,52,40,50,59,66,85,95,106,112,118,123,123,126,130,133,138,146,152,159,168,
170,175,178,175,173,170,162,173,168,163,173,159,149,135,113,96,73,62,53,45,52,58,63,79,89,98,108,116,120,125,130,133,138,143,150,155,162,169,172,176,
179,178,176,175,168,172,166,163,166,156,152,138,125,106,90,75,59,55,46,50,55,62,73,82,96,103,115,123,125,135,136,142,148,150,158,162,168,172,173,178,
178,176,179,178,175,178,172,165,156,143,129,112,99,80,70,62,55,55,58,62,70,79,89,98,108,116,120,128,133,138,143,149,153,159,165,169,172,176,175,176,
180,176,178,176,170,168,156,145,132,115,100,86,72,63,58,55,56,60,68,76,85,95,102,112,116,123,129,132,139,142,150,155,160,169,170,178,178,179,185,180,
183,182,175,173,162,153,142,128,116,100,90,79,69,66,62,65,66,70,78,82,90,98,103,110,116,122,129,135,142,148,156,160,166,172,173,180,180,182,185,179,
180,173,166,158,146,136,122,112,100,90,85,78,75,73,73,76,79,82,88,92,96,102,108,112,119,125,132,139,145,152,158,163,166,172,173,176,179,176,178,173,
168,162,153,145,135,125,115,106,98,92,88,83,83,82,83,85,88,92,93,99,103,108,115,119,125,132,138,143,149,155,158,163,166,169,173,170,172,170,166,163,
156,149,142,133,126,118,110,106,100,96,93,92,90,90,90,90,92,95,98,100,105,109,113,119,125,130,136,140,145,150,153,158,160,162,163,163,162,162,159,156,
152,148,143,139,133,129,123,119,115,112,108,105,103,102,100,99,100,100,102,105,108,110,113,118,120,125,129,132,135,138,139,140,142,143,143,143,143,142,142,140,
139,136,135,133,130,129,128,126,125,123,122,120,120,120,120,120,120,120,122,122,123,123,125,126,126,128,128,129,129,129,129,130,130,130,130,130,129,129,129,129,
129,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,129,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,129,128,126,126,128,126,126,128,126,128,128,128,129,
130,130,130,128,126,126,128,129,129,126,123,126,126,125,126,129,130,129,128,129,126,126,126,123,123,125,128,128,130,130,132,132,132,129,128,126,126,128,126,128,
129,130,129,129,129,128,125,126,128,128,126,126,128,129,129,129,126,126,126,126,128,128,129,128,129,129,129,130,130,129,129,128,128,128,128,126,126,125,126,126,
128,128,126,126,128,128,129,129,130,132,140,149,155,153,146,138,129,122,116,109,105,102,103,108,113,119,122,123,123,123,123,122,122,122,122,123,128,132,138,142,
143,145,145,143,143,143,143,140,140,149,159,169,168,158,140,123,109,98,89,83,80,83,90,102,110,118,119,119,118,115,115,115,116,119,125,132,139,146,152,155,
156,155,153,150,149,146,143,142,148,156,168,172,163,148,126,108,92,79,70,66,66,75,89,103,116,123,125,125,123,122,120,119,120,125,130,140,149,158,163,165,
165,160,156,152,148,145,140,143,150,163,172,168,153,130,109,90,78,69,65,65,72,85,100,113,120,123,122,119,116,116,116,119,125,132,140,150,159,166,168,166,
162,156,152,148,143,140,140,149,162,175,176,165,140,112,88,69,59,52,50,58,72,92,112,126,132,132,128,123,120,119,119,122,129,138,149,160,169,175,173,169,
162,156,149,143,139,136,142,153,168,173,165,145,119,95,76,65,58,55,59,72,89,108,122,128,128,125,122,119,118,119,122,128,136,148,158,166,170,172,168,162,
155,149,143,140,136,138,146,159,173,175,162,136,106,83,65,58,53,55,62,76,96,113,128,132,130,125,120,119,119,120,125,132,142,152,163,170,173,172,166,160,
153,148,143,139,136,143,155,169,175,165,143,115,90,72,62,58,58,63,76,93,110,122,128,126,122,118,116,118,120,125,130,140,150,160,166,169,168,163,158,152,
148,143,140,136,139,148,160,175,173,158,130,102,80,66,62,60,62,70,83,100,116,126,128,125,119,116,116,119,122,128,135,145,155,163,169,170,168,162,156,150,
145,142,139,136,140,150,163,172,165,146,118,93,78,70,69,70,75,83,95,108,118,122,120,116,116,118,120,125,129,133,139,146,153,159,160,160,158,153,148,145,
142,139,139,136,140,150,163,172,165,145,118,95,82,75,76,76,80,88,98,110,119,122,119,115,113,113,118,122,126,132,138,145,153,159,162,160,158,152,149,145,
142,139,138,136,139,145,150,149,138,120,105,96,95,98,100,103,105,108,112,116,119,116,118,118,125,130,135,135,133,136,138,142,140,138,135,132,135,136,139,138,
136,133,133,130,140,150,159,158,135,116,100,100,108,115,119,118,118,116,118,116,113,113,115,122,125,125,123,122,126,130,135,138,140,143,143,142,139,135,135,133,
135,130,132,129,135,146,150,149,128,113,106,112,122,125,123,115,113,113,116,116,112,113,116,128,130,132,125,122,125,130,140,143,143,140,138,138,135,135,133,132,
132,130,135,135,136,145,143,143,128,115,103,100,109,113,120,120,119,120,119,119,115,115,119,128,136,135,132,128,129,133,136,140,138,135,130,129,130,129,126,128,
130,136,136,133,126,135,149,158,146,119,102,100,120,135,139,132,120,119,119,122,113,106,106,116,130,139,138,123,113,110,120,130,136,135,129,132,136,142,139,129,
123,125,136,146,146,139,143,150,156,140,116,100,105,125,136,132,118,106,110,115,116,113,109,116,123,130,128,122,120,123,133,140,139,135,129,128,130,138,140,139,
132,128,128,129,135,133,139,150,158,155,129,108,99,113,136,146,139,119,106,105,109,113,112,110,116,125,130,132,123,118,118,125,135,142,143,136,135,129,129,129,
130,139,139,139,132,130,128,143,152,156,143,120,109,103,118,120,123,119,116,122,116,116,106,108,112,125,133,135,130,120,120,122,136,138,140,138,139,145,145,142,
130,123,120,126,133,138,139,132,143,148,153,133,108,96,100,130,139,142,125,109,106,100,106,105,115,126,135,140,132,123,110,113,123,142,155,152,143,135,130,132,
130,129,128,129,135,138,136,129,115,125,139,155,152,119,102,92,120,138,142,130,112,118,119,128,120,113,113,125,139,142,133,119,115,120,130,136,132,129,126,129,
132,133,129,125,125,128,136,139,143,133,139,143,149,145,118,112,106,133,142,139,122,103,110,113,126,118,110,108,113,126,129,128,120,119,126,130,139,139,139,138,
132,135,132,138,133,133,130,128,129,130,128,136,146,153,145,113,99,90,120,139,149,140,118,112,108,118,116,119,120,130,140,139,130,110,108,113,132,145,145,136,
125,122,125,133,135,132,128,132,139,143,142,128,122,133,152,166,146,122,99,106,129,136,138,118,118,118,123,119,103,100,105,123,130,129,122,116,125,128,132,126,
126,135,145,153,143,133,122,122,130,133,138,128,130,126,139,153,155,152,116,106,99,120,138,140,139,125,120,112,106,103,105,119,132,138,133,122,115,110,112,122,
132,140,140,135,128,123,126,132,140,142,145,140,136,132,128,128,146,160,168,142,112,90,96,128,142,145,120,103,99,106,119,118,118,119,126,135,132,122,112,115,
129,143,148,139,129,125,133,138,136,130,128,136,140,145,132,130,125,139,160,163,158,118,100,92,116,138,139,133,113,113,112,108,100,95,110,126,145,138,123,110,
109,123,129,135,132,136,140,142,140,133,129,128,128,132,136,140,142,138,129,132,145,158,155,128,102,90,108,132,145,138,120,109,108,113,118,119,120,128,130,135,
128,126,120,120,120,123,133,139,146,139,129,119,118,129,133,140,135,133,130,132,130,133,145,148,146,119,110,102,123,139,142,135,115,113,109,115,115,115,123,129,
136,128,119,112,112,126,132,138,132,130,130,135,138,135,135,130,135,136,140,139,133,132,122,132,145,155,149,120,100,95,116,136,139,130,109,108,109,118,122,122,
126,122,126,126,129,129,129,128,130,136,139,138,132,129,125,128,128,128,126,126,129,132,132,130,122,132,152,169,168,135,106,93,110,138,145,138,116,109,110,116,
116,108,106,110,126,132,133,125,120,125,132,140,140,138,136,136,138,138,136,136,138,136,133,132,130,129,126,128,143,153,159,136,112,93,96,118,130,136,125,113,
110,106,109,108,110,118,123,130,128,125,122,122,128,130,138,139,142,140,140,140,138,139,135,135,136,138,143,140,140,133,149,170,183,172,133,106,92,105,119,125,
118,105,95,86,79,78,79,88,100,109,118,119,119,120,125,132,142,152,159,163,163,162,158,155,153,149,148,148,150,149,143,133,135,156,172,173,138,99,79,83,
106,118,118,103,90,85,78,79,78,86,98,108,118,120,125,126,128,133,139,149,160,166,168,166,163,160,158,155,152,152,150,149,145,139,135,149,165,172,155,120,
93,78,85,95,105,105,98,88,73,68,66,75,86,99,110,118,123,126,129,135,139,150,160,169,175,173,175,170,168,160,156,155,153,153,149,143,136,138,155,162,
159,132,108,89,80,86,89,98,93,88,78,69,66,68,79,89,100,110,120,128,132,138,142,148,156,166,173,179,180,179,173,169,165,158,155,153,150,145,140,130,
138,149,156,153,128,109,86,80,83,86,93,90,86,75,66,63,68,78,88,99,109,118,126,133,139,148,153,163,173,178,185,186,185,180,175,169,162,158,153,149,
145,138,130,136,143,150,142,125,105,88,79,76,82,85,83,79,69,65,62,66,76,88,100,109,119,129,136,145,155,163,173,182,188,192,193,192,189,185,176,169,
162,155,148,140,132,126,130,133,136,128,112,99,83,79,76,76,79,76,75,68,62,63,65,76,85,99,109,118,128,136,148,156,168,178,186,192,196,196,196,192,
186,179,173,166,158,150,142,133,130,135,135,132,119,105,92,82,78,76,76,76,73,69,65,63,66,70,80,88,98,109,119,130,140,150,160,170,180,186,193,195,
196,198,192,188,178,170,162,155,146,136,129,122,128,126,126,116,105,96,85,82,79,80,82,82,78,73,70,72,75,82,90,99,108,116,126,133,143,150,162,170,
178,183,188,190,189,188,183,179,172,166,159,152,143,135,132,135,133,132,122,110,100,90,86,82,82,83,80,78,72,69,70,72,79,88,98,106,113,122,130,140,
149,159,168,176,182,186,189,189,189,185,179,172,166,162,153,148,138,136,136,132,130,118,112,102,95,90,83,85,82,80,78,75,73,72,75,79,86,95,103,113,
120,129,136,145,153,163,170,178,183,186,186,185,180,178,172,168,162,155,149,140,139,139,136,133,122,116,105,98,92,88,88,83,83,78,76,73,72,76,78,85,
92,99,108,116,125,133,142,149,159,166,173,179,183,185,185,182,180,176,172,166,159,153,145,142,140,138,133,125,118,112,102,98,90,89,88,85,83,79,78,78,
78,82,86,92,100,108,115,122,129,136,143,152,159,165,170,175,176,176,176,173,170,166,163,159,153,148,146,146,142,139,132,126,120,112,106,100,98,93,90,86,
83,80,79,78,78,82,85,89,96,102,108,115,120,129,136,145,152,159,166,169,175,175,176,176,175,173,169,168,162,159,158,153,150,143,138,132,123,118,110,105,
99,93,90,86,82,80,79,79,79,82,85,88,93,99,103,110,116,125,130,138,145,152,158,162,166,169,170,172,172,172,170,169,168,166,162,159,155,150,145,138,
133,126,120,115,109,103,98,95,90,88,86,83,83,83,85,88,89,93,98,102,108,113,119,126,132,138,143,148,153,156,160,162,163,165,165,166,166,165,163,160,
158,155,150,146,140,136,130,126,120,115,110,106,102,99,96,95,93,92,92,93,95,98,99,102,106,110,115,119,123,128,132,136,140,143,146,149,150,152,155,155,
155,155,153,153,150,149,146,143,140,138,133,130,128,123,120,118,115,113,110,109,109,108,108,108,109,110,112,113,115,118,119,122,125,126,129,130,132,133,135,136,
138,138,138,138,138,136,136,136,133,133,132,130,130,129,128,126,126,125,125,123,123,123,123,123,123,125,125,125,126,126,126,126,128,128,128,129,129,129,129,129,
129,129,129,129,128,129,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,129,128,128,128,128,129,128,128,128,128,128,128,128,128,
128,128,128,128,126,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,129,128,128,128,129,128,128,128,129,128,128,128,128,128,
126,128,126,128,126,128,128,126,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,
128,128,129,128,128,128,129,128,128,129,128,129,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,129,128,129,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,129,128,128,128,128,126,128,128,126,128,128,
128,128,128,128,128,129,128,128,126,128,126,128,126,128,128,128,128,126,129,128,128,128,128,129,126,128,128,126,128,128,128,126,129,128,128,129,128,128,128,128,
126,129,128,128,128,128,128,126,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,128,128,126,128,128,128,128,128,
128,128,128,126,128,128,128,128,128,128,128,128,128,128,129,128,129,126,128,128,128,128,128,129,126,128,128,128,128,129,128,128,128,126,128,128,126,128,128,126,
128,128,128,128,128,128,128,128,128,126,128,128,128,128,126,128,128,129,128,129,128,128,128,128,128,128,128,128,126,128,126,128,129,128,129,126,129,128,128,129,
126,129,126,129,126,129,128,128,128,128,129,128,128,128,129,126,128,128,128,128,128,126,128,128,128,126,129,126,128,128,128,128,128,128,128,128,128,128,126,129,
128,128,128,128,126,128,128,128,128,129,126,128,129,126,128,128,128,128,128,126,128,128,128,128,128,129,126,129,128,128,128,126,129,126,129,128,128,128,128,128,
126,129,126,128,128,128,128,126,129,128,128,128,128,128,128,128,129,128,128,128,128,129,128,128,128,128,128,128,129,128,128,129,126,128,128,128,128,128,128,128,
128,128,126,128,128,128,128,128,128,128,129,126,126,128,129,126,128,128,126,129,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,129,128,128,126,129,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,126,128,128,128,128,128,128,128,128,128,128,129,128,126,128,128,128,126,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,129,128,128,128,128,128,128,128,126,128,128,128,128,128,129,128,128,128,128,128,128,126,129,128,128,
128,128,128,126,128,128,128,128,128,129,126,129,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,126,129,128,128,128,128,
};
static const uint8_t CLIP9[] PROGMEM = {
134,134,133,131,131,128,126,126,124,122,122,121,121,121,121,121,122,122,122,124,124,126,128,128,129,131,131,133,134,134,138,141,141,141,141,141,138,136,133,129,
126,124,121,119,117,117,116,116,117,116,116,119,119,119,121,122,122,126,129,129,129,133,134,134,136,138,143,146,148,148,148,148,143,138,133,128,124,117,114,110,
109,109,109,107,109,110,112,114,117,119,121,124,126,129,131,134,138,141,145,146,150,153,160,169,170,165,164,158,148,134,122,109,97,90,86,85,85,90,93,97,
100,105,107,109,112,116,119,124,129,136,143,148,153,158,162,164,165,164,170,182,186,176,170,165,150,131,112,97,83,74,71,71,74,80,86,92,97,104,107,109,
112,117,122,128,136,145,148,153,158,165,167,165,165,165,169,184,189,179,169,162,146,121,97,81,69,62,59,62,73,83,92,100,104,109,114,116,112,117,129,136,
141,153,164,170,170,174,174,169,162,157,158,177,188,176,162,157,138,105,83,69,57,50,57,68,81,95,104,107,109,112,112,112,114,117,126,134,146,158,165,165,
169,174,172,167,164,160,160,182,200,182,160,157,136,95,64,50,44,40,47,64,85,100,110,112,110,110,114,112,110,122,138,148,158,172,182,181,176,176,172,167,
157,155,150,162,196,201,169,148,138,97,47,32,30,33,45,68,95,116,119,116,110,105,104,107,114,126,140,153,164,169,172,174,170,167,167,169,162,160,157,162,
201,212,169,146,141,97,44,26,28,37,47,64,95,117,114,110,110,104,97,104,114,122,136,153,164,169,176,179,174,167,170,172,165,157,158,155,174,213,196,150,
136,121,64,25,26,42,56,64,88,119,124,107,102,100,95,98,112,128,141,152,157,164,169,165,165,167,164,164,167,164,153,157,148,172,218,191,140,141,121,50,
20,28,35,44,62,90,117,122,112,109,107,98,102,116,124,138,155,160,167,174,174,172,174,167,165,169,162,148,155,146,165,215,191,140,145,121,50,20,30,37,
44,61,95,122,119,109,110,104,90,98,116,124,136,152,162,167,170,172,172,169,165,165,164,162,153,155,146,169,217,188,148,153,121,54,30,32,30,38,59,90,
112,114,116,110,100,97,102,110,124,138,150,160,167,170,172,174,172,165,164,167,160,150,153,152,143,198,210,158,152,146,81,35,35,37,40,56,88,114,116,112,
110,97,85,92,105,117,131,148,158,162,164,167,169,167,165,162,162,164,157,150,153,146,143,203,203,153,165,145,69,42,44,30,37,62,92,110,114,119,109,90,
85,90,102,117,136,153,160,165,169,165,165,167,165,165,164,165,164,152,152,146,148,203,189,157,164,126,62,40,38,32,44,71,100,110,112,114,98,80,80,93,
105,122,146,160,160,164,169,162,164,170,169,167,169,165,158,153,143,150,134,153,212,170,158,170,107,56,45,33,26,38,76,100,102,117,119,92,81,86,90,102,
128,148,157,165,170,170,165,167,172,169,170,169,167,162,155,146,150,136,162,203,164,164,158,102,57,45,35,26,45,78,93,104,116,110,93,85,86,92,104,126,
145,157,165,167,169,169,165,169,172,169,165,164,160,152,146,145,145,131,186,193,153,184,150,90,66,49,28,21,50,74,81,105,117,104,92,86,83,86,102,126,
141,155,167,172,170,172,174,174,176,177,176,170,167,162,152,152,145,141,191,174,165,176,134,93,61,49,28,25,52,68,80,100,109,98,92,90,88,92,107,126,
138,152,162,169,170,170,174,172,172,172,169,165,160,155,153,146,148,141,162,191,169,182,170,128,95,68,45,23,37,50,59,80,92,88,83,78,73,69,85,100,
117,140,152,162,169,174,177,179,186,189,191,189,186,179,170,164,157,153,143,182,181,170,184,150,116,78,61,30,21,38,45,62,85,97,97,98,97,92,95,102,
114,128,138,148,158,162,164,170,172,177,176,179,177,165,167,158,150,145,146,141,152,181,169,181,176,155,122,95,71,37,32,28,28,40,56,68,74,85,90,88,
93,104,105,116,129,138,150,160,176,182,191,201,201,201,198,193,184,176,167,162,150,153,170,157,167,158,146,121,95,76,42,30,18,16,20,33,50,62,80,92,
102,107,117,121,129,136,143,152,164,176,186,198,206,213,213,213,208,200,188,176,164,152,141,145,140,138,141,134,129,109,98,76,54,40,23,18,16,20,33,44,
57,73,88,102,112,122,133,140,148,157,165,176,184,194,203,208,212,213,212,206,198,189,179,167,158,153,143,141,134,131,124,117,109,97,85,73,61,52,44,40,
38,40,44,50,59,69,80,92,102,114,124,136,148,158,170,179,189,196,203,206,208,208,205,201,194,186,181,174,165,160,152,145,134,126,116,104,93,81,71,62,
56,50,49,50,52,57,62,71,80,88,98,109,119,128,138,146,157,164,170,177,181,184,186,186,184,182,179,177,174,170,167,162,158,150,143,134,124,114,102,92,
83,74,69,64,62,64,66,71,76,81,90,97,105,112,121,129,136,143,150,157,162,167,170,174,174,176,172,174,174,170,170,167,164,160,152,146,136,126,116,104,
97,86,81,76,74,73,74,76,80,86,90,97,104,109,116,124,129,136,143,148,153,157,162,164,164,165,165,164,169,165,169,167,164,160,153,146,134,122,112,98,
90,81,76,76,74,80,85,90,98,102,110,114,119,124,126,133,134,140,145,148,152,155,158,158,162,160,160,164,169,164,174,170,165,160,150,136,116,107,83,71,
68,56,61,61,69,76,85,97,104,114,119,126,128,134,136,143,145,155,157,164,170,170,179,172,181,170,176,179,167,181,167,165,157,140,128,105,90,69,54,49,
40,44,50,57,73,81,97,109,114,129,128,136,140,141,150,152,160,165,169,176,177,179,181,176,172,179,176,169,184,162,164,152,129,116,88,73,47,37,30,25,
33,42,57,73,92,105,119,133,138,145,150,152,158,160,169,174,179,188,189,193,194,191,182,189,174,174,176,153,160,133,121,100,76,57,37,23,14,16,18,35,
47,68,90,105,126,138,146,158,160,165,172,172,179,181,186,188,191,191,189,186,182,186,170,181,165,160,150,129,116,88,69,44,30,14,9,13,16,35,49,71,
92,110,129,141,155,162,170,176,181,184,189,193,198,201,200,203,194,201,188,188,184,164,165,134,126,100,76,57,33,21,8,9,9,23,35,56,74,95,116,129,
146,153,164,172,174,182,184,188,193,193,194,194,193,188,194,177,189,172,167,162,134,129,97,81,57,37,21,11,9,11,25,35,57,74,98,116,133,146,158,169,
174,182,184,191,193,198,200,201,203,196,200,193,184,188,167,165,148,129,116,90,69,49,32,18,13,9,18,26,42,62,80,100,119,134,148,158,169,176,181,184,
189,191,196,196,196,198,193,196,186,191,177,174,164,141,133,104,86,64,42,28,16,9,13,18,30,47,64,85,104,122,138,150,162,170,179,184,189,196,198,201,
205,201,203,203,193,201,184,181,170,148,141,114,95,73,52,35,23,13,14,16,25,40,54,74,90,110,126,140,153,162,172,177,184,189,193,200,198,201,201,201,
200,201,191,189,177,160,150,122,107,83,61,45,26,18,13,14,20,32,44,62,78,95,114,126,143,153,164,174,181,189,194,200,203,205,206,206,201,205,194,193,
186,169,165,140,129,105,86,69,47,38,23,21,20,23,33,42,57,71,88,104,119,131,145,157,165,176,181,191,194,200,203,203,208,201,208,198,200,189,176,170,
146,136,114,92,76,54,42,30,23,21,21,32,38,52,66,81,97,110,126,138,152,160,172,181,188,194,200,203,208,206,210,210,203,205,189,186,170,153,140,116,
100,80,61,49,33,26,23,23,28,35,47,57,71,85,97,114,124,140,150,162,174,182,193,198,205,208,212,212,213,208,205,201,189,182,165,153,136,116,100,80,
66,52,38,33,26,28,32,35,47,54,69,80,93,107,119,134,145,157,169,177,188,193,201,203,208,206,208,206,201,201,189,186,172,160,148,131,117,98,86,69,
59,49,40,38,33,38,40,49,57,64,78,86,100,112,122,138,148,160,170,179,188,193,200,203,205,208,205,205,200,193,188,176,167,152,138,124,107,95,80,69,
57,50,47,42,45,45,52,59,66,76,83,98,105,119,129,140,152,158,170,176,184,189,191,196,194,198,194,193,193,182,182,169,164,152,140,129,114,105,90,81,
71,62,59,54,54,54,56,61,66,73,81,90,98,109,117,129,140,148,160,165,174,182,182,191,191,191,194,189,191,186,181,177,167,160,150,138,126,116,102,93,
81,74,68,61,59,57,59,61,64,69,76,83,92,100,110,119,129,138,148,157,164,170,176,181,182,184,186,184,188,181,182,174,172,167,157,155,140,134,122,114,
105,95,88,81,74,71,68,66,69,68,73,74,81,86,93,104,109,121,126,136,145,152,162,164,172,176,177,181,179,181,177,177,174,170,169,160,160,150,145,138,
128,124,112,107,98,93,88,83,81,76,78,76,78,80,81,88,90,97,102,109,117,122,131,138,145,150,155,160,164,165,169,167,170,169,167,167,164,162,158,155,
152,146,141,136,131,126,119,114,109,104,102,97,95,93,90,92,90,92,95,95,100,102,105,112,116,121,126,129,136,140,145,148,150,153,153,155,155,155,153,155,
152,153,150,148,146,143,141,136,134,131,128,124,121,119,114,112,110,107,107,104,104,104,104,105,105,107,109,112,114,119,122,124,129,131,136,138,140,143,145,145,
146,145,145,146,143,145,141,141,140,136,136,134,133,131,128,126,124,122,121,119,117,117,116,116,114,114,114,114,114,116,117,119,121,122,124,126,128,131,133,134,
136,136,138,138,138,138,138,136,134,134,133,131,131,
};
static const uint8_t CLIP10[] PROGMEM = {
137,137,137,137,137,136,134,132,129,126,124,121,120,118,116,115,115,113,115,115,115,116,118,120,121,123,126,128,129,132,134,136,140,144,145,150,152,153,155,155,
153,150,145,140,132,126,120,113,107,100,97,94,92,92,92,94,97,100,105,110,115,120,126,131,136,140,145,150,153,156,161,164,168,171,172,174,172,171,166,158,
150,139,128,116,105,94,86,78,73,72,70,72,73,78,84,91,97,105,113,121,129,137,145,152,158,164,169,172,179,182,187,190,190,190,187,182,172,161,148,131,
116,100,84,73,62,54,49,48,51,54,60,68,76,86,96,105,116,126,137,145,156,164,172,180,185,193,198,203,209,209,209,206,198,190,174,160,140,120,100,83,
67,52,43,36,33,35,38,43,51,60,70,83,94,107,118,131,144,153,166,176,185,193,198,208,211,217,220,219,217,209,201,188,171,152,128,107,84,65,49,36,
30,27,27,32,38,48,57,70,81,92,107,118,128,140,150,160,169,177,185,192,200,206,212,217,217,214,206,195,180,163,142,123,104,88,72,62,52,49,44,48,
49,56,62,72,80,89,100,108,121,129,139,147,158,166,172,180,185,192,203,208,214,212,212,201,188,174,155,132,112,94,78,62,54,48,43,43,43,49,54,64,
72,81,94,104,113,126,136,145,152,166,172,180,185,192,196,201,214,217,222,216,212,195,177,156,134,110,89,72,62,51,44,38,40,38,41,49,59,68,83,97,
105,116,126,137,144,156,161,176,179,188,188,198,192,216,220,220,212,203,185,142,128,105,96,72,76,73,67,54,51,49,41,49,57,78,86,104,110,118,121,128,
137,145,163,168,180,184,190,188,184,188,180,212,219,222,206,190,163,113,99,81,83,62,73,67,59,40,33,33,35,48,67,91,105,112,121,124,129,132,152,164,
177,187,195,196,196,190,188,185,195,232,225,222,187,174,120,96,76,81,78,70,72,54,44,20,25,25,44,64,86,105,115,116,115,123,132,148,163,187,195,193,
192,190,188,179,187,184,201,235,228,208,166,152,97,78,73,94,89,80,73,54,32,9,22,38,64,88,108,118,112,112,112,124,142,166,182,192,195,184,179,174,
180,174,185,177,200,235,219,190,137,132,89,81,73,105,105,83,59,41,27,8,22,51,83,99,112,115,112,105,113,129,155,176,193,196,193,184,177,172,182,182,
193,182,204,233,222,180,129,123,92,86,75,104,99,76,43,32,22,17,27,56,88,99,105,105,107,110,123,142,163,180,192,193,185,177,176,180,184,185,187,176,
192,228,225,180,120,110,96,94,83,104,104,84,43,20,17,24,36,59,88,104,102,99,100,110,124,147,172,187,196,193,188,177,176,179,188,190,185,180,168,208,
217,200,129,102,94,102,91,84,94,89,62,25,19,27,44,57,81,99,108,104,102,105,123,142,168,185,192,188,180,177,171,177,184,193,182,179,161,187,220,216,
156,94,88,97,105,88,91,92,80,41,16,16,36,62,80,92,100,104,100,100,113,134,160,177,190,190,187,177,172,176,184,193,190,182,169,171,212,220,185,108,
86,91,110,92,86,91,86,57,20,17,33,60,75,91,97,104,102,102,110,128,152,169,184,188,185,177,172,172,179,188,190,180,172,160,195,222,209,136,83,81,
105,108,89,88,88,75,38,14,20,51,72,88,94,102,100,102,105,123,145,168,180,185,185,184,180,174,176,180,188,184,174,164,158,196,219,196,115,72,78,110,
108,91,86,86,68,32,17,33,67,86,92,97,99,102,104,115,132,153,171,179,180,180,180,177,174,176,182,187,177,168,155,158,201,219,185,105,68,76,107,105,
91,86,84,64,28,12,30,68,96,99,94,99,107,113,118,137,161,179,184,184,180,179,177,176,174,177,180,174,161,152,144,180,212,198,120,59,67,104,115,89,
86,92,84,43,19,35,72,97,102,104,107,108,110,116,132,153,172,180,182,176,172,172,174,171,169,171,176,166,153,140,144,192,209,177,94,57,70,105,105,88,
89,91,73,35,24,49,88,105,105,107,112,115,115,124,142,160,169,176,180,176,171,168,166,163,163,168,174,161,147,131,152,204,208,153,67,54,83,118,105,97,
102,102,70,32,36,68,104,113,113,113,115,115,112,120,136,161,176,177,164,161,163,163,160,156,163,168,168,150,140,134,150,212,193,139,60,68,96,107,96,99,
121,104,62,33,51,84,100,104,112,124,124,118,113,121,137,155,171,176,172,168,163,163,156,156,153,161,161,156,142,142,129,160,196,163,108,57,89,96,99,88,
112,121,91,60,49,76,91,102,115,132,134,116,115,120,139,145,158,164,174,168,166,155,148,158,153,153,147,156,156,132,123,123,160,182,142,112,78,99,84,88,
91,116,120,104,81,76,89,89,91,107,137,150,132,120,123,131,132,132,153,177,172,160,152,152,144,139,140,150,160,156,139,126,126,132,168,153,137,112,107,107,
84,102,107,121,104,97,94,83,81,81,102,112,126,121,123,131,123,126,137,156,156,150,168,168,156,152,155,160,153,139,145,142,140,129,126,131,142,150,124,118,
105,115,94,89,104,116,112,94,96,96,94,97,107,113,129,136,140,140,139,137,137,145,150,153,153,155,145,145,142,134,128,121,131,131,132,131,131,128,129,150,
145,139,120,128,123,116,110,121,124,112,108,107,112,108,104,115,110,118,120,128,132,124,131,131,134,132,137,142,150,140,145,137,137,134,132,137,136,139,131,139,
128,140,145,137,137,128,131,116,116,110,113,110,112,110,107,108,104,104,104,108,113,118,126,131,132,134,137,139,140,142,145,147,145,147,147,147,142,137,137,134,
131,129,126,128,123,129,131,128,128,124,124,118,118,115,116,113,116,116,115,118,116,120,121,123,126,128,129,132,132,134,136,134,136,132,136,134,132,132,131,131,
131,131,129,131,129,131,129,131,129,132,136,134,136,132,134,131,128,126,121,120,116,113,113,113,113,115,115,116,118,121,123,124,128,129,131,134,136,139,139,140,
139,139,139,136,134,132,131,129,126,124,124,123,123,121,124,126,128,129,131,132,132,131,131,129,128,126,124,123,121,120,120,120,120,120,120,120,121,123,124,128,
129,132,134,136,137,137,137,137,136,134,134,131,129,128,126,126,124,124,124,128,128,131,131,132,134,131,131,129,126,124,121,120,120,118,118,120,120,121,123,124,
126,126,128,129,131,131,132,134,134,134,136,136,136,134,132,132,129,128,126,124,124,124,124,124,129,136,137,142,142,142,142,137,134,128,121,116,112,108,108,107,
107,108,112,115,116,120,121,124,126,129,131,134,137,139,142,142,144,144,142,140,139,137,134,132,129,128,126,124,124,124,129,131,132,136,134,136,134,131,129,126,
121,120,115,113,112,112,112,112,113,115,116,120,120,121,124,126,129,131,134,136,137,139,140,142,142,142,140,139,137,134,132,131,129,128,126,128,129,131,134,134,
134,134,134,132,129,124,123,118,115,113,112,112,112,112,113,115,116,118,120,121,123,124,126,128,131,132,134,136,137,139,140,140,140,139,139,137,136,134,132,131,
131,129,131,131,131,131,131,129,129,128,124,123,120,118,116,115,116,116,118,120,121,124,126,128,128,129,128,128,128,128,128,128,128,129,131,132,134,134,136,136,
134,134,134,134,134,132,134,136,137,140,142,142,140,137,132,129,123,118,113,108,107,104,104,105,107,110,112,113,116,120,123,126,129,132,136,137,140,142,144,144,
144,144,144,142,140,137,134,132,129,126,123,121,120,121,124,128,131,132,134,132,131,129,126,124,121,118,115,113,113,115,116,118,121,123,126,128,129,131,131,132,
132,131,132,131,132,132,132,132,132,131,131,131,131,131,132,132,132,132,134,139,142,147,148,147,142,136,129,124,118,112,107,102,100,100,104,107,110,115,116,120,
121,121,123,124,126,128,131,134,137,142,145,148,150,150,148,147,144,139,136,131,129,128,126,128,131,136,142,145,145,142,136,128,120,112,107,102,97,97,97,100,
105,110,115,118,120,121,121,123,123,126,128,132,137,142,147,150,153,153,153,150,147,142,139,134,132,131,129,129,129,132,137,144,148,147,142,132,123,112,104,97,
92,89,88,89,94,100,107,113,118,121,124,126,126,128,131,134,137,142,145,150,155,158,160,160,158,155,150,145,142,137,132,129,126,126,128,134,140,142,140,134,
124,115,107,99,92,88,84,84,88,94,102,110,116,121,124,128,131,132,134,137,139,142,147,150,153,155,156,155,153,152,147,142,137,132,129,128,128,126,131,139,
150,161,166,161,148,131,112,94,81,73,68,68,73,83,94,104,112,116,120,120,121,123,126,129,136,140,147,155,161,164,168,166,163,160,153,147,140,134,131,129,
129,129,132,140,152,164,169,161,144,118,92,72,60,56,59,65,73,86,99,112,120,123,123,121,120,123,128,136,144,152,160,169,174,177,177,172,166,158,150,142,
134,129,126,124,124,126,132,144,158,169,169,153,121,88,59,44,46,56,72,84,99,112,121,126,124,120,113,110,115,124,137,148,156,163,168,171,174,172,169,163,
155,148,142,136,131,129,128,129,129,134,145,160,174,174,158,123,84,54,41,44,59,73,86,94,105,113,120,120,113,107,105,112,126,140,152,158,161,164,169,172,
174,171,164,158,150,144,139,136,134,132,134,131,137,152,171,182,171,137,89,49,33,41,64,84,97,100,102,107,112,113,105,96,94,104,124,145,156,160,156,155,
161,169,176,176,171,163,156,152,148,142,139,136,136,136,137,148,164,180,174,144,96,51,32,40,64,88,99,100,97,100,105,104,97,91,92,110,134,153,160,156,
150,148,155,166,172,172,168,163,158,155,147,140,134,134,136,140,137,147,161,180,180,148,99,49,32,44,75,99,105,97,92,92,97,96,88,83,91,113,139,155,
155,147,140,144,158,171,177,176,172,169,166,161,152,140,136,137,142,147,140,148,163,180,177,139,84,41,33,57,89,107,104,94,91,94,94,86,78,84,105,134,
153,156,147,134,134,142,156,166,172,174,171,168,160,150,140,136,140,145,150,144,142,163,182,192,155,99,51,36,60,89,108,107,99,94,92,88,78,73,83,105,
131,147,150,140,134,131,137,147,158,168,174,174,169,158,148,137,136,142,150,152,150,136,150,177,192,172,110,60,41,64,97,112,115,107,102,94,80,68,65,83,
110,132,145,147,140,131,126,131,139,155,168,176,174,168,158,150,140,137,144,150,152,152,139,152,180,192,172,110,64,46,64,92,105,110,105,99,89,73,65,67,
84,112,129,142,144,139,134,128,132,144,156,171,177,176,168,156,147,139,139,144,153,153,148,136,147,185,190,163,99,54,54,75,100,108,112,110,100,84,64,59,
72,92,116,132,142,140,136,129,129,136,148,163,176,179,174,166,156,148,140,144,152,156,150,144,136,166,195,184,139,75,52,59,81,97,102,113,107,89,68,52,
64,83,107,124,136,144,142,137,131,136,145,156,169,179,182,176,163,153,142,137,140,148,155,148,144,126,158,198,187,134,67,51,62,76,89,100,115,113,89,65,
57,65,84,104,124,140,148,145,134,132,140,152,163,176,184,180,174,163,152,142,140,147,155,150,144,136,129,182,195,161,104,59,56,56,70,89,110,118,104,80,
62,59,67,84,110,136,150,153,148,139,134,139,152,169,182,185,184,174,160,144,137,142,152,153,148,145,129,177,200,160,105,62,62,59,60,91,115,121,102,76,
64,60,64,84,108,136,150,150,145,134,132,137,150,166,182,188,182,174,160,145,137,140,150,156,148,147,134,139,195,184,139,94,70,56,41,65,100,116,116,100,
80,64,56,67,89,120,145,155,153,145,134,128,137,153,171,184,187,182,169,153,139,139,145,153,150,153,145,136,196,188,134,91,70,68,44,67,113,126,108,91,
83,64,52,67,96,120,139,152,153,142,131,126,134,152,168,182,188,185,172,156,142,137,144,150,152,150,152,134,190,188,144,104,80,62,28,57,91,105,110,112,
100,70,64,65,73,99,131,153,158,158,147,134,131,139,153,172,184,188,184,168,153,139,139,142,145,148,156,137,188,196,152,121,94,70,30,48,76,94,104,112,
108,86,67,64,70,86,112,142,160,160,155,145,134,134,145,164,177,187,190,179,161,145,139,139,142,140,155,142,179,201,160,137,107,76,33,43,64,76,96,112,
108,91,81,67,62,75,100,124,147,163,160,152,142,139,142,153,172,182,188,187,174,155,148,140,142,137,156,140,168,209,164,152,123,91,41,36,59,60,83,105,
112,97,86,78,64,68,91,113,137,158,160,158,150,144,142,148,166,172,187,187,179,164,153,150,140,136,152,142,155,203,172,160,144,108,62,46,54,51,72,88,
97,97,86,76,62,62,73,92,118,140,153,161,158,148,153,153,160,171,187,185,182,180,169,155,150,147,147,140,152,195,171,169,158,123,78,57,56,36,59,78,
83,91,94,84,70,73,72,81,107,123,142,153,158,160,152,156,161,164,174,184,184,176,174,168,156,148,153,145,158,190,172,174,160,126,88,65,56,44,57,73,
80,89,91,81,72,70,68,76,97,116,129,150,155,155,158,160,161,164,174,177,179,179,174,168,164,152,155,147,150,187,166,176,171,136,102,81,60,36,56,60,
67,89,97,88,89,88,75,83,96,107,118,142,148,152,161,163,161,164,176,168,172,174,169,163,163,155,152,145,163,185,160,179,164,126,104,83,59,46,60,57,
72,86,88,88,88,84,80,88,96,107,124,132,147,156,160,164,169,174,171,179,176,169,169,164,158,152,156,142,153,187,155,176,169,129,112,88,64,46,56,54,
62,80,83,88,88,86,84,89,94,107,118,129,145,152,160,168,174,177,180,182,179,174,169,166,160,153,153,148,144,184,161,160,176,134,108,92,67,48,52,54,
57,73,81,84,88,86,86,89,94,105,116,129,140,152,160,171,176,179,185,182,179,176,172,164,163,155,156,147,155,185,160,168,164,126,102,81,60,48,51,52,
64,75,80,86,84,81,84,89,97,105,120,134,142,155,166,171,179,185,184,185,182,174,171,166,160,153,155,147,145,182,166,158,164,132,99,80,62,46,48,52,
62,75,81,88,89,86,86,92,96,105,120,129,142,155,163,174,179,184,185,184,180,174,168,163,158,153,152,150,140,174,180,155,160,145,102,75,65,49,44,54,
65,76,84,91,92,89,86,91,99,107,118,131,144,153,163,171,179,180,184,184,179,172,166,163,156,152,147,150,139,163,185,158,152,148,107,70,62,48,40,51,
62,76,88,94,97,94,91,94,99,108,121,134,145,156,164,172,177,179,182,182,177,172,166,158,152,152,144,140,142,139,171,182,155,142,139,96,56,52,49,43,
54,72,84,94,99,99,96,94,94,102,113,124,137,150,160,169,176,182,185,185,184,179,172,164,158,152,150,142,142,137,147,177,168,142,140,126,76,49,51,43,
38,54,75,86,94,102,105,100,99,105,113,118,129,145,153,161,171,177,180,182,182,179,172,164,155,148,145,142,136,137,136,142,172,176,148,140,134,92,52,48,
46,40,46,67,88,99,104,110,110,107,105,112,116,124,137,148,160,169,179,187,188,187,184,177,168,156,150,145,139,132,136,134,136,169,176,148,139,140,102,56,
48,48,36,36,57,81,96,107,116,123,123,120,121,124,128,132,140,150,158,168,176,179,180,179,174,164,155,147,139,131,131,128,128,128,137,171,177,153,150,150,
110,65,56,49,33,33,52,75,91,107,123,129,128,126,126,124,124,128,136,145,156,168,177,184,185,182,179,171,158,145,137,131,124,121,123,128,126,153,180,164,
148,156,136,83,56,52,35,22,35,59,81,102,121,137,145,144,139,136,131,128,129,136,145,155,166,176,182,182,179,174,163,150,137,129,124,120,115,121,124,137,
172,176,153,156,153,107,65,57,43,22,24,46,68,91,113,132,147,150,145,142,139,132,128,131,139,148,158,168,177,182,182,177,171,160,148,134,123,120,118,110,
116,123,128,158,177,160,155,161,129,81,64,54,28,22,38,60,81,105,129,147,153,152,150,142,134,131,129,131,140,150,160,169,177,179,177,172,161,150,139,128,
120,118,115,113,121,124,152,177,166,155,169,148,92,70,65,33,14,30,48,64,89,115,136,150,153,150,150,144,134,131,132,134,140,150,160,171,177,180,180,176,
169,158,144,132,126,118,110,113,118,120,152,168,153,156,168,136,96,81,67,33,24,32,44,60,83,105,129,142,147,152,153,148,142,139,139,140,142,147,160,169,
171,176,180,177,169,161,153,140,129,124,116,116,113,124,153,152,145,161,160,123,102,92,65,36,33,36,38,51,73,92,112,126,136,144,148,147,145,145,144,144,
147,155,163,168,176,184,184,184,180,172,160,152,140,129,121,120,115,126,147,139,147,161,150,128,116,100,72,49,36,33,30,35,52,70,88,105,121,131,140,145,
144,147,145,147,150,156,163,171,179,187,192,193,192,184,174,166,155,140,136,129,120,129,129,126,134,134,126,118,112,97,80,70,57,46,43,43,46,51,62,72,
80,92,102,112,120,131,139,147,160,169,180,192,201,209,217,220,220,219,214,206,198,187,174,163,152,140,129,121,113,105,97,89,81,73,65,59,51,46,43,40,
40,41,46,51,59,67,78,89,102,115,129,142,155,168,180,190,200,208,212,216,217,217,214,208,201,193,184,172,163,152,142,132,123,115,107,100,94,88,81,75,
70,67,64,62,60,62,64,67,72,78,84,91,99,108,116,126,134,144,152,158,166,171,176,180,182,184,184,182,180,177,172,169,164,158,153,148,144,139,134,128,
123,116,112,105,100,96,91,86,83,81,80,80,80,81,84,88,92,99,104,110,118,124,131,139,145,150,156,161,164,168,169,169,169,168,166,164,161,158,155,152,
148,144,140,136,131,126,121,115,110,105,100,97,92,89,88,86,86,86,88,89,92,97,102,107,113,120,126,132,137,144,148,153,158,161,163,164,164,164,163,161,
160,156,155,152,147,144,139,136,131,126,123,118,115,110,107,104,102,99,97,97,97,97,99,102,104,107,110,113,118,121,126,129,134,137,140,144,145,147,148,150,
150,150,150,150,148,148,147,145,142,140,137,134,131,128,124,121,118,116,113,112,110,108,107,107,108,108,110,112,113,116,118,121,124,126,129,131,134,136,136,139,
139,139,140,140,140,140,142,140,140,140,139,137,136,134,131,129,126,124,121,120,118,116,115,115,113,115,115,115,116,118,120,121,123,126,128,129,131,132,134,134,
136,136,136,136,137,136,137,139,139,139,140,140,139,139,137,134,132,128,126,123,118,116,115,113,112,110,110,110,112,113,113,116,118,120,121,124,126,129,129,134,
134,137,137,140,139,142,140,142,140,142,144,140,144,142,144,140,140,139,136,132,129,124,121,118,113,112,108,107,107,105,107,107,108,112,113,116,118,121,124,126,
129,131,134,136,137,139,140,142,142,142,144,142,145,145,145,147,145,147,144,144,139,136,131,126,121,116,110,107,104,100,100,99,100,100,104,105,110,112,115,120,
123,126,131,134,139,142,144,147,148,150,150,150,148,148,145,147,144,144,142,140,140,139,136,132,129,124,120,113,110,104,102,99,97,99,99,102,104,107,110,115,
118,121,128,129,134,136,139,140,144,145,147,147,148,145,148,145,145,144,145,145,145,148,147,150,147,145,142,136,128,121,110,102,94,88,84,81,83,84,89,94,
100,107,113,120,126,131,136,140,144,148,152,153,156,158,160,158,158,156,153,150,145,145,140,144,144,145,148,148,150,145,144,132,124,112,96,84,72,64,57,56,
59,62,72,83,94,107,118,129,137,147,153,158,163,166,171,172,174,174,172,171,168,163,158,152,150,144,145,142,144,144,142,140,137,132,121,112,99,86,73,64,
56,51,51,54,60,70,81,96,108,121,134,145,156,164,171,177,180,184,184,184,182,179,174,169,161,155,147,142,136,136,132,134,134,134,134,131,128,118,110,96,
84,73,62,56,52,51,56,64,73,88,100,115,128,140,150,160,168,172,179,180,182,182,182,177,176,169,163,156,147,142,139,132,136,132,136,137,137,136,132,126,
116,107,92,80,68,60,54,52,56,60,70,81,96,108,123,134,147,156,164,172,176,180,182,182,182,179,176,171,164,156,150,142,137,134,129,134,131,137,137,139,
137,132,124,113,100,86,75,60,56,49,51,54,62,75,86,102,115,131,142,153,163,169,176,180,184,184,185,180,179,174,166,161,152,145,136,136,131,132,137,134,
142,137,139,131,124,112,99,88,72,65,54,52,52,56,62,73,86,97,113,126,137,152,158,171,176,182,185,187,188,185,182,176,171,160,153,145,137,128,131,124,
131,136,132,142,134,136,124,118,104,92,81,70,65,59,60,60,68,73,83,94,105,118,129,140,152,161,169,177,179,185,184,184,180,176,169,160,156,144,140,131,
131,134,131,145,139,145,142,136,126,116,105,91,84,72,67,62,59,59,62,65,73,83,96,108,121,136,147,161,169,179,187,190,195,193,190,185,179,171,161,152,
142,137,126,129,131,128,144,132,139,131,123,113,102,96,83,80,75,70,70,65,67,70,70,83,91,102,118,128,144,152,164,171,180,185,187,190,187,185,177,172,
163,153,148,137,134,126,139,131,144,147,137,137,123,112,99,92,80,76,73,68,68,67,67,67,73,78,92,102,118,131,142,155,163,171,179,184,185,192,188,184,
182,171,163,155,145,137,134,124,126,137,132,145,145,136,131,116,105,92,91,83,76,83,75,75,73,72,72,78,88,94,115,126,134,148,155,161,169,174,177,182,
180,182,177,169,163,156,145,142,136,131,128,131,150,142,156,153,139,128,110,96,83,81,78,76,80,80,73,75,72,72,80,91,104,120,132,145,152,160,163,168,
176,176,182,182,179,176,166,158,150,144,137,134,136,124,153,155,150,164,144,126,107,94,76,76,80,78,88,88,88,84,83,81,88,97,108,121,134,140,145,150,
152,153,160,163,164,169,169,163,160,152,145,142,139,137,140,137,134,174,166,164,176,147,121,96,80,64,65,68,76,88,88,88,84,78,76,83,94,110,126,139,
147,152,153,155,161,163,171,176,176,179,171,163,153,147,140,139,137,140,137,139,182,166,163,168,129,99,75,62,52,59,76,86,91,97,91,81,80,78,89,108,
123,139,148,150,148,144,148,152,158,172,177,177,179,169,156,148,140,137,139,139,139,145,131,171,177,160,166,139,105,72,65,56,60,76,89,99,96,94,83,76,
75,84,105,121,140,148,150,147,142,140,147,156,166,176,180,177,169,160,144,139,140,140,140,148,148,136,188,179,163,161,126,92,56,62,52,64,88,99,100,96,
91,73,70,75,88,110,132,147,152,148,142,136,137,144,158,171,179,185,179,169,156,144,136,137,142,144,145,148,134,164,185,156,160,131,100,62,59,64,65,89,
102,108,99,92,80,72,76,89,112,132,147,152,150,137,132,134,137,156,168,177,179,177,163,153,142,134,132,139,145,144,145,144,131,172,176,156,155,121,92,57,
64,72,78,104,113,108,99,86,76,70,78,99,118,139,150,153,144,132,128,129,136,152,169,177,179,174,163,150,139,132,132,140,145,147,152,139,139,176,164,155,
144,123,91,64,75,73,81,96,107,102,89,83,72,68,78,97,116,139,148,148,147,136,131,134,145,156,177,184,179,176,161,148,140,131,139,140,150,148,145,140,
139,163,142,145,134,120,99,81,89,75,86,92,92,99,94,96,88,88,96,97,112,128,136,142,147,145,142,145,147,152,160,163,166,160,158,152,140,139,134,132,
131,132,136,129,145,153,140,147,136,126,110,104,107,99,100,108,107,107,107,105,102,99,104,107,110,120,126,128,132,132,134,136,142,144,147,150,153,152,150,148,
142,142,136,134,134,132,131,131,136,137,136,139,134,132,124,120,118,110,112,108,108,110,107,110,108,108,110,112,113,115,118,121,124,128,132,134,137,139,142,144,
144,147,145,145,144,144,140,136,137,134,132,131,129,132,129,129,129,126,124,121,121,118,116,118,116,118,118,118,118,116,116,118,118,120,120,123,126,128,131,132,
136,136,136,139,137,139,139,139,139,139,137,136,134,132,129,128,126,124,124,123,124,124,124,126,124,126,124,124,124,123,123,123,123,
};
static const uint8_t CLIP11[] PROGMEM = {
136,136,136,134,132,131,129,126,124,123,121,121,120,120,120,120,120,121,121,121,123,123,124,126,126,128,129,129,131,132,134,136,137,139,140,142,142,142,140,139,
136,132,129,124,121,118,115,113,112,112,113,113,115,116,120,121,123,124,126,128,129,131,132,132,134,136,137,137,139,142,145,148,150,150,148,147,144,139,131,123,
115,108,102,99,96,96,96,99,102,107,112,116,121,124,128,131,134,137,140,142,145,147,150,152,152,153,155,158,158,156,153,148,142,136,126,116,107,99,92,88,
86,84,86,89,94,100,107,112,118,124,129,134,139,142,145,148,150,153,155,156,156,155,156,160,163,163,163,158,152,145,137,126,113,100,89,80,75,73,73,76,
81,88,97,105,115,121,128,134,139,142,145,148,152,153,156,158,160,161,161,163,164,168,169,168,163,155,147,136,124,108,94,81,72,65,64,64,67,73,81,91,
102,112,120,128,134,140,145,150,155,156,160,161,164,164,166,164,164,166,171,172,171,164,156,145,134,121,107,92,78,68,64,62,64,68,75,83,92,102,112,120,
128,132,137,144,148,152,155,158,161,164,166,168,168,168,169,174,177,176,168,158,147,136,121,105,88,73,62,56,56,57,62,70,80,91,102,113,121,129,136,140,
145,150,155,158,161,163,166,168,169,168,166,168,171,176,176,171,160,147,134,120,105,88,72,60,56,56,60,65,73,84,96,107,116,124,129,134,139,144,148,152,
156,160,163,164,168,168,168,166,164,168,174,177,172,163,148,134,118,102,84,68,57,54,56,60,68,76,88,97,108,116,123,128,132,137,142,147,152,156,161,166,
168,169,169,169,166,163,164,169,176,176,168,152,136,120,104,86,68,56,51,52,59,67,75,84,96,107,116,123,128,132,137,142,147,153,158,163,166,168,168,169,
168,166,161,160,166,174,179,171,155,134,115,96,78,62,52,49,57,67,78,88,96,102,108,115,120,124,128,132,137,144,150,156,161,166,168,168,168,166,163,161,
155,158,164,177,182,174,156,137,120,100,83,65,54,52,57,67,76,84,92,99,104,108,113,118,123,129,137,144,152,156,163,166,166,166,164,168,164,164,156,161,
172,188,188,169,142,113,96,72,57,46,54,67,84,94,99,100,100,100,100,104,112,123,132,140,147,150,155,156,160,160,161,161,163,164,163,161,153,161,179,198,
193,166,134,104,84,60,51,49,67,81,94,97,97,92,89,88,89,100,115,129,139,145,148,148,150,153,158,164,166,169,168,169,163,161,153,153,172,190,203,174,
147,108,88,59,46,46,60,81,92,100,96,94,83,81,84,97,115,131,144,147,148,145,148,148,155,161,169,171,171,169,166,158,153,147,164,190,204,188,150,118,
84,65,40,48,60,86,96,102,97,89,80,73,78,91,113,131,145,148,150,144,144,142,152,160,171,174,177,174,169,161,153,148,155,187,201,203,160,132,91,68,
40,40,57,80,99,100,104,89,80,67,72,86,107,132,145,156,150,147,139,137,147,155,171,172,179,172,172,161,153,148,144,179,201,211,172,137,104,67,41,32,
60,80,100,104,105,91,73,59,60,78,105,131,148,156,153,144,136,134,142,156,174,182,185,180,172,163,152,148,147,148,192,204,203,160,129,92,51,35,35,67,
80,100,104,102,86,65,57,62,84,108,134,155,160,156,142,137,136,145,158,174,187,188,182,174,168,150,150,142,176,203,204,185,144,118,62,43,30,59,72,89,
100,102,89,64,56,59,73,94,124,150,158,152,145,137,132,137,155,172,187,188,185,180,164,158,145,155,145,182,200,195,176,140,113,57,43,40,54,70,88,105,
104,91,80,70,67,75,94,120,139,152,155,148,139,136,139,147,161,176,184,182,179,176,163,155,155,147,184,195,187,177,148,116,65,54,49,52,64,81,94,92,
84,75,72,68,76,94,116,132,144,150,152,145,142,148,152,163,172,177,179,177,174,161,156,153,150,184,185,182,172,150,120,76,65,51,51,60,78,89,96,97,
91,80,80,80,89,105,121,136,142,147,148,145,148,153,161,166,172,177,177,174,169,164,158,158,184,182,184,174,161,131,97,84,68,62,60,68,73,76,75,75,
70,72,73,84,97,112,123,132,140,144,150,155,163,168,177,180,184,184,182,176,169,164,164,179,171,176,161,152,123,100,83,62,62,51,59,60,68,73,72,76,
78,78,84,92,105,113,123,134,142,148,155,163,171,176,184,192,195,195,193,193,188,187,185,180,177,169,160,148,134,120,105,91,80,70,62,57,54,52,54,56,
60,65,73,80,89,99,107,116,128,137,147,155,164,171,176,180,182,184,182,182,180,177,172,169,164,156,150,142,134,124,116,110,102,96,91,86,84,83,83,83,
84,88,89,94,99,104,110,115,121,128,134,139,144,148,153,155,158,160,160,160,160,160,158,156,153,150,147,144,139,136,131,126,123,118,115,112,110,107,105,105,
104,104,105,105,107,108,112,115,116,120,123,126,129,131,134,136,139,140,140,142,142,142,142,140,140,139,137,136,134,131,129,128,126,124,123,121,120,120,120,120,
120,120,121,121,123,124,126,126,128,129,129,131,131,132,132,132,134,134,134,134,134,134,132,132,131,131,129,128,128,126,124,124,123,123,121,121,121,121,121,121,
121,123,123,124,126,126,128,128,129,131,131,131,131,132,132,132,132,131,131,131,131,129,129,129,128,128,128,126,126,126,126,126,126,126,126,126,126,126,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,129,128,128,129,128,129,128,128,129,128,128,128,126,128,
126,128,128,128,128,128,128,129,128,129,128,128,129,128,128,126,126,126,126,126,128,128,126,128,126,126,126,126,126,126,124,128,128,128,128,126,128,128,128,128,
126,128,128,129,128,129,128,129,128,128,128,128,128,126,126,126,124,124,124,124,124,126,126,128,128,128,128,128,128,128,126,126,128,126,128,126,128,128,128,128,
128,129,128,129,129,128,128,126,128,128,128,128,128,129,128,129,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,126,126,126,126,126,128,128,128,128,128,128,129,128,128,128,129,131,131,129,129,129,129,129,128,129,128,128,
128,128,128,128,128,128,128,126,126,126,126,126,124,126,124,126,126,126,126,126,126,128,128,128,129,129,131,132,132,132,132,132,132,132,131,131,129,129,128,128,
126,126,124,124,123,123,123,123,123,123,124,124,126,126,126,126,128,128,129,129,129,131,132,134,137,139,140,140,139,137,136,134,131,128,124,121,120,118,116,116,
118,118,118,118,120,121,123,124,126,128,128,129,129,131,132,132,132,132,132,134,134,134,137,140,145,147,147,145,142,139,132,126,118,115,112,110,108,108,108,110,
112,113,115,115,116,118,120,123,124,128,131,132,136,139,142,145,145,147,147,147,150,153,158,160,156,152,144,136,126,115,105,96,89,86,84,86,91,96,102,108,
113,120,123,126,129,132,136,139,142,145,150,152,153,153,153,153,152,148,147,148,153,156,155,148,139,128,116,105,94,84,78,76,80,84,92,99,107,115,123,128,
132,136,136,139,139,142,145,150,152,155,156,156,153,152,148,145,142,142,145,153,158,156,147,131,115,100,88,75,67,62,67,73,86,97,107,115,121,128,132,136,
137,139,142,147,152,155,158,161,161,160,158,156,153,150,145,144,150,163,176,174,161,134,113,91,76,62,52,48,56,68,84,94,102,107,112,118,123,128,129,134,
140,147,155,161,166,171,172,172,168,164,160,156,152,145,145,155,166,177,171,153,126,104,83,67,52,44,44,54,70,84,96,104,110,115,118,123,128,132,137,144,
150,156,163,168,171,171,169,166,161,160,153,152,144,147,160,176,185,172,150,118,96,73,59,44,43,48,64,81,92,100,104,108,108,113,116,123,128,137,145,155,
161,168,171,174,174,171,168,161,160,153,152,145,144,156,172,185,174,153,121,99,75,59,44,40,48,62,81,92,102,105,108,108,110,113,120,128,137,145,152,158,
166,169,172,171,168,163,160,158,155,152,147,142,155,169,187,180,160,129,102,80,57,44,40,48,64,81,94,104,108,108,107,104,107,115,124,136,144,153,160,168,
171,174,171,169,166,164,160,156,152,150,145,155,169,185,184,161,131,99,76,51,41,35,52,68,91,100,107,107,102,97,92,100,108,126,137,153,160,164,168,166,
168,164,168,166,164,160,156,155,150,148,140,161,177,195,177,150,118,88,64,38,33,38,64,81,96,102,108,105,96,88,91,100,115,128,144,155,164,168,169,169,
169,171,169,166,163,161,160,155,148,145,145,171,185,193,163,140,107,76,48,30,38,49,73,86,104,108,107,96,83,84,91,107,121,140,155,161,166,166,169,168,
168,171,172,171,164,163,158,152,145,145,140,166,180,188,164,145,115,76,46,25,33,36,59,73,100,104,107,99,92,88,89,104,118,137,148,164,169,174,172,174,
176,174,174,172,169,164,163,155,150,145,144,177,185,187,156,137,100,60,33,24,38,44,68,86,104,104,97,89,81,83,91,107,128,145,160,168,174,172,172,176,
176,179,179,180,171,168,163,153,144,142,137,148,176,171,171,150,136,81,54,35,19,25,36,64,80,99,105,102,99,92,94,100,115,132,148,164,171,176,177,179,
176,179,179,180,174,172,168,158,152,147,140,144,184,169,177,160,140,89,57,35,8,19,24,51,68,96,99,102,99,94,91,99,112,124,147,158,172,177,182,182,
184,188,184,188,187,177,174,166,156,142,147,126,156,171,155,176,147,124,75,60,25,9,22,28,52,75,96,104,105,105,99,100,104,118,134,145,161,171,174,176,
177,177,177,180,180,176,171,169,160,150,147,140,136,177,166,171,177,144,112,70,51,11,14,22,36,62,86,102,105,110,100,97,97,105,118,137,148,164,172,172,
179,176,177,179,184,180,180,177,168,158,152,140,139,128,174,164,160,185,137,112,70,54,12,14,38,36,78,104,113,120,120,108,96,100,104,113,132,147,156,163,
164,163,163,163,166,169,174,169,168,166,152,147,140,142,124,182,177,160,200,145,116,80,59,17,19,44,43,81,110,113,118,120,102,91,96,100,110,131,147,153,
160,164,158,158,161,163,168,172,171,166,164,152,148,136,142,131,145,203,156,188,179,123,94,62,40,8,41,51,68,112,123,124,121,115,91,88,97,99,123,140,
148,158,160,156,152,156,155,160,171,171,169,166,161,147,140,145,131,142,206,160,185,192,126,100,70,41,11,40,52,64,108,124,120,120,113,89,86,94,100,118,
139,150,156,158,155,152,152,155,160,168,171,169,163,160,147,142,136,142,128,182,196,158,209,152,110,83,49,25,16,54,59,88,126,123,120,115,99,81,88,97,
108,131,150,155,158,158,152,150,152,160,164,171,174,169,161,153,144,137,131,144,128,182,193,158,204,148,108,81,54,24,27,57,60,96,124,118,118,113,92,84,
89,97,116,136,147,156,158,152,148,148,145,156,166,168,174,168,160,153,145,134,144,136,144,212,164,188,193,123,99,67,46,14,46,64,68,113,121,110,107,99,
76,76,94,102,124,147,156,158,156,153,147,148,156,163,171,179,174,166,160,148,139,136,144,134,148,214,160,190,187,116,100,65,44,22,57,64,76,118,112,104,
102,86,72,80,92,107,136,148,153,161,148,145,147,144,155,169,172,180,179,164,156,148,134,137,142,142,139,196,182,172,203,136,113,89,56,43,52,68,72,97,
105,88,86,76,60,65,80,102,120,144,158,150,156,148,144,155,156,171,182,187,184,177,168,152,150,142,142,150,140,192,184,174,192,137,110,73,54,33,41,65,
65,96,100,91,84,75,60,65,80,96,128,140,156,163,156,153,155,153,161,176,180,190,190,180,166,160,145,144,139,147,140,160,195,158,190,158,112,96,64,40,
40,62,57,86,97,89,86,80,64,60,78,84,113,137,150,160,163,158,153,158,160,171,180,185,192,184,172,164,150,142,145,139,142,147,184,169,177,179,124,107,
75,46,40,49,56,73,91,94,89,81,72,60,68,78,102,124,144,163,160,163,161,153,161,171,174,187,193,187,180,172,156,145,148,136,145,137,166,182,161,185,
145,112,91,54,41,40,54,64,81,96,89,86,78,64,64,75,91,115,139,155,163,164,160,161,160,164,176,182,190,188,187,176,160,155,144,139,144,137,148,184,
161,180,169,121,105,72,44,40,48,54,73,86,92,86,83,70,62,70,81,102,126,147,158,163,166,161,160,168,169,179,190,187,192,180,169,158,145,145,134,142,
137,148,179,158,179,155,121,99,64,49,36,52,54,73,92,88,92,84,70,68,73,83,105,129,145,158,168,163,163,164,166,172,180,187,190,188,179,169,156,147,
144,136,142,134,155,172,156,182,148,121,102,62,46,43,44,54,73,84,91,89,84,76,68,75,86,107,128,147,160,164,168,164,166,169,172,182,187,187,188,177,
168,156,147,140,137,142,129,161,168,156,184,144,123,97,64,44,38,46,48,70,83,88,92,86,80,75,80,89,104,128,144,153,168,168,169,172,174,179,182,188,
188,182,179,166,156,150,142,137,139,131,161,160,160,180,136,126,96,57,48,36,40,49,68,80,88,96,86,83,80,78,92,107,124,145,156,168,172,176,174,179,
180,184,187,185,184,174,169,153,147,144,129,139,126,144,160,145,177,147,132,113,75,57,36,32,30,46,62,76,94,100,100,104,102,100,110,121,128,145,156,164,
177,179,185,185,185,187,182,179,171,161,153,145,140,132,137,123,150,158,148,182,145,136,115,75,57,35,33,27,48,60,73,97,100,105,108,107,107,115,120,129,
142,152,163,171,179,184,188,188,187,185,177,171,158,153,140,139,129,131,124,136,156,139,179,150,142,132,86,73,44,33,27,36,49,65,88,102,108,116,118,113,
123,121,128,137,142,156,164,174,180,185,188,185,187,176,171,161,148,145,134,129,129,124,126,156,140,168,169,140,145,104,80,57,36,25,32,41,52,83,91,110,
120,118,128,121,124,124,132,134,147,158,164,180,185,188,195,188,184,179,161,155,147,131,132,126,123,123,147,144,155,176,140,153,116,88,72,40,32,27,36,44,
73,86,102,120,118,128,126,124,126,129,131,142,153,158,177,180,187,196,188,188,182,169,158,148,137,129,128,124,116,140,147,142,180,148,148,139,94,83,52,33,
22,32,32,56,80,89,118,123,129,134,131,129,129,129,132,144,152,168,177,188,195,196,196,187,179,163,152,144,128,126,121,121,116,145,137,152,176,140,156,124,
96,80,52,35,27,36,35,64,76,92,115,118,129,129,129,129,128,131,136,147,155,171,180,190,198,196,198,185,177,164,152,140,132,124,123,118,134,145,137,174,
148,148,142,96,92,57,38,30,27,33,49,68,84,104,116,124,128,132,128,129,131,134,145,153,168,179,188,200,198,200,193,180,171,156,144,136,126,120,124,112,
142,136,142,174,132,155,124,92,89,48,41,28,30,40,54,73,92,110,118,131,132,132,134,131,132,140,145,156,171,180,192,200,198,196,192,176,168,152,139,131,
123,120,118,116,142,131,153,161,137,153,112,97,78,48,40,30,33,41,60,76,94,112,120,129,132,131,134,131,137,142,147,164,171,185,195,196,201,195,187,176,
161,152,136,129,121,116,118,116,144,128,161,152,136,150,99,99,65,44,38,27,38,44,68,80,104,116,123,136,132,136,134,134,137,145,153,164,179,185,198,196,
200,195,180,176,156,147,134,124,118,118,110,128,137,128,169,134,148,134,96,99,56,48,36,30,40,51,70,86,108,116,131,132,136,139,134,140,140,148,158,169,
179,190,195,196,196,187,179,164,152,139,128,121,113,116,107,132,129,137,164,129,152,121,94,92,48,48,35,33,46,60,81,97,116,128,136,140,142,137,142,140,
144,153,160,171,179,188,192,190,188,179,169,156,140,134,121,115,113,110,108,126,132,132,164,134,147,132,94,99,57,49,43,38,51,62,84,99,120,129,137,144,
139,142,140,139,145,150,158,169,176,185,188,187,185,177,166,155,140,131,120,115,110,112,107,134,128,142,163,131,153,116,99,84,54,49,36,44,49,72,89,107,
128,134,144,144,145,140,142,140,145,153,158,172,174,185,187,184,184,171,163,148,136,128,115,113,108,110,107,126,134,132,166,134,148,131,92,99,54,52,44,38,
59,67,89,110,123,136,145,144,145,145,137,144,144,145,160,163,172,184,180,187,179,172,163,147,139,126,116,113,108,108,112,132,129,145,160,132,150,115,92,89,
51,51,48,46,65,83,97,121,132,137,148,142,140,140,136,137,145,147,158,171,169,182,180,174,176,161,152,144,128,124,115,115,110,116,113,132,144,132,171,132,
142,128,89,89,59,49,49,52,64,86,99,123,132,142,148,140,144,136,134,134,137,147,153,166,176,177,185,177,174,163,153,139,131,120,112,116,104,120,110,129,
147,126,169,134,129,131,83,89,70,57,65,73,81,99,121,120,136,140,126,139,128,123,136,129,136,153,150,161,176,161,176,168,155,161,142,142,131,129,124,120,
124,123,120,131,152,124,166,140,123,137,88,89,75,64,64,78,81,96,120,113,132,129,123,126,120,121,124,131,140,148,164,160,177,166,168,169,152,156,142,145,
132,129,137,116,132,123,120,124,139,137,134,164,121,136,118,89,91,70,76,67,84,96,97,123,121,121,131,120,116,123,120,123,136,142,148,163,169,164,176,168,
160,166,147,147,144,126,139,120,128,121,123,121,129,155,120,174,132,128,139,88,100,78,73,78,80,96,100,113,123,120,121,118,118,110,118,121,124,139,142,156,
156,163,169,155,169,153,148,152,137,139,134,137,129,131,136,123,137,155,124,171,142,131,145,99,104,84,76,73,78,86,92,108,108,116,115,116,108,116,112,118,
126,129,145,145,158,163,160,168,160,160,160,148,153,142,139,142,128,139,126,132,123,147,142,137,174,123,155,123,102,112,72,84,80,72,97,89,102,110,102,113,
105,105,107,112,113,128,129,144,152,152,168,156,169,163,160,168,148,160,145,144,147,134,140,136,129,142,145,134,164,132,145,132,105,116,83,86,80,72,86,80,
94,96,105,107,107,118,105,126,115,126,139,129,156,150,160,169,161,171,161,161,158,147,145,137,134,131,129,124,132,123,142,137,140,158,132,155,124,121,113,88,
96,76,80,89,80,102,97,107,116,107,123,112,123,123,126,134,136,147,145,158,155,160,164,153,164,150,153,148,137,142,134,131,139,126,140,144,139,161,140,153,
137,126,115,97,88,73,72,65,72,75,81,92,97,107,113,116,124,124,132,137,137,152,150,161,168,164,177,171,171,171,160,160,147,140,136,126,124,123,115,132,
123,140,144,140,152,132,132,116,100,94,78,73,72,70,78,83,92,99,108,113,116,123,123,126,131,134,139,147,152,161,168,171,177,176,176,174,164,161,153,145,
139,134,126,129,129,129,142,137,144,140,134,128,113,100,88,75,67,64,60,68,73,83,96,100,113,118,123,128,129,132,137,142,148,155,161,169,174,177,177,177,
174,166,161,150,147,137,131,129,126,129,129,137,136,139,136,131,124,113,104,89,83,72,67,67,65,73,78,88,96,102,112,115,124,126,132,137,140,153,152,168,
169,174,184,179,187,179,177,171,163,156,147,142,132,134,124,132,129,128,134,121,129,115,108,102,88,86,73,70,68,67,73,75,83,91,94,105,108,115,123,126,
134,140,145,153,161,168,176,179,184,187,185,185,179,174,168,161,153,150,142,139,139,129,136,128,124,123,110,108,99,89,84,76,72,72,68,70,73,76,83,88,
92,100,105,110,120,123,132,139,144,155,158,168,172,177,182,182,184,180,179,174,169,164,161,156,153,152,145,147,137,136,131,120,118,102,100,89,81,81,68,73,
67,68,72,72,78,78,84,89,96,100,108,118,124,136,142,153,160,168,174,179,184,184,187,185,182,179,174,169,161,156,148,140,134,126,120,113,105,100,96,92,
89,88,88,88,89,92,96,100,104,108,113,118,123,126,132,134,140,142,144,147,145,150,145,148,147,144,147,140,142,137,136,134,129,129,126,126,123,121,121,120,
121,118,120,120,120,120,121,121,123,123,124,126,126,128,128,128,129,129,131,131,
};
static const uint8_t CLIP12[] PROGMEM = {
125,126,128,126,126,128,128,128,126,128,129,129,131,129,128,129,128,131,131,128,126,129,129,128,126,126,128,126,125,125,126,126,125,126,128,126,128,129,129,128,
128,128,128,126,128,129,129,128,128,129,132,131,129,129,131,131,134,135,131,131,132,134,129,126,128,128,128,125,123,125,128,125,123,125,128,125,123,125,126,128,
125,125,128,128,126,125,126,128,126,126,128,129,129,129,131,135,137,135,138,140,140,138,137,135,134,131,128,125,122,119,117,116,116,116,114,114,116,119,119,120,
122,123,125,128,129,131,132,134,135,137,137,137,138,138,140,144,144,144,146,146,143,138,134,128,120,116,111,107,104,104,105,107,108,113,116,120,123,125,128,129,
131,131,132,135,137,137,138,141,143,141,141,143,143,146,147,147,146,146,144,138,131,123,116,108,102,96,93,93,96,99,104,108,116,122,125,128,132,135,135,138,
140,141,143,146,144,144,146,146,144,144,143,141,147,150,147,146,147,143,131,123,114,102,93,87,81,80,83,89,96,105,114,123,131,137,140,143,143,143,144,144,
143,144,147,149,147,149,149,146,144,141,141,147,146,141,141,140,131,119,110,101,89,81,78,78,83,90,101,111,122,132,140,143,144,144,144,141,140,140,141,141,
144,147,147,147,149,146,140,138,138,147,155,147,144,152,143,120,105,93,77,63,57,60,71,84,96,116,134,144,152,155,153,149,144,140,135,137,141,144,149,153,
156,156,153,150,144,138,132,141,155,152,140,146,147,122,95,83,72,54,45,51,68,83,98,117,137,149,153,155,153,147,144,141,140,143,149,153,158,161,162,162,
158,150,143,137,129,132,150,158,143,141,147,126,90,75,65,48,39,47,63,86,105,122,141,153,153,152,149,143,137,135,138,143,149,158,165,168,167,164,158,150,
138,131,128,123,141,164,158,141,149,141,96,65,57,47,33,38,62,90,111,126,146,158,152,146,143,135,128,132,140,147,158,168,174,177,173,164,156,149,135,126,
126,125,147,173,158,141,152,131,77,51,50,35,26,44,72,98,117,135,150,150,144,141,137,128,129,135,141,150,164,174,177,177,173,164,155,146,134,129,128,126,
158,179,152,143,155,119,62,50,48,30,29,54,87,111,125,140,152,144,132,129,128,123,126,138,150,161,170,179,182,174,167,158,149,138,132,126,128,126,149,185,
168,141,158,134,65,48,51,27,26,54,83,107,125,138,149,140,129,128,122,117,126,135,144,159,170,177,182,179,171,164,155,147,137,129,134,128,152,188,164,146,
161,123,63,53,44,21,27,50,77,102,120,137,147,138,131,132,125,117,128,138,144,158,174,180,182,183,177,164,158,150,137,131,134,126,156,180,153,153,162,111,
72,66,41,24,36,51,72,96,113,129,132,126,128,125,116,120,129,134,146,164,173,177,188,186,176,170,167,152,141,140,138,128,165,180,150,165,164,105,78,68,
30,17,29,39,60,87,110,128,134,132,132,128,122,122,129,134,147,162,173,185,191,188,185,174,165,156,146,135,140,126,146,180,149,161,173,116,92,83,38,20,
33,33,47,77,98,113,126,129,129,128,122,123,128,132,143,156,168,179,186,191,188,180,176,164,153,147,144,128,156,173,144,174,165,116,105,77,30,21,20,12,
38,68,86,114,131,135,140,137,131,129,128,134,141,150,165,176,185,191,191,183,180,170,158,149,146,131,137,176,143,159,177,122,114,96,47,36,27,18,35,50,
72,96,110,125,129,132,131,131,132,132,141,149,158,171,180,186,192,188,186,179,168,159,152,140,146,165,143,170,159,128,125,83,53,32,15,8,21,42,63,98,
117,135,146,147,144,140,134,132,134,140,152,159,173,182,186,188,183,177,167,159,146,143,128,144,159,143,173,153,132,122,84,56,39,23,17,36,48,71,104,119,
135,146,146,144,140,135,132,135,138,146,159,167,177,185,185,185,179,170,159,150,138,134,155,137,158,156,135,134,99,77,51,32,20,24,35,54,81,107,128,143,
153,153,149,149,138,138,141,140,153,159,168,177,179,179,174,168,159,149,141,132,134,153,138,165,156,138,138,99,75,56,30,21,30,33,57,87,102,134,143,152,
156,150,149,141,138,140,143,147,161,167,174,182,177,179,170,162,152,143,129,141,146,137,168,141,144,132,95,81,51,33,24,27,38,56,86,104,126,144,147,156,
150,146,146,138,141,147,149,162,168,174,180,177,176,165,159,147,140,134,126,155,134,161,162,134,150,104,89,69,36,32,26,30,53,69,95,120,129,150,149,153,
150,143,141,141,141,147,162,159,180,179,177,183,171,164,155,144,135,128,147,131,147,158,129,144,113,93,80,56,45,41,48,54,75,92,105,125,129,137,140,137,
141,135,144,146,149,164,161,173,174,170,174,162,162,150,143,144,126,152,146,137,171,135,138,132,93,90,74,56,57,62,63,81,93,99,120,110,125,123,113,134,
114,134,138,138,161,162,170,180,180,176,179,167,159,150,147,134,137,143,128,146,134,132,129,111,113,89,89,83,74,86,80,95,99,105,117,114,129,125,128,135,
129,141,137,144,149,144,158,147,153,152,144,149,138,137,135,129,131,135,134,137,140,138,140,134,129,125,113,108,101,95,98,92,102,102,105,119,116,123,128,126,
129,131,129,134,137,138,144,147,147,152,149,150,146,141,141,135,129,135,129,129,138,131,135,134,128,126,116,114,108,102,102,101,99,105,107,108,119,119,123,129,
128,134,135,134,141,138,141,146,143,147,147,146,144,144,141,137,138,134,135,134,132,135,129,129,126,120,119,113,108,107,104,104,107,107,113,116,122,125,128,132,
131,134,135,132,134,135,132,134,134,135,134,134,137,134,135,134,135,132,134,138,132,138,138,135,138,134,132,128,123,119,114,110,108,105,107,108,108,114,116,119,
125,126,131,134,134,137,138,138,140,140,140,138,138,137,135,134,132,129,129,126,128,126,128,128,128,129,129,129,129,128,126,125,123,122,120,119,119,119,119,119,
120,122,123,125,126,128,129,131,134,135,135,137,137,138,137,137,137,134,134,132,131,129,128,128,126,125,125,125,125,123,123,122,122,122,122,122,122,122,122,123,
123,125,125,126,128,129,129,131,132,132,132,134,134,134,132,132,132,131,131,129,129,128,128,128,128,128,128,129,129,129,129,129,128,128,126,126,125,125,123,123,
122,122,122,123,123,125,126,126,128,129,131,131,132,132,132,134,132,132,132,131,131,129,128,128,128,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,126,126,126,126,128,128,128,128,129,129,129,129,129,129,129,129,129,129,129,
128,128,128,128,128,126,126,126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,128,129,129,129,129,129,129,129,129,129,129,129,129,129,129,128,128,128,
128,128,128,126,126,126,126,126,126,126,126,126,126,126,128,128,128,128,128,128,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,
128,128,128,128,126,126,126,126,126,126,126,125,126,126,126,126,126,126,128,128,128,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,129,
129,128,128,126,126,126,125,125,125,125,125,125,125,125,125,125,126,126,126,128,128,128,128,129,129,129,131,131,131,132,132,132,132,132,132,132,132,132,132,132,
131,129,129,128,126,125,123,122,122,120,120,120,120,120,122,123,125,126,128,129,129,131,132,132,134,134,134,132,132,132,131,131,129,128,128,128,128,129,129,131,
131,131,131,132,131,131,131,129,128,126,125,123,122,120,120,120,120,120,122,123,125,125,128,128,129,131,132,132,134,134,134,134,132,132,131,129,128,128,126,129,
131,132,135,138,140,141,141,141,138,135,132,126,122,117,113,110,107,105,105,105,107,107,110,111,116,117,122,125,128,132,135,140,143,144,147,149,150,150,150,149,
147,147,147,146,146,146,146,143,141,140,135,131,126,120,114,108,102,98,93,90,87,87,87,89,90,95,98,104,108,116,122,129,135,143,149,153,159,162,165,167,
167,167,165,164,161,158,155,152,149,146,141,137,132,128,123,117,111,105,101,96,93,90,87,86,86,87,89,92,96,101,105,111,117,123,131,137,141,147,152,156,
159,162,164,164,164,162,161,159,158,155,152,149,146,143,140,135,131,126,122,116,111,107,102,99,96,93,92,90,90,92,93,96,101,105,110,114,120,125,131,135,
141,146,149,152,155,156,156,156,156,156,155,153,152,149,147,144,141,138,134,131,126,122,119,114,111,108,105,102,101,99,99,99,101,102,105,108,111,114,119,123,
128,131,135,140,143,146,147,150,150,152,152,152,150,150,149,147,146,144,141,138,135,132,129,126,123,119,116,113,110,108,107,105,105,105,105,107,108,110,113,116,
119,122,125,128,131,134,135,138,140,141,143,143,144,146,146,146,146,147,146,146,144,143,141,138,135,132,128,125,120,117,114,111,107,105,104,102,101,101,102,104,
107,108,113,116,120,125,129,132,137,140,143,144,146,147,149,147,147,147,146,144,143,141,138,137,134,131,129,126,123,122,120,119,117,117,116,116,116,116,117,117,
119,120,122,123,125,125,128,128,131,129,132,131,134,132,134,134,135,134,135,134,135,134,134,134,132,132,131,129,129,126,126,125,125,123,122,123,122,122,122,122,
122,123,122,125,123,126,126,128,128,129,129,129,129,131,131,132,131,132,132,132,132,132,132,132,132,131,131,129,129,128,128,126,126,125,126,123,125,123,125,125,
125,125,126,125,128,125,128,128,128,128,128,129,129,128,129,129,129,128,129,128,129,128,128,128,128,126,128,126,128,128,128,128,128,128,128,128,128,128,129,128,
129,128,129,128,128,128,128,128,128,128,128,126,128,126,128,126,128,126,128,126,129,126,129,126,129,128,129,128,128,129,128,129,128,129,128,128,129,128,129,128,
128,128,128,128,128,128,128,126,129,126,129,126,129,126,128,128,128,128,129,126,129,126,129,126,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
129,126,128,128,128,128,128,126,128,126,128,126,128,128,126,128,126,129,126,131,125,129,126,129,128,129,128,128,128,128,129,126,129,126,129,128,128,129,125,131,
125,131,126,129,126,128,128,126,129,125,129,126,129,128,128,128,128,128,128,129,126,129,126,129,126,129,128,128,128,126,129,126,129,126,128,128,126,128,128,126,
128,128,129,126,129,126,131,126,129,128,126,129,125,131,125,131,126,129,128,128,131,125,129,126,128,128,126,128,126,129,126,128,128,128,129,128,128,128,128,129,
126,131,126,128,129,125,131,125,129,126,126,129,125,131,125,131,125,128,129,126,131,126,131,126,129,126,129,126,129,126,128,128,128,129,126,129,126,129,125,129,
126,128,128,126,126,128,126,128,126,129,126,129,126,128,128,128,129,126,129,126,129,126,128,128,128,129,126,131,126,131,126,131,126,128,129,125,132,122,132,125,
128,131,123,134,122,132,126,125,132,122,132,123,128,128,125,131,123,129,126,126,128,126,126,129,125,129,126,128,128,128,129,129,128,131,129,129,131,129,132,128,
135,126,132,129,131,128,128,129,125,126,126,125,126,125,128,123,128,125,126,126,125,128,125,128,126,125,129,126,128,128,128,131,126,131,128,129,129,135,132,134,
135,134,134,132,131,129,126,126,123,123,125,122,125,123,122,126,120,126,122,125,126,123,128,126,128,129,128,131,131,129,134,128,132,131,129,132,129,137,135,137,
143,138,141,135,135,129,122,123,116,114,114,111,116,116,117,120,120,123,123,123,126,122,126,125,126,129,129,135,134,135,140,137,141,138,140,138,143,150,150,158,
155,156,150,140,132,119,105,95,86,83,81,83,90,96,104,110,117,120,126,128,131,132,135,138,140,144,149,150,152,155,156,153,152,153,147,155,155,158,161,158,
158,144,134,120,102,90,77,71,68,71,78,84,95,102,110,117,120,126,126,129,134,135,141,146,150,155,155,159,159,159,158,156,156,149,161,167,167,171,164,158,
135,116,101,80,68,60,60,65,69,81,89,96,102,108,113,117,123,129,135,140,147,152,156,162,165,167,165,165,167,162,158,158,152,165,170,171,168,156,141,116,
96,78,65,60,57,65,74,81,87,95,98,102,104,113,119,128,137,143,149,153,159,159,164,165,162,162,161,161,156,155,153,150,170,179,179,164,146,126,96,74,
62,57,60,66,78,89,95,96,95,98,99,104,116,126,138,144,149,156,158,158,156,162,164,161,164,165,164,156,155,147,164,188,188,177,144,117,84,54,47,51,
68,83,95,104,102,95,87,84,89,102,120,138,149,153,153,147,144,146,153,161,164,168,170,167,161,152,149,140,164,197,194,168,125,95,71,45,47,68,90,104,
107,105,96,81,75,78,92,110,131,147,153,150,143,140,141,147,158,168,174,173,170,162,158,149,147,143,150,191,203,185,135,93,72,54,51,65,89,107,110,104,
92,81,74,75,90,111,131,146,152,150,141,132,135,144,155,165,171,170,167,161,158,150,146,143,152,195,209,183,126,77,62,60,68,81,101,117,116,99,81,69,
69,80,98,119,137,146,147,140,131,129,135,147,161,170,173,168,161,156,155,150,149,143,156,200,213,185,116,63,54,66,83,93,105,116,116,98,75,63,69,87,
107,125,138,146,146,135,126,125,134,150,164,171,170,164,159,155,152,147,147,146,180,213,206,153,78,50,59,83,98,105,116,119,108,81,62,60,80,104,120,135,
143,143,135,123,120,129,144,159,171,171,165,156,149,152,152,149,149,144,183,215,204,141,62,41,63,99,114,110,110,113,104,78,59,60,86,113,128,137,141,140,
132,122,119,129,149,165,170,167,159,153,152,153,149,149,144,159,206,218,180,96,38,45,87,123,125,111,107,105,92,66,54,72,104,132,138,135,132,129,123,120,
125,141,161,173,170,158,149,147,152,153,152,150,146,180,213,204,140,53,27,68,123,143,122,99,95,93,77,57,60,93,129,144,137,123,122,125,125,126,135,156,
171,173,161,146,146,153,159,155,149,144,179,218,209,146,56,29,66,120,141,119,102,96,95,80,59,60,87,120,135,131,126,128,131,126,123,129,150,170,174,165,
152,146,152,159,156,153,146,156,203,216,188,101,38,47,93,137,134,111,99,92,81,65,60,78,108,122,123,120,126,131,123,120,126,143,162,167,165,156,150,152,
156,162,156,153,146,180,221,213,153,60,36,72,122,141,119,104,95,86,68,57,69,95,114,117,117,122,129,131,123,120,129,149,167,170,162,153,153,159,162,158,
155,149,182,224,216,159,69,44,74,119,140,120,104,95,84,68,57,68,92,105,113,114,122,129,125,120,119,131,149,162,170,165,156,152,153,162,162,158,153,158,
207,224,194,113,47,57,95,132,129,111,104,89,75,59,57,80,99,113,116,117,125,126,123,120,123,140,158,167,168,161,156,153,156,161,159,162,152,179,222,215,
159,69,45,77,120,140,117,104,92,80,68,56,68,90,107,114,113,116,123,126,125,123,131,147,161,168,165,158,155,159,164,161,161,152,180,225,219,165,75,47,
78,116,140,119,104,93,80,68,56,66,89,105,111,110,116,123,123,122,123,132,147,159,167,167,158,155,156,162,162,159,158,159,206,225,197,119,50,59,95,132,
132,111,102,83,68,56,59,83,101,108,108,111,120,125,123,120,126,141,155,165,168,162,156,153,158,165,162,161,152,179,224,218,162,75,45,77,119,138,119,104,
93,77,62,56,71,93,104,107,110,116,126,126,122,122,131,149,161,167,165,159,156,156,164,164,161,156,158,209,227,200,119,50,60,96,134,129,108,99,83,71,
59,60,80,98,104,107,114,123,125,122,122,128,140,153,165,168,164,155,155,162,168,162,158,150,177,225,218,167,75,47,77,116,137,117,104,92,78,62,53,68,
90,105,105,108,116,125,125,120,123,134,149,161,167,168,164,158,155,162,164,164,159,159,209,228,197,117,53,63,98,129,126,107,99,84,68,53,59,80,99,104,
105,111,120,128,123,120,126,141,158,165,168,162,158,156,156,165,162,161,155,158,209,224,191,111,48,66,99,129,123,105,101,80,66,54,60,81,95,104,107,113,
125,123,122,122,131,146,156,165,168,167,162,158,161,165,164,162,156,162,207,221,191,111,54,68,98,125,120,104,98,80,65,56,60,80,96,104,108,113,122,125,
125,126,132,146,155,167,168,167,162,155,159,164,165,164,155,164,209,218,185,108,62,74,98,120,111,105,98,80,62,48,60,77,90,98,102,113,120,126,125,126,
132,144,158,168,174,173,167,164,161,168,167,168,159,168,212,216,183,108,63,75,96,117,110,102,96,77,60,48,59,78,93,99,104,114,122,125,125,128,132,144,
158,168,173,171,165,161,164,167,167,164,161,159,195,215,198,140,84,77,86,101,105,105,104,83,59,44,48,63,74,81,89,101,110,119,123,128,134,141,155,165,
177,183,183,177,174,176,177,174,171,161,185,212,204,168,111,90,81,84,93,96,101,86,63,47,41,53,63,75,86,95,105,113,123,129,134,143,150,162,174,180,
185,182,180,177,177,174,173,167,171,203,203,186,135,104,93,84,89,89,98,92,72,54,42,47,53,62,74,83,96,105,117,125,131,138,147,159,170,180,185,188,
182,180,179,179,176,173,165,173,198,195,174,128,102,89,83,86,90,96,87,68,51,45,51,56,66,77,87,101,114,122,128,134,141,152,159,174,183,191,189,182,
180,176,176,173,171,167,176,189,182,164,132,110,95,86,84,86,89,84,68,59,51,51,54,62,77,84,95,108,120,128,129,137,149,159,170,180,186,188,182,177,
177,177,176,170,168,170,186,179,170,147,126,111,93,93,89,93,86,77,68,62,60,59,63,69,78,87,96,108,119,125,132,140,147,159,165,174,180,179,180,177,
179,177,173,171,167,182,182,179,162,141,132,113,102,92,89,89,83,75,68,65,65,60,63,69,80,90,98,110,117,126,132,138,149,156,164,173,174,177,177,179,
176,171,173,167,180,186,182,173,152,144,126,111,102,95,92,86,80,74,69,65,60,60,63,71,80,90,99,108,117,125,134,143,152,162,168,176,179,180,182,182,
180,177,174,171,183,185,182,171,155,144,126,111,98,87,81,75,71,69,65,65,63,65,68,74,84,93,104,113,123,132,140,150,158,165,171,174,179,179,177,177,
174,171,168,165,174,179,179,173,159,149,132,117,102,89,80,72,66,63,60,59,59,59,60,66,72,81,90,101,113,123,135,147,158,168,177,185,188,191,192,191,
189,183,180,174,176,180,179,174,162,152,138,122,105,89,75,65,59,56,54,56,57,62,66,72,80,89,98,105,116,126,137,147,156,165,173,179,183,185,185,185,
185,182,179,176,173,174,174,173,167,158,149,137,123,107,92,78,66,57,51,48,47,45,48,53,57,63,72,81,92,102,116,129,144,158,171,183,195,203,209,212,
213,212,209,204,197,191,185,177,168,158,149,138,126,114,101,89,77,66,59,51,45,42,41,42,45,50,57,65,75,86,99,111,125,140,152,165,176,186,195,201,
206,207,207,206,201,197,191,183,176,167,158,149,140,131,120,110,99,90,81,74,66,62,57,56,54,56,59,65,71,78,87,98,108,120,131,143,152,162,171,177,
183,186,189,189,188,186,182,177,173,168,162,156,150,143,137,129,122,116,108,101,95,89,83,78,75,74,72,74,75,78,83,89,96,104,113,120,131,138,147,155,
162,168,173,176,177,177,177,174,173,168,165,161,156,152,147,141,135,129,123,117,111,105,99,93,89,84,81,80,78,78,81,84,87,93,99,107,114,123,131,138,
147,153,159,165,168,171,173,173,171,170,168,164,161,158,153,149,144,140,134,128,123,117,111,105,101,95,92,87,86,83,83,83,86,87,92,98,104,110,117,125,
132,140,146,153,158,162,165,168,170,170,168,167,164,161,158,155,152,147,143,138,134,129,125,119,114,110,104,101,96,93,90,89,87,87,87,90,92,96,101,107,
113,119,125,132,138,144,150,155,159,162,165,167,167,165,165,162,161,158,155,150,147,143,138,134,128,123,119,114,110,105,101,98,95,92,90,90,90,92,93,96,
99,104,108,114,120,125,131,137,143,147,152,155,158,161,162,162,162,162,161,159,156,155,152,147,144,140,135,131,126,122,117,113,110,105,102,99,96,95,95,95,
95,96,98,101,104,108,113,117,122,126,132,137,141,144,149,152,153,155,156,156,156,156,155,153,152,149,146,143,140,137,132,129,125,122,117,114,111,108,105,104,
102,101,101,101,102,104,105,107,110,114,117,120,125,129,132,137,140,143,146,147,149,150,150,150,150,149,149,147,144,143,140,138,135,132,129,128,125,122,120,117,
116,114,114,113,113,111,111,113,113,114,116,117,119,120,122,123,126,128,129,131,132,134,135,137,137,138,138,138,138,138,138,138,137,137,135,135,134,132,131,129,
129,128,126,125,123,123,122,122,120,120,120,120,120,120,120,122,122,123,123,125,126,126,128,128,129,129,131,131,132,132,132,134,134,134,134,134,132,132,132,132,
131,131,131,129,129,128,128,126,126,126,126,125,125,125,125,125,125,125,125,125,126,126,126,126,126,128,128,128,129,129,129,131,131,131,131,131,131,131,131,129,
131,129,129,129,128,128,126,126,126,125,126,125,125,125,125,125,126,126,125,126,126,126,126,126,126,128,128,128,129,129,129,131,132,132,132,132,134,132,131,132,
128,129,126,128,125,125,126,125,126,123,126,126,128,126,129,128,129,129,129,129,128,129,128,129,131,129,126,131,129,128,128,128,128,126,129,129,131,132,129,131,
131,129,126,125,126,125,126,125,123,123,123,123,123,123,123,123,125,125,126,126,126,128,128,129,129,131,131,131,132,131,132,131,131,131,131,131,129,131,129,134,
135,134,135,134,134,132,129,128,126,125,123,120,120,120,119,119,119,119,119,120,119,122,122,123,123,126,126,129,129,131,132,132,134,134,134,134,134,134,134,
};
static const uint8_t CLIP13[] PROGMEM = {
129,128,128,128,129,128,126,125,125,126,125,126,125,125,125,125,125,125,126,125,126,128,128,128,128,129,129,129,130,129,130,129,130,136,139,140,133,129,123,125,
123,125,126,126,126,123,125,122,120,122,123,126,129,130,130,129,128,128,129,130,130,130,129,130,129,128,126,128,130,133,133,129,130,123,144,150,159,140,118,106,
102,118,122,130,126,125,119,113,112,115,122,122,126,123,129,130,130,130,126,129,132,136,136,132,137,136,139,130,128,128,130,142,140,137,129,157,170,177,132,91,
63,75,111,133,143,122,112,101,102,99,99,108,119,137,146,144,136,125,123,125,139,146,156,156,153,144,135,129,125,132,142,147,147,133,135,161,184,176,111,50,
23,63,120,157,147,112,89,82,91,94,99,112,133,153,157,146,129,119,120,135,152,164,168,164,154,143,132,128,130,140,147,149,139,130,154,174,181,126,65,27,
47,104,142,150,118,94,80,87,92,96,108,123,146,154,152,136,122,120,130,147,161,170,168,159,147,136,130,133,143,147,150,137,135,156,180,184,126,58,10,39,
98,152,156,120,85,71,82,92,101,106,122,144,160,156,139,120,116,129,150,167,174,170,157,146,135,133,137,144,147,144,137,153,177,191,156,87,24,16,67,130,
160,139,99,70,72,85,98,101,108,126,149,161,153,133,118,118,135,157,174,178,170,156,143,136,139,144,149,147,142,156,177,194,163,98,29,8,48,113,159,147,
108,71,64,77,94,102,111,126,147,160,154,135,118,119,139,163,180,180,167,152,142,140,144,147,149,143,149,168,190,185,135,64,12,23,77,135,153,128,87,64,
71,89,102,105,108,123,147,161,156,136,119,122,142,167,181,178,164,150,147,150,153,150,142,154,176,200,181,126,51,10,29,85,137,144,116,80,67,74,89,95,
99,111,132,153,159,146,128,120,132,156,176,181,171,159,152,152,152,152,144,144,163,190,202,167,102,32,15,44,104,137,133,102,75,72,81,91,89,92,105,132,
154,159,144,128,123,136,160,176,177,168,160,159,160,161,153,147,159,184,205,185,130,58,23,34,82,120,128,106,84,77,84,89,85,81,89,116,144,160,152,135,
125,133,153,170,176,171,164,164,166,167,156,153,163,191,204,181,122,57,29,41,85,113,118,96,82,80,89,89,82,74,84,109,139,156,150,137,129,137,153,168,
173,171,171,173,176,170,163,166,184,202,194,153,89,47,39,68,96,109,99,85,82,88,91,82,70,70,91,123,147,153,143,133,136,149,164,171,171,170,173,178,
178,170,168,177,198,198,173,115,67,40,56,80,98,95,85,80,87,94,88,75,65,74,99,129,146,147,140,137,146,160,171,174,174,176,180,184,178,176,180,197,
202,187,142,91,54,51,67,85,89,84,80,84,91,88,78,65,68,87,113,135,143,139,139,143,156,167,173,177,178,183,183,184,188,201,208,198,164,116,78,61,
67,78,85,81,78,81,89,91,82,70,65,74,95,116,129,133,136,143,156,166,173,174,178,181,185,184,184,187,200,205,194,159,115,78,63,67,74,77,74,75,
81,91,92,85,72,70,80,99,116,129,133,139,147,157,167,174,178,180,183,181,187,192,207,209,197,160,118,84,70,70,72,72,71,74,82,94,95,89,77,74,
80,95,109,120,128,136,147,160,170,177,183,184,187,184,187,192,205,207,195,161,123,89,72,67,65,65,65,71,82,94,95,91,84,82,88,99,109,119,128,139,
149,160,167,173,176,180,181,178,181,190,204,205,192,159,123,94,78,70,64,58,58,65,77,87,91,89,87,87,91,96,105,112,122,133,144,157,170,180,188,195,
195,197,201,209,215,209,190,159,128,102,85,72,63,56,56,61,71,78,81,81,80,82,87,94,101,111,120,132,144,157,168,178,185,190,190,194,202,215,216,202,
173,142,112,95,81,71,63,61,65,75,82,87,85,81,80,81,87,94,102,112,123,136,150,163,177,185,192,194,197,202,214,222,218,198,167,136,109,91,75,65,
57,58,63,72,80,82,80,77,77,81,88,95,105,115,128,142,156,168,178,187,190,192,201,214,224,216,195,161,133,108,94,81,74,70,72,78,87,88,87,81,
77,75,80,85,94,105,115,129,142,154,167,177,184,188,190,198,212,224,221,200,166,133,105,88,75,68,64,68,77,88,94,94,87,81,77,80,84,92,104,118,
130,144,156,164,171,177,181,185,198,209,219,211,188,154,126,102,88,78,78,84,94,101,102,98,89,81,72,71,75,85,98,112,126,137,144,152,159,167,174,180,
180,184,195,208,214,202,177,144,116,94,81,71,67,65,70,77,84,89,91,89,87,87,92,99,108,119,130,142,152,160,170,177,181,183,183,190,198,207,201,184,
160,135,111,92,80,72,72,75,81,85,88,87,82,80,80,84,89,99,109,120,132,142,152,160,170,176,181,181,187,195,207,214,207,188,160,133,109,89,75,65,
61,61,67,75,84,88,88,88,88,91,94,99,106,116,128,140,153,166,177,184,188,190,194,201,209,208,195,173,143,118,96,82,72,68,70,74,82,89,95,95,
92,89,89,91,95,102,112,123,133,146,157,167,176,181,181,184,191,201,208,201,184,156,126,102,84,72,64,63,65,75,88,98,104,104,101,96,95,96,101,106,
115,125,137,152,164,176,184,187,185,188,194,202,202,190,167,137,111,89,77,70,65,67,74,85,96,105,108,105,99,95,95,98,104,112,122,133,144,157,167,176,
180,180,178,181,188,198,198,185,161,132,105,85,74,67,64,65,74,85,99,109,112,109,104,101,99,102,106,113,123,133,146,159,170,177,181,181,178,181,187,194,
194,181,157,128,102,84,72,65,63,67,74,87,101,111,113,111,105,101,99,102,108,115,125,136,147,160,170,177,181,180,177,180,185,194,192,177,153,123,98,81,
71,67,65,68,77,91,104,112,113,111,105,101,101,105,111,118,126,137,149,160,168,176,177,177,173,174,178,187,190,180,160,132,106,87,74,68,65,67,74,85,
99,111,115,113,109,105,102,105,109,115,123,133,144,156,166,173,177,177,173,171,176,183,190,184,168,142,113,92,78,71,68,70,75,84,96,108,115,113,109,105,
104,105,109,115,122,130,142,152,160,167,170,171,170,166,166,171,180,185,178,159,132,106,88,78,74,72,74,80,89,102,111,115,113,109,105,104,108,112,118,125,
133,143,152,161,166,170,170,168,166,167,173,181,183,174,153,126,102,87,80,77,77,80,85,95,105,111,112,109,105,104,105,111,116,122,129,136,144,152,157,161,
163,163,161,160,164,173,181,180,163,140,115,98,89,87,87,87,89,94,102,108,111,109,105,104,105,109,115,120,125,130,137,144,150,154,157,159,159,159,157,160,
166,176,178,166,144,116,98,89,91,95,98,98,96,99,102,105,105,102,104,108,116,125,130,133,136,137,143,147,152,153,154,153,152,150,149,156,160,160,149,130,
113,102,99,101,104,108,112,112,111,109,108,112,116,120,120,120,123,128,133,135,135,135,136,136,137,139,140,140,140,139,140,153,161,166,147,126,108,102,111,116,
119,116,113,113,113,112,108,105,106,113,122,129,132,129,129,128,130,135,137,140,142,143,146,146,147,142,139,147,152,153,136,118,106,109,119,122,116,109,108,112,
116,115,111,106,111,120,132,132,130,125,125,128,133,137,139,139,136,137,137,139,139,137,137,152,159,161,143,120,109,108,122,126,128,122,118,116,112,108,104,105,
113,120,125,122,118,113,116,125,135,140,140,135,130,130,136,140,140,143,139,149,157,159,149,120,111,106,125,132,130,123,113,115,115,118,113,106,108,115,129,137,
137,129,122,123,130,137,139,133,128,128,130,135,135,130,132,129,139,153,156,152,126,109,101,112,130,140,142,128,119,109,111,111,115,118,123,132,132,129,118,112,
115,125,137,140,140,132,129,129,129,133,129,135,137,143,157,160,159,130,106,95,108,133,144,143,129,120,115,112,105,98,102,116,129,135,129,118,109,111,120,130,
139,140,137,136,137,140,142,140,136,133,130,143,157,167,157,125,106,96,120,133,139,128,112,115,108,111,101,99,108,119,130,128,123,115,116,123,132,142,140,142,
135,133,133,135,137,135,139,139,146,144,154,150,144,123,105,104,113,135,140,136,118,106,101,102,104,109,116,125,130,128,123,116,118,123,136,143,143,142,137,137,
135,132,130,136,144,146,143,132,135,143,150,143,118,108,102,123,133,137,129,113,108,102,109,111,118,120,123,125,119,119,115,119,122,130,140,146,149,139,132,128,
129,139,143,147,140,140,133,150,157,157,137,106,98,102,129,140,136,118,102,102,108,112,113,108,112,119,128,129,122,119,118,128,135,137,136,130,133,137,142,142,
136,136,135,142,144,143,154,156,160,140,119,105,106,126,132,136,119,113,106,108,105,98,101,106,125,133,130,120,109,111,119,133,143,146,147,142,142,137,136,136,
135,142,142,147,137,135,142,143,150,125,111,101,113,132,130,125,108,109,118,122,116,104,102,118,132,143,132,122,112,113,126,129,139,139,147,146,140,130,119,123,
129,142,147,144,144,135,149,146,147,122,101,101,108,139,140,135,112,99,102,106,118,116,122,125,130,135,129,125,115,116,126,137,146,142,135,133,132,139,132,126,
120,126,139,142,140,122,137,147,166,150,115,99,96,132,143,146,125,109,111,112,119,112,111,112,119,126,125,120,115,118,125,133,136,135,133,135,136,136,133,130,
132,133,139,137,139,136,135,147,150,157,135,115,102,108,129,137,137,122,111,105,108,109,112,113,119,120,128,126,120,119,113,125,130,140,142,136,136,130,136,133,
135,133,135,139,140,139,133,126,139,152,163,150,120,102,95,118,132,144,132,116,106,101,111,112,116,115,118,126,128,132,120,115,112,120,135,142,144,135,130,128,
130,136,136,133,136,137,143,140,136,128,137,157,164,154,116,95,92,119,142,143,130,108,105,106,115,113,109,111,113,128,133,132,125,115,119,126,139,143,140,135,
133,136,137,133,130,128,132,139,144,143,137,128,133,144,157,146,119,101,98,123,137,142,126,108,105,106,112,109,108,111,118,128,129,125,116,116,122,133,142,147,
146,142,133,129,135,140,150,144,142,133,136,135,132,125,118,136,150,163,136,102,81,84,116,132,135,118,111,115,120,119,105,99,109,130,147,147,130,115,111,125,
143,157,153,143,136,140,147,147,140,130,128,133,140,140,133,123,120,118,135,142,149,126,101,88,92,118,125,125,111,105,109,113,122,118,120,125,133,142,139,135,
126,128,135,139,146,143,142,139,136,137,130,132,130,135,133,129,128,120,123,125,129,152,176,187,161,108,72,64,106,139,153,135,106,94,88,95,94,98,105,115,
128,132,129,122,119,122,133,143,153,159,159,157,150,147,143,144,144,144,142,137,136,135,132,128,123,146,168,183,154,102,65,63,98,128,137,120,96,87,87,94,
95,98,105,116,129,132,132,126,123,128,137,150,160,160,160,153,149,146,146,149,147,142,137,133,136,136,133,129,129,154,173,178,144,99,70,70,98,118,126,115,
99,85,77,74,80,91,104,113,119,125,125,128,130,133,143,150,161,166,170,168,166,161,156,154,152,149,144,140,139,135,130,126,120,132,150,166,161,128,96,74,
78,92,104,112,106,99,84,75,74,80,92,102,115,120,128,129,133,136,140,149,156,167,171,173,174,168,167,161,159,154,147,144,139,139,133,129,123,119,136,150,
161,149,119,96,77,81,87,99,105,99,92,78,72,70,77,88,101,112,120,128,132,136,140,146,154,163,173,180,183,181,176,170,164,161,157,153,146,140,136,128,
125,116,120,133,144,150,133,113,94,81,81,84,94,96,94,87,75,71,72,80,92,101,113,120,130,136,142,149,154,163,168,178,184,188,187,183,177,171,164,159,
152,146,139,130,122,115,111,112,125,132,135,125,111,99,87,84,85,89,92,91,85,78,74,71,77,85,95,108,116,126,132,142,150,159,170,177,187,190,195,194,
191,188,180,174,164,157,149,140,132,122,115,106,104,112,119,128,119,108,95,85,84,84,92,94,96,92,84,81,77,84,89,99,108,115,125,130,137,143,153,161,
168,178,184,190,190,187,183,177,173,168,161,153,143,135,126,118,112,108,115,119,125,118,108,99,91,89,87,91,94,95,94,87,82,80,82,87,95,104,112,119,
125,132,139,147,156,166,174,180,183,184,184,184,183,177,173,164,156,147,137,133,123,119,111,112,118,118,122,112,106,99,94,94,91,95,95,95,94,88,85,85,
88,92,98,106,113,120,128,133,140,149,157,166,171,177,181,183,183,181,177,173,167,161,154,146,139,132,123,116,112,116,119,122,118,111,105,98,96,94,95,96,
95,95,92,89,88,87,89,95,101,108,113,120,126,132,139,146,156,161,168,174,177,178,178,177,173,171,166,160,153,144,139,132,126,120,118,123,122,123,116,111,
106,99,98,96,98,98,95,94,91,88,88,88,91,95,99,106,111,118,125,130,139,146,153,160,167,173,176,178,178,178,177,171,168,163,156,150,143,137,130,125,
126,125,125,119,113,111,104,101,95,95,95,94,94,91,91,89,88,91,94,99,104,111,116,122,128,133,140,147,153,160,164,168,171,173,171,170,168,164,160,156,
152,146,142,136,129,128,129,128,126,122,120,115,111,105,102,101,98,98,95,95,92,89,91,89,92,94,98,102,106,113,118,123,130,136,143,149,156,160,164,168,
170,171,171,170,168,164,161,157,152,147,143,142,139,136,132,128,125,118,113,109,105,102,98,96,94,91,89,88,88,88,91,92,95,99,102,108,112,118,122,129,
135,140,146,152,156,160,163,166,168,168,168,167,166,163,160,156,153,152,147,144,139,136,132,126,123,118,115,111,106,104,101,98,95,95,92,94,92,94,95,98,
99,102,106,109,113,118,122,126,132,136,140,144,147,152,153,156,157,157,159,157,157,156,154,153,152,149,146,143,140,137,133,130,128,123,120,116,115,112,109,106,
105,104,102,102,102,102,104,105,106,108,111,113,116,119,122,126,129,132,135,137,140,142,143,144,146,146,146,147,147,146,146,144,143,142,139,137,136,133,132,129,
128,125,123,120,119,118,116,115,115,113,113,113,113,115,115,116,118,118,119,122,123,123,126,128,129,130,130,132,133,133,135,135,135,135,135,133,135,133,133,132,
130,132,130,129,129,129,128,128,128,126,126,126,126,126,126,126,125,126,126,125,126,126,126,126,126,126,126,128,128,126,128,128,126,128,128,128,128,128,128,128,
128,128,129,128,129,128,129,129,128,129,129,129,129,129,129,128,128,128,128,128,128,128,128,128,128,128,126,128,128,126,128,128,128,128,126,128,128,128,128,128,
128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,129,128,129,128,128,128,129,128,128,128,128,
126,128,128,128,126,126,128,126,128,126,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,128,129,128,128,
126,129,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
128,128,128,128,126,128,128,128,128,129,128,128,129,126,129,128,128,128,129,126,128,128,128,126,129,128,126,128,128,129,126,128,128,128,126,128,128,126,129,126,
128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,128,126,128,128,126,129,126,128,128,128,128,128,129,128,129,128,128,128,128,128,128,128,
126,128,128,128,128,129,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,129,126,129,126,128,129,128,128,128,128,128,128,126,128,128,128,
129,128,128,128,126,129,128,128,126,128,129,126,128,128,126,128,126,128,128,128,128,126,129,128,128,126,129,128,128,129,128,128,128,129,126,128,128,128,129,128,
128,128,128,129,128,128,128,126,128,128,128,126,126,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,126,129,128,128,128,128,128,126,129,
126,129,128,126,129,128,128,128,129,128,128,128,129,128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,126,128,128,128,128,128,128,
128,128,128,128,129,128,128,128,128,128,128,129,126,128,128,128,128,129,126,128,128,128,126,128,128,128,128,128,129,128,128,128,129,126,129,126,128,129,128,126,
126,129,128,126,128,128,126,129,129,128,128,128,128,128,128,126,128,128,128,126,128,128,128,128,129,128,128,128,128,129,128,128,128,129,128,128,128,128,126,129,
128,128,128,128,128,128,128,128,129,128,128,128,128,128,128,128,128,128,128,128,128,128,126,129,128,128,128,128,128,128,128,128,128,128,128,128,128,128,129,128,
126,129,128,128,128,128,128,128,128,128,128,128,129,126,129,128,128,128,128,128,126,129,128,126,128,128,128,128,128,129,128,128,128,128,128,126,129,126,128,126,
128,128,128,128,128,129,128,128,129,126,128,128,128,128,128,128,128,129,128,128,128,128,128,126,129,126,128,128,129,128,128,128,128,128,128,128,128,
};
#define CLIPS 14
static const uint8_t* const CLIP_P[CLIPS] = {CLIP0,CLIP1,CLIP2,CLIP3,CLIP4,CLIP5,CLIP6,CLIP7,CLIP8,CLIP9,CLIP10,CLIP11,CLIP12,CLIP13};
static const uint32_t CLIP_N[CLIPS] = {sizeof(CLIP0),sizeof(CLIP1),sizeof(CLIP2),sizeof(CLIP3),sizeof(CLIP4),sizeof(CLIP5),sizeof(CLIP6),sizeof(CLIP7),sizeof(CLIP8),sizeof(CLIP9),sizeof(CLIP10),sizeof(CLIP11),sizeof(CLIP12),sizeof(CLIP13)};

// ------------------------------------------------------------ sound: the touch board's ES8311 chip and speaker (pins 4 5 6 7, amplifier switch on pin 1)
#define AU_RATE 16000
#define FXN 40
struct AuNote { uint16_t hz; uint16_t ms; };
AuNote fxQ[FXN]; volatile uint8_t fxH = 0, fxT = 0; volatile int fxBusy = 0;
const uint8_t* volatile clipP = NULL; volatile uint32_t clipN = 0; volatile bool clipNew = false;
int8_t SINE[256];
bool fxIdle(){ return fxH == fxT && fxBusy <= 0; }
void fxNote(int hz, int ms){                                     // put one note (0 = a rest) in line for the speaker
  if(!spk || ms < 1) return;
  uint8_t n = (uint8_t)((fxH + 1) % FXN); if(n == fxT) return;
  fxQ[fxH].hz = (uint16_t)hz; fxQ[fxH].ms = (uint16_t)ms; fxH = n;
}
void sayClip(int k){ if(!spk || k < 0 || k >= CLIPS) return; clipP = CLIP_P[k]; clipN = CLIP_N[k]; clipNew = true; }      // her voice
// the songs: pairs of (note, how long). note = piano key number (60 = middle C), 0 = rest. All old songs anyone may play.
static const uint8_t SONG0[] PROGMEM = {64,2,64,2,64,4, 64,2,64,2,64,4, 64,2,67,2,60,3,62,1,64,8, 65,2,65,2,65,3,65,1,65,2,64,2,64,2,64,1,64,1,64,2,62,2,62,2,64,2,62,4,67,4,
  64,2,64,2,64,4, 64,2,64,2,64,4, 64,2,67,2,60,3,62,1,64,8, 65,2,65,2,65,3,65,1,65,2,64,2,64,2,64,1,64,1,67,2,67,2,65,2,62,2,60,8, 0,4};      // Jingle Bells
static const uint8_t SONG1[] PROGMEM = {72,4,71,3,69,1,67,6,65,2,64,4,62,4,60,6,67,2,69,6,69,2,71,6,71,2,72,6, 72,2,72,2,71,2,69,2,67,2,67,3,65,1,64,2,72,2,72,2,71,2,69,2,67,2,67,3,65,1,64,2,
  64,2,64,2,64,2,64,2,64,1,65,1,67,6,65,1,64,1,62,2,62,2,62,2,62,1,64,1,65,6,64,1,62,1,60,2,72,4,69,2,67,3,65,1,64,2,65,2,64,4,62,4,60,8, 0,4};      // Joy to the World
static const uint8_t SONG2[] PROGMEM = {67,3,69,1,67,2,64,6, 67,3,69,1,67,2,64,6, 74,4,74,2,71,6, 72,4,72,2,67,6, 69,4,69,2,72,3,71,1,69,2,67,3,69,1,67,2,64,6, 69,4,69,2,72,3,71,1,69,2,67,3,69,1,67,2,64,6,
  74,4,74,2,77,3,74,1,71,2,72,6,76,6, 72,2,67,2,64,2,67,3,65,1,62,2,60,12, 0,4};      // Silent Night
static const uint8_t SONG3[] PROGMEM = {62,2,67,4,71,1,67,1,71,4,69,2,67,4,64,2,62,4, 62,2,67,4,71,1,67,1,71,4,69,2,74,10, 71,2,74,3,71,1,74,1,71,1,67,4,62,2,64,3,67,1,67,1,64,1,62,4, 62,2,67,4,71,1,67,1,71,4,69,2,67,10, 0,4};      // Amazing Grace
static const uint8_t* const SONGS[4] = {SONG0, SONG1, SONG2, SONG3};
static const uint16_t SONG_N[4] = {sizeof(SONG0), sizeof(SONG1), sizeof(SONG2), sizeof(SONG3)};
static const uint16_t SONG_MS[4] = {130, 120, 190, 200};
volatile int song = 0, songWant = -1;
static const char* const SONG_NAME[4] = {"JINGLE BELLS", "JOY TO THE WORLD", "SILENT NIGHT", "AMAZING GRACE"};
void auFill(int16_t* buf, int n){                                // makes the next bit of sound: chimes first, then her voice, songs underneath
  static uint32_t fxPh = 0, fxInc = 0, muPh = 0, muInc = 0, cpos = 0, cn = 0; static uint32_t muPh2 = 0, muPh3 = 0; static int fxLeft = 0, fxTot = 0, muLeft = 0, muTot = 0, muIdx = 0;
  static const uint8_t* cp = NULL; static bool wasMusic = false;
  if(musicOn && !wasMusic){ muLeft = 0; muIdx = 0; }
  if(!musicOn && wasMusic){ muLeft = 0; song = (song + 1) % 4; muT = muH; }
  wasMusic = musicOn;
  if(songWant >= 0){ song = songWant % 4; songWant = -1; muLeft = 0; muIdx = 0; }
  for(int i = 0; i < n; i++){
    int s = 0;
    if(fxLeft <= 0 && fxT != fxH){ AuNote q = fxQ[fxT]; fxT = (uint8_t)((fxT + 1) % FXN); fxTot = fxLeft = (int)q.ms * (AU_RATE / 1000); fxInc = q.hz ? (uint32_t)(((uint64_t)q.hz << 32) / AU_RATE) : 0; fxPh = 0; }
    if(fxLeft > 0){
      if(fxInc){ int e = 96, pos = fxTot - fxLeft; if(pos < 64) e = e * pos / 64; if(fxLeft < 320) e = e * fxLeft / 320;
        s += (SINE[fxPh >> 24] * e) + (SINE[(fxPh >> 23) & 255] * e) / 4; fxPh += fxInc; }
      fxLeft--;
    } else {
      if(clipNew){ clipNew = false; cp = clipP; cn = clipN; cpos = 0; }
      if(cp){ uint32_t k = cpos >> 1; if(k < cn){ s += ((int)pgm_read_byte(cp + k) - 128) * 190; cpos++; } else cp = NULL; }
    }
    if(muPlay){                                                   // real music (or the sound of a web page): internet radio or a song from the SD card
      static bool muGo = false; uint32_t av = muH - muT;
      if(!muGo && av >= 8000) muGo = true;                        // ran dry: wait for half a second of sound, so it never stutters
      if(muBuf && muGo){ if(av){ int mv = muBuf[muT & (MU_N - 1)]; s += vPlay ? (mv >> 3) : ((mv * 3) >> 2); muT++; } else muGo = false; }      // quiet while Mary talks
    }
    else if(musicOn){
      if(muLeft <= 0){
        if(muIdx + 1 >= (int)SONG_N[song]){ muIdx = 0; song = (song + 1) % 4; }
        int key = pgm_read_byte(SONGS[song] + muIdx), len = pgm_read_byte(SONGS[song] + muIdx + 1); muIdx += 2;
        muTot = muLeft = len * (int)SONG_MS[song] * (AU_RATE / 1000);
        muInc = key ? (uint32_t)((440.0 * pow(2.0, (key - 69) / 12.0)) * 4294967296.0 / AU_RATE) : 0; muPh = 0;
      }
      if(muInc){ int pos = muTot - muLeft, e = 62 - (int)((long)pos * 34 / (muTot > 0 ? muTot : 1)); if(pos < 60) e = e * pos / 60; if(muLeft < 800) e = e * muLeft / 800;      // rings like a bell: strong, then fading
        s += (SINE[muPh >> 24] * e) + (SINE[(muPh >> 23) & 255] * e) / 3 + (SINE[muPh3 >> 24] * e) / 2 + (SINE[muPh2 >> 24] * 30);      // the note, its octave, a second voice a hair apart, and a soft low voice under it
        muPh += muInc; muPh3 += muInc + (muInc >> 8); muPh2 += muInc >> 1; }
      muLeft--;
    }
    if(vPlay){ if(vPos < vN) s += vBuf[vPos++]; else vPlay = false; }      // Mary's voice
    if(s > 32000) s = 32000; if(s < -32000) s = -32000;
    buf[2 * i] = (int16_t)s; buf[2 * i + 1] = (int16_t)s;
  }
  fxBusy = fxLeft > 0 || cp != NULL ? 1 : 0;
}
bool auActive(){ return musicOn || !fxIdle() || clipNew || vPlay; }
void auTask(void* arg){                                          // runs by itself beside everything else, so sound never waits for the Wi-Fi
  static int16_t buf[512]; bool amp = false; int quiet = 0;
  for(;;){
    bool act = auActive();
    auFill(buf, 256);
    if(board == 2){
      if(act){ quiet = 0; if(!amp){ digitalWrite(1, LOW); amp = true; } }               // pin 1 LOW = amplifier on
      else if(amp && ++quiet > 12){ digitalWrite(1, HIGH); amp = false; }                // quiet for a moment: amplifier off, no hiss
    }
#if CYD
    { int g = volSet <= 1 ? 1 : (volSet == 2 ? 2 : 4); for(int i = 0; i < 512; i++) buf[i] = (int16_t)((((int)buf[i] * g) >> 2) ^ 0x8000); }      // the yellow board plays through its own pin 26: quieter, and counted from 0 up
#endif
    size_t w = 0;
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &w, portMAX_DELAY);
  }
}
bool esW(uint8_t r, uint8_t v){ Wire.beginTransmission(0x18); Wire.write(r); Wire.write(v); return Wire.endTransmission() == 0; }
void esVol(){ esW(0x32, volSet <= 1 ? 0xB0 : (volSet == 2 ? 0xC4 : 0xD2)); }      // how loud
int floatScore(int p){                                           // is this wire free (it follows a weak pull up and down)? The chip's own output wire does not.
  int n = 0;
  for(int k = 0; k < 8; k++){ pinMode(p, INPUT_PULLUP); delayMicroseconds(300); int a = digitalRead(p); pinMode(p, INPUT_PULLDOWN); delayMicroseconds(300); int c = digitalRead(p); if(a == HIGH && c == LOW) n++; }
  pinMode(p, INPUT);
  return n;
}
void auInit(){                                                   // start the sound wires, wake the ES8311 chip, find the speaker wire, start the sound task
  for(int i = 0; i < 256; i++) SINE[i] = (int8_t)(127.0 * sin(i * 6.2831853 / 256.0));
#if CYD
  {                                                              // yellow board: the chip makes the sound itself on pin 26 (the SPEAK plug)
    i2s_config_t c3; memset(&c3, 0, sizeof(c3));
    c3.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    c3.sample_rate = AU_RATE; c3.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT; c3.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    c3.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_MSB;
    c3.dma_buf_count = 6; c3.dma_buf_len = 256; c3.use_apll = false; c3.tx_desc_auto_clear = true;
    if(i2s_driver_install(I2S_NUM_0, &c3, 0, NULL) != ESP_OK) return;
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);                   // only pin 26; pin 25 stays with the touch screen
    spk = true; spkUsed = 26;
    xTaskCreatePinnedToCore(auTask, "sound", 6144, NULL, 3, NULL, 1);
    fxNote(523, 110); fxNote(659, 110); fxNote(784, 200);
    return;
  }
#endif
  pinMode(1, OUTPUT); digitalWrite(1, HIGH);
  i2s_config_t cfg; memset(&cfg, 0, sizeof(cfg));
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | (board == 2 ? I2S_MODE_RX : 0));
  cfg.sample_rate = AU_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = (i2s_comm_format_t)I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 6; cfg.dma_buf_len = 256;
  cfg.use_apll = false; cfg.tx_desc_auto_clear = true;
  cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  if(i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return;
  i2s_pin_config_t pc; memset(&pc, 0, sizeof(pc));
  pc.mck_io_num = 4; pc.bck_io_num = 5; pc.ws_io_num = 7; pc.data_out_num = I2S_PIN_NO_CHANGE; pc.data_in_num = I2S_PIN_NO_CHANGE;      // clocks first
  if(i2s_set_pin(I2S_NUM_0, &pc) != ESP_OK) return;
  delay(30);
  if(!esW(0x00, 0x1F)) return;                                   // the chip did not answer: no speaker on this board
  delay(20); esW(0x00, 0x00);
  esW(0x01, 0x3F); esW(0x02, 0x00); esW(0x03, 0x10); esW(0x16, 0x24); esW(0x04, 0x10); esW(0x05, 0x00); esW(0x06, 0x03); esW(0x07, 0x00); esW(0x08, 0xFF);      // clocks for 16000 a second
  esW(0x0B, 0x00); esW(0x0C, 0x00); esW(0x10, 0x1F); esW(0x11, 0x7F); esW(0x00, 0x80);
  esW(0x09, 0x0C); esW(0x0A, 0x0C);                              // 16 bit sound
  esW(0x13, 0x10); esW(0x1B, 0x0A); esW(0x1C, 0x6A);
  esW(0x17, 0xBF); esW(0x0E, 0x02); esW(0x12, 0x00); esW(0x14, 0x1A); esW(0x0D, 0x01); esW(0x15, 0x40); esW(0x37, 0x08); esW(0x45, 0x00);
  esW(0x31, 0x00); esVol();
  delay(30);
  int d = spkPin;
  if(d != 6 && d != 8){ int f6 = floatScore(6), f8 = floatScore(8); d = f6 > f8 ? 6 : 8; }      // the free wire is the one that goes INTO the chip = the speaker wire
  spkUsed = d;
  pc.mck_io_num = I2S_PIN_NO_CHANGE; pc.bck_io_num = I2S_PIN_NO_CHANGE; pc.ws_io_num = I2S_PIN_NO_CHANGE; pc.data_out_num = d;
  if(board == 2){ micPin = d == 8 ? 6 : 8; pc.data_in_num = micPin; micOk = true; }      // the microphone comes in on the other data wire
  i2s_set_pin(I2S_NUM_0, &pc);
  spk = true;
  xTaskCreatePinnedToCore(auTask, "sound", 6144, NULL, 3, NULL, 1);
  fxNote(523, 110); fxNote(659, 110); fxNote(784, 200);          // three rising notes: the box is awake
}

// ------------------------------------------------------------ the picture screen: photo behind, glass buttons with names, a helper who talks
#define CVH 48
static uint16_t CV[LCD_W * CVH]; int cvY0 = 0, cvHh = 0;        // one strip of the picture is built here, then sent to the screen
String msgText = ""; unsigned long msgUntil = 0;
bool gameOn = false, asleep = false; int pgNo = 0, gWin = 0; int8_t gb[9]; unsigned long lastTouch = 0; uint32_t gSeed = 12345;      // gb: tic tac toe squares, 1 = you (X), 2 = the box (O)
uint16_t mix(uint16_t a, uint16_t b, int k){                     // k = 0: all a ... 255: all b
  if(k <= 0) return a; if(k >= 255) return b;
  int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  return (uint16_t)((((ar * (255 - k) + br * k) / 255) << 11) | (((ag * (255 - k) + bg * k) / 255) << 5) | ((ab * (255 - k) + bb * k) / 255));
}
void cvBox(int x, int y, int w, int h, uint16_t c){ for(int yy = (y < cvY0 ? cvY0 : y); yy < y + h && yy < cvY0 + cvHh; yy++) for(int xx = (x < 0 ? 0 : x); xx < x + w && xx < LCD_W; xx++) CV[(yy - cvY0) * LCD_W + xx] = c; }
void cvBand(int y0, int h){ cvY0 = y0; cvHh = h; for(int y = 0; y < h; y++) for(int x = 0; x < LCD_W; x++) CV[y * LCD_W + x] = pgm_read_word(&BG[(y0 + y) * LCD_W + x]); }
void cvBg(int y, int h){ for(int yy = (y < cvY0 ? cvY0 : y); yy < y + h && yy < cvY0 + cvHh; yy++) for(int x = 0; x < LCD_W; x++) CV[(yy - cvY0) * LCD_W + x] = pgm_read_word(&BG[yy * LCD_W + x]); }      // the plain picture again over some rows
void cvFlush(){
  if(!lcdOn) return;
  uint8_t buf[128]; long total = (long)LCD_W * cvHh, done = 0;
  lcdOpen(); lcdWindow(0, cvY0, LCD_W, cvHh);
  while(done < total){ int n = 0; while(n < 128 && done < total){ uint16_t c = CV[done++]; buf[n++] = (uint8_t)(c >> 8); buf[n++] = (uint8_t)(c & 255); } lcdSpi.writeBytes(buf, n); }
  lcdClose();
}
void cvGlass(int x, int y, int w, int h, int r, uint16_t tint, int a, int flat){      // a rounded piece of glass: see-through color, shine on top, bright rim
  int y1 = y < cvY0 ? cvY0 : y, y2 = y + h > cvY0 + cvHh ? cvY0 + cvHh : y + h, top = h * 45 / 100;
  for(int yy = y1; yy < y2; yy++){
    int ry = yy - y;
    for(int xx = (x < 0 ? 0 : x); xx < x + w && xx < LCD_W; xx++){
      int rx = xx - x; bool rim = false;
      int cx = rx < r ? r - rx : (rx >= w - r ? rx - (w - r - 1) : 0), cy = ry < r ? r - ry : (ry >= h - r ? ry - (h - r - 1) : 0);
      if(cx && cy){ int d = cx * cx + cy * cy; if(d > r * r) continue; if(d > (r - 2) * (r - 2)) rim = true; }
      else if(r > 0 && (rx < 2 || ry < 2 || rx >= w - 2 || ry >= h - 2)) rim = true;
      uint16_t* p = &CV[(yy - cvY0) * LCD_W + xx];
      uint16_t c = mix(*p, tint, a);
      if(!flat){
        if(top > 0 && ry < top) c = mix(c, 0xFFFF, 95 - 95 * ry / top);
        if(ry > h - 14) c = mix(c, 0, (ry - (h - 14)) * 10);
        if(rim) c = mix(c, 0xFFFF, 170);
      }
      *p = c;
    }
  }
}
void cvChar(int x, int y, int ch, uint16_t col, int sc){
  if(ch < 32 || ch > 126) ch = '?';
  const uint8_t* g = FONT + (ch - 32) * FH * FB;
  for(int r = 0; r < FH; r++){
    for(int e = 0; e < sc; e++){
      int yy = y + r * sc + e; if(yy < cvY0 || yy >= cvY0 + cvHh) continue;
      for(int c = 0; c < FW; c++){
        if(!(pgm_read_byte(g + r * FB + (c >> 3)) & (0x80 >> (c & 7)))) continue;
        for(int f = 0; f < sc; f++){ int xx = x + c * sc + f; if(xx >= 0 && xx < LCD_W) CV[(yy - cvY0) * LCD_W + xx] = col; }
      }
    }
  }
}
void cvText(int x, int y, const char* s, uint16_t col, int sc){   // words with a soft dark shadow, nothing behind them painted over
  int k = 0; for(const char* q = s; *q; q++, k++) cvChar(x + k * FW * sc + 1, y + 2, (uint8_t)*q, 0x0000, sc);
  k = 0; for(const char* q = s; *q; q++, k++) cvChar(x + k * FW * sc, y, (uint8_t)*q, col, sc);
}
void cvSprite(int x, int y, const uint16_t* p){                    // a 36 x 36 little picture; the key color is see-through
  for(int r = 0; r < 36; r++){
    int yy = y + r; if(yy < cvY0 || yy >= cvY0 + cvHh) continue;
    for(int c = 0; c < 36; c++){ uint16_t v = pgm_read_word(&p[r * 36 + c]); int xx = x + c; if(v != 0xF81F && xx >= 0 && xx < LCD_W) CV[(yy - cvY0) * LCD_W + xx] = v; }
  }
}
// ---- the Jetson hub: every box checks in with the Jetson on your own Wi-Fi (locked with the secret code).  Tap Amy's face at the bottom of the screen for the Amy AI page.
String lpw = "04170417";                                            // the local password: phones and the Jetson need it to get in
String hubHost = "", hubAns = "", hubInfo = "", hubPhone = ""; IPAddress hubIp; bool hubOk = false, hubWait = false, jetOn = false, mdnsUp = false;
unsigned long hubAt = 0, hubFindAt = 0; int ansPg = 0, hubFails = 0;
String urlEnc(const String& s){ String o; const char* H = "0123456789ABCDEF"; for(unsigned int i = 0; i < s.length(); i++){ char c = s.charAt(i); if(isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') o += c; else { o += '%'; o += H[(c >> 4) & 15]; o += H[c & 15]; } } return o; }
bool hubFind(){                                                     // the Jetson's address: moto-desktop.local or a number like 192.168.1.50
  if(!hubHost.length()) return false;
  IPAddress ip;
  if(ip.fromString(hubHost)){ hubIp = ip; return true; }
  if(hubHost.endsWith(".local")){
    if(!mdnsUp){ mdnsUp = MDNS.begin("amy-box"); }
    ip = MDNS.queryHost(hubHost.substring(0, hubHost.length() - 6), 1500);
    if((uint32_t)ip != 0){ hubIp = ip; return true; }
    return false;
  }
  if(WiFi.hostByName(hubHost.c_str(), ip) && (uint32_t)ip != 0){ hubIp = ip; return true; }
  return false;
}
bool hubGet(const String& path, String& body){
  if((uint32_t)hubIp == 0){ if(millis() - hubFindAt < 120000UL && hubFindAt) return false; hubFindAt = millis(); if(!hubFind()) return false; }
  WiFiClient c; HTTPClient h; h.setConnectTimeout(1500); h.setTimeout(2000);
  if(!h.begin(c, String("http://") + hubIp.toString() + ":8090" + path)) return false;
  int r = h.GET(); if(r == 200) body = h.getString(); h.end();
  if(r != 200){ if(++hubFails >= 3){ hubIp = IPAddress(); hubFails = 0; } return false; }
  hubFails = 0; return true;
}
String hubName(){ return board == 3 ? String("YELLOW ") + nm[0] : board == 2 ? String("BLACK ") + nm[0] : board == 4 ? String("SMALL ") + nm[0] : String("BOX ") + nm[0]; }
void hubHello(){
  String b;
  bool ok = hubGet("/hello?k=" + urlEnc(lpw) + "&n=" + urlEnc(hubName()) + "&ip=" + WiFi.localIP().toString() + "&s=" + urlEnc(stateText()), b);
  hubOk = ok;
  if(!ok) return;
  String a = jsonField(b, "a"); if(a != hubAns){ hubAns = a; ansPg = 0; }
  hubWait = jsonField(b, "w") == "1"; hubInfo = jsonField(b, "i"); hubPhone = jsonField(b, "p");
}
void hubLoop(){
  if(gKey.length()) return;                                       // Mary talks through Google: no Jetson needed
  if(!hubHost.length() || apUp || WiFi.status() != WL_CONNECTED || !lpw.length()) return;
  unsigned long gap = jetOn ? (hubWait ? 1500UL : 3000UL) : 20000UL;
  if(hubAt && millis() - hubAt < gap) return;
  hubAt = millis(); if(!hubAt) hubAt = 1;
  hubHello();
}
void hubAsk(const char* q){
  String b;
  if(!hubHost.length()){ hubAns = "Put the Jetson address on the setup page first."; return; }
  if(hubGet("/ask?k=" + urlEnc(lpw) + "&src=" + urlEnc(hubName()) + "&q=" + urlEnc(q), b)){ hubWait = true; hubOk = true; hubAt = millis(); }
  else { hubOk = false; hubAns = "I can't reach the Jetson. Is it on, and on the same Wi-Fi?"; ansPg = 0; }
}
int ansLines(String* L, int max){                                   // Amy's answer cut into lines of 25 letters
  String s = ""; for(unsigned int i = 0; i < hubAns.length(); i++){ char c = hubAns.charAt(i); s += (c >= 32 && c <= 126) ? c : ' '; }
  int n = 0; s.trim();
  while(s.length() && n < max){
    if(s.length() <= 25){ L[n++] = s; break; }
    int cut = s.lastIndexOf(' ', 25); if(cut <= 0) cut = 25;
    L[n++] = s.substring(0, cut); s = s.substring(cut); s.trim();
  }
  return n;
}
static const char* JQ[3] = {"Say hello to Moto in one short friendly sentence.", "Tell Moto one short clean joke.", "Give Moto one short tip to keep going today."};
static const char* JB[5] = {"HELLO", "JOKE", "BOOST", "NEXT", "EXIT"};
static const char* MB[5] = {"TALK", "JOKE", "BLESS", "NEXT", "EXIT"};
static const char* MQ[3] = {"", "Tell Mr. Terry one short, clean, funny joke.", "Share one short encouraging Bible verse (King James) with Mr. Terry and one warm sentence about it."};
void aiAskText(const char* q){ if(aiState != 0) return; aiQ = q; recN = 0; hubAns = ""; ansPg = 0; aiState = 2; }
void jetTap(int i){
  fxNote(1500, 30);
  if(gKey.length()){
    if(i == 4){ jetOn = false; vPlay = false; return; }
    if(i == 3){ String L[24]; int n = ansLines(L, 24); ansPg = (ansPg + 1) * 4 < n ? ansPg + 1 : 0; return; }
    if(i == 0){ if(aiState == 0 && recL){ vPlay = false; recN = 0; aiRec = true; aiState = 1; hubAns = ""; ansPg = 0; } return; }      // hold TALK and speak
    if(i == 1 || i == 2){ vPlay = false; aiAskText(MQ[i]); }
    return;
  }
  if(i == 4){ jetOn = false; return; }
  if(!hubOk) return;                                              // no key and no Jetson: only EXIT works
  if(i == 3){ String L[24]; int n = ansLines(L, 24); ansPg = (ansPg + 1) * 4 < n ? ansPg + 1 : 0; return; }
  if(i >= 0 && i < 3){ hubAns = ""; ansPg = 0; hubAsk(JQ[i]); }
}
void paintJet(){
  bool mary = gKey.length() > 0;
  if(!mary && !hubOk){                                            // no Google key yet: say so plainly (not "no Jetson")
    cvText(8, 3, "MARY", rgb(255, 220, 90), 1);
    cvText(LCD_W - 6 - 8 * FW, 3, "NEED KEY", rgb(255, 150, 150), 1);
    cvGlass(4, 33, LCD_W - 8, 106, 12, rgb(14, 34, 84), 170, 0);
    String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("192.168.4.1");
    cvText(14, 38, "Mary needs her Google", rgb(255, 230, 120), 1);
    cvText(14, 63, "key. On a phone open", rgb(200, 210, 230), 1);
    cvText(14, 88, ip.c_str(), 0xFFFF, 1);
    cvText(14, 113, "and paste it in 7.", rgb(200, 210, 230), 1);
    cvGlass(254, 151, 59, 47, 10, 0x0000, 140, 1); cvGlass(252, 146, 59, 47, 10, rgb(225, 50, 60), 190, 0); cvText(252 + (59 - 4 * FW) / 2, 158, "EXIT", 0xFFFF, 1);
    return;
  }
  cvText(8, 3, mary ? "MARY" : "MARY AI", rgb(255, 220, 90), 1);
  String st = mary ? (aiState == 1 ? String("LISTENING") : aiState == 2 ? String("THINKING") : aiState == 4 ? String("VOICE...") : vPlay ? String("TALKING") : String("READY"))
                   : (!hubHost.length() ? String("NO JETSON SET") : hubOk ? String("JETSON OK") : String("NO JETSON"));
  bool good = mary ? true : hubOk;
  cvText(LCD_W - 6 - (int)st.length() * FW, 3, st.c_str(), good ? rgb(140, 255, 170) : rgb(255, 150, 150), 1);
  cvGlass(4, 33, LCD_W - 8, 106, 12, rgb(14, 34, 84), 170, 0);
  String L[24]; int n = ansLines(L, 24);
  if(!n && mary && maryBig){                                      // Mary's own picture from the SD card
    cvImg(12, 36, 100, 100, maryBig);
    const char* a = aiState == 1 ? "I'm listening" : aiState == 2 ? "Mary is" : "HOLD TALK";
    const char* b = aiState == 1 ? "Let go when" : aiState == 2 ? "thinking..." : "and speak.";
    const char* c3 = aiState == 1 ? "you're done." : aiState == 2 ? "" : "Or tap here";
    const char* d = aiState == 0 ? "to type." : "";
    cvText(122, 38, a, rgb(255, 230, 120), 1); cvText(122, 63, b, rgb(200, 210, 230), 1); cvText(122, 88, c3, rgb(200, 210, 230), 1); cvText(122, 113, d, rgb(200, 210, 230), 1);
  }
  else if(!n){
    if(mary && aiState == 1){ cvText(14, 38, "I'm listening...", rgb(255, 230, 120), 1); cvText(14, 63, "Let go when you're done", rgb(200, 210, 230), 1); }
    else if(mary && aiState == 2) cvText(14, 38, "Mary is thinking...", rgb(200, 210, 230), 1);
    else if(mary){ cvText(14, 38, "HOLD TALK and speak.", rgb(200, 210, 230), 1); cvText(14, 63, "Let go when done.", rgb(200, 210, 230), 1); cvText(14, 88, "Or tap here to type.", rgb(200, 210, 230), 1); }
    else cvText(14, 38, hubWait ? "Mary is thinking..." : "Tap a button to ask Mary", rgb(200, 210, 230), 1);
  }
  for(int k = 0; k < 4; k++){ int j = ansPg * 4 + k; if(j < n) cvText(10, 37 + k * 25, L[j].c_str(), 0xFFFF, 1); }
  for(int i = 0; i < 5; i++){
    int x = 2 + i * 63;
    cvGlass(x + 2, 151, 59, 47, 10, 0x0000, 140, 1);
    const char* lb = mary ? MB[i] : JB[i];
    uint16_t tc = i == 4 ? rgb(225, 50, 60) : i == 3 ? rgb(60, 100, 170) : (mary && i == 0) ? (aiState == 1 ? rgb(255, 196, 40) : rgb(230, 80, 150)) : rgb(20, 170, 150);
    cvGlass(x, 146, 59, 47, 10, tc, 190, 0);
    cvText(x + (59 - (int)strlen(lb) * FW) / 2, 158, lb, 0xFFFF, 1);
  }
}
// two pages of six buttons, never hidden.  Page 1: switch 1 2 3, switch 4 (buzzer), MUSIC, MORE.   Page 2: GAME, SLEEP, CAMERA, AI, LOUD, BACK.
bool nameHas(int i, const char* w){ return nm[i].indexOf(w) >= 0; }
bool isDoor(int i){ return i < NREL && (nameHas(i, "GARAGE") || nameHas(i, "DOOR") || nameHas(i, "GATE")) && !nameHas(i, "LIGHT"); }      // a door gets one short press
bool tileBuzz(int i){ return i == NREL - 1 && buzzIs4(); }
// 6.2: ONE long page.  All the buttons in rows of three; a finger drags the rows up and down (no more MORE and BACK).  pgNo is the row at the top.
static const uint8_t TL[] = {0, 1, 2, 3, 4, 16, 9, 18, 10, 12, 14, 15, 6, 13, 7, 8};
#define TLN ((int)sizeof(TL))
#define TLROWS ((TLN + 2) / 3)
int pgY = 0;                                                      // 6.3: how far the long page is slid up, in dots.  It glides with the finger
#define PGMAX ((TLROWS - 2) * 85)
int tileId(int p){ return (p >= 0 && p < TLN) ? TL[p] : -1; }
bool btSel(){ return sdCur == sdSongN + NRADIO; }
bool tileUsed(int id){ return true; }
bool tileOn(int id){ return id == 18 ? (musicOn && btSel()) : id == 4 ? (bool)musicOn : (id < NREL ? (tileBuzz(id) ? bzAlarm : on[id]) : false); }
bool drainTile(int id){ return id == 2 && pins[2] < 0 && drainIp.length() && millis() - drainSeen < 120000UL; }
void drawTile(int slot){
  int id = tileId(slot), x = 5 + (slot % 3) * 105, y = 33 + (slot / 3) * 85 - pgY;
  if(id < 0 || y + 84 <= 31 || y >= 204) return;
  if(drainTile(id)){
    if(y + 84 <= cvY0 || y >= cvY0 + cvHh) return;
    bool wet = drainWet == 1;
    cvGlass(x + 3, y + 7, 97, 77, 14, 0x0000, 150, 1);
    cvGlass(x, y, 100, 77, 14, wet ? rgb(230, 30, 40) : rgb(30, 170, 80), wet ? 215 : 185, 0);
    cvSprite(x + 7, y + 6, IC_CAM);
    const char* st = wet ? "WET" : "DRY";
    cvText(x + 100 - 7 - (int)strlen(st) * FW, y + 10, st, 0xFFFF, 1);
    cvText(x + (100 - 5 * FW) / 2, y + 47, "DRAIN", 0xFFFF, 1);
    return;
  }
  if(y + 84 <= cvY0 || y >= cvY0 + cvHh) return;
  bool used = tileUsed(id), isOn = tileOn(id), door = id < NREL && isDoor(id);
  uint16_t tint = !used ? rgb(70, 78, 92) : isOn ? rgb(255, 196, 40) : door ? rgb(40, 120, 240) : id == 4 ? rgb(150, 70, 220) : id == 6 ? rgb(20, 170, 150) : id == 7 ? rgb(50, 50, 130)
                : id == 8 ? rgb(205, 60, 60) : id == 9 ? rgb(150, 70, 220) : id == 10 ? rgb(235, 120, 30) : (id == 5 || id == 11 || id == 17) ? rgb(60, 100, 170)
                : id == 12 ? rgb(30, 120, 210) : id == 13 ? rgb(20, 150, 90) : id == 14 ? rgb(200, 90, 160) : id == 15 ? rgb(230, 140, 30) : id == 16 ? rgb(230, 80, 150) : id == 18 ? rgb(40, 110, 235) : rgb(14, 34, 84);
  cvGlass(x + 3, y + 7, 97, 77, 14, 0x0000, 150, 1);              // the dark slab underneath: the button stands up off the picture
  cvGlass(x, y, 100, 77, 14, tint, isOn ? 195 : 165, 0);
  const uint16_t* ic = id == 4 ? IC_MUSIC : id == 5 ? IC_MORE : id == 6 ? IC_STAR : id == 7 ? IC_MOON : id == 8 ? IC_PLUG : id == 9 ? IC_MUSIC : id == 10 ? IC_LOUD : id == 11 ? IC_MORE
                     : id == 12 ? IC_STAR : id == 13 ? IC_PLUG : id == 14 ? IC_BELL : id == 15 ? IC_CAM : id == 16 ? IC_GIRL : id == 17 ? IC_BACK : id == 18 ? IC_MUSIC
                     : tileBuzz(id) ? IC_BELL : door ? IC_GARAGE : (nameHas(id, "LIGHT") || nameHas(id, "LAMP")) ? IC_LIGHT : IC_PLUG;
  cvSprite(x + 7, y + 6, ic);
  String st = id == 5 ? String(">>") : id == 6 ? String("GO") : id == 7 ? String("Zz") : id == 8 ? String("ALL") : id == 9 ? String(">>") : id == 10 ? String(volSet) : id == 11 ? String(">>")
            : id == 12 ? String("GO") : id == 13 ? (WiFi.status() == WL_CONNECTED ? String(wifiBars()) + "/4" : String("--")) : id == 14 ? String("HOLD")
            : id == 15 ? (sdPicN ? String(sdPicN) : String("--")) : id == 16 ? String("AI") : id == 17 ? String("<<") : id == 18 ? String(isOn ? "ON" : "BT")
            : !used ? String("--") : door ? String("TAP") : isOn ? String("ON") : String("OFF");
  cvText(x + 100 - 7 - (int)st.length() * FW, y + 10, st.c_str(), 0xFFFF, 1);
  String name = id == 4 ? String("MUSIC") : id == 5 ? String("MORE") : id == 6 ? String("GAME") : id == 7 ? String("SLEEP") : id == 8 ? String("ALL OFF") : id == 9 ? String("SONG") : id == 10 ? String("LOUD") : id == 11 ? String("MORE")
              : id == 12 ? String("WEB") : id == 13 ? String("WI-FI") : id == 14 ? String("CALL") : id == 15 ? String("PHOTOS") : id == 16 ? String("MARY") : id == 17 ? String("BACK") : id == 18 ? String("PHONE BT") : nm[id];
  if(name.length() > 8) name = name.substring(0, 8);
  cvText(x + (100 - (int)name.length() * FW) / 2, y + 47, name.c_str(), 0xFFFF, 1);
}
void paintScene(){                                                 // paints whatever part of the screen lies in the strip being built
  if(kbOn){ paintKb(); return; }
  if(callOn){ paintCall(); return; }                                  // the keyboard and the Wi-Fi page fill the whole screen
  if(wfOn){ paintWf(); return; }
  String net = apUp ? "SETUP" : (WiFi.status() != WL_CONNECTED ? "NO NET" : ((subOk && httpCode == 200) ? "READY" : "ONLINE"));
  cvGlass(0, 0, LCD_W, 30, 0, rgb(6, 14, 40), 150, 0);
  String foot;
  if(gameOn){
    cvText(8, 3, "TIC TAC TOE", rgb(255, 220, 90), 1);
    cvText(LCD_W - 8 - 4 * FW, 3, "EXIT", rgb(255, 150, 150), 1);
    cvGlass(260, 151, 57, 49, 10, 0x0000, 140, 1); cvGlass(258, 146, 57, 49, 10, rgb(225, 50, 60), 205, 0); cvText(263, 158, "EXIT", 0xFFFF, 1);      // a big EXIT button
    cvText(10, 60, "YOU", 0xFFFF, 1); cvText(16, 88, "X", rgb(255, 210, 60), 2);
    cvText(272, 60, "BOX", 0xFFFF, 1); cvText(278, 88, "O", rgb(255, 120, 140), 2);
    for(int i = 0; i < 9; i++){
      int x = 67 + (i % 3) * 63, y = 34 + (i / 3) * 56;
      cvGlass(x + 2, y + 5, 57, 48, 10, 0x0000, 140, 1);
      cvGlass(x, y, 58, 49, 10, gb[i] == 1 ? rgb(255, 196, 40) : gb[i] == 2 ? rgb(230, 70, 100) : rgb(14, 34, 84), gb[i] ? 190 : 150, 0);
      if(gb[i]) cvText(x + 17, y, gb[i] == 1 ? "X" : "O", 0xFFFF, 2);
    }
    foot = gWin == 0 ? String("YOUR TURN") : gWin == 1 ? String("YOU WIN! TAP TO PLAY") : gWin == 2 ? String("I WIN! TAP TO PLAY") : String("TIE! TAP TO PLAY");
  } else if(jetOn){
    paintJet();
    foot = hubWait ? String("MARY IS THINKING...") : hubPhone.length() ? (hubPhone.length() <= 15 ? String("PHONE ") + hubPhone : hubPhone) : hubInfo.length() ? hubInfo : String("TAP EXIT TO GO BACK");
  } else {
    for(int i = 0; i < TLN; i++) drawTile(i);
    cvBox(317, 34, 3, 168, rgb(40, 50, 80));                       // the slim bar on the right shows where you are on the long page
    { int th = 168 * 2 / TLROWS, ty = 34 + (168 - th) * pgY / (PGMAX > 0 ? PGMAX : 1); cvBox(317, ty, 3, th, rgb(235, 240, 255)); }
    cvBg(0, 31); cvBg(204, 36);                                    // buttons sliding past the top and the bottom go out of sight there
    cvGlass(0, 0, LCD_W, 30, 0, rgb(6, 14, 40), 150, 0);
    cvText(8, 3, "MARY'S MAGIC BOX", 0xFFFF, 1);
    int nx = LCD_W - 6 - (int)net.length() * FW, wb = wifiBars();
    cvText(nx, 3, net.c_str(), net == "READY" ? rgb(140, 255, 170) : rgb(255, 214, 90), 1);
    for(int i = 0; i < 4; i++)                                     // the Wi-Fi bars
      cvBox(nx - 36 + i * 8, 26 - (i + 1) * 5 - 1, 6, (i + 1) * 5 + 1, i < wb ? (wb >= 3 ? rgb(120, 255, 150) : wb == 2 ? rgb(255, 214, 90) : rgb(255, 90, 90)) : rgb(70, 80, 100));
    foot = (!apUp && gKey.length() && WiFi.status() == WL_CONNECTED) ? String("TAP ME TO TALK") : apUp ? (((millis() / 3000) & 1) && ssid.length() ? wifiWhy() : String("SETUP: ") + AP_NAME) : (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : wifiWhy());
  }
  if(msgUntil && (long)(msgUntil - millis()) > 0) foot = msgText;
  cvGlass(0, 204, LCD_W, 36, 0, rgb(6, 14, 40), 160, 0);
  { int bob = (vPlay && ((millis() / 180) & 1)) ? 3 : 0;          // Mary moves while she talks
    if(maryFace) cvImg(3, 204 - bob, 36, 36, maryFace); else cvSprite(3, 204 - bob, IC_GIRL); }
  if(foot.length() > 22) foot = foot.substring(0, 22);
  if(foot.length() > 22 && jetOn && !hubWait && hubPhone.length()) foot = hubPhone.substring(0, 22);
  cvText(44, 210, foot.c_str(), 0xFFFF, 1);
  if(!gameOn && !jetOn && foot.length() <= 18) cvText(LCD_W - 5 - 3 * FW, 210, BOX_VER, rgb(150, 165, 200), 1);      // the number of this build, small, in the corner
}
void lcdDraw(bool force){
  if(!lcdOn) return;
  bool msgOn = msgUntil && (long)(msgUntil - millis()) > 0;
  String key = stateText() + "|" + String(apUp ? 1 : 0) + String((int)WiFi.status()) + String(subOk && httpCode == 200 ? 1 : 0) + "|" + (msgOn ? msgText : String("")) + "|" + String(musicOn ? 1 : 0) + String(pgY) + String(volSet) + "|" + String(wifiBars()) + wifiWhy() + String(WiFi.status() == WL_CONNECTED ? 0 : (int)((millis() / 3000) & 1));
  if(camOn) return;                                               // the drain picture is on the screen
  if((webOn || phOn || pcOn) && !kbOn) return;                            // a web page or a photo is on the screen
  if(kbOn) key += "|k" + String(kbFor) + String(kbShift ? 1 : 0) + String(kbSym ? 1 : 0) + kbText;
  if(callOn) key += "|c" + callMsg + String(aiState) + String(callMode ? 1 : 0) + WiFi.gatewayIP().toString();
  if(wfOn){ key += "|w" + String(wfN) + WiFi.localIP().toString(); for(int i = 0; i < wfN && i < 5; i++) key += wfName[i] + String(barsOf(wfRs[i])); }
  key += "|d" + String(drainWet) + String(drainTile(2) ? 1 : 0);
  key += "|m" + String(aiState) + String(vPlay ? (int)((millis() / 180) & 1) : 2);
  if(jetOn){ key += "|j" + String(hubOk) + String(hubWait) + String(ansPg) + hubAns + hubInfo + hubPhone; }
  if(gameOn){ key += "|g"; for(int i = 0; i < 9; i++) key += String((int)gb[i]); key += String(gWin); }
  if(!force && key == lcdShown) return;
  lcdShown = key;
  for(int y = 0; y < LCD_H; y += CVH){ cvBand(y, LCD_H - y < CVH ? LCD_H - y : CVH); paintScene(); cvFlush(); }
}
void lcdSleep(bool s){ asleep = s; if(lcdOn) digitalWrite(LCD_BL, s ? LOW : HIGH); if(!s) lastTouch = millis(); }      // screen light off saves power; a touch wakes it
void showMsg(const String& m){ msgText = m; msgUntil = millis() + 3000; if(!msgUntil) msgUntil = 1; if(asleep) lcdSleep(false); }      // her words at the bottom for 3 seconds
void announce(int i, bool pulse){                                  // a switch changed: chime, her voice, and the words
  if(i < 0 || i >= NREL) return;
  bool light = nameHas(i, "LIGHT") || nameHas(i, "LAMP");
  if(pulse){ fxNote(523, 90); fxNote(659, 90); fxNote(784, 170); sayClip(isDoor(i) ? 2 : 3); showMsg(nm[i] + "!"); }
  else if(on[i]){ fxNote(660, 90); fxNote(990, 170); sayClip(light ? 0 : 3); showMsg(nm[i] + " ON"); }
  else { fxNote(600, 90); fxNote(400, 170); sayClip(light ? 1 : 4); showMsg(nm[i] + " OFF"); }
}
// ---- boxes find each other on your Wi-Fi (no Jetson needed), and every box serves the Moto Hub phone page at /hub
WiFiUDP pUdp; bool pUdpOn = false; unsigned long pSentAt = 0;
struct Peer { String ip, n, s, m; unsigned long seen; };
String nmList(){ String o; for(int i = 0; i < NREL; i++){ if(i) o += ','; o += nm[i]; } return o; }
Peer peers[12]; int nPeers = 0;
void peerPut(const String& ip, const String& n, const String& st, const String& mm = ""){
  if(n == "Drain pan"){ drainIp = ip; int w = st == "WET" ? 1 : 0; if(w && drainWet != 1) drainAlarmAt = 0; drainWet = w; drainSeen = millis(); }
  int k = -1; for(int i = 0; i < nPeers; i++) if(peers[i].ip == ip) k = i;
  if(k < 0){ if(nPeers < 12) k = nPeers++; else { k = 0; for(int i = 1; i < nPeers; i++) if(peers[i].seen < peers[k].seen) k = i; } }
  peers[k].ip = ip; peers[k].n = n.substring(0, 24); peers[k].s = st.substring(0, 40); peers[k].m = mm.substring(0, 40); peers[k].seen = millis();
}
void peerLoop(){
  if(apUp || WiFi.status() != WL_CONNECTED){ return; }
  if(!pUdpOn){ pUdpOn = pUdp.begin(4210); }
  if(!pSentAt || millis() - pSentAt > 10000UL){
    pSentAt = millis(); if(!pSentAt) pSentAt = 1;
    String m = "AMYBOX|" + hubName() + "|" + stateText() + "|" + nmList();
    pUdp.beginPacket(IPAddress(255, 255, 255, 255), 4210); pUdp.write((const uint8_t*)m.c_str(), m.length()); pUdp.endPacket();
  }
  int n = pUdp.parsePacket();
  if(n > 0 && n < 200){
    char b[200]; int r = pUdp.read((uint8_t*)b, 199); if(r < 0) r = 0; b[r] = 0; String m(b);
    String ip = pUdp.remoteIP().toString();
    if(m.startsWith("AMYBOX|") && ip != WiFi.localIP().toString()){ int a = m.indexOf('|', 7); if(a > 0){ int c2 = m.indexOf('|', a + 1); if(c2 > 0) peerPut(ip, m.substring(7, a), m.substring(a + 1, c2), m.substring(c2 + 1)); else peerPut(ip, m.substring(7, a), m.substring(a + 1)); } }
  }
}
String jsq(const String& s){ String o = "\""; for(unsigned int i = 0; i < s.length(); i++){ char c = s.charAt(i); if(c == '"' || c == '\\') o += '\\'; if(c >= 32) o += c; } return o + "\""; }
void handlePeers(){
  web.sendHeader("Access-Control-Allow-Origin", "*");
  if(!lpw.length() || web.arg("k") != lpw){ web.send(403, "application/json", "{}"); return; }
  String me = WiFi.localIP().toString();
  String j = "{\"me\":" + jsq(me) + ",\"jet\":" + jsq(hubOk && (uint32_t)hubIp ? hubIp.toString() + ":8090" : String("")) + ",\"boxes\":[";
  j += "{\"n\":" + jsq(hubName()) + ",\"ip\":" + jsq(me) + ",\"s\":" + jsq(stateText()) + ",\"nm\":" + jsq(nmList()) + ",\"age\":0}";
  for(int i = 0; i < nPeers; i++){ unsigned long age = (millis() - peers[i].seen) / 1000; if(age > 300) continue; j += ",{\"n\":" + jsq(peers[i].n) + ",\"ip\":" + jsq(peers[i].ip) + ",\"s\":" + jsq(peers[i].s) + ",\"nm\":" + jsq(peers[i].m) + ",\"age\":" + String(age) + "}"; }
  j += "]}";
  web.send(200, "application/json", j);
}
static const char HUB_PAGE[] PROGMEM = "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\"><meta name=theme-color content=\"#02060d\"><title>Moto Hub</title><style>\n"
":root{--c:#2ef2ff;--c2:#7a5cff;--a:#ffb21e;--r:#ff3b5c;--g:#33ff9a;--bg:#02060d;--p:#071425}\n"
"*{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none}\n"
"html,body{margin:0;height:100%;background:var(--bg);color:#d8f6ff;font-family:\"Rajdhani\",\"Orbitron\",system-ui,Roboto,Arial;letter-spacing:.5px;overflow-x:hidden}\n"
"#sky{position:fixed;inset:0;z-index:0}#lockv{position:fixed;inset:0;z-index:20;background:#02060df2;display:none;align-items:center;justify-content:center}#lockv .pan{width:90%;max-width:380px;text-align:center}\n"
".grid{position:fixed;left:-50%;right:-50%;bottom:-10%;height:55%;z-index:0;background:linear-gradient(transparent 0,#2ef2ff10 2px,transparent 3px) 0 0/100% 40px,linear-gradient(90deg,transparent 0,#2ef2ff10 2px,transparent 3px) 0 0/40px 100%;transform:perspective(380px) rotateX(62deg);transform-origin:bottom;animation:gr 3s linear infinite;mask-image:linear-gradient(transparent,#000 70%)}\n"
"@keyframes gr{to{background-position:0 40px,0 0}}\n"
"#flash{position:fixed;inset:0;z-index:50;pointer-events:none;background:radial-gradient(circle at 50% 40%,#fff6,#2ef2ff22 40%,transparent 70%);opacity:0;transition:opacity .5s}\n"
".wrap{position:relative;z-index:2;padding:10px 12px 108px;max-width:560px;margin:0 auto}\n"
".top{display:flex;align-items:center;gap:10px;padding:6px 4px 10px}\n"
".emb{width:44px;height:44px;clip-path:polygon(50% 0,100% 25%,100% 75%,50% 100%,0 75%,0 25%);background:conic-gradient(from 0deg,var(--c),var(--c2),var(--a),var(--c));display:grid;place-items:center;animation:spin 8s linear infinite}\n"
".emb b{width:36px;height:36px;clip-path:inherit;background:#04101c;display:grid;place-items:center;color:var(--c);font-weight:900;font-size:18px;animation:spin 8s linear infinite reverse}\n"
"@keyframes spin{to{transform:rotate(360deg)}}\n"
".tt{font-size:22px;font-weight:800;text-transform:uppercase;letter-spacing:3px;text-shadow:0 0 12px var(--c)}\n"
".st{font-size:12px;color:#7fb9c9;letter-spacing:1.5px;text-transform:uppercase}\n"
".lock{margin-left:auto;font-size:11px;padding:5px 10px;border:1px solid #2ef2ff55;border-radius:3px;color:var(--g);text-transform:uppercase;letter-spacing:2px;box-shadow:inset 0 0 12px #33ff9a22}\n"
".lock i{display:inline-block;width:7px;height:7px;background:var(--g);border-radius:50%;margin-right:6px;box-shadow:0 0 8px var(--g);animation:bl 1.6s infinite}\n"
"@keyframes bl{50%{opacity:.3}}\n"
".pan{position:relative;margin:12px 0;padding:16px 14px;background:linear-gradient(160deg,#0a1d33ee,#050d18ee);border:1px solid #2ef2ff33;clip-path:polygon(14px 0,calc(100% - 14px) 0,100% 14px,100% calc(100% - 14px),calc(100% - 14px) 100%,14px 100%,0 calc(100% - 14px),0 14px);box-shadow:inset 0 0 40px #2ef2ff12}\n"
".pan:before{content:\"\";position:absolute;inset:0;background:repeating-linear-gradient(0deg,#ffffff05 0 1px,transparent 1px 3px);pointer-events:none}\n"
".pan h2{margin:0 0 12px;font-size:13px;letter-spacing:4px;color:var(--c);text-transform:uppercase;display:flex;align-items:center;gap:8px}\n"
".pan h2:after{content:\"\";flex:1;height:1px;background:linear-gradient(90deg,#2ef2ff88,transparent)}\n"
".core{display:flex;gap:14px;align-items:center}\n"
".orb{flex:0 0 92px;height:92px;position:relative}\n"
".orb .r{position:absolute;inset:0;border-radius:50%;border:2px solid transparent;border-top-color:var(--c);border-left-color:#2ef2ff55;animation:spin 3s linear infinite}\n"
".orb .r2{inset:9px;border-top-color:var(--a);border-right-color:#ffb21e55;animation-duration:2s;animation-direction:reverse}\n"
".orb .c{position:absolute;inset:20px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#fff,var(--c) 30%,#0a5f7a 70%,#03141f);box-shadow:0 0 30px var(--c),inset 0 0 12px #000}\n"
".think .c{animation:pu .6s ease-in-out infinite alternate;background:radial-gradient(circle at 35% 30%,#fff,var(--a) 30%,#7a4a00 70%,#140a00);box-shadow:0 0 40px var(--a)}\n"
".think .r{animation-duration:.8s}\n"
"@keyframes pu{to{transform:scale(1.15)}}\n"
".say{flex:1;min-height:92px;padding:10px 12px;border-left:2px solid var(--c);background:#02101bcc;font-size:18px;line-height:1.4;font-family:system-ui,Roboto,Arial;letter-spacing:0;color:#e9fbff;white-space:pre-wrap;max-height:42vh;overflow:auto;user-select:text}\n"
".row{display:flex;gap:8px;margin-top:12px}\n"
"input{flex:1;min-width:0;font-size:17px;padding:13px 12px;background:#010a12;border:1px solid #2ef2ff55;color:#fff;outline:none;font-family:system-ui;border-radius:4px;box-shadow:inset 0 0 14px #2ef2ff22;user-select:text}\n"
".k{position:relative;border:0;cursor:pointer;color:#e8fbff;font:800 15px/1 inherit;letter-spacing:2px;text-transform:uppercase;padding:14px 14px;border-radius:6px;\n"
" background:linear-gradient(180deg,#1b3c5c,#0b1c2e 55%,#07121e);box-shadow:0 5px 0 #020a12,0 7px 14px #000a,inset 0 1px 0 #6fe9ff66,inset 0 0 0 1px #2ef2ff44;transition:transform .07s,box-shadow .07s;text-shadow:0 0 6px #2ef2ff99}\n"
".k:active,.k.pr{transform:translateY(4px);box-shadow:0 1px 0 #020a12,0 2px 6px #000a,inset 0 1px 0 #6fe9ff44,inset 0 0 0 1px #2ef2ff88}\n"
".k.c{background:linear-gradient(180deg,#0fa9c0,#06637a 55%,#044657);box-shadow:0 5px 0 #022a35,0 7px 18px #2ef2ff44,inset 0 1px 0 #b8f8ff,inset 0 0 0 1px #2ef2ff}\n"
".k.a{background:linear-gradient(180deg,#e89a10,#9a5c00 55%,#6a3e00);box-shadow:0 5px 0 #3a2200,0 7px 18px #ffb21e44,inset 0 1px 0 #ffe2a0,inset 0 0 0 1px var(--a);text-shadow:0 0 6px #ffcf6a}\n"
".k.r{background:linear-gradient(180deg,#e0304e,#8c1026 55%,#5c0818);box-shadow:0 5px 0 #33040d,0 7px 18px #ff3b5c44,inset 0 1px 0 #ffb0bd,inset 0 0 0 1px var(--r)}\n"
".k.ic{width:52px;padding:0;font-size:22px}.k.on{animation:gl 1.5s infinite alternate}@keyframes gl{to{box-shadow:0 5px 0 #022a35,0 0 26px var(--c),inset 0 1px 0 #b8f8ff,inset 0 0 0 1px #2ef2ff}}\n"
".br{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:12px}.br .k{padding:12px 4px;font-size:13px}.br small{display:block;font-size:10px;letter-spacing:1px;opacity:.7;margin-top:5px}\n"
".box{margin-top:12px}.bx{display:flex;justify-content:space-between;font-size:15px;font-weight:800;letter-spacing:2px;color:#bff6ff;text-transform:uppercase}.bx span{font-size:11px;color:#6fa4b4;font-weight:600}\n"
".sw{display:grid;grid-template-columns:repeat(2,1fr);gap:12px;margin-top:10px}\n"
".s{position:relative;height:96px;text-align:left;padding:12px;display:flex;flex-direction:column;justify-content:space-between}\n"
".s .nm{font-size:15px}.s .vl{font-size:12px;letter-spacing:3px;opacity:.85}\n"
".s .led{position:absolute;right:12px;top:12px;width:14px;height:14px;border-radius:50%;background:#0b2230;box-shadow:inset 0 0 4px #000}\n"
".s.on{background:linear-gradient(180deg,#ffd35a,#e29400 55%,#9a5c00);color:#2a1600;text-shadow:none;box-shadow:0 5px 0 #4a2a00,0 0 30px #ffb21e88,inset 0 1px 0 #fff3c0}\n"
".s.on .led{background:#fff7c8;box-shadow:0 0 14px #fff,0 0 30px var(--a)}\n"
".s.door{background:linear-gradient(180deg,#2848b8,#152a74 55%,#0c1a4a);box-shadow:0 5px 0 #060d26,0 7px 18px #4a6cff44,inset 0 1px 0 #a8b8ff,inset 0 0 0 1px #5a7cff}\n"
".cams{display:grid;gap:12px}.cam{position:relative;border:1px solid #2ef2ff66;background:#000;clip-path:polygon(10px 0,100% 0,100% calc(100% - 10px),calc(100% - 10px) 100%,0 100%,0 10px)}\n"
".cam img,.cam video{width:100%;display:block;min-height:140px}.cam .tg{position:absolute;left:10px;top:8px;font-size:12px;letter-spacing:2px;background:#000a;padding:3px 8px;color:var(--c)}\n"
".cam .rec{position:absolute;right:10px;top:8px;font-size:12px;letter-spacing:2px;color:var(--r);font-weight:800}.cam .rec:before{content:\"\\25CF \";animation:bl 1s infinite}\n"
".cam .sn{position:absolute;right:8px;bottom:8px}\n"
".em{color:#6fa4b4;font-size:14px;font-family:system-ui;letter-spacing:0}\n"
"nav{position:fixed;z-index:5;left:8px;right:8px;bottom:8px;max-width:560px;margin:0 auto;display:grid;grid-template-columns:repeat(3,1fr);gap:8px;padding:10px;background:linear-gradient(180deg,#0a1a2cf2,#040b14f2);border:1px solid #2ef2ff44;clip-path:polygon(18px 0,calc(100% - 18px) 0,100% 18px,100% 100%,0 100%,0 18px)}\n"
"nav .k{padding:10px 0 8px;font-size:12px}nav .k b{display:block;font-size:22px;margin-bottom:3px}\n"
".tab{display:none}.tab.on{display:block;animation:in .3s}@keyframes in{from{opacity:0;transform:translateY(8px)}}\n"
".ft{text-align:center;font-size:11px;color:#4f7f8f;letter-spacing:1.5px;margin:12px 0}\n"
"</style></head><body>\n"
"<canvas id=sky></canvas><div class=grid></div><div id=flash></div>\n"
"<div id=lockv><div class=pan><div class=emb style=\"margin:0 auto 12px\"><b>M</b></div><h2>Moto Hub locked</h2><input id=pw type=password inputmode=numeric placeholder=\"Password\"><div class=row><button class=\"k c\" style=flex:1 onclick=unlock()>Unlock</button></div><div class=em id=pwm style=\"margin-top:8px\"></div></div></div>\n"
"<div class=wrap>\n"
"<div class=top><div class=emb><b>M</b></div><div><div class=tt>Moto Hub</div><div class=st id=st>Linking...</div></div><div class=lock><i id=dot></i>Secure</div></div>\n"
"<div class=\"tab on\" id=t0><div class=pan><h2>Switches</h2><div id=boxes><div class=em>Looking for boxes...</div></div>\n"
"<div class=em style=\"margin-top:12px\">Hold a button 1 second to rename it. A name with DOOR or GARAGE gives one short press.</div></div></div>\n"
"<div class=tab id=t1><div class=pan><h2>Mary Core</h2>\n"
"<div class=core><div class=orb id=orb><div class=r></div><div class=\"r r2\"></div><div class=c></div></div><div class=say id=ans>...</div></div>\n"
"<div class=row><input id=q placeholder=\"Command Mary...\" onkeydown=\"if(event.key=='Enter')ask()\"><button class=\"k c\" onclick=ask()>Send</button></div>\n"
"<div class=row><button class=\"k r\" style=flex:1 onclick=stopAll()>&#9632; Stop</button><button class=k style=flex:1 onclick=jopen(0)>&#128640; Full cockpit</button></div></div></div>\n"
"<div class=tab id=t2><div class=pan><h2>Security cams</h2><div class=cams id=cams><div class=em>The cameras come through the Jetson.</div></div>\n"
"<div class=row><button class=\"k c\" style=flex:1 onclick=jopen(2)>&#128247; Share a camera</button></div></div></div>\n"
"<div class=ft>&copy; Moto Takeda 316-304-5398 &middot; works with or without the Jetson</div>\n"
"</div>\n"
"<nav><button class=\"k c\" onclick=tab(0)><b>&#9211;</b>Switches</button><button class=k onclick=tab(1)><b>&#9673;</b>Mary</button><button class=k onclick=tab(2)><b>&#9678;</b>Cams</button></nav>\n"
"<script>\n"
"var K='',J='',last='',AC=null;try{K=localStorage.getItem('mk')||''}catch(e){}\n"
"function $(i){return document.getElementById(i)}function esc(s){return String(s).replace(/[&<>\"']/g,c=>'&#'+c.charCodeAt(0)+';')}\n"
"function snd(type){try{AC=AC||new(window.AudioContext||window.webkitAudioContext)();var t=AC.currentTime,o=AC.createOscillator(),g=AC.createGain();o.connect(g);g.connect(AC.destination);\n"
" if(type=='on'){o.type='sawtooth';o.frequency.setValueAtTime(220,t);o.frequency.exponentialRampToValueAtTime(1320,t+.18);g.gain.setValueAtTime(.0001,t);g.gain.exponentialRampToValueAtTime(.2,t+.03);g.gain.exponentialRampToValueAtTime(.0001,t+.35);o.start(t);o.stop(t+.36)}\n"
" else if(type=='off'){o.type='sawtooth';o.frequency.setValueAtTime(900,t);o.frequency.exponentialRampToValueAtTime(110,t+.25);g.gain.setValueAtTime(.18,t);g.gain.exponentialRampToValueAtTime(.0001,t+.3);o.start(t);o.stop(t+.31)}\n"
" else{o.type='triangle';o.frequency.setValueAtTime(1600,t);o.frequency.exponentialRampToValueAtTime(700,t+.06);g.gain.setValueAtTime(.15,t);g.gain.exponentialRampToValueAtTime(.0001,t+.09);o.start(t);o.stop(t+.1)}}catch(e){}}\n"
"function flash(){var f=$('flash');f.style.transition='none';f.style.opacity=1;setTimeout(()=>{f.style.transition='opacity .6s';f.style.opacity=0},60)}\n"
"document.addEventListener('pointerdown',e=>{if(e.target.closest('.k'))snd('tick')});\n"
"function tab(n){document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('on',i==n));document.querySelectorAll('nav .k').forEach((t,i)=>t.classList.toggle('c',i==n))}\n"
"function unlock(){K=$('pw').value.trim();try{localStorage.setItem('mk',K)}catch(e){}$('lockv').style.display='none';tick()}\n"
"function NM(){try{return JSON.parse(localStorage.getItem('mn')||'{}')}catch(e){return{}}}\n"
"function nm(id,def){return NM()[id]||def}\n"
"function isDoor(n){return /DOOR|GARAGE|GATE/i.test(n)}\n"
"var holdT=null,held=false;\n"
"function hd(id){held=false;holdT=setTimeout(()=>{held=true;var v=prompt('Name for this button (like LIGHT or GARAGE DOOR):',nm(id,''));if(v!==null){var m=NM();m[id]=v.toUpperCase().slice(0,14);try{localStorage.setItem('mn',JSON.stringify(m))}catch(e){}tick()}},900)}\n"
"function hu(){clearTimeout(holdT)}\n"
"function sw(ip,n,name,isOn){if(held){held=false;return}var door=isDoor(name);if(door||!isOn){snd('on');flash()}else snd('off');\n"
" fetch('http://'+ip+'/cmd?k='+encodeURIComponent(K)+'&c='+encodeURIComponent(n+(door?' pulse':' toggle'))).then(()=>setTimeout(tick,300))}\n"
"function ask(){var q=$('q').value.trim();if(!q)return;if(!J){$('ans').textContent='The Jetson is off. Switches still work.';return}$('q').value='';$('ans').textContent='Thinking...';$('orb').classList.add('think');fetch('http://'+J+'/ask?src=phone&k='+encodeURIComponent(K)+'&q='+encodeURIComponent(q))}\n"
"function stopAll(){try{speechSynthesis.cancel()}catch(e){}if(J)fetch('http://'+J+'/stop?k='+encodeURIComponent(K));snd('off')}\n"
"function jopen(t){if(J)location.href='https://'+J.split(':')[0]+':8443/?k='+encodeURIComponent(K)+'&tab='+t;else alert('The Jetson is off right now.')}\n"
"function tick(){if(!K){$('lockv').style.display='flex';return}\n"
" fetch('/peers?k='+encodeURIComponent(K)).then(r=>{if(r.status==403){K='';$('pwm').textContent='Wrong password';$('lockv').style.display='flex';throw 0}return r.json()}).then(d=>{\n"
"  J=d.jet;$('st').textContent=d.boxes.length+' box'+(d.boxes.length==1?'':'es')+' · Jetson '+(J?'online':'off');$('dot').style.background='#33ff9a';\n"
"  var h='';d.boxes.forEach(x=>{h+='<div class=box><div class=bx>'+esc(x.n)+'<span>'+esc(x.ip)+(x.age>40?' · offline '+x.age+'s':' · online')+'</span></div><div class=sw>';var names=(x.nm||'').split(',');\n"
"   (x.s||'').replace(/^s /,'').split(' ').forEach(p=>{var m=p.split(':');if(m.length!=2||m[1]=='-')return;var n=+m[0],id=x.ip+'#'+n,on=m[1]=='on',name=nm(id,names[n-1]||('Switch '+n)),door=isDoor(name);\n"
"    h+='<button class=\"k s'+(door?' door':on?' on':'')+'\" onpointerdown=\"hd(\\''+esc(id)+'\\')\" onpointerup=hu() onpointerleave=hu() onclick=\"sw(\\''+esc(x.ip)+'\\','+n+',\\''+esc(name)+'\\','+on+')\"><i class=led></i><span class=nm>'+esc(name)+'</span><span class=vl>'+(door?'TAP = OPEN / CLOSE':on?'ON':'OFF')+'</span></button>'});h+='</div></div>'});\n"
"  $('boxes').innerHTML=h||'<div class=em>No boxes found yet.</div>';\n"
"  if(J)fetch('http://'+J+'/status?k='+encodeURIComponent(K)).then(r=>r.json()).then(s=>{$('orb').classList.toggle('think',s.busy);$('ans').textContent=s.busy?'Thinking...':(s.answer||'Systems online, Moto.');\n"
"   if(!s.busy&&s.answer&&s.answer!=last&&s.answer!='Stopped.'){last=s.answer;if(window.speechSynthesis){speechSynthesis.cancel();speechSynthesis.speak(new SpeechSynthesisUtterance(s.answer.slice(0,600)))}}\n"
"   var c='';(s.cams||[]).forEach(id=>{c+='<div class=cam><img src=\"http://'+J+'/cam/get?id='+encodeURIComponent(id)+'&k='+encodeURIComponent(K)+'&t='+Date.now()+'\"><span class=tg>CAM '+esc(id)+'</span><span class=rec>LIVE</span></div>'});$('cams').innerHTML=c||'<div class=em>No cameras sharing yet.</div>'}).catch(e=>{});\n"
"  else{$('ans').textContent='The Jetson is off. Switches still work.'}\n"
" }).catch(e=>{if(e!==0){$('st').textContent='No connection';$('dot').style.background='#ff3b5c'}})}\n"
"(function(){var c=$('sky'),x=c.getContext('2d'),S=[];function rs(){c.width=innerWidth;c.height=innerHeight}rs();onresize=rs;for(var i=0;i<120;i++)S.push([Math.random()*2-1,Math.random()*2-1,Math.random()]);\n"
" (function f(){x.fillStyle='#02060d';x.fillRect(0,0,c.width,c.height);var w=c.width/2,h=c.height/2;S.forEach(s=>{s[2]-=.004;if(s[2]<=.02){s[0]=Math.random()*2-1;s[1]=Math.random()*2-1;s[2]=1}x.fillStyle='rgba(160,240,255,'+(1-s[2])+')';x.fillRect(w+s[0]/s[2]*w*.6,h+s[1]/s[2]*h*.6,(1-s[2])*2,(1-s[2])*2)});requestAnimationFrame(f)})()})();\n"
"setInterval(tick,2500);tick();\n"
"</script></body></html>";
void handleHub(){ web.send_P(200, "text/html", HUB_PAGE); }
// ---- the game: tic tac toe against the box
uint32_t gRand(){ gSeed = gSeed * 1664525UL + 1013904223UL + (uint32_t)millis(); return gSeed >> 8; }
static const uint8_t GL[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
bool gLine(int p){ for(int k = 0; k < 8; k++) if(gb[GL[k][0]] == p && gb[GL[k][1]] == p && gb[GL[k][2]] == p) return true; return false; }
int gFind(int p){                                                  // the square that would finish a line for p, or -1
  for(int k = 0; k < 8; k++){ int n = 0, e = -1; for(int j = 0; j < 3; j++){ int c = GL[k][j]; if(gb[c] == p) n++; else if(gb[c] == 0) e = c; } if(n == 2 && e >= 0) return e; }
  return -1;
}
void gameStart(bool talk){ gameOn = true; gWin = 0; for(int i = 0; i < 9; i++) gb[i] = 0; if(talk) sayClip(11); }
void gCheck(){
  if(gLine(1)) gWin = 1; else if(gLine(2)) gWin = 2; else { bool full = true; for(int i = 0; i < 9; i++) if(!gb[i]) full = false; if(full) gWin = 3; }
  if(gWin == 1){ fxNote(523, 100); fxNote(659, 100); fxNote(784, 100); fxNote(1047, 260); sayClip(12); }
  else if(gWin == 2){ fxNote(400, 160); fxNote(300, 260); }
  else if(gWin == 3){ fxNote(500, 120); fxNote(500, 120); }
}
void gTap(int cell){
  if(cell < 0 || cell > 8 || gb[cell]){ fxNote(240, 50); return; }
  gb[cell] = 1; fxNote(880, 70); gCheck(); if(gWin) return;
  int m = gFind(2); if(m < 0) m = gFind(1); if(m < 0 && !gb[4]) m = 4;      // the box: win, or block, or the middle, or a corner
  if(m < 0){ static const uint8_t C[4] = {0, 2, 6, 8}; int s = (int)(gRand() % 4); for(int k = 0; k < 4 && m < 0; k++){ int c = C[(s + k) % 4]; if(!gb[c]) m = c; } }
  for(int i = 0; i < 9 && m < 0; i++) if(!gb[i]) m = i;
  if(m >= 0){ gb[m] = 2; fxNote(0, 140); fxNote(520, 90); }
  gCheck();
}
void tileTap(int slot){                                            // a finger on a button
  int id = tileId(slot);
  if(id < 0) return;
  fxNote(1500, 30);
  if(drainTile(id)){ camOn = true; camAt = 0; camOpened = millis(); return; }
  if(id == 4){ musicOn = !musicOn; sayClip(musicOn ? 7 : 8); showMsg(musicOn ? (realMusic() ? songName(sdCur) : String("MUSIC ON")) : String("MUSIC OFF")); return; }
  if(id == 18){                                                    // the phone's music by Bluetooth: the phone plays to the Jetson, the Jetson hands it to this box
    if(musicOn && btSel()){ musicOn = false; sdWant = 0; sayClip(8); showMsg("PHONE MUSIC OFF"); return; }
    if(!muBuf || WiFi.status() != WL_CONNECTED){ showMsg("NEEDS WI-FI + JETSON"); return; }
    sdWant = sdSongN + NRADIO; musicOn = true; showMsg("BLUETOOTH: MARY BOX"); return; }
  if(id == 12){ webOn = true; webAt = 0; webAct = ""; webErr = false; webFull = true; webSkip = true; return; }
  if(id == 13){ wfOn = true; wfN = -1; return; }                 // -1: look for Wi-Fi right after the screen says so
  if(id == 14){ callOn = true; return; }                          // the CALL page: talk, type, camera
  if(id == 15){ if(!sdPicN){ showMsg("NO PHOTOS ON SD CARD"); return; } phOn = true; phAt = 0; return; }
  if(id == 16){ if(!gKey.length() && !hubHost.length()){ showMsg("MARY NEEDS THE KEY"); return; } jetOn = true; ansPg = 0; hubAt = 0; return; }
  if(id == 6){ gameStart(true); return; }
  if(id == 7){ lcdSleep(true); return; }
  if(id == 8){ for(int i = 0; i < NREL; i++) setRelay(i, false); if(bzAlarm) setBuzz(false); musicOn = false; fxNote(600, 90); fxNote(400, 170); sayClip(13); showMsg("ALL OFF"); reportTag = ""; needReport = true; return; }
  if(id == 9){
    if(realMusic()){ int k = btSel() ? 0 : (sdCur + 1) % (sdSongN + NRADIO); sdWant = k; musicOn = true; showMsg(songName(k)); return; }      // the next station or SD card song
    int k = (song + 1) % 4; songWant = k; musicOn = true; showMsg(SONG_NAME[k]); return; }
  if(id == 10){ volSet = volSet % 3 + 1; if(spk && board == 2) esVol(); prefs.putInt("vo", volSet); fxNote(784, 200); sayClip(9); showMsg(String("LOUD ") + String(volSet)); return; }
  if(tileBuzz(id)){ setBuzz(!bzAlarm); if(!bzAlarm) sayClip(6); showMsg(bzAlarm ? "BUZZER ON" : "BUZZER OFF"); }
  else if(isDoor(id)){ setRelay(id, true); pulseEnd[id] = millis() + 700; if(!pulseEnd[id]) pulseEnd[id] = 1; if(spk) announce(id, true); else beep(1, 90, 110); }
  else { setRelay(id, !on[id]); if(spk) announce(id, false); else if(bzClick) beep(on[id] ? 1 : 2, 90, 110); }
  reportTag = ""; needReport = true;                               // tell the phone app too
}

// ------------------------------------------------------------ the touch screen (2.8 inch touch board: FT6336 touch chip on pins 16 and 15)
bool tpProbe(){ Wire.beginTransmission(0x38); return Wire.endTransmission() == 0; }
void tpInit(){ pinMode(18, OUTPUT); digitalWrite(18, LOW); delay(10); digitalWrite(18, HIGH); delay(250); Wire.begin(16, 15); tpOk = tpProbe(); }
bool tpRead(int &x, int &y){
  Wire.beginTransmission(0x38); Wire.write(0x02);
  if(Wire.endTransmission(false) != 0) return false;
  if(Wire.requestFrom(0x38, 5) != 5) return false;
  int n = Wire.read() & 0x0F, xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
  if(n < 1 || n > 2) return false;
  int tx = ((xh & 0x0F) << 8) | xl, ty = ((yh & 0x0F) << 8) | yl;      // the glass reports upright (240 wide, 320 tall); the picture lies sideways
  int r = (lcdRot + tpFlip + 2) & 3;      // (+2: on the real board the glass counts from the other edge)
  x = (r == 0 || r == 2) ? ty : 319 - ty;
  y = (r == 0 || r == 3) ? tx : 239 - tx;
  return true;
}
int tpZ = 0, tpLX = -1, tpLY = -1;                           // last press strength and place (shown on the setup page, to check the touch)
int tpZ1 = 0, tpZ2 = 0, tpRX = 0, tpRY = 0, tpTaps = 0, tpStuck = 0, tpKind = 0;      // raw numbers for the touch test page; kind 1 = press screen (XPT2046), 2 = light-touch (CST820)
#if CYD
SPIClass tSpi(VSPI);                                             // yellow board: the touch chip gets its own hardware wire set
#else
SPIClass tSpi(FSPI);                                             // the black S3 box has no VSPI; this one is never used there
#endif
uint16_t xpt16(uint8_t cmd){ tSpi.transfer(cmd); uint16_t v = tSpi.transfer16(0); return (v >> 3) & 0xFFF; }
bool cstFound(){
  Wire.begin(33, 32); Wire.beginTransmission(0x15); bool ok = Wire.endTransmission() == 0;
  if(!ok){ Wire.end(); }
  return ok;
}
void tpInit3(){
#if CYD
  dac_output_disable(DAC_CHANNEL_1); rtc_gpio_deinit(GPIO_NUM_25);      // pin 25 belongs to the touch, never to the sound
#endif
  pinMode(25, OUTPUT); digitalWrite(25, HIGH); delay(20);
  if(cstFound()){ tpKind = 2; tpOk = true; Serial.println("Touch: light-touch chip CST820 found."); return; }
  pinMode(25, INPUT); pinMode(33, OUTPUT); digitalWrite(33, HIGH); pinMode(36, INPUT);
  tSpi.begin(25, 39, 32, 33);
  tpKind = 1; tpOk = true;
  Serial.println("Touch: press-type chip XPT2046 on pins 25, 39, 32, 33.");
}
bool tpRead3(int &x, int &y){
  int sx = 0, sy = 0;
  if(tpKind == 2){
    uint8_t d[5] = {0};
    Wire.beginTransmission(0x15); Wire.write((uint8_t)0x02); if(Wire.endTransmission(false) != 0) return false;
    if(Wire.requestFrom(0x15, 5) != 5) return false;
    for(int i = 0; i < 5; i++) d[i] = Wire.read();
    tpZ = d[0];
    if(d[0] == 0 || d[0] > 2) return false;
    tpRX = ((d[1] & 0x0F) << 8) | d[2]; tpRY = ((d[3] & 0x0F) << 8) | d[4];
    sx = tpRY * 320 / 320; sy = 239 - tpRX * 240 / 240;          // this chip reports in screen pixels, standing up
  } else {
    tSpi.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    digitalWrite(33, LOW);
    tpZ1 = xpt16(0xB1); tpZ2 = xpt16(0xC1);
    int z = tpZ1 + 4095 - tpZ2;
    xpt16(0x91);                                                  // first reading is thrown away (it is noisy)
    long a = 0, b = 0; for(int k = 0; k < 3; k++){ a += xpt16(0x91); b += xpt16(0xD1); }
    tSpi.transfer(0xD0); tSpi.transfer16(0);                      // back to resting
    digitalWrite(33, HIGH); tSpi.endTransaction();
    tpZ = z; tpRX = a / 3; tpRY = b / 3;
    bool stuck = tpZ1 > 4000 && tpZ2 > 4000;                      // every wire reads high: the chip is not answering, that is NOT a press
    if(stuck) tpStuck++; else tpStuck = 0;
    if(stuck || z < 400 || z > 3900) return false;
    sx = (int)((tpRX - 200) * 320L / 3500); sy = (int)((tpRY - 240) * 240L / 3560);
  }
  if(sx < 0) sx = 0; if(sx > 319) sx = 319; if(sy < 0) sy = 0; if(sy > 239) sy = 239;
  int r = (lcdRot + tpFlip) & 3;
  x = (r == 0 || r == 3) ? sx : 319 - sx;
  y = (r == 0 || r == 2) ? sy : 239 - sy;
  tpLX = x; tpLY = y;
  return true;
}
void handleTouch(){                                              // live touch numbers, refreshes itself every second
  String h = "<!doctype html><meta name=viewport content='width=device-width'><meta http-equiv=refresh content=1><body style='font:20px sans-serif;background:#111;color:#eee;padding:14px'><h3>Touch test</h3>";
  h += "Chip: " + String(tpKind == 2 ? "light-touch CST820" : tpKind == 1 ? "press-type XPT2046" : "none") + "<br>";
  h += "Pressed now: <b>" + String(tpDown ? "YES" : "no") + "</b><br>Taps counted: " + String(tpTaps) + "<br>";
  h += "Strength: " + String(tpZ) + " (z1 " + String(tpZ1) + ", z2 " + String(tpZ2) + ")<br>Raw: " + String(tpRX) + ", " + String(tpRY) + "<br>";
  h += "Last press at: " + String(tpLX) + ", " + String(tpLY) + "<br>Chip silent count: " + String(tpStuck) + "<br>";
  h += "<small>Mary&#39;s Magic Box 6.5. Press the yellow screen and watch these numbers.</small><p><a href=/ style=color:#9cf>Back to setup</a>";
  web.send(200, "text/html", h);
}
bool hDn = false, hDrag = false; int hX0 = 0, hY0 = 0, hP0 = 0, hUp = 0;
void homeTp(bool down, int x, int y, unsigned long now){          // the long page of buttons: a short touch presses a button, a slide moves the whole page with the finger
  if(down){
    hUp = 0;
    if(!hDn){ hDn = true; hDrag = false; hX0 = x; hY0 = y; hP0 = pgY; lastTouch = now; return; }
    int dy = y - hY0;
    if(!hDrag && (dy >= 10 || dy <= -10)) hDrag = true;
    if(hDrag){
      int np = hP0 - dy; if(np > PGMAX) np = PGMAX; if(np < 0) np = 0;
      if(np - pgY >= 2 || pgY - np >= 2){ pgY = np; lastTouch = now; lcdDraw(false); }
    }
    return;
  }
  if(!hDn) return;
  if(++hUp < 2) return;                                           // the finger must be off for two looks: a skipped reading is not a lift
  hDn = false; hUp = 0;
  if(!hDrag && now - tpAt > 200){
    int yy = hY0 - 33 + pgY, row = yy / 85, col = hX0 < 108 ? 0 : (hX0 < 213 ? 1 : 2);
    if(yy >= 0 && tileId(row * 3 + col) >= 0){ tpAt = now; tpTaps++; lastTouch = now; tileTap(row * 3 + col); lcdDraw(false); }
  }
}
void tpLoop(){
  if(!tpOk || !lcdOn) return;
  unsigned long now = millis();
  if(now - tpSeen < 30) return;
  tpSeen = now;
  int x = 0, y = 0; bool down = board == 3 ? tpRead3(x, y) : tpRead(x, y);
  if(board == 3){ static int n3 = 0; if(down){ if(++n3 < 2) down = false; } else n3 = 0; }      // a pressed screen must hold for two looks: no ghost taps
  if(webOn && !kbOn && !asleep && !camOn){ webTp(down, x, y, now); tpDown = down; return; }      // a web page: tap, or drag to scroll
  { bool home = !kbOn && !wfOn && !callOn && !gameOn && !jetOn && !phOn && !pcOn && !camOn && !asleep;
    if(!home) hDn = false;
    else if(hDn || (down && !tpDown && y >= 31 && y < 203)){ homeTp(down, x, y, now); tpDown = down || hDn; return; } }
  if(down && !tpDown && now - tpAt > (kbOn ? 140UL : gameOn ? 160UL : 280UL)){
    tpAt = now; tpTaps++;
    if(asleep){ lcdSleep(false); tpDown = true; return; }         // the first touch only wakes the screen
    if(camOn){ camOn = false; lastTouch = now; fxNote(600, 60); lcdDraw(true); tpDown = true; return; }      // a tap on the drain picture goes back
    lastTouch = now;
    if((phOn || pcOn) && !kbOn){ if(pcOn){ pcOn = false; fxNote(600, 60); } else phTouch(x, y); if(!phOn && !pcOn){ lcdShown = ""; lcdDraw(true); } tpDown = true; return; }
    if(kbOn) kbTouch(x, y);
    else if(wfOn) wfTouch(x, y);
    else if(callOn) callTouch(x, y);
    else if(gameOn){
      if((x > 215 && y < 40) || (x >= 256 && y >= 140 && y < 204)){ gameOn = false; fxNote(600, 60); }
      else if(gWin) gameStart(false);
      else if(x >= 67 && x < 256 && y >= 34 && y < 202) gTap((x - 67) / 63 + ((y - 34) / 56) * 3);
    }
    else if(jetOn){ if(y >= 140 && y < 204){ int i = x / 64; if(i > 4) i = 4; jetTap(i); }
                    else if(y >= 33 && y < 140 && gKey.length() && aiState == 0){ fxNote(1500, 30); kbOpen(3, ""); } }      // tap Mary's box to type a question
    else if(y >= 204 && x < 60 && (hubHost.length() || gKey.length())){ jetOn = true; ansPg = 0; hubAt = 0; fxNote(1500, 30); }      // Amy's face at the bottom: the Amy AI page
    else if(y >= 31 && y < 203){ int i = (x < 108 ? 0 : (x < 213 ? 1 : 2)) + (y < 117 ? 0 : 3); tileTap(i); }
    lcdDraw(false);
  }
  if(!down && tpDown && aiRec) aiRec = false;                    // finger off TALK: Mary stops listening
  tpDown = down;
}

// ------------------------------------------------------------ the SMALL board (ideaspark ESP32 with the tiny 0.96 inch screen, SSD1306 on pins 21 and 22)
bool oledOn = false, oledBlank = false; uint8_t OB[1024]; String oledShown = "", oledState = ""; unsigned long oledWake = 0;
void oCmd(uint8_t c){ Wire.beginTransmission(0x3C); Wire.write((uint8_t)0x00); Wire.write(c); Wire.endTransmission(); }
void oInit(){
  static const uint8_t I[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6};
  for(unsigned int i = 0; i < sizeof(I); i++) oCmd(I[i]);
  oCmd((lcdRot & 1) ? 0xA0 : 0xA1); oCmd((lcdRot & 1) ? 0xC0 : 0xC8);      // "Screen turn" on the setup page flips it upside down
  oCmd(0xAF); oledOn = true; oledWake = millis();
}
void oPx(int x, int y, bool v){ if(x < 0 || x > 127 || y < 0 || y > 63) return; if(v) OB[x + (y >> 3) * 128] |= (uint8_t)(1 << (y & 7)); else OB[x + (y >> 3) * 128] &= (uint8_t)~(1 << (y & 7)); }
void oText(int x, int y, const char* t, bool inv){                 // small letters: the big letters at half size (6 wide, 12 tall)
  for(; *t; t++, x += 6){
    int ch = (uint8_t)*t; if(ch < 32 || ch > 126) ch = '?';
    const uint8_t* g = FONT + (ch - 32) * FH * FB;
    for(int r = 0; r < 12; r++) for(int c = 0; c < 6; c++){
      int n = 0;
      for(int dr = 0; dr < 2; dr++) for(int dc = 0; dc < 2; dc++){ int rr = r * 2 + dr, cc = c * 2 + dc; if(pgm_read_byte(g + rr * FB + (cc >> 3)) & (0x80 >> (cc & 7))) n++; }
      bool b = n >= 2;
      oPx(x + c, y + r, b != inv);
    }
  }
}
void oFlush(){
  oCmd(0x21); oCmd(0); oCmd(127); oCmd(0x22); oCmd(0); oCmd(7);
  for(int i = 0; i < 1024; i += 16){ Wire.beginTransmission(0x3C); Wire.write((uint8_t)0x40); for(int k = 0; k < 16; k++) Wire.write(OB[i + k]); Wire.endTransmission(); }
}
void oledDraw(){
  if(!oledOn) return;
  String net = apUp ? String("JOIN ") + AP_NAME : (WiFi.status() != WL_CONNECTED ? String("LOOKING FOR WI-FI") : ((subOk && httpCode == 200) ? String("READY ") + WiFi.localIP().toString() : String("WI-FI OK")));
  String st = stateText() + "|" + net; for(int i = 0; i < NREL; i++) st += pulseEnd[i] ? "p" : "-";
  if(st != oledState){ oledState = st; oledWake = millis(); if(oledBlank){ oledBlank = false; oCmd(0xAF); } }
  if(!oledBlank && millis() - oledWake > 600000UL){ oledBlank = true; oCmd(0xAE); return; }      // 10 quiet minutes: screen off, so it does not wear out. Any change wakes it.
  if(oledBlank) return;
  bool alt = ((millis() / 4000) & 1) && !apUp;
  String key = st + (alt ? "a" : "b");
  if(key == oledShown) return;
  oledShown = key;
  memset(OB, 0, sizeof(OB));
  String top = alt ? String("LET THERE BE LIGHT") : net; if(top.length() > 21) top = top.substring(0, 21);
  oText((128 - (int)top.length() * 6) / 2, 0, top.c_str(), false);
  for(int x = 0; x < 128; x++) oPx(x, 13, true);
  for(int i = 0; i < NREL; i++){
    int y = 16 + i * 12; bool door = isDoor(i), isOn = (i == NREL - 1 && buzzIs4()) ? bzAlarm : on[i];
    oText(2, y, nm[i].c_str(), false);
    const char* v = door ? (pulseEnd[i] ? "GO" : "DOOR") : (isOn ? "ON" : "OFF"); bool lit = door ? (pulseEnd[i] != 0) : isOn;
    int vx = 128 - (int)strlen(v) * 6 - 1; oText(vx, y, v, false);
    if(lit) for(int yy = 2; yy < 10; yy++) for(int xx = 0; xx < 8; xx++) oPx(vx - 12 + xx, y + yy, true);      // a solid square beside ON: easy to see from across the room
  }
  oFlush();
}

// ------------------------------------------------------------ the setup page
String cleanName(String s, int i){                              // big letters, numbers and spaces only, 8 at most
  static const char* DEF[NREL] = {"LIGHT", "GARAGE", "SWITCH 3", "BUZZER"};
  String o = "";
  for(unsigned int k = 0; k < s.length() && o.length() < 8; k++){ char c = s.charAt(k); if(c >= 'a' && c <= 'z') c = (char)(c - 32); if((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ') o += c; }
  o.trim();
  return o.length() ? o : String(DEF[i]);
}
String page(const String& note){
  String h = String("<!doctype html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><title>Moto Box</title><style>"
    "@keyframes st{to{background-position:0 600px,0 300px}}@keyframes gl{50%{box-shadow:0 0 22px #2ef2ff88,inset 0 0 18px #2ef2ff22}}@keyframes sp{to{transform:rotate(360deg)}}"
    "body{font-family:system-ui,Arial;color:#dff6ff;margin:0;padding:14px;font-size:17px;background:radial-gradient(1px 1px at 20px 30px,#fff,transparent),radial-gradient(1px 1px at 120px 80px,#9ff,transparent),radial-gradient(1.5px 1.5px at 260px 150px,#fff,transparent),radial-gradient(1px 1px at 330px 260px,#bdf,transparent),radial-gradient(1px 1px at 80px 220px,#fff,transparent),#02060d;background-size:360px 300px;animation:st 20s linear infinite}"
    "h2{margin:6px 0 12px;letter-spacing:3px;text-transform:uppercase;color:#2ef2ff;text-shadow:0 0 12px #2ef2ff;display:flex;align-items:center;gap:10px}"
    ".emb{width:40px;height:40px;clip-path:polygon(50% 0,100% 25%,100% 75%,50% 100%,0 75%,0 25%);background:conic-gradient(#2ef2ff,#7a5cff,#ffb21e,#2ef2ff);animation:sp 6s linear infinite}"
    "input,select,button{font-size:18px;padding:11px;border-radius:8px;border:1px solid #2ef2ff55;width:100%;box-sizing:border-box;margin:5px 0 11px;background:#010a12;color:#fff}"
    "button{background:linear-gradient(180deg,#0fa9c0,#06637a 55%,#044657);color:#fff;font-weight:800;letter-spacing:2px;text-transform:uppercase;box-shadow:0 5px 0 #022a35,0 8px 18px #2ef2ff44;border:0}button:active{transform:translateY(4px);box-shadow:0 1px 0 #022a35}"
    ".b2{background:linear-gradient(180deg,#1b3c5c,#0b1c2e 55%,#07121e);box-shadow:0 5px 0 #020a12,inset 0 0 0 1px #2ef2ff44}"
    ".card{background:linear-gradient(160deg,#0a1d33ee,#050d18ee);border:1px solid #2ef2ff33;border-radius:14px;padding:12px;margin:12px 0;animation:gl 4s ease-in-out infinite}"
    ".big{display:block;text-align:center;text-decoration:none;padding:18px;font-size:20px;font-weight:900;letter-spacing:3px;color:#2a1600;border-radius:12px;background:linear-gradient(180deg,#ffd35a,#e29400 55%,#9a5c00);box-shadow:0 6px 0 #4a2a00,0 0 30px #ffb21e88}"
    "small{color:#8fb9c9}.pin{width:23%;display:inline-block;margin-right:1%}.ok{color:#33ff9a}.no{color:#ffb37d}</style>"
    "<script>var AC;function bp(){try{AC=AC||new AudioContext();var t=AC.currentTime,o=AC.createOscillator(),g=AC.createGain();o.connect(g);g.connect(AC.destination);o.frequency.setValueAtTime(1500,t);o.frequency.exponentialRampToValueAtTime(600,t+.08);g.gain.setValueAtTime(.15,t);g.gain.exponentialRampToValueAtTime(.0001,t+.1);o.start(t);o.stop(t+.11)}catch(e){}}document.addEventListener('pointerdown',function(e){if(e.target.closest('button,a'))bp()})</script>"
    "</head><body><h2><span class=emb></span>Mary&#39;s Magic Box</h2>");
  if(WiFi.status() == WL_CONNECTED) h += "<a class=big href=/hub>&#9889; OPEN MOTO HUB</a>";
  if(note.length()) h += "<div class=card><b>" + note + "</b></div>";
  h += "<div class=card>Wi-Fi: ";
  if(WiFi.status() == WL_CONNECTED){ h += "<span class=ok>on "; h += esc(ssid); h += " ("; h += WiFi.localIP().toString(); h += ")</span>"; }
  else h += "<span class=no>not connected</span>";
  h += "<br>Mary's message service: ";
  h += (subOk && httpCode == 200) ? "<span class=ok>connected</span>" : "<span class=no>not connected yet</span>";
  h += "</div><form method=post action=/save><div class=card><b>1. Your Wi-Fi</b> <small>(2.4 GHz)</small><select name=ssid><option value=''>pick it here...</option>";
  if(ssid.length() && wifiList.indexOf("\"" + esc(ssid) + "\"") < 0) h += "<option value=\"" + esc(ssid) + "\" selected>" + esc(ssid) + " (now)</option>";      // the Wi-Fi it uses now is always in the list
  h += wifiList;
  h += "</select><input name=ssid2 placeholder='...or type the Wi-Fi name' autocapitalize=off autocomplete=off><input name=pass placeholder='Wi-Fi password' value=\"" + esc(pass) + "\" autocapitalize=off autocomplete=off></div>";
  h += "<div class=card><b>2. Secret code (you can SKIP this)</b><br><small>Only needed to switch from far away. Leave it empty if you do not have it.</small><input name=code value=\"" + esc(code) + "\" autocapitalize=off autocomplete=off></div>";
  h += "<div class=card><b>3. Relay pins</b> <small>(-1 = not used)</small><br>";
  for(int i = 0; i < NREL; i++) h += "<input class=pin type=number name=p" + String(i + 1) + " value=" + String(pins[i]) + ">";
  h += String("<small>") + (board == 4 ? "On the small board use 26, 27, 25, 32, 33, 18, 19 or 23 (21 and 22 run the tiny screen). A short press on its BOOT button flips switch 1." : board == 3 ? "On the yellow board use only 22 or 27 (the small plug marked CN1)." : board == 2 ? "On the 2.8 inch touch board use only 2, 14, 21 (the small plug marked IO2 IO3 IO14 IO21) or 44 (the RXD wire of the UART plug)." : board == 1 ? "On the red MaTouch use only 5 6 7 (4 is the buzzer)." : "Good pins on the ESP32-S3: 5 6 7 15 16 17 18 (4 is the buzzer). Not 0, 3, 19, 20, 35-37, 45, 46.") + "</small><select name=al><option value=1";
  h += activeLow ? " selected" : "";
  h += ">Relay board clicks ON when the pin goes LOW (most blue relay boards)</option><option value=0";
  h += activeLow ? "" : " selected";
  h += ">Relay board clicks ON when the pin goes HIGH</option></select><select name=cc><option value=1";
  h += checkCert ? " selected" : "";
  h += ">Check that it is really the message service (safer)</option><option value=0";
  h += checkCert ? "" : " selected";
  h += ">Do not check (only if it will not connect)</option></select></div>";
  h += "<div class=card><b>4. Buzzer pin</b> <small>(-1 = no buzzer)</small><input type=number name=bz value=" + String(bzPin) + "><select name=bf><option value=0";
  h += bzFlip ? "" : " selected";
  h += ">Buzzer is quiet until it beeps (normal)</option><option value=1";
  h += bzFlip ? " selected" : "";
  h += ">Flip it - pick this if the buzzer sounds ALL the time</option></select><select name=bc><option value=1";
  h += bzClick ? " selected" : "";
  h += ">Beep when a switch flips</option><option value=0";
  h += bzClick ? "" : " selected";
  h += ">No beep when a switch flips</option></select></div>";
  h += "<div class=card><b>Names on the screen buttons</b> <small>(8 letters. A name with GARAGE, DOOR or GATE gets one short press)</small><br>";
  for(int i = 0; i < NREL; i++) h += "<input class=pin name=n" + String(i + 1) + " maxlength=8 value=\"" + esc(nm[i]) + "\">";
  h += "<select name=vo>";
  for(int i = 1; i <= 3; i++){ h += "<option value=" + String(i) + (volSet == i ? " selected" : "") + ">Speaker: " + (i == 1 ? "quiet" : i == 2 ? "normal" : "loud") + "</option>"; }
  h += "</select><select name=sd>";
  h += String("<option value=0") + (spkPin == 0 ? " selected" : "") + ">Speaker wire: find it by itself" + (spkUsed ? String(" (using pin ") + String(spkUsed) + ")" : String("")) + "</option>";
  h += String("<option value=6") + (spkPin == 6 ? " selected" : "") + ">Speaker wire: pin 6</option><option value=8" + (spkPin == 8 ? " selected" : "") + ">Speaker wire: pin 8</option></select></div>";
  h += "<div class=card><b>5. Which board is this?</b><select name=bd><option value=0";
  h += board == 0 ? " selected" : "";
  h += ">Plain ESP32-S3, no screen</option><option value=2";
  h += board == 2 ? " selected" : "";
  h += ">2.8 inch touch screen board (plug says IO2 IO3 IO14 IO21)</option><option value=1";
  h += board == 1 ? " selected" : "";
  h += ">Red MaTouch with the 2.8 inch screen</option>"; if(CYD) h += board == 4 ? "<option value=4 selected>Small board with the tiny screen</option>" : "<option value=3 selected>Yellow 2.8 inch touch board</option>"; h += "</select>";
  h += "<small>Picture upside down or backwards? Pick the next screen turn. Colors look like a film negative? Change the colors. A tap hits the wrong square? Pick the next touch.</small><select name=lr>";
  for(int i = 0; i < 4; i++){ h += "<option value=" + String(i) + (lcdRot == i ? " selected" : "") + ">Screen turn " + String(i + 1) + "</option>"; }
  h += "</select><select name=li><option value=1";
  h += lcdInv ? " selected" : "";
  h += ">Colors: way 1</option><option value=0";
  h += lcdInv ? "" : " selected";
  h += ">Colors: way 2</option></select><select name=tf>";
  for(int i = 0; i < 4; i++){ h += "<option value=" + String(i) + (tpFlip == i ? " selected" : "") + ">Touch " + String(i + 1) + "</option>"; }
  h += "</select></div>";
  h += "<div class=card><b>7. Mary talks (free Google Gemini key)</b> <small>(get it free at aistudio.google.com/apikey and paste it here. Empty = Mary does not talk.)</small><input name=gk value=\"" + esc(gKey) + "\" autocapitalize=off autocomplete=off placeholder='AIza...'>"
       "<small>Talk model</small><input name=gm value=\"" + esc(gTalk) + "\" autocapitalize=off><small>Voice model</small><input name=gv value=\"" + esc(gVoice) + "\" autocapitalize=off></div>";
  h += "<div class=card><b>6. Jetson hub</b> <small>(your Jetson's name or number. Empty = off.)</small><input name=jh value=\"" + esc(hubHost) + "\" autocapitalize=off autocomplete=off><small>Jetson: " + String(hubHost.length() ? (hubOk ? "<span class=ok>connected</span>" : "<span class=no>not found yet</span>") : "off") + "</small><br><b>Moto Hub password</b> <small>(phones and the Jetson need it. Phone page: http://" + WiFi.localIP().toString() + "/hub)</small><input name=lp value=\"" + esc(lpw) + "\" autocapitalize=off autocomplete=off></div>";
  h += "<button>Save and restart</button></form><div class=card><b>Test</b> <small>(you should hear a click or a beep)</small>";
  for(int i = 0; i < NREL; i++) if(pins[i] >= 0) h += "<form method=post action=/t><input type=hidden name=n value=" + String(i + 1) + "><button class=b2>Switch " + String(i + 1) + " is " + (on[i] ? "ON" : "OFF") + " - tap to flip</button></form>";
  if(bzPin >= 0) h += "<form method=post action=/t><input type=hidden name=n value=9><button class=b2>Beep the buzzer</button></form>";
  if(board == 3) h += "<br><small>Touch check: strength " + String(tpZ) + ", last press at " + String(tpLX) + "," + String(tpLY) + ", silent " + String(tpStuck) + ". <a href=/touch>Open the live touch test</a></small>";
  h += "</div><small>Mary&#39;s Magic Box 6.0 - hold the BOOT button 6 seconds to erase the setup.</small></body></html>";
  return h;
}
void handleRoot(){ web.send(200, "text/html", page("")); }
void handleSave(){
  String s = web.arg("ssid2"); s.trim(); if(!s.length()) s = web.arg("ssid");
  String c = web.arg("code"); c.trim();
  if(!s.length()) s = ssid;                                       // nothing picked: keep the Wi-Fi the box already uses (the key and the rest still save)
  if(!s.length()){ web.send(200, "text/html", page("Pick your Wi-Fi first.")); return; }
  if(!c.length()) c = code;                                       // no code typed: keep the old one (or none - the box still works at home)
  if(c.length() && !goodCode(c)){ web.send(200, "text/html", page("That secret code is not right. Leave it empty if you do not have it.")); return; }
  String pw = web.arg("pass"); if(!pw.length() && s == ssid) pw = pass;      // password left empty: keep the saved one
  int nb = web.arg("bd").toInt(); if(nb < 0 || nb > 2) nb = 0; if(CYD) nb = board;
  bool swap = nb != board;                                       // a different board was picked: start from that board's own pins
  int np[NREL];
  for(int i = 0; i < NREL; i++){
    np[i] = web.hasArg("p" + String(i + 1)) && web.arg("p" + String(i + 1)).length() ? web.arg("p" + String(i + 1)).toInt() : -1;
    if(swap) np[i] = (i == 0 ? (nb == 2 ? 2 : nb == 3 ? 22 : 5) : -1);
    if(!goodPinFor(np[i], nb)){ web.send(200, "text/html", page("Pin " + String(np[i]) + " cannot be used for a relay on this board.")); return; }
    for(int j = 0; j < i; j++) if(np[i] >= 0 && np[i] == np[j]){ web.send(200, "text/html", page("Two relays cannot share pin " + String(np[i]) + ".")); return; }
  }
  int nz = web.hasArg("bz") && web.arg("bz").length() ? web.arg("bz").toInt() : bzPin;
  if(swap) nz = nb >= 2 ? -1 : 4;
  if(!goodPinFor(nz, nb)){ web.send(200, "text/html", page("Pin " + String(nz) + " cannot be used for the buzzer on this board.")); return; }
  for(int i = 0; i < NREL; i++) if(nz >= 0 && nz == np[i]){ web.send(200, "text/html", page("The buzzer cannot share pin " + String(nz) + " with a relay.")); return; }
  prefs.putString("ssid", s); prefs.putString("pass", pw); prefs.putString("code", c);
  { String gk = web.arg("gk"); gk.trim(); if(gk.length() > 20) sdKeySave(gk); }
  prefs.putInt("bz", nz); prefs.putBool("bf", web.arg("bf") == "1"); prefs.putBool("bc", web.arg("bc") != "0"); prefs.putInt("bd", nb);
  prefs.putInt("lr", web.arg("lr").toInt() & 3); prefs.putInt("tf", web.arg("tf").toInt() & 3); prefs.putBool("li", web.arg("li") != "0");
  for(int i = 0; i < NREL; i++) prefs.putInt((String("p") + String(i + 1)).c_str(), np[i]);
  prefs.putBool("al", web.arg("al") != "0"); prefs.putBool("cc", web.arg("cc") != "0");
  for(int i = 0; i < NREL; i++) if(web.hasArg("n" + String(i + 1))) prefs.putString((String("n") + String(i + 1)).c_str(), cleanName(web.arg("n" + String(i + 1)), i));
  if(web.hasArg("lp")){ String j = web.arg("lp"); j.trim(); if(j.length() >= 4) prefs.putString("lp", j); }
  if(web.hasArg("jh")){ String j = web.arg("jh"); j.trim(); prefs.putString("jh", j); }
  if(web.hasArg("gk")){ String j = web.arg("gk"); j.trim(); prefs.putString("gk", j); }
  if(web.hasArg("gm")){ String j = web.arg("gm"); j.trim(); if(j.length()) prefs.putString("gm", j); }
  if(web.hasArg("gv")){ String j = web.arg("gv"); j.trim(); if(j.length()) prefs.putString("gv", j); }
  if(web.hasArg("vo")){ int v = web.arg("vo").toInt(); prefs.putInt("vo", v < 1 || v > 3 ? 2 : v); }
  if(web.hasArg("sd")){ int v = web.arg("sd").toInt(); prefs.putInt("sd", v == 6 || v == 8 ? v : 0); }
  web.send(200, "text/html", String("<meta name=viewport content='width=device-width,initial-scale=1'><body style='font-family:Arial;background:#11151b;color:#eee;padding:20px;font-size:19px'><h2>Saved.</h2>"
    "The box is restarting and joining your Wi-Fi. Put the phone back on its normal Wi-Fi, open Mary, and say: <b>switch one on</b>."));
  delay(1200);
  ESP.restart();
}
void handleTest(){
  int n = web.arg("n").toInt();
  if(n == 9){ beep(2, 150, 120); sayClip(9); }
  else if(n >= 1 && n <= NREL){ setRelay(n - 1, !on[n - 1]); reportTag = ""; needReport = true; }
  web.sendHeader("Location", "/", true); web.send(302, "text/plain", "");
}
void handleCmd(){                                               // same commands on your own network:  /cmd?k=CODE&c=1+on
  web.sendHeader("Access-Control-Allow-Origin", "*");
  if(!((goodCode(code) && web.arg("k") == code) || (lpw.length() && web.arg("k") == lpw))){ web.send(403, "text/plain", "wrong code"); return; }
  doCommand(web.arg("c")); needReport = false;
  web.send(200, "text/plain", stateText());
}
void handleOther(){                                             // any other address: send the phone to the setup page (this makes the page pop up by itself)
  String ip = apUp ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  web.sendHeader("Location", "http://" + ip + "/", true); web.send(302, "text/plain", "");
}

void setup(){
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  Serial.begin(115200, SERIAL_8N1, -1, 43);                       // 6.5: the box only sends text (TXD 43).  RXD (44) is left free: the LIGHT relay sits on it
#else
  Serial.begin(115200);
#endif
  delay(300);
  Serial.println(); Serial.println("Mary's Magic Box 6.0 - Wi-Fi switch box for Mary AI (buzzer + screen + touch)");
  pinMode(BOOT_PIN, INPUT_PULLUP);
  prefs.begin("amybox", false);
  ssid = prefs.getString("ssid", ""); pass = prefs.getString("pass", "");
  if(!ssid.length()){ ssid = "mjcaviness2"; pass = "Shocker1"; }      // Mary's Wi-Fi is built in: nothing to type
  code = prefs.getString("code", "");
  for(int i = 0; i < NREL; i++){ pins[i] = prefs.getInt((String("p") + String(i + 1)).c_str(), i == 0 ? 5 : -1); if(!goodPin(pins[i])) pins[i] = -1; }
  activeLow = prefs.getBool("al", true); checkCert = prefs.getBool("cc", true);
  for(int i = 0; i < NREL; i++) nm[i] = cleanName(prefs.getString((String("n") + String(i + 1)).c_str(), ""), i);
  gKey = prefs.getString("gk", ""); gKey.trim(); gTalk = prefs.getString("gm", gTalk); gVoice = prefs.getString("gv", gVoice);
  if(psramFound()){ recL = (int16_t*)ps_malloc(16000 * 8 * 2); recR = (int16_t*)ps_malloc(16000 * 8 * 2); vBuf = (int16_t*)ps_malloc(16000 * 40 * 2); }
  if(!recL || !recR || !vBuf) gKey = "";                         // no big memory on this board: Mary does not talk here
  else xTaskCreatePinnedToCore(aiTask, "mary", 16384, NULL, 1, NULL, 0);
  hubHost = prefs.getString("jh", "moto-desktop.local"); hubHost.trim(); lpw = prefs.getString("lp", "04170417");
  volSet = prefs.getInt("vo", 2); if(volSet < 1 || volSet > 3) volSet = 2;
  spkPin = prefs.getInt("sd", 0); if(spkPin != 6 && spkPin != 8) spkPin = 0;
  board = prefs.getInt("bd", 0); if(board < 0 || board > 2) board = 0;
  if(CYD){
    board = 3;
    pinMode(21, INPUT_PULLDOWN); pinMode(22, INPUT_PULLDOWN); delay(3);      // the small board's tiny screen holds both of these wires up; the yellow board does not
    if(digitalRead(21) == HIGH && digitalRead(22) == HIGH){ Wire.begin(21, 22); Wire.beginTransmission(0x3C); if(Wire.endTransmission() == 0) board = 4; else { Wire.end(); pinMode(21, INPUT); pinMode(22, INPUT); } }
    else { pinMode(21, INPUT); pinMode(22, INPUT); }
    if(!prefs.isKey("p1")){ pins[0] = board == 4 ? 26 : 22; pins[1] = 27; pins[2] = -1; pins[3] = -1; }
    if(prefs.getInt("bd", -1) != board) prefs.putInt("bd", board);
    if(board == 4) Serial.println("Found the small board with the tiny screen.");
  }
  lcdRot = prefs.getInt("lr", 0) & 3; tpFlip = prefs.getInt("tf", 0) & 3; lcdInv = prefs.getBool("li", !CYD); tpOk = false;
  bool busy16 = false; for(int i = 0; i < NREL; i++) if(pins[i] == 15 || pins[i] == 16 || pins[i] == 18) busy16 = true;
  if(!CYD && !prefs.isKey("bd") && !busy16){                             // first start: is this the 2.8 inch touch board? Its touch chip answers on pins 16 and 15.
    tpInit();
    if(tpOk){
      board = 2; prefs.putInt("bd", 2);
      if(!prefs.isKey("p1")){ pins[0] = 44; pins[1] = 14; for(int i = 2; i < NREL; i++) pins[i] = -1; prefs.putInt("p1", 44); prefs.putInt("p2", 14); }      // 6.5: LIGHT on the UART plug RXD wire (44), GARAGE on 14      // LIGHT on 2, GARAGE on 14
      if(!prefs.isKey("bz")) prefs.putInt("bz", -1);
      Serial.println("Found the 2.8 inch touch screen board.");
    } else Wire.end();
  }
  bzPin = prefs.getInt("bz", board >= 2 ? -1 : 4); bzFlip = prefs.getBool("bf", false); bzClick = prefs.getBool("bc", true);
  if(!goodPinFor(bzPin, board)) bzPin = -1;
  for(int i = 0; i < NREL; i++){ if(!goodPinFor(pins[i], board)) pins[i] = -1; if(pins[i] >= 0 && pins[i] == bzPin) bzPin = -1; }
  for(int i = 0; i < NREL; i++) setRelay(i, false);              // everything OFF at power-up
  bzIdle(); beep(1, 120, 60);                                    // one beep: the box has power
  if(board == 1){ pinMode(47, OUTPUT); digitalWrite(47, HIGH); }             // red MaTouch: pin 47 is the memory card - keep the card quiet
  if(board == 2){ pinMode(1, OUTPUT); digitalWrite(1, HIGH); if(!tpOk) tpInit(); auInit(); }      // touch board: pin 1 high = speaker amplifier off (no hiss)
  if(board == 3){ pinMode(4, OUTPUT); digitalWrite(4, HIGH); pinMode(16, OUTPUT); digitalWrite(16, HIGH); pinMode(17, OUTPUT); digitalWrite(17, HIGH); auInit(); tpInit3(); }      // yellow board: its little color light off, sound, touch
  if(board == 4){ oInit(); oledDraw(); }
  if(board && board != 4){ lcdInit(); lcdDraw(true); }
  WiFi.persistent(false);
  web.on("/", HTTP_GET, handleRoot);
  web.on("/save", HTTP_POST, handleSave);
  web.on("/t", HTTP_POST, handleTest);
  web.on("/cmd", HTTP_GET, handleCmd);
  web.on("/touch", HTTP_GET, handleTouch);
  web.on("/hub", HTTP_GET, handleHub);
  web.on("/peers", HTTP_GET, handlePeers);
  web.onNotFound(handleOther);
  WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t info){ wReason = info.wifi_sta_disconnected.reason; }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);      // remember WHY Wi-Fi failed
  sdStart();                                                       // the SD card: songs, photos, Mary's picture
  if(ssid.length()) startSTA(); else startAP();
  web.begin();
}
// ---- Mary talks: hold TALK and speak; Google Gemini hears it and answers; a second call turns the answer into a natural voice
static const char B64C[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
String jesc(const String& t){ String o; for(unsigned int i = 0; i < t.length(); i++){ char c = t.charAt(i); if(c == '"' || c == '\\'){ o += '\\'; o += c; } else if(c == '\n') o += "\\n"; else if((uint8_t)c >= 32) o += c; } return o; }
String sysText(){
  String n = ""; for(int i = 0; i < NREL; i++) if(pins[i] >= 0){ if(n.length()) n += ", "; n += nm[i]; }
  return "You are Mary, a kind, warm, cheerful helper living in a touch-screen box in Mr. Terry's garage in Wichita, Kansas. Terry and his wife are older people. "
         "Answer in one to three short, friendly sentences with plain words. No lists, no symbols, no emojis. The box switches: " + n + ". "
         "If the person asks to turn the light on, start your answer with [LIGHT ON]. To turn it off, start with [LIGHT OFF]. To open or close the garage door, start with [GARAGE]. Otherwise use no brackets." +
         (wxText.length() ? String(" ") + wxText : String(" You cannot see the weather or a clock right now. If asked about them, say so kindly and do not guess."));
}
double jNum(const String& b, int from, const char* key){          // the number after a name in Google-style text
  int k = b.indexOf(key, from); if(k < 0) return -999;
  k += strlen(key);
  while(k < (int)b.length() && (b.charAt(k) == ':' || b.charAt(k) == '[' || b.charAt(k) == ' ' || b.charAt(k) == '"')) k++;
  return b.substring(k, k + 12).toFloat();
}
const char* wxWord(int c){ return c == 0 ? "clear sky" : c <= 2 ? "partly cloudy" : c == 3 ? "cloudy" : c <= 48 ? "foggy" : c <= 57 ? "drizzle" : c <= 67 ? "rain" : c <= 77 ? "snow" : c <= 82 ? "rain showers" : c <= 86 ? "snow showers" : "thunderstorms"; }
void wxLoad(){                                                    // the real weather and time for Wichita (open-meteo.com, free, no key), fresh every 20 minutes
  if(wxAt && millis() - wxAt < 1200000UL) return;
  if(WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure c; c.setInsecure(); HTTPClient h; h.setConnectTimeout(5000); h.setTimeout(6000);
  if(!h.begin(c, "https://api.open-meteo.com/v1/forecast?latitude=37.69&longitude=-97.34&current=temperature_2m,weather_code,wind_speed_10m&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max&temperature_unit=fahrenheit&wind_speed_unit=mph&timezone=America%2FChicago&forecast_days=1")) return;
  if(h.GET() == 200){
    String b = h.getString(); int cu = b.indexOf("\"current\":{"), da = b.indexOf("\"daily\":{");
    if(cu >= 0 && da >= 0){
      double t = jNum(b, cu, "\"temperature_2m\""), w = jNum(b, cu, "\"wind_speed_10m\""), hi = jNum(b, da, "\"temperature_2m_max\""), lo = jNum(b, da, "\"temperature_2m_min\""), pr = jNum(b, da, "\"precipitation_probability_max\"");
      int wc = (int)jNum(b, cu, "\"weather_code\"");
      int tk = b.indexOf("\"time\":\"", cu); String tms = tk >= 0 ? b.substring(tk + 8, tk + 24) : String(""); tms.replace('T', ' ');
      if(t > -200 && wc >= 0){
        wxText = "Real facts you may use: right now in Wichita it is " + String((int)(t + (t >= 0 ? 0.5 : -0.5))) + " degrees Fahrenheit and " + wxWord(wc) + ", wind " + String((int)(w + 0.5)) + " miles per hour. Today's high is " +
                 String((int)(hi + (hi >= 0 ? 0.5 : -0.5))) + " and the low is " + String((int)(lo + (lo >= 0 ? 0.5 : -0.5))) + (pr >= 0 ? String(", chance of rain or snow ") + String((int)pr) + " percent" : String("")) +
                 ". The local date and time is about " + tms + " (24-hour clock; say it the everyday way).";
        wxAt = millis(); if(!wxAt) wxAt = 1;
      }
    }
  }
  h.end();
}
int gHead(GRead& r){                                              // the status line and headers; gives back the status number
  String st = r.line(); int code = st.length() > 12 ? st.substring(9, 12).toInt() : 0;
  r.chunked = false; r.left = 0;
  for(;;){ String h = r.line(); if(!h.length()) break; h.toLowerCase(); if(h.startsWith("transfer-encoding:") && h.indexOf("chunked") > 0) r.chunked = true; }
  return code;
}
bool gOpen(WiFiClientSecure& c, const String& model, long len){
  c.setInsecure(); c.setTimeout(20);
  if(!c.connect("generativelanguage.googleapis.com", 443)) return false;
  c.print(String("POST /v1beta/models/") + model + ":generateContent?key=" + gKey + " HTTP/1.1\r\nHost: generativelanguage.googleapis.com\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " + String(len) + "\r\n\r\n");
  return true;
}
String gErr(const String& body, int code){                       // Google's own words about what went wrong
  int k = body.indexOf("\"message\""); String m = "";
  if(k >= 0){ int a = body.indexOf('"', body.indexOf(':', k) + 1); int b2 = body.indexOf('"', a + 1); if(a > 0 && b2 > a) m = body.substring(a + 1, b2); }
  return "Mary can't answer (" + String(code) + "). " + m.substring(0, 120);
}
String jText(const String& body){                                // the first "text" in the answer, made readable
  int k = body.indexOf("\"text\""); if(k < 0) return "";
  int a = body.indexOf('"', body.indexOf(':', k) + 1); if(a < 0) return "";
  String o; for(unsigned int i = a + 1; i < body.length(); i++){ char c = body.charAt(i);
    if(c == '\\' && i + 1 < body.length()){ char d = body.charAt(++i); if(d == 'n') o += ' '; else if(d == 'u'){ i += 4; o += '\''; } else o += d; }
    else if(c == '"') break; else o += c; }
  o.trim(); return o;
}
bool aiAnswer1(){                                                 // call 1: hear the question (or read it) and answer it
  bool voice = recN > 0;
  String pre = "{\"system_instruction\":{\"parts\":[{\"text\":\"" + jesc(sysText()) + "\"}]},\"contents\":[{\"role\":\"user\",\"parts\":[{\"text\":\"" +
               jesc(voice ? String("Listen to what I said in this recording and answer me.") : aiQ) + "\"}";
  String mid = voice ? String(",{\"inline_data\":{\"mime_type\":\"audio/wav\",\"data\":\"") : String("");
  String post = String(voice ? "\"}}" : "") + "]}],\"generationConfig\":{\"maxOutputTokens\":300,\"temperature\":0.7}}";
  long wavLen = 44 + (long)recN * 2, b64 = voice ? ((wavLen + 2) / 3) * 4 : 0;
  WiFiClientSecure c;
  if(!gOpen(c, gTalk, (long)pre.length() + mid.length() + b64 + post.length())){ aiText = "Mary can't reach the internet right now."; return false; }
  c.print(pre); c.print(mid);
  if(voice){                                                      // the recording as a WAV file, sent as base64 bit by bit
    int16_t* use = recL; long eL = 0, eR = 0; for(int i = 0; i < recN; i += 4){ eL += abs(recL[i]); eR += abs(recR[i]); } if(eR > eL) use = recR;
    uint8_t h[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0x3E,0,0,0,0x7D,0,0,2,0,16,0,'d','a','t','a',0,0,0,0};
    uint32_t dl = recN * 2, rl = dl + 36; memcpy(h + 4, &rl, 4); memcpy(h + 40, &dl, 4);
    uint8_t in[3]; int k = 0; char out[1025]; int o = 0; long pos = 0;
    while(pos < wavLen){
      uint8_t byt; if(pos < 44) byt = h[pos]; else { long s = (pos - 44) >> 1; int v = use[s]; byt = ((pos - 44) & 1) ? (uint8_t)((v >> 8) & 255) : (uint8_t)(v & 255); }
      in[k++] = byt; pos++;
      if(k == 3 || pos == wavLen){
        uint8_t a = in[0], b = k > 1 ? in[1] : 0, cc = k > 2 ? in[2] : 0;
        out[o++] = B64C[a >> 2]; out[o++] = B64C[((a & 3) << 4) | (b >> 4)]; out[o++] = k > 1 ? B64C[((b & 15) << 2) | (cc >> 6)] : '='; out[o++] = k > 2 ? B64C[cc & 63] : '=';
        k = 0;
        if(o >= 1024){ c.write((const uint8_t*)out, o); o = 0; }
      }
    }
    if(o) c.write((const uint8_t*)out, o);
  }
  c.print(post);
  GRead r; r.c = &c; r.n = 0; r.i = 0;
  int code = gHead(r); String body; int ch; while((ch = r.get()) >= 0 && body.length() < 12000) body += (char)ch;
  c.stop();
  if(code != 200){ aiText = gErr(body, code); return false; }
  String t = jText(body);
  aiCmd = "";
  if(t.startsWith("[")){ int e = t.indexOf(']'); if(e > 0){ aiCmd = t.substring(1, e); aiCmd.toUpperCase(); t = t.substring(e + 1); t.trim(); } }
  if(!t.length()) t = aiCmd.length() ? String("Okay!") : String("Sorry, I didn't catch that. Please try again.");
  aiText = t; return true;
}
bool aiAnswer(){                                                  // a second try when the line dropped
  for(int t = 0; t < 2; t++){
    if(aiAnswer1()) return true;
    if(aiText.indexOf("(0)") < 0 && aiText.indexOf("reach the internet") < 0) return false;
    delay(800);
  }
  return false;
}
void aiVoice(){                                                   // call 2: the answer as a natural voice (24000 a second, made into 16000 for the box)
  vN = 0;
  String body = "{\"contents\":[{\"parts\":[{\"text\":\"Say warmly and kindly: " + jesc(aiText) + "\"}]}],\"generationConfig\":{\"responseModalities\":[\"AUDIO\"],\"speechConfig\":{\"voiceConfig\":{\"prebuiltVoiceConfig\":{\"voiceName\":\"Aoede\"}}}}}";
  WiFiClientSecure c;
  if(!gOpen(c, gVoice, body.length())) return;
  c.print(body);
  GRead r; r.c = &c; r.n = 0; r.i = 0;
  if(gHead(r) != 200){ c.stop(); return; }
  const char* key = "\"data\""; int m = 0, ch;                  // find "data": "
  while((ch = r.get()) >= 0){ if(ch == key[m]){ if(!key[++m]) break; } else m = (ch == key[0]) ? 1 : 0; }
  while((ch = r.get()) >= 0 && ch != '"'){}
  uint32_t acc = 0; int bits = 0; uint8_t lo = 0; bool haveLo = false; long nIn = 0; int maxN = 16000 * 40;
  while((ch = r.get()) >= 0 && ch != '"'){
    const char* q = strchr(B64C, ch); if(!q || ch == 0) continue;
    acc = (acc << 6) | (uint32_t)(q - B64C); bits += 6;
    if(bits >= 8){ bits -= 8; uint8_t by = (uint8_t)((acc >> bits) & 255);
      if(!haveLo){ lo = by; haveLo = true; }
      else { haveLo = false; int16_t smp = (int16_t)(lo | (by << 8)); if((nIn++ % 3) != 2 && vN < maxN) vBuf[vN++] = smp; }      // keep 2 of every 3: 24000 -> 16000
    }
  }
  c.stop();
}
void aiTask(void* arg){
  for(;;){
    if(aiState == 1){                                             // listening while the finger is on TALK (8 seconds at most)
      static int16_t fr[512]; size_t got = 0; unsigned long t0 = millis();
      i2s_read(I2S_NUM_0, fr, sizeof(fr), &got, 20);              // throw away the old sound first
      while(aiRec && recN < 16000 * 8 && millis() - t0 < 8000){
        if(i2s_read(I2S_NUM_0, fr, sizeof(fr), &got, 100) == ESP_OK){
          int n = got / 4;
          for(int i = 0; i < n && recN < 16000 * 8; i++){ int a = fr[2 * i] * 4, b = fr[2 * i + 1] * 4;
            recL[recN] = (int16_t)(a > 32000 ? 32000 : a < -32000 ? -32000 : a); recR[recN] = (int16_t)(b > 32000 ? 32000 : b < -32000 ? -32000 : b); recN++; }
        }
      }
      aiRec = false;
      if(callMode){ if(recN >= 4800) callSend(); callMode = false; aiState = 0; continue; }      // CALL: off to the other boxes
      if(recN < 6400){ aiText = "Hold the TALK button the whole time you speak."; vN = 0; aiState = 3; continue; }      // under 0.4 seconds
      aiState = 2;
    }
    if(aiState == 2){
      if(WiFi.status() != WL_CONNECTED){ aiText = "Mary needs the Wi-Fi to talk."; vN = 0; aiState = 3; continue; }
      vN = 0;
      txtDone = false;
      wxLoad();
      if(aiAnswer()){ aiState = 4; aiVoice(); }
      aiState = 3;
    }
    delay(20);
  }
}

// ---- the drain pan (Moto Drain Watch board on the same Wi-Fi)
bool camShow(){                                                   // the drain camera picture, full screen
  if(!lcdOn || !drainIp.length() || WiFi.status() != WL_CONNECTED) return false;
  WiFiClient c; HTTPClient h; h.setConnectTimeout(2000); h.setTimeout(6000);
  if(!h.begin(c, "http://" + drainIp + "/pic.jpg")) return false;
  bool good = false;
  if(h.GET() == 200){
    int len = h.getSize();
    if(len > 100 && len < 90000){
      uint8_t* jpg = (uint8_t*)malloc(len);
      uint8_t* px = (uint8_t*)malloc(160 * 120 * 2);
      if(jpg && px){
        WiFiClient* st = h.getStreamPtr(); int got = 0; unsigned long t = millis();
        while(got < len && millis() - t < 6000){ int a = st->available(); if(a > 0) got += st->readBytes(jpg + got, a < len - got ? a : len - got); else delay(2); }
        if(got == len && jpg2rgb565(jpg, len, px, JPG_SCALE_2X)){
          fix565(px, 160 * 120);
          static uint8_t line[640];
          lcdOpen(); lcdWindow(0, 0, LCD_W, LCD_H);
          for(int y = 0; y < 120; y++){
            const uint8_t* r = px + y * 320;
            for(int x = 0; x < 160; x++){ line[x * 4] = line[x * 4 + 2] = r[x * 2]; line[x * 4 + 1] = line[x * 4 + 3] = r[x * 2 + 1]; }
            lcdSpi.writeBytes(line, 640); lcdSpi.writeBytes(line, 640);
          }
          lcdClose();
          good = true;
        }
      }
      if(jpg) free(jpg);
      if(px) free(px);
    }
  }
  h.end();
  return good;
}
void drainLoop(){
  if(camOn){
    if(millis() - camOpened > 120000UL){ camOn = false; lcdDraw(true); return; }      // 2 minutes: back to the buttons
    if(!camAt || millis() - camAt > 3000UL){
      camAt = millis(); if(!camAt) camAt = 1;
      if(camShow()){
        lcdText(4, 4, drainWet == 1 ? "WATER IN PAN!" : "DRAIN: DRY", 0xFFFF, drainWet == 1 ? rgb(200, 20, 30) : rgb(20, 120, 60), 1);
        lcdText(4, 212, "TAP TO GO BACK", 0xFFFF, 0x0000, 1);
      } else {
        lcdFill(0, 0, LCD_W, LCD_H, 0x0000);
        lcdText(40, 100, "NO PICTURE YET", 0xFFFF, 0x0000, 1);
        lcdText(40, 140, "TAP TO GO BACK", rgb(200, 200, 200), 0x0000, 1);
      }
    }
    return;
  }
  if(drainWet == 1 && millis() - drainSeen < 120000UL && (!drainAlarmAt || millis() - drainAlarmAt > 30000UL)){
    drainAlarmAt = millis(); if(!drainAlarmAt) drainAlarmAt = 1;
    if(asleep) lcdSleep(false);
    for(int k = 0; k < 3; k++){ fxNote(988, 160); fxNote(740, 160); }
    showMsg("WATER IN DRAIN PAN!");
  }
}

// ============================================================ 5.4 MARY'S MAGIC BOX: keyboard, Wi-Fi page, web pages, SD card
void cvImg(int x, int y, int w, int h, const uint16_t* p){        // a picture from memory into the strip being built
  for(int yy = (y < cvY0 ? cvY0 : y); yy < y + h && yy < cvY0 + cvHh; yy++)
    for(int xx = 0; xx < w; xx++){ int X = x + xx; if(X >= 0 && X < LCD_W) CV[(yy - cvY0) * LCD_W + X] = p[(yy - y) * w + xx]; }
}
// ---- the keyboard on the screen
static const char* const KB_A[4] = {"1234567890", "qwertyuiop", "asdfghjkl@", "zxcvbnm.-/"};
static const char* const KB_B[4] = {"!@#$%^&*()", "-_=+[]{}\\|", ";:'\",.<>?/", "~`@#&*+-_."};
char kbChar(int r, int c){
  char ch = (kbSym ? KB_B : KB_A)[r][c];
  if(!kbSym && kbShift && ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
  return ch;
}
void kbOpen(int f, const String& t){ kbFor = f; kbText = t; kbShift = (f == 3); kbSym = false; kbOn = true; lcdShown = ""; }
void kbClose(bool go){
  kbOn = false; lcdShown = "";
  String t = kbText; t.trim();
  if(kbFor == 1 || kbFor == 4){ webAt = 0; webFull = true; webSkip = true; if(!go) return; }      // back to the web page (it draws itself again)
  if(!go) return;
  if(kbFor == 1 && t.length()) webAct = "/go?u=" + urlEnc(t);
  else if(kbFor == 4 && kbText.length()) webAct = "/type?enter=1&t=" + urlEnc(kbText);
  else if(kbFor == 2){                                             // a new Wi-Fi: save it and join it
    ssid = wfPick; pass = kbText; prefs.putString("ssid", ssid); prefs.putString("pass", pass);
    wfOn = false; showMsg(String("JOINING ") + ssid); lcdDraw(true); startSTA();
  }
  else if(kbFor == 5 && t.length()){ callText(t); showMsg("MESSAGE SENT"); }      // a written message to the other boxes and phones
  else if(kbFor == 3 && t.length()){                               // ask Mary in writing
    if(!gKey.length()){ showMsg("MARY NEEDS THE KEY"); return; }
    jetOn = true; ansPg = 0; vPlay = false; aiAskText(t.c_str());
  }
}
void kbKey(int x, int y, int w, const char* lb, uint16_t col){
  cvGlass(x + 1, y + 2, w - 2, 38, 8, col, 205, 0);
  cvText(x + (w - (int)strlen(lb) * FW) / 2, y + 9, lb, 0xFFFF, 1);
}
void paintKb(){
  static const char* const TT[6] = {"", "WEB ADDRESS OR SEARCH", "WI-FI PASSWORD", "ASK MARY ANYTHING", "TYPE ON THE PAGE", "MESSAGE TO THE BOXES"};
  cvGlass(0, 0, LCD_W, 34, 0, rgb(6, 14, 40), 210, 0);
  if(!kbText.length()) cvText(6, 5, TT[kbFor >= 0 && kbFor <= 5 ? kbFor : 0], rgb(150, 165, 200), 1);
  else { String t = kbText; if(t.length() > 22) t = t.substring(t.length() - 22); t += "_"; cvText(6, 5, t.c_str(), 0xFFFF, 1); }
  cvGlass(284, 2, 34, 30, 8, rgb(225, 50, 60), 220, 0); cvText(295, 5, "X", 0xFFFF, 1);
  for(int r = 0; r < 4; r++){
    int y = 36 + r * 40; if(y + 40 <= cvY0 || y >= cvY0 + cvHh) continue;
    for(int c = 0; c < 10; c++){
      int x = c * 32;
      cvGlass(x + 1, y + 2, 30, 37, 6, rgb(30, 60, 125), 205, 0);
      char lb[2] = {kbChar(r, c), 0}; cvText(x + 10, y + 8, lb, 0xFFFF, 1);
    }
  }
  int y = 196;
  if(y + 44 > cvY0 && y < cvY0 + cvHh){
    kbKey(0, y, 64, kbSym ? "abc" : "SHFT", kbShift && !kbSym ? rgb(255, 170, 30) : rgb(60, 100, 170));
    kbKey(64, y, 64, kbSym ? "ABC" : "?123", rgb(60, 100, 170));
    kbKey(128, y, 96, "SPACE", rgb(30, 60, 125));
    kbKey(224, y, 48, "DEL", rgb(200, 110, 30));
    kbKey(272, y, 48, kbFor == 3 ? "ASK" : kbFor == 2 ? "OK" : kbFor == 5 ? "SEND" : "GO", rgb(30, 160, 80));
  }
}
void kbTouch(int x, int y){
  fxNote(1700, 15);
  if(y < 34){ if(x >= 276) kbClose(false); return; }
  if(y >= 196){
    if(x < 64){ if(kbSym) kbSym = false; else kbShift = !kbShift; }
    else if(x < 128) kbSym = !kbSym;
    else if(x < 224){ if(kbText.length() < 120) kbText += ' '; }
    else if(x < 272){ if(kbText.length()) kbText.remove(kbText.length() - 1); }
    else kbClose(true);
    return;
  }
  if(y < 36) return;
  int r = (y - 36) / 40, c = x / 32; if(r > 3) r = 3; if(c > 9) c = 9;
  if(kbText.length() < 120) kbText += kbChar(r, c);
  if(kbShift && !kbSym) kbShift = false;                           // one capital letter, then small again
}
// ---- the Wi-Fi page on the screen
void paintWf(){
  cvGlass(0, 0, LCD_W, 30, 0, rgb(6, 14, 40), 190, 0);
  cvText(8, 3, "WI-FI", rgb(255, 220, 90), 1);
  String ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("NOT ON WI-FI");
  cvText(LCD_W - 6 - (int)ip.length() * FW, 3, ip.c_str(), WiFi.status() == WL_CONNECTED ? rgb(140, 255, 170) : rgb(255, 150, 150), 1);
  if(wfN < 0) cvText(20, 90, "LOOKING FOR WI-FI...", 0xFFFF, 1);
  else if(wfN == 0) cvText(20, 90, "NO WI-FI FOUND", 0xFFFF, 1);
  String now = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : String("");
  for(int i = 0; i < wfN && i < 5; i++){
    int y = 34 + i * 34;
    bool mine = now.length() && wfName[i] == now;
    cvGlass(4, y, LCD_W - 8, 31, 8, mine ? rgb(20, 150, 90) : rgb(14, 34, 84), 190, 0);
    String nmx = (mine ? String("* ") : String("")) + wfName[i]; if(nmx.length() > 18) nmx = nmx.substring(0, 18);
    cvText(12, y + 3, nmx.c_str(), 0xFFFF, 1);
    int b = barsOf(wfRs[i]);
    for(int k = 0; k < 4; k++) cvBox(270 + k * 9, y + 26 - (k + 1) * 5, 6, (k + 1) * 5, k < b ? (b >= 3 ? rgb(120, 255, 150) : b == 2 ? rgb(255, 214, 90) : rgb(255, 90, 90)) : rgb(70, 80, 100));
  }
  cvGlass(4, 206, 154, 32, 8, rgb(60, 100, 170), 205, 0); cvText(4 + (154 - 10 * FW) / 2, 210, "LOOK AGAIN", 0xFFFF, 1);
  cvGlass(162, 206, 154, 32, 8, rgb(225, 50, 60), 205, 0); cvText(162 + (154 - 4 * FW) / 2, 210, "BACK", 0xFFFF, 1);
}
void wfTouch(int x, int y){
  fxNote(1500, 30);
  if(y >= 204){ if(x < 160) wfN = -1; else wfOn = false; return; }
  if(y >= 34 && wfN > 0){ int i = (y - 34) / 34; if(i < wfN && i < 5){ wfPick = wfName[i]; if(isKnown(wfPick) && knownPass(wfPick).length()){ kbFor = 2; kbText = knownPass(wfPick); kbClose(true); } else kbOpen(2, ""); } }      // a Wi-Fi the box knows: no typing
}
// ---- pictures: JPEG size, and making a picture fit
bool jpgSize(const uint8_t* b, int len, int& w, int& h){         // reads the width and height; false for "progressive" pictures this chip cannot open
  int i = 2;
  while(i + 9 < len){
    if(b[i] != 0xFF){ i++; continue; }
    uint8_t m = b[i + 1];
    if(m == 0xC0 || m == 0xC1 || m == 0xC2){ h = (b[i + 5] << 8) | b[i + 6]; w = (b[i + 7] << 8) | b[i + 8]; return m != 0xC2; }
    if(m == 0xD8 || m == 0x01 || m == 0xFF || (m >= 0xD0 && m <= 0xD7)){ i += (m == 0xFF ? 1 : 2); continue; }
    int L = (b[i + 2] << 8) | b[i + 3]; if(L < 2) return false;
    i += 2 + L;
  }
  return false;
}
uint8_t* jpgDecodeFit(const uint8_t* j, int len, int maxW, int maxH, int& ow, int& oh){      // opens the picture, made smaller (1/2, 1/4, 1/8) when it is big
  int w = 0, h = 0; if(!jpgSize(j, len, w, h) || w < 8 || h < 8) return NULL;
  int sc = 0; while(sc < 3 && ((w >> sc) > maxW * 2 || (h >> sc) > maxH * 2)) sc++;
  ow = w >> sc; oh = h >> sc;
  uint8_t* px = (uint8_t*)ps_malloc((size_t)(ow + 16) * (oh + 16) * 2); if(!px) return NULL;
  if(!jpg2rgb565(j, len, px, (jpg_scale_t)sc)){ free(px); return NULL; }
  fix565(px, (long)ow * oh);
  return px;
}
uint8_t* sdJpg(const String& path, int& len){
  len = 0;
#if HAS_SD
  if(!sdOk) return NULL;
  File f = SD_MMC.open(path.c_str()); if(!f) return NULL;
  len = (int)f.size(); if(len < 100 || len > 6000000){ f.close(); return NULL; }
  uint8_t* b = (uint8_t*)ps_malloc(len); if(!b){ f.close(); return NULL; }
  int got = (int)f.read(b, len); f.close();
  if(got != len){ free(b); return NULL; }
  return b;
#else
  return NULL;
#endif
}
void showPhoto(const String& p){                                  // a photo from the SD card, as big as it fits
  lcdFill(0, 0, LCD_W, LCD_H, 0x0000);
  bool ok = false; int len = 0;
  uint8_t* j = sdJpg(p, len);
  if(j){
    int ow = 0, oh = 0; uint8_t* px = jpgDecodeFit(j, len, LCD_W, LCD_H, ow, oh); free(j);
    if(px){
      int dw = LCD_W, dh = oh * LCD_W / ow; if(dh > LCD_H){ dh = LCD_H; dw = ow * LCD_H / oh; }
      if(dw < 1) dw = 1; if(dh < 1) dh = 1;
      static uint8_t line[LCD_W * 2];
      lcdOpen(); lcdWindow((LCD_W - dw) / 2, (LCD_H - dh) / 2, dw, dh);
      for(int y = 0; y < dh; y++){
        const uint8_t* r = px + (size_t)(y * oh / dh) * ow * 2;
        for(int x = 0; x < dw; x++){ int sx = x * ow / dw; line[x * 2] = r[sx * 2]; line[x * 2 + 1] = r[sx * 2 + 1]; }
        lcdSpi.writeBytes(line, dw * 2);
      }
      lcdClose(); free(px); ok = true;
    }
  }
  if(!ok){ lcdText(16, 100, "CAN'T OPEN THIS PHOTO", 0xFFFF, 0x0000, 1); lcdText(16, 130, "(save it as JPG)", rgb(200, 200, 200), 0x0000, 1); }
  lcdText(4, 4, "< BACK", 0xFFFF, rgb(225, 50, 60), 1);
  lcdText(LCD_W - 4 - 6 * FW, 4, "NEXT >", 0xFFFF, rgb(30, 120, 210), 1);
}
void phTouch(int x, int y){ fxNote(1500, 30); if(x < LCD_W / 3) phOn = false; else phAt = 0; }
void phLoop(){
  if(!phOn || kbOn) return;
  if(millis() - lastTouch > 1800000UL){ phOn = false; lcdShown = ""; lcdDraw(true); return; }      // 30 minutes: back to the buttons
  if(phAt && millis() - phAt < 8000UL) return;                     // a new photo every 8 seconds
  phAt = millis(); if(!phAt) phAt = 1;
  if(!sdPicN){ phOn = false; lcdShown = ""; lcdDraw(true); return; }
  if(phCur >= sdPicN) phCur = 0;
  showPhoto(sdPic[phCur]); phCur = (phCur + 1) % sdPicN;
}
// ---- web pages: the Jetson opens the page in a real browser and sends the picture (mary_web.py on the Jetson, port 8091)
void webBar(){
  static const char* const WB[6] = {"BACK", "GO", "ABC", "UP", "DOWN", "X"};
  for(int b = 0; b < 6; b++){
    uint16_t col = b == 5 ? rgb(225, 50, 60) : b == 1 ? rgb(30, 160, 80) : b == 2 ? rgb(200, 90, 160) : rgb(40, 80, 160);
    int x = b * 53, w = b == 5 ? LCD_W - x : 52;
    lcdFill(x, 0, w, 24, col);
    lcdText(x + (w - (int)strlen(WB[b]) * FW) / 2, 0, WB[b], 0xFFFF, col, 1);
  }
}
// ---- songs and photos from the SD card (black box: SD card slot on pins 38 40 39)
int webFetch(const String& path){                               // 1 = drew it, 2 = nothing changed, 3 = stopped for a finger, 0 = no answer
  if(WiFi.status() != WL_CONNECTED) return 0;
  if(!webJpg) webJpg = (uint8_t*)ps_malloc(WEBJ);
  if(!webJpg) return 0;
  bool idle = path.startsWith("/shot") || path.startsWith("/cam");
  uint32_t cand[3]; int nc = 0;
  if((uint32_t)webIp) cand[nc++] = (uint32_t)webIp;
  uint32_t g = (uint32_t)WiFi.gatewayIP(); if(g && g != (uint32_t)webIp) cand[nc++] = g;      // on Doctor Magic the Jetson IS the gateway
  if(hubHost.length() && (uint32_t)hubIp == 0 && (!hubFindAt || millis() - hubFindAt > 60000UL)){ hubFindAt = millis(); hubFind(); }
  uint32_t hj = (uint32_t)hubIp; if(hj && hj != (uint32_t)webIp && hj != g) cand[nc++] = hj;
  for(int k = 0; k < nc; k++){
    IPAddress ip(cand[k]);
    WiFiClient c; HTTPClient h; h.setConnectTimeout(1500); h.setTimeout(15000);
    String u = String("http://") + ip.toString() + ":8091" + path;
    if(webFull) u += (path.indexOf('?') >= 0 ? "&f=1" : "?f=1");
    if(!h.begin(c, u)) continue;
    static const char* HK[2] = {"X-Y0", "X-Kb"}; h.collectHeaders(HK, 2);
    int code = h.GET(), res = 0;
    if(code == 204) res = 2;
    else if(code == 200){
      int len = h.getSize(), y0 = h.header("X-Y0").toInt(), rows = len / (LCD_W * 2);
      if(len >= LCD_W * 2 && len <= WEBJ && (len % (LCD_W * 2)) == 0 && y0 >= 0 && y0 + rows <= LCD_H - 24){
        WiFiClient* st = h.getStreamPtr(); int got = 0; unsigned long t = millis(), tc = millis(); bool stop = false;
        while(got < len && millis() - t < 8000){
          int a = st->available();
          if(a > 0) got += st->readBytes(webJpg + got, a < len - got ? a : len - got); else delay(2);
          if(idle && millis() - tc > 40){ tc = millis(); int tx = 0, ty = 0; if(board == 3 ? tpRead3(tx, ty) : tpRead(tx, ty)){ stop = true; break; } }      // a finger: drop this picture, the finger goes first
        }
        if(got == len){
          lcdOpen(); lcdWindow(0, 24 + y0, LCD_W, rows);
          for(int y = 0; y < rows; y++) lcdSpi.writeBytes(webJpg + (size_t)y * LCD_W * 2, LCD_W * 2);
          lcdClose();
          res = 1; webFull = false;
        } else if(stop) res = 3;
      }
    }
    if(res == 1 || res == 2) webKb = h.header("X-Kb") == "1";
    h.end();
    if(res){ webIp = ip; return res; }
  }
  webIp = IPAddress();
  return 0;
}
void webTouch(int x, int y){
  fxNote(1500, 20);
  if(y < 24){
    int b = x / 53; if(b > 5) b = 5;
    if(b == 0) webAct = "/back";
    else if(b == 1){ kbOpen(1, ""); return; }
    else if(b == 2){ kbOpen(4, ""); return; }
    else if(b == 3) webAct = "/scroll?d=-1";
    else if(b == 4) webAct = "/scroll?d=1";
    else { webOn = false; return; }
    webAt = 0; return;
  }
  webAct = "/tap?x=" + String(x) + "&y=" + String(y - 24); webAt = 0;      // a finger on the page clicks there
}
void webTp(bool down, int x, int y, unsigned long now){           // a finger on a web page: a tap clicks, a drag scrolls (acts when the finger lifts)
  static bool wd = false; static int x0 = 0, y0 = 0, x1 = 0, y1 = 0, miss = 0;
  if(webSkip){ if(down) miss = 0; else if(++miss >= 2){ webSkip = false; miss = 0; wd = false; } return; }      // the finger that opened the page must lift first
  if(down){ if(!wd){ wd = true; x0 = x1 = x; y0 = y1 = y; } else { x1 = x; y1 = y; } miss = 0; lastTouch = now; return; }
  if(!wd || ++miss < 2) return;
  wd = false; miss = 0;
  int dy = y1 - y0, dx = x1 - x0;
  if(y0 >= 24 && abs(dy) >= 22 && abs(dy) > abs(dx)){ webAct = "/scroll?px=" + String(-dy * 15 / 8); webAt = 0; }      // drag up = the page moves up, like a phone
  else webTouch(x0, y0);
  if(!webOn){ lcdShown = ""; lcdDraw(true); }
}
void webLoop(){
  if(!webOn || kbOn) return;
  if(millis() - lastTouch > 600000UL){ webOn = false; lcdShown = ""; lcdDraw(true); return; }      // 10 quiet minutes: back to the buttons
  if(webAt && millis() - webAt < (webErr ? 8000UL : 1500UL)) return;      // looks for changes every 1.5 seconds (nothing is sent when the page stands still)
  String act = webAct; webAct = "";
  bool first = !webAt;
  if(first){ lcdFill(0, 0, LCD_W, 24, rgb(6, 14, 40)); lcdText(8, 0, "LOADING...", rgb(255, 220, 90), rgb(6, 14, 40), 1); }
  webAt = millis(); if(!webAt) webAt = 1;
  int r = webFetch(act.length() ? act : String("/shot"));
  if(r == 0){
    if(!webErr || act.length()){
      lcdFill(0, 24, LCD_W, LCD_H - 24, 0x0000);
      lcdText(10, 60, WiFi.status() == WL_CONNECTED ? "NO PAGE FROM JETSON" : "NO WI-FI", 0xFFFF, 0x0000, 1);
      lcdText(10, 100, "Is the Jetson on, and", rgb(200, 210, 230), 0x0000, 1);
      lcdText(10, 128, "the box on Doctor Magic?", rgb(200, 210, 230), 0x0000, 1);
      lcdText(10, 170, "Tap X to go back.", rgb(200, 210, 230), 0x0000, 1);
    }
    webErr = true; webFull = true;
  } else { webErr = false; if(r == 3) webFull = true; }
  if(first || r == 0) webBar();
  if((r == 1 || r == 2) && webKb && act.startsWith("/tap")){ webKb = false; kbOpen(4, ""); }      // the tap landed in a typing box: the keyboard comes up by itself
  webKb = false;
}
void keySync(){                                                   // Mary's key is also kept on the Jetson: a new flash gets it back by itself
  static unsigned long at = 0; static bool sent = false;
  if(WiFi.status() != WL_CONNECTED || webOn || kbOn || aiState != 0 || !recL) return;
  if(gKey.length() && sent) return;
  if(at && millis() - at < (gKey.length() ? 600000UL : 20000UL)) return;
  at = millis(); if(!at) at = 1;
  WiFiClient c; HTTPClient h; h.setConnectTimeout(800); h.setTimeout(3000);
  String base = String("http://") + WiFi.gatewayIP().toString() + ":8091/key";
  if(!gKey.length()){
    if(h.begin(c, base)){ if(h.GET() == 200){ String k = h.getString(); k.trim(); if(k.length() > 20 && k.length() < 120 && k.indexOf(' ') < 0){ gKey = k; prefs.putString("gk", k); sdKeySave(k); lcdShown = ""; } } h.end(); }
  } else {
    if(h.begin(c, base + "?set=" + urlEnc(gKey))){ if(h.GET() == 200) sent = true; h.end(); }
  }
}
void drainRelay(){                                                // the drain board is on Mary's Wi-Fi, this box on Doctor Magic: the Jetson hears the drain board and passes it on
  static unsigned long at = 0; static int bad = 0;
  if(WiFi.status() != WL_CONNECTED || webOn || kbOn || camOn || aiState != 0) return;
  if(drainIp.length() && millis() - drainSeen < 30000UL) return;  // heard it not long ago
  if(at && millis() - at < (bad >= 3 ? 300000UL : 15000UL)) return;
  at = millis(); if(!at) at = 1;
  WiFiClient c; HTTPClient h; h.setConnectTimeout(800); h.setTimeout(2500);
  bool ok = false;
  if(h.begin(c, String("http://") + WiFi.gatewayIP().toString() + ":8091/drain")){
    int code = h.GET();
    if(code == 200){
      String t = h.getString(); t.trim(); int a = t.indexOf('|'), b = t.lastIndexOf('|');
      if(a > 6 && b > a && t.substring(b + 1).toInt() < 120){
        drainIp = t.substring(0, a); int w = t.substring(a + 1, b) == "WET" ? 1 : 0;
        if(w && drainWet != 1) drainAlarmAt = 0;
        drainWet = w; drainSeen = millis();
      }
      ok = true;
    } else if(code == 204) ok = true;
    h.end();
  }
  bad = ok ? 0 : bad + 1;
}
void fix565(uint8_t* px, long n){                                 // the chip's picture opener can hand back the colors in another order: test it once with a red square, then put every picture right
  if(jpgMode < 0){
    jpgMode = 0;
    uint8_t* t = (uint8_t*)malloc(32 * 32 * 2);
    if(t){
      memset(t, 0x55, 32 * 32 * 2);
      if(jpg2rgb565(RED_JPG, sizeof(RED_JPG), t, JPG_SCALE_NONE)){
        uint8_t a = t[(5 * 16 + 5) * 2], b = t[(5 * 16 + 5) * 2 + 1];
        if(a >= 0xF0 && b < 0x10) jpgMode = 0;                    // already right
        else if(a < 0x10 && b >= 0xF0) jpgMode = 1;               // the two halves swapped
        else if(a < 0x10 && b >= 0x1C && b < 0x20) jpgMode = 2;   // red and blue swapped
        else if(b < 0x10 && a >= 0x1C && a < 0x20) jpgMode = 3;   // both
      }
      free(t);
    }
    Serial.printf("picture color order: %d\n", jpgMode);
  }
  if(jpgMode == 0) return;
  for(long i = 0; i < n; i++){
    uint8_t a = px[i * 2], b = px[i * 2 + 1];
    uint16_t v = (jpgMode == 1 || jpgMode == 3) ? (uint16_t)((b << 8) | a) : (uint16_t)((a << 8) | b);
    if(jpgMode >= 2) v = (uint16_t)(((v & 0x1F) << 11) | (v & 0x07E0) | (v >> 11));
    px[i * 2] = (uint8_t)(v >> 8); px[i * 2 + 1] = (uint8_t)(v & 255);
  }
}
bool realMusic(){ return muBuf && (sdSongN > 0 || WiFi.status() == WL_CONNECTED); }
void sdKeySave(const String& k){
#if HAS_SD
  if(!sdOk) return;
  File f = SD_MMC.open("/marykey.txt", FILE_WRITE); if(f){ f.print(k); f.close(); }
#endif
}
// ---- real music: internet radio stations and MP3 songs from the SD card (black box: SD card slot on pins 38 40 39)
static const char* const RADIO_NAME[NRADIO] = {"CHRISTMAS GOSPEL", "CHRISTMAS CLASSICS", "CHRISTIAN RADIO", "CHRISTMAS HITS"};
static const char* const RADIO_URL[NRADIO] = {"http://listen.181fm.com/181-xgospel_128k.mp3", "http://listen.181fm.com/181-xtraditional_128k.mp3", "http://emf.streamguys1.com/sk001_mp3_low_web", "http://listen.181fm.com/181-xblender_128k.mp3"};
String songName(int k){
  if(k < 0) return String("MUSIC ON");
  if(k >= sdSongN + NRADIO) return String("PHONE BLUETOOTH");
  if(k >= sdSongN) return String(RADIO_NAME[(k - sdSongN) % NRADIO]);
  String n = sdSong[k]; int a = n.lastIndexOf('/'); if(a >= 0) n = n.substring(a + 1);
  int d = n.lastIndexOf('.'); if(d > 0) n = n.substring(0, d);
  n.replace('_', ' '); n.toUpperCase(); if(n.length() > 22) n = n.substring(0, 22);
  return n;
}
void sdScanDir(const char* dir){
#if HAS_SD
  File d = SD_MMC.open(dir); if(!d) return;
  if(!d.isDirectory()){ d.close(); return; }
  File f = d.openNextFile();
  while(f){
    String p = f.path(); bool isDir = f.isDirectory(); f.close();
    String base = p.substring(p.lastIndexOf('/') + 1), low = p; low.toLowerCase();
    if(!isDir && !base.startsWith(".")){
      if(low.endsWith(".mp3") && sdSongN < 60) sdSong[sdSongN++] = p;
      else if((low.endsWith(".jpg") || low.endsWith(".jpeg")) && low != "/mary.jpg" && sdPicN < 60) sdPic[sdPicN++] = p;
    }
    f = d.openNextFile();
  }
  d.close();
#endif
}
void maryLoad(){                                                  // mary.jpg on the SD card becomes Mary's face
  int len = 0; uint8_t* j = sdJpg(String("/mary.jpg"), len); if(!j) return;
  int ow = 0, oh = 0; uint8_t* px = jpgDecodeFit(j, len, 100, 100, ow, oh); free(j);
  if(!px) return;
  maryBig = (uint16_t*)ps_malloc(100 * 100 * 2); maryFace = (uint16_t*)ps_malloc(36 * 36 * 2);
  if(maryBig && maryFace){
    int s = ow < oh ? ow : oh, x0 = (ow - s) / 2, y0 = (oh - s) / 4;      // a square from the middle, a bit high (that is where faces are)
    for(int y = 0; y < 100; y++) for(int x = 0; x < 100; x++){ const uint8_t* q = px + ((size_t)(y0 + y * s / 100) * ow + (x0 + x * s / 100)) * 2; maryBig[y * 100 + x] = (uint16_t)((q[0] << 8) | q[1]); }
    for(int y = 0; y < 36; y++) for(int x = 0; x < 36; x++) maryFace[y * 36 + x] = maryBig[(y * 100 / 36) * 100 + x * 100 / 36];
  } else {
    if(maryBig) free(maryBig); if(maryFace) free(maryFace); maryBig = NULL; maryFace = NULL;
  }
  free(px);
}
#if HAS_SD
uint8_t* nfBuf = NULL;
class NetFifo : public AudioFileSource {                           // the radio comes in ahead of time into a big store (about 15 seconds), so a slow moment on the Wi-Fi does not cut the song
 public:
  WiFiClient cli; HTTPClient http; WiFiClient* st = NULL;
  uint32_t h = 0, t = 0, pos = 0, prime = 24000, lastRx = 0; bool primed = false, opened = false;
  NetFifo(const char* url, uint32_t pr){
    prime = pr; if(!nfBuf) return;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); http.setConnectTimeout(6000); http.setTimeout(6000); http.setReuse(false);
    if(!http.begin(cli, url)) return;
    int c = http.GET(); if(c != 200){ http.end(); return; }
    st = http.getStreamPtr(); opened = st != NULL; lastRx = millis();
  }
  virtual ~NetFifo(){ if(opened) http.end(); opened = false; st = NULL; }
  void pump(){                                                     // take in whatever has arrived
    if(!st) return;
    for(int k = 0; k < 16; k++){
      int a = st->available(); if(a <= 0) break;
      uint32_t fr = NF_N - (h - t); if(fr == 0) break;
      uint32_t off = h & (NF_N - 1), n = (uint32_t)a; if(n > fr) n = fr; if(n > NF_N - off) n = NF_N - off; if(n > 4096) n = 4096;
      int r = st->read(nfBuf + off, n); if(r <= 0) break;
      h += (uint32_t)r; lastRx = millis();
    }
  }
  virtual uint32_t read(void* data, uint32_t len){
    uint8_t* d = (uint8_t*)data; uint32_t got = 0, t0 = millis();
    for(;;){
      pump();
      uint32_t have = h - t;
      if(!primed && (have >= prime || (have > 0 && millis() - t0 > 6000))) primed = true;
      if(primed && have){
        uint32_t off = t & (NF_N - 1), n = len - got; if(n > have) n = have; if(n > NF_N - off) n = NF_N - off;
        memcpy(d + got, nfBuf + off, n); t += n; got += n; pos += n;
        if(got >= len) return got;
        continue;
      }
      if(got) return got;
      if(primed && !have){ primed = false; t0 = millis(); }          // ran dry: fill up a little before going on
      if(!st || (!st->connected() && st->available() <= 0)) return 0;      // the station hung up
      if(millis() - lastRx > 9000) return 0;                       // nothing came for a long time
      if(!(musicOn || webOn) || sdWant >= 0) return 0;             // stopped, or another station picked
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
  virtual uint32_t readNonBlock(void* data, uint32_t len){ return read(data, len); }
  virtual bool seek(int32_t p, int dir){ return false; }
  virtual bool close(){ if(opened) http.end(); opened = false; st = NULL; return true; }
  virtual bool isOpen(){ return opened; }
  virtual uint32_t getSize(){ return 0; }
  virtual uint32_t getPos(){ return pos; }
};
class MuOut : public AudioOutput {                                 // the song decoder hands its sound to the box's speaker, made into 16000 a second
 public:
  uint32_t ph = 0; int32_t acc = 0; int cnt = 0;
  virtual bool begin(){ ph = 0; acc = 0; cnt = 0; return true; }
  virtual bool stop(){ return true; }
  virtual bool ConsumeSample(int16_t sample[2]){
    if(!muBuf) return true;
    if((uint32_t)(muH - muT) > (uint32_t)(MU_N - 8)) return false;      // full: the decoder waits a moment
    int v = channels == 2 ? ((int)sample[0] + (int)sample[1]) / 2 : (int)sample[0];
    acc += v; cnt++; ph += AU_RATE;
    uint32_t hz = hertz ? hertz : 44100;
    if(ph >= hz){
      int o = cnt ? (int)(acc / cnt) : 0; acc = 0; cnt = 0;
      while(ph >= hz){ ph -= hz; muBuf[muH & (MU_N - 1)] = (int16_t)o; muH++; }
    }
    return true;
  }
};
MuOut* muOut = NULL; AudioGeneratorMP3* mp3 = NULL; AudioFileSourceFS* mpSrc = NULL; NetFifo* mpNet = NULL; AudioFileSourceBuffer* mpBuf = NULL;
bool mpBegun = false;                                             // 6.4: stopping a player that never started crashed the box (WEB with no sound from the Jetson)
void mpStop(){ if(mp3){ if(mpBegun) mp3->stop(); mpBegun = false; delete mp3; mp3 = NULL; } if(mpBuf){ delete mpBuf; mpBuf = NULL; } if(mpNet){ delete mpNet; mpNet = NULL; } if(mpSrc){ delete mpSrc; mpSrc = NULL; } }
void sdTask(void* arg){                                           // plays the music by itself: SD card songs one after another, or a radio station
  int fails = 0;
  for(;;){
    muReal = realMusic();
    bool ws = webOn && muBuf && WiFi.status() == WL_CONNECTED;      // a web page is open: its sound comes from the Jetson
    static bool wasWs = false; if(ws != wasWs){ wasWs = ws; mpStop(); muT = muH; fails = 0; }
    muPlay = ws || (musicOn && muReal);
    if(!muPlay){ if(mp3) mpStop(); vTaskDelay(50 / portTICK_PERIOD_MS); continue; }
    int tot = sdSongN + NRADIO;
    if(sdWant >= 0){ sdCur = sdWant % (tot + 1); sdWant = -1; mpStop(); muT = muH; fails = 0; }
    if(sdCur > tot) sdCur = 0;
    bool bt = sdCur == tot;                                       // the phone by Bluetooth, through the Jetson
    if(!mp3){
      bool ok = false;
      mp3 = new AudioGeneratorMP3();
      if(ws){
        String u = String("http://") + ((uint32_t)webIp ? webIp : WiFi.gatewayIP()).toString() + ":8091/web.mp3";
        mpNet = new NetFifo(u.c_str(), 3000);
        if(mpNet->isOpen()) { mpBegun = true; ok = mp3->begin(mpNet, muOut); }
        if(!ok){ mpStop(); vTaskDelay(3000 / portTICK_PERIOD_MS); continue; }      // no sound from the Jetson yet: try again in a moment
      }
      else if(bt){
        String u = String("http://") + WiFi.gatewayIP().toString() + ":8091/bt.mp3";
        mpNet = new NetFifo(u.c_str(), 3000);
        if(mpNet->isOpen()) { mpBegun = true; ok = mp3->begin(mpNet, muOut); }
        if(!ok){ mpStop(); vTaskDelay(3000 / portTICK_PERIOD_MS); continue; }      // the Jetson is not answering yet: try again in a moment
      }
      else if(sdCur < sdSongN){ mpSrc = new AudioFileSourceFS(SD_MMC, sdSong[sdCur].c_str()); if(mpSrc->isOpen()){ mpBegun = true; ok = mp3->begin(mpSrc, muOut); } }
      else if(WiFi.status() == WL_CONNECTED){
        mpNet = new NetFifo(RADIO_URL[(sdCur - sdSongN) % NRADIO], 32000);
        if(mpNet->isOpen()) { mpBegun = true; ok = mp3->begin(mpNet, muOut); }
      }
      if(!ok){ mpStop(); if(++fails >= 2){ sdCur = (sdCur + 1) % tot; fails = 0; } vTaskDelay(1500 / portTICK_PERIOD_MS); continue; }      // no luck twice: the next one
    }
    if(mpNet) mpNet->pump();
    if(!mp3->isRunning() || !mp3->loop()){
      bool wasSd = !ws && !bt && sdCur < sdSongN; mpStop();
      if(wasSd) sdCur = (sdCur + 1) % tot; else vTaskDelay(1000 / portTICK_PERIOD_MS);      // a song ended: the next one.  The radio dropped: join it again
    } else fails = 0;
    vTaskDelay(1);
  }
}
#endif
void sdStart(){
#if HAS_SD
  if(board != 2) return;                                          // only the black 2.8 inch box has the SD card slot wired this way
  SD_MMC.setPins(38, 40, 39);
  if(SD_MMC.begin("/sdcard", true)){
    sdOk = true;
    sdScanDir("/"); sdScanDir("/music"); sdScanDir("/photos"); sdScanDir("/Music"); sdScanDir("/Photos");
    Serial.printf("SD card: %d songs, %d photos\n", sdSongN, sdPicN);
    maryLoad();
    if(!gKey.length() && recL){ File f = SD_MMC.open("/marykey.txt"); if(f){ String k = f.readString(); f.close(); k.trim(); if(k.length() > 20 && k.length() < 120){ gKey = k; prefs.putString("gk", k); } } }      // Mary's key, kept on the card
    else if(gKey.length()) sdKeySave(gKey);
  } else Serial.println("No SD card.");
  if(psramFound()){
    muBuf = (int16_t*)ps_malloc(MU_N * 2); nfBuf = (uint8_t*)ps_malloc(NF_N);
    if(muBuf){ muOut = new MuOut(); xTaskCreatePinnedToCore(sdTask, "music", 16384, NULL, 1, NULL, 0); }
  }
#endif
}
// ---- Mary's words are ready: on the screen, and the light switches right away
void aiShow(){
  hubAns = aiText; ansPg = 0; hubWait = false;
  if(aiCmd.length()){
    for(int i = 0; i < NREL; i++){
      if(pins[i] < 0) continue;
      bool light = nameHas(i, "LIGHT") || nameHas(i, "LAMP");
      if(aiCmd == "LIGHT ON" && light){ doCommand(String(i + 1) + " on"); break; }
      if(aiCmd == "LIGHT OFF" && light){ doCommand(String(i + 1) + " off"); break; }
      if(aiCmd == "GARAGE" && isDoor(i)){ doCommand(String(i + 1) + " pulse"); break; }
    }
    aiCmd = "";
  }
  if(asleep) lcdSleep(false);
}


// ============================================================ 5.7 CALL: box to box voice (hold, talk, let go)
uint8_t muEnc(int v){                                             // sound squeezed into one byte (the telephone way)
  int sign = v < 0 ? 0x80 : 0; if(v < 0) v = -v; if(v > 32635) v = 32635; v += 0x84;
  int ex = 7; for(int m = 0x4000; (v & m) == 0 && ex > 0; ex--, m >>= 1){}
  return (uint8_t)(~(sign | (ex << 4) | ((v >> (ex + 3)) & 0x0F)));
}
int muDec(uint8_t u){ u = (uint8_t)~u; int ex = (u >> 4) & 7, v = (((u & 0x0F) << 3) + 0x84) << ex; v -= 0x84; return (u & 0x80) ? -v : v; }
void callPut(const uint8_t* d, int len){                          // one piece of a call came in (by radio or by Wi-Fi)
  if(len >= 5 && d[0] == 'M' && d[1] == 'X'){                     // a written message
    uint16_t id = (uint16_t)(d[2] | (d[3] << 8));
    for(int i = 0; i < 6; i++) if(callSeen[i] == id) return;      // had this one already (it comes several times, and by two roads)
    callSeen[callSeenK] = id; callSeenK = (callSeenK + 1) % 6;
    int n = len - 4; if(n > 200) n = 200;
    memcpy(callTxt, d + 4, n); callTxt[n] = 0; callNew = true;
    return;
  }
  if(len < 9 || d[0] != 'M' || d[1] != 'C' || !callBuf) return;
  uint16_t id = (uint16_t)(d[2] | (d[3] << 8)), seq = (uint16_t)(d[4] | (d[5] << 8)), tot = (uint16_t)(d[6] | (d[7] << 8));
  int n = len - 8;
  if(id == callMine || tot < 1 || tot > 300 || seq >= tot || n > CALL_PAY) return;
  if(id != callId || !callIn){ if(callIn && id != callId && callGot > 0 && millis() - callAt < 400) return; callId = id; callTot = tot; callGot = 0; memset(callMap, 0, sizeof(callMap)); memset(callBuf, 0xFF, 300 * CALL_PAY); callIn = true; }
  if(!callMap[seq]){ callMap[seq] = 1; memcpy(callBuf + (size_t)seq * CALL_PAY, d + 8, n); callGot++; }
  callAt = millis();
}
void callNowRx(const uint8_t* mac, const uint8_t* data, int len){ callPut(data, len); }
void callStart(){                                                 // the radio that needs no router
  if(nowOn || WiFi.getMode() == WIFI_OFF) return;
  if(esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(callNowRx);
  esp_now_peer_info_t pi; memset(&pi, 0, sizeof(pi)); memset(pi.peer_addr, 0xFF, 6); pi.channel = 0; pi.encrypt = false;
  pi.ifidx = (WiFi.getMode() == WIFI_AP) ? WIFI_IF_AP : WIFI_IF_STA;
  esp_now_add_peer(&pi);
  nowOn = true;
}
void callSend(){                                                  // the recording goes out twice: by radio and over the Wi-Fi the boxes share
  int16_t* use = recL; long eL = 0, eR = 0; for(int i = 0; i < recN; i += 4){ eL += abs(recL[i]); eR += abs(recR[i]); } if(eR > eL) use = recR;
  int n8 = recN / 2; if(n8 > 300 * CALL_PAY) n8 = 300 * CALL_PAY;
  int tot = (n8 + CALL_PAY - 1) / CALL_PAY; if(tot < 1) return;
  callMine = (uint16_t)(esp_random() | 1);
  static const uint8_t BC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool wifi = WiFi.status() == WL_CONNECTED;
  uint8_t pk[8 + CALL_PAY];
  for(int rounds = 0; rounds < 2; rounds++){                      // twice: a lost piece gets a second chance
    for(int q = 0; q < tot; q++){
      pk[0] = 'M'; pk[1] = 'C'; pk[2] = callMine & 255; pk[3] = callMine >> 8; pk[4] = q & 255; pk[5] = q >> 8; pk[6] = tot & 255; pk[7] = tot >> 8;
      int n = n8 - q * CALL_PAY; if(n > CALL_PAY) n = CALL_PAY;
      for(int i = 0; i < n; i++){ int k = (q * CALL_PAY + i) * 2; pk[8 + i] = muEnc(((int)use[k] + (int)use[k + 1]) / 2); }
      if(nowOn) esp_now_send(BC, pk, 8 + n);
      if(wifi){ callUdpOut.beginPacket(IPAddress(255, 255, 255, 255), 4211); callUdpOut.write(pk, 8 + n); callUdpOut.endPacket(); }
      delay(4);
    }
  }
  callSent = true;
}
void callText(const String& t){                                  // a written message: out by radio and over the Wi-Fi, three times
  uint8_t pk[4 + 200]; uint16_t id = (uint16_t)(esp_random() | 1);
  callSeen[callSeenK] = id; callSeenK = (callSeenK + 1) % 6;      // my own: not shown back to me
  int n = t.length(); if(n > 200) n = 200;
  pk[0] = 'M'; pk[1] = 'X'; pk[2] = id & 255; pk[3] = id >> 8; memcpy(pk + 4, t.c_str(), n);
  static const uint8_t BC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  for(int r = 0; r < 3; r++){
    if(nowOn) esp_now_send(BC, pk, 4 + n);
    if(WiFi.status() == WL_CONNECTED){ callUdpTxt.beginPacket(IPAddress(255, 255, 255, 255), 4211); callUdpTxt.write(pk, 4 + n); callUdpTxt.endPacket(); }
    delay(15);
  }
}
void paintCall(){
  cvGlass(0, 0, LCD_W, 30, 0, rgb(6, 14, 40), 190, 0);
  cvText(8, 3, "CALL", rgb(255, 220, 90), 1);
  String st = (callMode && aiState == 1) ? String("TALKING...") : (msgUntil && (long)(msgUntil - millis()) > 0) ? msgText : String("READY");
  if(st.length() > 20) st = st.substring(0, 20);
  cvText(LCD_W - 6 - (int)st.length() * FW, 3, st.c_str(), rgb(140, 255, 170), 1);
  cvGlass(4, 33, LCD_W - 8, 106, 12, rgb(14, 34, 84), 170, 0);
  if(callMsg.length()){
    cvText(14, 38, "MESSAGE:", rgb(255, 230, 120), 1);
    String m = callMsg; int y = 63;
    while(m.length() && y <= 113){                                // three lines, cut at a space when there is one
      int cut = m.length() > 24 ? 24 : m.length();
      if(m.length() > 24){ int sp = m.lastIndexOf(' ', 24); if(sp > 8) cut = sp; }
      cvText(14, y, m.substring(0, cut).c_str(), 0xFFFF, 1);
      m = m.substring(cut); m.trim(); y += 25;
    }
  } else {
    cvText(14, 38, "HOLD TALK and speak.", rgb(255, 230, 120), 1);
    cvText(14, 63, "TYPE sends words.", rgb(200, 210, 230), 1);
    cvText(14, 88, "Phone page (Jetson):", rgb(200, 210, 230), 1);
    String a = WiFi.status() == WL_CONNECTED ? String("https://") + WiFi.gatewayIP().toString() + ":8092" : String("needs Wi-Fi");
    cvText(14, 113, a.c_str(), 0xFFFF, 1);
  }
  static const char* const CB[4] = {"TALK", "TYPE", "CAM", "EXIT"};
  for(int b = 0; b < 4; b++){
    uint16_t col = b == 0 ? ((callMode && aiState == 1) ? rgb(255, 170, 30) : rgb(230, 80, 150)) : b == 1 ? rgb(30, 120, 210) : b == 2 ? rgb(20, 150, 90) : rgb(225, 50, 60);
    int x = 4 + b * 79;
    cvGlass(x + 2, 151, 74, 47, 10, 0x0000, 140, 1); cvGlass(x, 146, 74, 47, 10, col, 190, 0);
    cvText(x + (74 - (int)strlen(CB[b]) * FW) / 2, 158, CB[b], 0xFFFF, 1);
  }
  cvText(8, 210, "Hold TALK the whole time.", rgb(200, 210, 230), 1);
}
void callTouch(int x, int y){
  fxNote(1500, 30);
  if(y >= 140 && y < 204){
    int b = x / 80; if(b > 3) b = 3;
    if(b == 0){ if(!recL || !micOk){ showMsg("NO MICROPHONE"); return; } if(aiState == 0){ vPlay = false; callMode = true; recN = 0; aiRec = true; aiState = 1; } }      // hold and talk
    else if(b == 1) kbOpen(5, "");
    else if(b == 2){ pcOn = true; pcAt = 0; }
    else callOn = false;
  }
  else if(y >= 33 && y < 140) callMsg = "";                        // a tap on the message puts it away
}
void pcLoop(){                                                    // the picture from a phone's camera (the phone page on the Jetson sends it)
  if(!pcOn || kbOn) return;
  if(millis() - lastTouch > 600000UL){ pcOn = false; lcdShown = ""; lcdDraw(true); return; }
  if(pcAt && millis() - pcAt < 2000UL) return;
  bool first = !pcAt;
  pcAt = millis(); if(!pcAt) pcAt = 1;
  if(first){ lcdFill(0, 0, LCD_W, LCD_H, 0x0000); lcdFill(0, 0, LCD_W, 24, rgb(225, 50, 60)); lcdText(8, 0, "< BACK   PHONE CAMERA", 0xFFFF, rgb(225, 50, 60), 1); webFull = true; }
  int r = webFetch(String("/cam"));
  if(r == 3){ pcOn = false; lcdShown = ""; lcdDraw(true); return; }
  if((r == 0 || r == 2) && first){
    lcdText(10, 80, "NO CAMERA PICTURE YET", 0xFFFF, 0x0000, 1);
    lcdText(10, 115, "On the phone page tap", rgb(200, 210, 230), 0x0000, 1);
    lcdText(10, 143, "CAMERA.", rgb(200, 210, 230), 0x0000, 1);
  }
}
void callLoop(){
  if(!vBuf) return;                                               // a board without the big memory: no calls
  if(!callBuf) callBuf = (uint8_t*)ps_malloc(300 * CALL_PAY);
  callStart();
  if(WiFi.status() == WL_CONNECTED){ if(!callUdpOn){ callUdpOn = callUdpIn.begin(4211) == 1; } }
  else if(callUdpOn){ callUdpIn.stop(); callUdpOn = false; }
  if(callUdpOn){ int n; uint8_t b[8 + CALL_PAY + 4]; int guard = 0; while((n = callUdpIn.parsePacket()) > 0 && guard++ < 40){ int g = callUdpIn.read(b, sizeof(b)); if(g > 0) callPut(b, g); } }
  if(callSent){ callSent = false; showMsg("SENT"); fxNote(1200, 80); }
  if(callNew){                                                    // a written message came in: show it on the CALL page
    callNew = false; callMsg = String(callTxt);
    lastTouch = millis(); if(asleep) lcdSleep(false);
    if(!kbOn && !webOn && !pcOn && !phOn){ callOn = true; jetOn = false; wfOn = false; lcdShown = ""; }
    else showMsg("NEW MESSAGE - OPEN CALL");
    fxNote(988, 120); fxNote(1319, 160);
  }
  if(callIn && callGot > 0 && (callGot >= callTot || millis() - callAt > 600) && aiState == 0 && !vPlay){      // all here (or the rest is lost): play it
    int n8 = callTot * CALL_PAY; if(n8 > 16000 * 20) n8 = 16000 * 20;
    for(int i = 0; i < n8; i++){ int v = muDec(callBuf[i]) * 2; if(v > 32000) v = 32000; if(v < -32000) v = -32000; vBuf[2 * i] = (int16_t)v; vBuf[2 * i + 1] = (int16_t)v; }
    callIn = false; callGot = 0;
    vN = n8 * 2; vPos = 0; vPlay = true;
    lastTouch = millis(); if(asleep) lcdSleep(false);
    showMsg("VOICE CALL");
  }
}

void loop(){
  wifiLoop();
  bzLoop();
  tpLoop();
  if(lcdOn && !asleep && !gameOn && !jetOn && !musicOn && !camOn && !kbOn && !webOn && !wfOn && !phOn && !pcOn && drainWet != 1 && millis() - lastTouch > 180000UL) lcdSleep(true);      // 3 quiet minutes: screen light off
  web.handleClient();
  subLoop();
  hubLoop();
  peerLoop();
  drainLoop();
  webLoop();
  phLoop();
  pcLoop();
  keySync();
  drainRelay();
  callLoop();
  if(wfOn && wfN < 0 && !kbOn){                                   // the Wi-Fi page: look for Wi-Fi
    if(apUp) WiFi.mode(WIFI_AP_STA);
    else if(WiFi.status() != WL_CONNECTED){ WiFi.disconnect(); delay(200); wifiLostAt = millis(); }      // a try that keeps failing blocks the listening: stop it first
    scanWifi();
    if(wfN <= 0){ delay(600); scanWifi(); }                        // busy the first time: once more
    if(wfN < 0) wfN = 0;
    lcdDraw(true);
  }
  if(aiState == 4 && !txtDone){ txtDone = true; aiShow(); }       // Mary's words are ready: show them and switch the light NOW, her voice comes a moment later
  if(aiState == 3){                                               // Mary's answer is ready
    if(!txtDone) aiShow();
    txtDone = false;
    if(vN > 0){ vPos = 0; vPlay = true; }
    aiCmd = ""; aiState = 0; lastTouch = millis(); if(asleep) lcdSleep(false);
  }
  if(needReport && WiFi.status() == WL_CONNECTED && goodCode(code)){
    needReport = false;
    if(!postState()) Serial.println("  (could not send the answer back)");
  }
  for(int i = 0; i < NREL; i++) if(pulseEnd[i] && (long)(millis() - pulseEnd[i]) >= 0){ setRelay(i, false); reportTag = ""; needReport = true; }
  if(lcdOn && millis() - lcdAt > 200UL){ lcdAt = millis(); lcdDraw(false); }
  if(digitalRead(BOOT_PIN) == LOW){                              // BOOT held 6 seconds: erase the setup
    if(!bootHeld) bootHeld = millis();
    else if(millis() - bootHeld > 6000){ Serial.println("Erasing the setup."); setBuzz(false); for(int i = 0; i < NREL; i++) setRelay(i, false); prefs.clear(); delay(300); ESP.restart(); }
  } else {
    if(board == 4 && bootHeld && millis() - bootHeld > 40 && millis() - bootHeld < 1500) doCommand(isDoor(0) ? "1 pulse" : "1 toggle");      // small board: a short press on its BOOT button flips switch 1
    bootHeld = 0;
  }
  if(oledOn && millis() - lcdAt > 250UL){ lcdAt = millis(); oledDraw(); }
  delay(5);
}
