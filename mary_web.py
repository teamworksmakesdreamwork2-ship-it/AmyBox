#!/usr/bin/env python3
# Mary Web - lets Mary's Magic Box show real web pages.
# The Jetson opens the page in a real Chromium browser (no window), takes a small picture of it,
# and sends the picture to the box. Taps, typing and scrolling from the box go back to the browser.
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
        self.proc = None; self.ws = None; self.n = 0; self.lock = threading.Lock()

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
                "--hide-scrollbars", "--mute-audio", "--window-size=%d,%d" % (VIEW_W, VIEW_H), "about:blank"]
        log("starting", exe)
        self.proc = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
                    wait = 0.2
                    if what == "go":
                        u = (q.get("u") or [""])[0].strip()
                        if u:
                            if " " not in u and "." in u and not u.startswith(("http://", "https://")): u = "https://" + u
                            elif not u.startswith(("http://", "https://")): u = "https://www.google.com/search?q=" + urllib.parse.quote_plus(u)
                            self.cmd("Page.navigate", url=u); wait = 3.0
                    elif what == "tap":
                        x = float((q.get("x") or ["0"])[0]) / SCALE; y = float((q.get("y") or ["0"])[0]) / SCALE
                        for t in ("mousePressed", "mouseReleased"):
                            self.cmd("Input.dispatchMouseEvent", type=t, x=x, y=y, button="left", clickCount=1)
                        wait = 1.5
                    elif what == "scroll":
                        d = int((q.get("d") or ["1"])[0]); px = int(VIEW_H * 0.8) * d
                        self.cmd("Runtime.evaluate", expression="(function(){var e=document.scrollingElement||document.documentElement||document.body;var b=e.scrollTop;window.scrollBy(0,%d);if(e.scrollTop==b){var all=document.querySelectorAll('*');for(var i=0;i<all.length;i++){var s=all[i];if(s.scrollHeight>s.clientHeight+20){var st=getComputedStyle(s).overflowY;if(st=='auto'||st=='scroll'){s.scrollTop+=%d;}}}}})()" % (px, px))
                        wait = 0.6
                    elif what == "type":
                        t = (q.get("t") or [""])[0]
                        if t: self.cmd("Input.insertText", text=t)
                        if (q.get("enter") or ["0"])[0] == "1":
                            for k in ("keyDown", "keyUp"):
                                self.cmd("Input.dispatchKeyEvent", type=k, key="Enter", code="Enter", windowsVirtualKeyCode=13, nativeVirtualKeyCode=13, text="\r" if k == "keyDown" else "")
                        wait = 2.5
                    elif what == "back":
                        self.cmd("Runtime.evaluate", expression="history.back()"); wait = 2.0
                    elif what == "home":
                        self.cmd("Page.navigate", url=HOME); wait = 3.0
                    time.sleep(wait)
                    return self.shot()
                except Exception as e:
                    log("problem:", e, "(try", attempt, ")")
                    self.stop()
            return None

B = Browser()

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        what = u.path.strip("/") or "shot"
        if what not in ("shot", "go", "tap", "scroll", "type", "back", "home"):
            self.send_response(404); self.end_headers(); return
        img = B.do(what, urllib.parse.parse_qs(u.query))
        if not img:
            self.send_response(503); self.send_header("Content-Length", "0"); self.end_headers(); return
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(img)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(img)

    def log_message(self, *a):
        pass

def install():
    me = os.path.abspath(__file__)
    auto = os.path.join(os.path.expanduser("~"), ".config", "autostart")
    os.makedirs(auto, exist_ok=True)
    with open(os.path.join(auto, "mary-web.desktop"), "w") as f:
        f.write("[Desktop Entry]\nType=Application\nName=Mary Web\nComment=Web pages for Mary's Magic Box\n"
                "Exec=python3 %s\nX-GNOME-Autostart-enabled=true\nNoDisplay=false\n" % me)
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
    threading.Thread(target=lambda: B.do("shot", {}), daemon=True).start()      # open the browser now, not on the first tap
    srv.serve_forever()
