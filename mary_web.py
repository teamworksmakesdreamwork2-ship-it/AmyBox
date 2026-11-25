#!/usr/bin/env python3
# Mary Web - lets Mary's Magic Box show real web pages.
# The Jetson opens the page in a real Chromium browser (no window), takes a small picture of it,
# and sends the picture to the box. Taps, typing and scrolling from the box go back to the browser.
# It is also the CALL switchboard: boxes and phones talk, type and share a phone camera through it (phone page: https://<this Jetson>:8092).
# Only Python's own parts are used: nothing to install except Chromium.
# Start once with:  python3 mary_web.py --install   (it then starts by itself every time the Jetson starts)
# (c) Moto Takeda 316-304-5398

import base64, json, zlib, os, random, shutil, socket, struct, subprocess, sys, threading, time, urllib.parse, urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = 8091          # the box asks here
DEBUG_PORT = 9222    # Chromium listens here (only on the Jetson itself)
VIEW_W, VIEW_H, SCALE = 400, 270, 0.8      # a phone-shaped page, shrunk to 320 x 216 for the 2.8 inch screen
HOME = "https://www.google.com"
UA = "Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Mobile Safari/537.36"
HERE = os.path.dirname(os.path.abspath(__file__))
LOG = os.path.join(HERE, "mary_web.log")

def log(*a):
    line = time.strftime("%Y-%m-%d %H:%M:%S ") + " ".join(str(x) for x in a)
    print(line, flush=True)
    try:
        with open(LOG, "a") as f: f.write(line + "\n")
    except Exception: pass

OUT_W, OUT_H = 320, 216      # exactly what the box's screen shows under its buttons

def png_to_565(data, W, H):
    """PNG picture -> plain screen pixels (RGB565, big end first), cut or padded to W x H. Python only, no extras."""
    if data[:8] != b"\x89PNG\r\n\x1a\n": raise IOError("not a PNG")
    pos, idat, w, h, ctype, depth = 8, [], 0, 0, 0, 0
    while pos < len(data):
        n = struct.unpack("!I", data[pos:pos + 4])[0]; kind = data[pos + 4:pos + 8]; body = data[pos + 8:pos + 8 + n]; pos += 12 + n
        if kind == b"IHDR": w, h, depth, ctype = struct.unpack("!IIBB", body[:10])
        elif kind == b"IDAT": idat.append(body)
        elif kind == b"IEND": break
    if depth != 8 or ctype not in (2, 6): raise IOError("PNG kind %d/%d" % (ctype, depth))
    bpp = 3 if ctype == 2 else 4
    raw = zlib.decompress(b"".join(idat)); stride = w * bpp
    out = bytearray(W * H * 2)          # black where the page is smaller
    prev = bytearray(stride); p = 0
    for y in range(h):
        ft = raw[p]; cur = bytearray(raw[p + 1:p + 1 + stride]); p += 1 + stride
        if ft == 1:
            for i in range(bpp, stride): cur[i] = (cur[i] + cur[i - bpp]) & 255
        elif ft == 2:
            for i in range(stride): cur[i] = (cur[i] + prev[i]) & 255
        elif ft == 3:
            for i in range(stride): cur[i] = (cur[i] + (((cur[i - bpp] if i >= bpp else 0) + prev[i]) >> 1)) & 255
        elif ft == 4:
            for i in range(stride):
                a = cur[i - bpp] if i >= bpp else 0; b = prev[i]; c = prev[i - bpp] if i >= bpp else 0
                pa = abs(b - c); pb = abs(a - c); pc = abs(a + b - 2 * c)
                cur[i] = (cur[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        prev = cur
        if y < H:
            o = y * W * 2
            for x in range(min(w, W)):
                i = x * bpp; v = ((cur[i] & 0xF8) << 8) | ((cur[i + 1] & 0xFC) << 3) | (cur[i + 2] >> 3)
                out[o] = v >> 8; out[o + 1] = v & 255; o += 2
    return bytes(out)

def find_chromium():
    for n in ("chromium-browser", "chromium", "google-chrome", "google-chrome-stable"):
        p = shutil.which(n)
        if p: return p
    for p in ("/snap/bin/chromium", "/usr/lib/chromium-browser/chromium-browser", "/usr/bin/chromium-browser"):
        if os.path.exists(p): return p
    return None

def sound_sink():
    """A sound line of its own for the web pages ("marybox"). The box listens to it through /web.mp3."""
    try:
        if not shutil.which("pactl"): return False
        have = subprocess.run(["pactl", "list", "short", "sinks"], capture_output=True, text=True, timeout=5).stdout
        if "marybox" not in have:
            subprocess.run(["pactl", "load-module", "module-null-sink", "sink_name=marybox", "sink_properties=device.description=MaryBox"], timeout=5, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            have = subprocess.run(["pactl", "list", "short", "sinks"], capture_output=True, text=True, timeout=5).stdout
        return "marybox" in have
    except Exception as e:
        log("sound line:", e); return False

def sound_source():
    if sound_sink(): return "marybox.monitor"
    try: return subprocess.run(["pactl", "get-default-sink"], capture_output=True, text=True, timeout=5).stdout.strip() + ".monitor"
    except Exception: return "default"


# ---- 6.2: the phone's music by Bluetooth.  The phone plays to the Jetson; the box listens to it through /bt.mp3
BT = {"until": 0.0, "disc": False}
def bt_source():
    """The name of the sound coming in from a phone by Bluetooth, or "" when no phone is playing to the Jetson."""
    try:
        out = subprocess.run(["pactl", "list", "short", "sources"], capture_output=True, text=True, timeout=5).stdout
        for ln in out.splitlines():
            c = ln.split("\t")
            if len(c) > 1 and ("bluez_source" in c[1] or "bluez_input" in c[1]): return c[1]
    except Exception:
        pass
    return ""

def bt_agent():
    """Lets a phone find the Jetson as "Mary Box" and join without anyone at the Jetson, for 5 minutes after the box's PHONE BT button is tapped."""
    import pty, re, select
    if not shutil.which("bluetoothctl"): log("no bluetoothctl: phone Bluetooth music is off"); return
    while True:
        try:
            m, sl = pty.openpty()
            p = subprocess.Popen(["bluetoothctl"], stdin=sl, stdout=sl, stderr=sl, close_fds=True); os.close(sl)
            def send(t): os.write(m, (t + "\n").encode())
            for c in ("power on", "agent off", "agent NoInputNoOutput", "default-agent", "system-alias Mary Box", "pairable on", "discoverable-timeout 0", "discoverable off"):
                send(c); time.sleep(0.5)
            buf = ""; seen = set(); BT["disc"] = False
            while p.poll() is None:
                want = time.time() < BT["until"]
                if want != BT["disc"]: BT["disc"] = want; send("discoverable on" if want else "discoverable off")
                r, _, _ = select.select([m], [], [], 1.0)
                if not r: continue
                d = os.read(m, 4096).decode("utf-8", "replace")
                if not d: break
                buf = (buf + d)[-2000:]
                if "(yes/no)" in buf:                              # the phone asks to join, or to play: yes while the door is open
                    send("yes" if want else "no"); buf = ""
                for mac in re.findall(r"Device ((?:[0-9A-F]{2}:){5}[0-9A-F]{2}) (?:Paired: yes|Connected: yes)", buf):
                    if want and mac not in seen: seen.add(mac); send("trust " + mac)
            try: os.close(m)
            except Exception: pass
        except Exception as e:
            log("bluetooth:", e)
        time.sleep(10)

class WS:
    """A very small websocket client, just enough to talk to Chromium."""
    def __init__(self, url):
        u = urllib.parse.urlparse(url)
        self.s = socket.create_connection((u.hostname, u.port or 80), timeout=30)
        key = base64.b64encode(os.urandom(16)).decode()
        path = u.path + (("?" + u.query) if u.query else "")
        req = ("GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n") % (path, u.hostname, u.port or 80, key)
        self.s.sendall(req.encode())
        head = b""
        while b"\r\n\r\n" not in head:
            c = self.s.recv(1)
            if not c: raise IOError("browser closed the line")
            head += c
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise IOError("browser said no: " + head.split(b"\r\n")[0].decode(errors="ignore"))
        self.buf = b""

    def _recv(self, n):
        while len(self.buf) < n:
            c = self.s.recv(65536)
            if not c: raise IOError("browser closed the line")
            self.buf += c
        d, self.buf = self.buf[:n], self.buf[n:]
        return d

    def send(self, text, op=1):
        data = text.encode() if isinstance(text, str) else text
        mask = os.urandom(4)
        n = len(data)
        if n < 126: hdr = struct.pack("!BB", 0x80 | op, 0x80 | n)
        elif n < 65536: hdr = struct.pack("!BBH", 0x80 | op, 0x80 | 126, n)
        else: hdr = struct.pack("!BBQ", 0x80 | op, 0x80 | 127, n)
        masked = bytes(b ^ mask[i & 3] for i, b in enumerate(data))
        self.s.sendall(hdr + mask + masked)

    def recv(self):
        msg = b""
        while True:
            b1, b2 = self._recv(2)
            fin, op, n = b1 & 0x80, b1 & 0x0F, b2 & 0x7F
            if n == 126: n = struct.unpack("!H", self._recv(2))[0]
            elif n == 127: n = struct.unpack("!Q", self._recv(8))[0]
            mk = self._recv(4) if b2 & 0x80 else None
            data = self._recv(n)
            if mk: data = bytes(b ^ mk[i & 3] for i, b in enumerate(data))
            if op == 9: self.send(data, 10); continue          # ping -> pong
            if op == 8: raise IOError("browser closed the line")
            if op in (0, 1, 2): msg += data
            if fin and op != 9: return msg.decode(errors="ignore")

    def close(self):
        try: self.s.close()
        except Exception: pass

class Browser:
    def __init__(self):
        self.proc = None; self.ws = None; self.n = 0; self.lock = threading.Lock(); self.kb = False

    def start(self):
        exe = find_chromium()
        if not exe:
            log("NO CHROMIUM FOUND. Install it once with:  sudo snap install chromium")
            raise IOError("no chromium")
        prof = os.path.join(os.path.expanduser("~"), "MaryWeb")
        if exe.startswith("/snap/") or "snap" in os.path.realpath(exe):
            prof = os.path.join(os.path.expanduser("~"), "snap", "chromium", "common", "MaryWeb")
        os.makedirs(prof, exist_ok=True)
        args = [exe, "--headless=new", "--remote-debugging-port=%d" % DEBUG_PORT, "--remote-allow-origins=*",
                "--user-data-dir=" + prof, "--no-first-run", "--no-default-browser-check", "--disable-gpu",
                "--hide-scrollbars", "--autoplay-policy=no-user-gesture-required", "--window-size=%d,%d" % (VIEW_W, VIEW_H), "about:blank"]
        log("starting", exe)
        env = dict(os.environ)
        if sound_sink(): env["PULSE_SINK"] = "marybox"      # the page's sound goes to its own quiet line, not to the Jetson's speakers
        self.proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
        url = None
        for _ in range(60):
            time.sleep(0.5)
            try:
                tabs = json.loads(urllib.request.urlopen("http://127.0.0.1:%d/json/list" % DEBUG_PORT, timeout=2).read().decode())
                pages = [t for t in tabs if t.get("type") == "page" and t.get("webSocketDebuggerUrl")]
                if pages: url = pages[0]["webSocketDebuggerUrl"]; break
            except Exception:
                pass
        if not url: raise IOError("Chromium did not start")
        self.ws = WS(url)
        self.cmd("Page.enable")
        self.cmd("Emulation.setDeviceMetricsOverride", width=VIEW_W, height=VIEW_H, deviceScaleFactor=SCALE, mobile=True)
        self.cmd("Emulation.setUserAgentOverride", userAgent=UA)
        self.cmd("Page.navigate", url=HOME)
        time.sleep(3)
        log("browser ready")

    def cmd(self, method, **params):
        self.n += 1; my = self.n
        self.ws.send(json.dumps({"id": my, "method": method, "params": params}))
        end = time.time() + 30
        while time.time() < end:
            m = json.loads(self.ws.recv())
            if m.get("id") == my:
                if "error" in m: raise IOError(str(m["error"]))
                return m.get("result", {})
        raise IOError("browser too slow")

    def alive(self):
        return self.proc is not None and self.proc.poll() is None and self.ws is not None

    def stop(self):
        if self.ws: self.ws.close()
        self.ws = None
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try: self.proc.wait(5)
            except Exception: self.proc.kill()
        self.proc = None

    def shot(self):
        r = self.cmd("Page.captureScreenshot", format="png")
        return png_to_565(base64.b64decode(r["data"]), OUT_W, OUT_H)

    def do(self, what, q):
        with self.lock:
            for attempt in (1, 2):
                try:
                    if not self.alive(): self.stop(); self.start()
                    wait = 0.0; self.kb = False
                    if what == "go":
                        u = (q.get("u") or [""])[0].strip()
                        if u:
                            if " " not in u and "." in u and not u.startswith(("http://", "https://")): u = "https://" + u
                            elif not u.startswith(("http://", "https://")): u = "https://www.google.com/search?q=" + urllib.parse.quote_plus(u)
                            self.cmd("Page.navigate", url=u); wait = 1.8
                    elif what == "tap":
                        x = float((q.get("x") or ["0"])[0]) / SCALE; y = float((q.get("y") or ["0"])[0]) / SCALE
                        for t in ("mousePressed", "mouseReleased"):
                            self.cmd("Input.dispatchMouseEvent", type=t, x=x, y=y, button="left", clickCount=1)
                        time.sleep(0.8); wait = 0.0
                        try:      # did the tap land in a typing box?  Then the box opens its keyboard.
                            r = self.cmd("Runtime.evaluate", returnByValue=True, expression="(function(){var a=document.activeElement;if(!a)return 0;var t=(a.tagName||'').toLowerCase();if(t=='textarea')return 1;if(t=='input'){var y=(a.type||'text').toLowerCase();return ['text','search','email','url','tel','number','password',''].indexOf(y)>=0?1:0}return a.isContentEditable?1:0})()")
                            self.kb = r.get("result", {}).get("value", 0) == 1
                        except Exception:
                            self.kb = False
                    elif what == "scroll":
                        d = int((q.get("d") or ["1"])[0]); px = int(VIEW_H * 0.8) * d
                        if q.get("px"): px = max(-2000, min(2000, int(float(q["px"][0]))))
                        self.cmd("Runtime.evaluate", expression="(function(){var e=document.scrollingElement||document.documentElement||document.body;var b=e.scrollTop;window.scrollBy(0,%d);if(e.scrollTop==b){var all=document.querySelectorAll('*');for(var i=0;i<all.length;i++){var s=all[i];if(s.scrollHeight>s.clientHeight+20){var st=getComputedStyle(s).overflowY;if(st=='auto'||st=='scroll'){s.scrollTop+=%d;}}}}})()" % (px, px))
                        wait = 0.4
                    elif what == "type":
                        t = (q.get("t") or [""])[0]
                        if t: self.cmd("Input.insertText", text=t)
                        if (q.get("enter") or ["0"])[0] == "1":
                            for k in ("keyDown", "keyUp"):
                                self.cmd("Input.dispatchKeyEvent", type=k, key="Enter", code="Enter", windowsVirtualKeyCode=13, nativeVirtualKeyCode=13, text="\r" if k == "keyDown" else "")
                        wait = 1.5
                    elif what == "back":
                        self.cmd("Runtime.evaluate", expression="history.back()"); wait = 1.2
                    elif what == "home":
                        self.cmd("Page.navigate", url=HOME); wait = 3.0
                    time.sleep(wait)
                    return self.shot()
                except Exception as e:
                    log("problem:", e, "(try", attempt, ")")
                    self.stop()
            return None

B = Browser()

DRAIN = {"ip": "", "state": "", "at": 0.0}

def drain_listen():
    """The drain board shouts 'AMYBOX|Drain pan|DRY|Water' on Mary's Wi-Fi (UDP 4210). Remember where it is and what it said."""
    while True:
        try:
            u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            u.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try: u.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            except Exception: pass
            u.bind(("0.0.0.0", 4210))
            while True:
                data, addr = u.recvfrom(512)
                parts = data.decode(errors="ignore").strip().split("|")
                if len(parts) >= 3 and parts[0] == "AMYBOX" and parts[1] == "Drain pan" and not addr[0].startswith("10.42."):
                    DRAIN["ip"] = addr[0]; DRAIN["state"] = parts[2]; DRAIN["at"] = time.time()
        except Exception as e:
            log("drain listener:", e); time.sleep(10)

KEYFILE = os.path.join(os.path.expanduser("~"), ".mary_key")
LAST = {"frame": None}
ROW = OUT_W * 2

class Handler(BaseHTTPRequestHandler):
    def plain(self, code, text=""):
        data = text.encode()
        self.send_response(code); self.send_header("Content-Length", str(len(data))); self.send_header("Connection", "close"); self.end_headers()
        if data: self.wfile.write(data)

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        what = u.path.strip("/") or "shot"
        q = urllib.parse.parse_qs(u.query)
        if what == "bt.mp3":                               # the phone's music (Bluetooth to the Jetson), as a radio the box can play
            ff = shutil.which("ffmpeg")
            if not ff: log("no ffmpeg: install it with  sudo apt install -y ffmpeg"); return self.plain(503)
            BT["until"] = time.time() + 300                # a phone may find "Mary Box" and join for the next 5 minutes
            p = None
            try:
                self.send_response(200); self.send_header("Content-Type", "audio/mpeg"); self.send_header("Cache-Control", "no-store"); self.send_header("Connection", "close"); self.end_headers()
                while True:
                    src = bt_source()
                    if src: cmd = [ff, "-loglevel", "quiet", "-fflags", "nobuffer", "-f", "pulse", "-i", src]
                    else:   cmd = [ff, "-loglevel", "quiet", "-re", "-f", "lavfi", "-i", "anullsrc=r=44100:cl=mono", "-t", "3"]      # no phone yet: quiet, and look again
                    p = subprocess.Popen(cmd + ["-ac", "1", "-ar", "44100", "-c:a", "libmp3lame", "-b:a", "64k", "-flush_packets", "1", "-f", "mp3", "pipe:1"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
                    t0 = time.time(); n = 0
                    while True:
                        chunk = p.stdout.read(1024)
                        if not chunk: break
                        self.wfile.write(chunk); self.wfile.flush(); n += 1
                        if src and n % 24 == 0 and bt_source() != src: break      # the phone left
                    try: p.kill()
                    except Exception: pass
                    if src and n == 0: time.sleep(1)
            except Exception:
                pass
            finally:
                try:
                    if p: p.kill()
                except Exception: pass
            return
        if what == "web.mp3":                              # the sound of the web page (YouTube...), as a radio the box can play
            ff = shutil.which("ffmpeg")
            if not ff: log("no ffmpeg: install it with  sudo apt install -y ffmpeg"); return self.plain(503)
            p = subprocess.Popen([ff, "-loglevel", "quiet", "-fflags", "nobuffer", "-f", "pulse", "-i", sound_source(), "-ac", "1", "-ar", "44100",
                                  "-c:a", "libmp3lame", "-b:a", "64k", "-flush_packets", "1", "-f", "mp3", "pipe:1"], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            try:
                self.send_response(200); self.send_header("Content-Type", "audio/mpeg"); self.send_header("Cache-Control", "no-store"); self.send_header("Connection", "close"); self.end_headers()
                while True:
                    chunk = p.stdout.read(1024)
                    if not chunk: break
                    self.wfile.write(chunk); self.wfile.flush()
            except Exception:
                pass
            finally:
                try: p.kill()
                except Exception: pass
            return
        if what == "cam":                                  # the newest picture from a phone's camera, for the box's CAM button
            if CAM["frame"] is None or time.time() - CAM["stamp"] > 20: return self.plain(204)
            if CAM["served"] == CAM["stamp"] and q.get("f") != ["1"]: return self.plain(204)
            CAM["served"] = CAM["stamp"]; body = CAM["frame"]
            self.send_response(200); self.send_header("Content-Type", "application/octet-stream"); self.send_header("X-Y0", "0"); self.send_header("X-Kb", "0")
            self.send_header("Content-Length", str(len(body))); self.send_header("Connection", "close"); self.end_headers(); self.wfile.write(body); return
        if what == "drain":
            if not DRAIN["ip"]: return self.plain(204)
            return self.plain(200, "%s|%s|%d" % (DRAIN["ip"], DRAIN["state"], int(time.time() - DRAIN["at"])))
        if what == "key":                                  # Mary's Google key, kept here so a new flash of the box gets it back
            ip = self.client_address[0]
            if not (ip.startswith("10.42.") or ip.startswith("127.") or ip.startswith("192.168.")): return self.plain(403)
            if q.get("set"):
                k = q["set"][0].strip()
                if 20 < len(k) < 120:
                    with open(KEYFILE, "w") as f: f.write(k)
                    os.chmod(KEYFILE, 0o600)
                return self.plain(200, "ok")
            try:
                with open(KEYFILE) as f: return self.plain(200, f.read().strip())
            except Exception:
                return self.plain(204)
        if what not in ("shot", "go", "tap", "scroll", "type", "back", "home"): return self.plain(404)
        frame = B.do(what, q)
        if not frame or len(frame) != ROW * OUT_H: return self.plain(503)
        kb = "1" if (what == "tap" and B.kb) else "0"
        old = LAST["frame"]; y0, y1 = 0, OUT_H
        if q.get("f") != ["1"] and old is not None:
            if frame == old:                               # the page stands still: send nothing
                self.send_response(204); self.send_header("X-Kb", kb); self.send_header("Content-Length", "0"); self.send_header("Connection", "close"); self.end_headers(); return
            while y0 < OUT_H and frame[y0 * ROW:(y0 + 1) * ROW] == old[y0 * ROW:(y0 + 1) * ROW]: y0 += 1
            while y1 > y0 + 1 and frame[(y1 - 1) * ROW:y1 * ROW] == old[(y1 - 1) * ROW:y1 * ROW]: y1 -= 1
        LAST["frame"] = frame
        body = frame[y0 * ROW:y1 * ROW]                    # only the band of the page that changed
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("X-Y0", str(y0)); self.send_header("X-Kb", kb)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass

# ============================================================ CALL hub: boxes <-> phones (voice, written messages, phone camera)
CALL_PORT, PHONE_PORT, PAY = 4211, 8092, 240
MSGS = []                      # what the boxes and phones said: {"n", "kind", "who", "text" or "pcm"}
MSG_N = [0]
SEEN = {}                      # pieces already handled (they come several times and by two roads)
VOICE = {}                     # voice calls being put together: id -> {"tot", "parts", "at"}
CAM = {"frame": None, "stamp": 0, "served": -1}
HUB_LOCK = threading.Lock()

def mu_dec(u):
    u = (~u) & 255; ex = (u >> 4) & 7; v = (((u & 15) << 3) + 0x84) << ex; v -= 0x84
    return -v if u & 0x80 else v
MU_TAB = [mu_dec(i) for i in range(256)]
def mu_enc(v):
    sign = 0x80 if v < 0 else 0
    if v < 0: v = -v
    if v > 32635: v = 32635
    v += 0x84; ex = 7; m = 0x4000
    while (v & m) == 0 and ex > 0: ex -= 1; m >>= 1
    return (~(sign | (ex << 4) | ((v >> (ex + 3)) & 15))) & 255

def add_msg(kind, who, text="", pcm=b""):
    with HUB_LOCK:
        MSG_N[0] += 1
        MSGS.append({"n": MSG_N[0], "kind": kind, "who": who, "text": text, "pcm": pcm, "t": time.strftime("%I:%M %p").lstrip("0")})
        del MSGS[:-40]

def hub_send(pk, to_hotspot=True, to_lan=True):
    try:
        u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); u.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        if to_hotspot:
            try: u.sendto(pk, ("10.42.0.255", CALL_PORT))
            except Exception: pass
        if to_lan:
            try: u.sendto(pk, ("255.255.255.255", CALL_PORT))
            except Exception: pass
        u.close()
    except Exception as e:
        log("hub send:", e)

def voice_done(vid):
    v = VOICE.pop(vid, None)
    if not v or not v["parts"]: return
    out = bytearray()
    for q in range(v["tot"]):
        part = v["parts"].get(q, b"\xff" * PAY)
        for b in part:
            s = max(-32000, min(32000, MU_TAB[b] * 2)); out += struct.pack("<h", s)
    add_msg("voice", "box", pcm=bytes(out))

def hub_listen():
    """Hears every box. Passes pieces between Doctor Magic and Mary's Wi-Fi, and keeps the calls for the phone page."""
    while True:
        try:
            u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            u.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1); u.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
            u.bind(("0.0.0.0", CALL_PORT)); u.settimeout(0.3)
            while True:
                now = time.time()
                for vid in [k for k, v in VOICE.items() if now - v["at"] > 0.8]: voice_done(vid)
                for k in [k for k, t in SEEN.items() if now - t > 30]: SEEN.pop(k, None)
                try: data, addr = u.recvfrom(600)
                except socket.timeout: continue
                if len(data) < 5 or data[0:1] != b"M": continue
                kind = data[1:2]; vid = data[2] | (data[3] << 8)
                if kind == b"X":
                    key = ("X", vid)
                    if key in SEEN: continue
                    SEEN[key] = now
                    hub_send(data, to_hotspot=not addr[0].startswith("10.42."), to_lan=addr[0].startswith("10.42."))
                    add_msg("text", "box", text=data[4:204].decode(errors="ignore"))
                elif kind == b"C" and len(data) >= 9:
                    seq = data[4] | (data[5] << 8); tot = data[6] | (data[7] << 8)
                    key = ("C", vid, seq)
                    if key in SEEN or tot < 1 or tot > 300 or seq >= tot: continue
                    SEEN[key] = now
                    hub_send(data, to_hotspot=not addr[0].startswith("10.42."), to_lan=addr[0].startswith("10.42."))
                    v = VOICE.setdefault(vid, {"tot": tot, "parts": {}, "at": now})
                    v["parts"][seq] = data[8:8 + PAY]; v["at"] = now
                    if len(v["parts"]) >= tot: voice_done(vid)
        except Exception as e:
            log("hub listener:", e); time.sleep(5)

def phone_say(text):
    text = text.strip()[:200]
    if not text: return
    vid = random.randrange(1, 65535) | 1
    pk = b"MX" + bytes([vid & 255, vid >> 8]) + text.encode(errors="ignore")[:200]
    SEEN[("X", vid)] = time.time()
    for _ in range(3): hub_send(pk); time.sleep(0.02)
    add_msg("text", "phone", text=text)

def phone_talk(pcm8k):
    """pcm8k: 16-bit little-end samples, 8000 a second, from the phone's microphone."""
    n = min(len(pcm8k) // 2, 300 * PAY)
    if n < 2000: return
    mu = bytes(mu_enc(struct.unpack_from("<h", pcm8k, i * 2)[0]) for i in range(n))
    tot = (n + PAY - 1) // PAY; vid = random.randrange(1, 65535) | 1
    for _ in range(2):
        for q in range(tot):
            SEEN[("C", vid, q)] = time.time()
            hub_send(b"MC" + bytes([vid & 255, vid >> 8, q & 255, q >> 8, tot & 255, tot >> 8]) + mu[q * PAY:(q + 1) * PAY]); time.sleep(0.004)
    add_msg("voice", "phone", pcm=pcm8k[:n * 2])

def wav8k(pcm):
    return b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, 8000, 16000, 2, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm

PHONE_PAGE = r"""<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1,user-scalable=no">
<title>Mary's Magic Box - Call</title><style>
body{margin:0;font:18px system-ui,sans-serif;background:#0b1430;color:#eef;display:flex;flex-direction:column;height:100vh}
h1{font-size:20px;margin:0;padding:12px 14px;background:#132250;color:#ffd95a}
#log{flex:1;overflow:auto;padding:10px 12px}
.m{margin:8px 0;padding:10px 12px;border-radius:14px;max-width:85%;background:#1c2f66}.me{background:#1f7a4d;margin-left:auto}
.m small{display:block;opacity:.7;font-size:12px}
#bar{padding:10px;background:#132250}
#row{display:flex;gap:8px;margin-bottom:8px}#t{flex:1;font-size:18px;padding:12px;border-radius:12px;border:0}
button{font-size:18px;font-weight:700;border:0;border-radius:14px;padding:14px;color:#fff;background:#1e78d2}
#talk{width:100%;padding:22px;background:#e6508f;font-size:22px;user-select:none;-webkit-user-select:none;touch-action:none}#talk.on{background:#ffaa1e}
#cam{width:100%;margin-top:8px;background:#14965a}#cam.on{background:#e13c3c}
video{display:none}#note{font-size:13px;opacity:.75;padding:6px 2px 0}footer{font-size:11px;opacity:.5;text-align:center;padding:4px}
</style></head><body><h1>Mary's Magic Box &middot; Call</h1><div id=log></div>
<div id=bar><div id=row><input id=t placeholder="Type a message" maxlength=200><button id=send>SEND</button></div>
<button id=talk>HOLD TO TALK</button><button id=cam>CAMERA: OFF</button><div id=note></div></div>
<video id=v playsinline muted></video><canvas id=c width=320 height=216 style="display:none"></canvas>
<footer>&copy; Moto Takeda 316-304-5398</footer>
<script>
var last=0,log=document.getElementById('log'),note=document.getElementById('note'),q=[],playing=false;
function say(s){note.textContent=s}
function add(m){var d=document.createElement('div');d.className='m'+(m.who=='phone'?' me':'');d.innerHTML='<small>'+(m.who=='phone'?'Phone':'Box')+' &middot; '+m.t+'</small>';
 if(m.kind=='text'){d.appendChild(document.createTextNode(m.text))}else{var b=document.createElement('button');b.textContent='▶ Voice';b.onclick=function(){new Audio('/voice?n='+m.n).play()};d.appendChild(b);if(m.who!='phone'){q.push(m.n);next()}}
 log.appendChild(d);log.scrollTop=1e9}
function next(){if(playing||!q.length)return;playing=true;var a=new Audio('/voice?n='+q.shift());a.onended=a.onerror=function(){playing=false;next()};a.play().catch(function(){playing=false;say('Tap a Voice button once so the phone may play sound.')})}
function poll(){fetch('/msgs?after='+last).then(function(r){return r.json()}).then(function(l){l.forEach(function(m){last=Math.max(last,m.n);add(m)})}).catch(function(){}).then(function(){setTimeout(poll,1500)})}
poll();
function send(){var t=document.getElementById('t');if(!t.value.trim())return;fetch('/say?t='+encodeURIComponent(t.value));t.value=''}
document.getElementById('send').onclick=send;document.getElementById('t').onkeydown=function(e){if(e.key=='Enter')send()};
var ctx,src,proc,stream,buf=[],rec=false,tb=document.getElementById('talk');
function mic(){if(stream)return Promise.resolve();if(!navigator.mediaDevices)return Promise.reject('This page must be opened with https:// for the microphone.');
 return navigator.mediaDevices.getUserMedia({audio:true}).then(function(s){stream=s;ctx=new (window.AudioContext||window.webkitAudioContext)();src=ctx.createMediaStreamSource(s);proc=ctx.createScriptProcessor(4096,1,1);
 proc.onaudioprocess=function(e){if(rec)buf.push(new Float32Array(e.inputBuffer.getChannelData(0)))};src.connect(proc);proc.connect(ctx.destination)})}
function down(e){e.preventDefault();mic().then(function(){if(ctx.state=='suspended')ctx.resume();buf=[];rec=true;tb.className='on';tb.textContent='TALKING... LET GO TO SEND'}).catch(function(x){say(''+x)})}
function up(e){e.preventDefault();if(!rec)return;rec=false;tb.className='';tb.textContent='HOLD TO TALK';
 var n=0;buf.forEach(function(b){n+=b.length});var all=new Float32Array(n),o=0;buf.forEach(function(b){all.set(b,o);o+=b.length});
 var step=ctx.sampleRate/8000,m=Math.min(Math.floor(n/step),64000),out=new Int16Array(m);
 for(var i=0;i<m;i++){var a=Math.floor(i*step),b2=Math.min(n,Math.floor((i+1)*step)),s=0;for(var k=a;k<b2;k++)s+=all[k];s=s/Math.max(1,b2-a)*2.5;out[i]=Math.max(-1,Math.min(1,s))*32000}
 if(m<2000){say('Hold the button the whole time you speak.');return}
 fetch('/talk',{method:'POST',body:out.buffer}).then(function(){say('Sent.')}).catch(function(){say('Could not send.')})}
tb.addEventListener('touchstart',down,{passive:false});tb.addEventListener('touchend',up,{passive:false});tb.addEventListener('mousedown',down);tb.addEventListener('mouseup',up);
var camOn=false,cs,ct,cb=document.getElementById('cam'),v=document.getElementById('v'),c=document.getElementById('c');
function shot(){if(!camOn)return;var g=c.getContext('2d'),vw=v.videoWidth,vh=v.videoHeight;if(vw){var sc=Math.max(320/vw,216/vh),w=vw*sc,h=vh*sc;g.drawImage(v,(320-w)/2,(216-h)/2,w,h);
 var d=g.getImageData(0,0,320,216).data,o=new Uint8Array(320*216*2);for(var i=0,j=0;i<d.length;i+=4,j+=2){var p=((d[i]&0xF8)<<8)|((d[i+1]&0xFC)<<3)|(d[i+2]>>3);o[j]=p>>8;o[j+1]=p&255}
 fetch('/campost',{method:'POST',body:o.buffer}).catch(function(){})}
 ct=setTimeout(shot,2000)}
cb.onclick=function(){if(camOn){camOn=false;clearTimeout(ct);cs.getTracks().forEach(function(t){t.stop()});cb.className='';cb.textContent='CAMERA: OFF';return}
 if(!navigator.mediaDevices){say('This page must be opened with https:// for the camera.');return}
 navigator.mediaDevices.getUserMedia({video:{facingMode:'environment'}}).then(function(s){cs=s;v.srcObject=s;v.play();camOn=true;cb.className='on';cb.textContent='CAMERA: ON (tap to stop)';setTimeout(shot,800)}).catch(function(x){say('Camera: '+x)})};
</script></body></html>"""

class PhoneHandler(BaseHTTPRequestHandler):
    def out(self, code, body=b"", ctype="text/plain"):
        if isinstance(body, str): body = body.encode()
        self.send_response(code); self.send_header("Content-Type", ctype); self.send_header("Content-Length", str(len(body))); self.send_header("Cache-Control", "no-store"); self.end_headers()
        if body: self.wfile.write(body)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path); q = urllib.parse.parse_qs(u.query); what = u.path.strip("/")
        if what == "": return self.out(200, PHONE_PAGE, "text/html; charset=utf-8")
        if what == "msgs":
            after = int((q.get("after") or ["0"])[0])
            with HUB_LOCK: l = [{"n": m["n"], "kind": m["kind"], "who": m["who"], "text": m["text"], "t": m["t"]} for m in MSGS if m["n"] > after]
            return self.out(200, json.dumps(l), "application/json")
        if what == "voice":
            n = int((q.get("n") or ["0"])[0])
            with HUB_LOCK: pcm = next((m["pcm"] for m in MSGS if m["n"] == n and m["kind"] == "voice"), None)
            return self.out(200, wav8k(pcm), "audio/wav") if pcm else self.out(404)
        if what == "say":
            threading.Thread(target=phone_say, args=((q.get("t") or [""])[0],), daemon=True).start(); return self.out(200, "ok")
        return self.out(404)
    def do_POST(self):
        what = urllib.parse.urlparse(self.path).path.strip("/"); n = int(self.headers.get("Content-Length") or 0)
        if n < 0 or n > 400000: return self.out(413)
        body = self.rfile.read(n)
        if what == "talk": threading.Thread(target=phone_talk, args=(body,), daemon=True).start(); return self.out(200, "ok")
        if what == "campost":
            if len(body) == OUT_W * OUT_H * 2: CAM["frame"] = body; CAM["stamp"] = time.time()
            return self.out(200, "ok")
        return self.out(404)
    def log_message(self, *a): pass

def phone_server():
    """The phone page. Phones only allow the microphone and camera on https, so the Jetson makes its own certificate once."""
    try:
        import ssl
        crt = os.path.join(HERE, "mary_cert.pem"); key = os.path.join(HERE, "mary_key.pem")
        if not (os.path.exists(crt) and os.path.exists(key)):
            subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "3650", "-subj", "/CN=Mary Magic Box", "-keyout", key, "-out", crt],
                           timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        srv = ThreadingHTTPServer(("0.0.0.0", PHONE_PORT), PhoneHandler)
        if os.path.exists(crt) and os.path.exists(key):
            c = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER); c.load_cert_chain(crt, key); srv.socket = c.wrap_socket(srv.socket, server_side=True)
            log("phone page: https on port", PHONE_PORT)
        else:
            log("phone page: no certificate, plain http on port", PHONE_PORT, "(typing works, microphone and camera do not)")
        srv.serve_forever()
    except Exception as e:
        log("phone page problem:", e)


def install():
    me = os.path.abspath(__file__)
    auto = os.path.join(os.path.expanduser("~"), ".config", "autostart")
    os.makedirs(auto, exist_ok=True)
    with open(os.path.join(auto, "mary-web.desktop"), "w") as f:
        f.write("[Desktop Entry]\nType=Application\nName=Mary Web\nComment=Web pages for Mary's Magic Box\n"
                "Exec=python3 %s\nX-GNOME-Autostart-enabled=true\nNoDisplay=false\n" % me)
    try: subprocess.run(["nmcli", "connection", "modify", "Hotspot", "connection.autoconnect", "yes"], timeout=10, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)      # Doctor Magic comes back by itself after a restart
    except Exception: pass
    if not shutil.which("ffmpeg"):
        print("\n*** For the sound of web pages paste this line next:  sudo apt install -y ffmpeg\n")
    if not find_chromium():
        print("\n*** Chromium is missing. Paste this line next:  sudo snap install chromium\n")
    subprocess.Popen([sys.executable, me], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
    print("\nMARY WEB IS ON. It starts by itself every time the Jetson starts. You can close this window.\n")

if __name__ == "__main__":
    if "--install" in sys.argv:
        install(); sys.exit(0)
    try:
        s = socket.socket(); s.settimeout(1)
        if s.connect_ex(("127.0.0.1", PORT)) == 0:
            print("Mary Web is already running."); sys.exit(0)
        s.close()
    except Exception:
        pass
    log("Mary Web starting on port", PORT)
    srv = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    threading.Thread(target=drain_listen, daemon=True).start()
    threading.Thread(target=hub_listen, daemon=True).start()
    threading.Thread(target=bt_agent, daemon=True).start()
    threading.Thread(target=phone_server, daemon=True).start()
    threading.Thread(target=lambda: B.do("shot", {}), daemon=True).start()      # open the browser now, not on the first tap
    srv.serve_forever()
