#!/usr/bin/env python3
# BoatControl — предпросмотр сервисного веб-интерфейса на PC без прошивки.
#
# Страница берётся из src/web.cpp (между R"rawliteral( и )rawliteral"),
# поэтому предпросмотр всегда синхронен с тем, что поедает на плату.
# API эмулируется в памяти: /api/status, /api/params, /api/erase,
# /api/calibrate, /api/close. Калибровка — 15 с, как на железе.
#
# Запуск:  python tools/web_preview.py [порт]   (по умолчанию 8080)
# Открыть: http://localhost:8080
#
# Границы параметров ниже дублируют include/config.h — при правке границ
# синхронизировать вручную (файл только для предпросмотра, в прошивку не входит).

import json
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB_CPP = ROOT / "src" / "web.cpp"

DEF = {  # дефолты = config.h
    "thr_span": 1700, "thr_deadband": 40, "thr_rate_up": 8, "thr_rate_down": 25,
    "thr_zero_ms": 250, "x_span": 1700, "x_deadband": 30, "x_exponent": 1.4,
    "stb_slew_ms": 2000, "stb_min_us": 1000, "stb_max_us": 2000,
    "cruise_min_pct": 12, "cruise_step_pct": 10, "cruise_edge_pct": 95,
    "cruise_ramp_ms": 800,
}
BND = {  # границы = config.h (X_*_MIN -> ключ, X_*_MAX -> ключ_x)
    "thr_span": (400, 2047), "thr_deadband": (5, 300),
    "thr_rate_up": (1, 100), "thr_rate_down": (1, 100), "thr_zero_ms": (0, 2000),
    "x_span": (400, 2047), "x_deadband": (5, 300),
    "x_exponent": (1.0, 3.0), "stb_slew_ms": (500, 10000),
    "stb_min_us": (1000, 1999), "stb_max_us": (1001, 2000),
    "cruise_min_pct": (1, 100), "cruise_step_pct": (1, 50),
    "cruise_edge_pct": (50, 100), "cruise_ramp_ms": (100, 5000),
}

S = {
    "prm": dict(DEF),
    "state": "READY",
    "web_active": True,
    "remain": 60,
    "cal_until": 0.0,
}
LOCK = threading.Lock()


def index_html() -> str:
    src = WEB_CPP.read_text(encoding="utf-8")
    m = re.search(r'R"rawliteral\((.*?)\)rawliteral"', src, re.S)
    if not m:
        raise SystemExit("INDEX_HTML (rawliteral) не найден в src/web.cpp")
    return m.group(1)


def status_json() -> str:
    with LOCK:
        st = S["state"]
        if st == "CAL_MANUAL" and time.time() >= S["cal_until"]:
            S["state"] = st = "READY"
        if S["web_active"] and S["remain"] > 0 and st == "READY":
            pass  # счётчик тикает в tick(), не в момент запроса
        t = time.time()
        wob = int(6 * (1 if int(t * 2) % 2 else -1))
        return json.dumps({
            "fw": "1.1-preview",
            "state": st,
            "esc": 1500, "stb": 1500,
            "ax": 2048 + wob, "ay": 2048 - wob,
            "xoff": wob, "yoff": -wob,
            "cx": 2048, "cy": 2048,
            "sx": 1700, "sy": 1700,
            "crs": 1500, "cal_pending": st == "CAL_MANUAL",
            "prm": dict(S["prm"]),
            "bnd": {k: lo for k, (lo, _) in BND.items()}
                   | {f"{k}_x": hi for k, (_, hi) in BND.items()},
            "web": {"active": S["web_active"],
                    "remain_s": S["remain"] if S["web_active"] else 0},
        })


def tick():
    while True:
        time.sleep(1)
        with LOCK:
            if S["web_active"] and S["remain"] > 0 and S["state"] != "CAL_MANUAL":
                S["remain"] -= 1
                if S["remain"] == 0:
                    S["web_active"] = False


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def _send(self, code: int, ctype: str, body: str):
        data = body.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", ctype + "; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/index"):
            self._send(200, "text/html", index_html())
        elif self.path.startswith("/api/status"):
            self._send(200, "application/json", status_json())
        else:
            self._send(404, "text/plain", "not found")

    def do_POST(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n).decode("utf-8") if n else ""
        if self.path == "/api/params":
            try:
                patch = json.loads(raw or "{}")
            except json.JSONDecodeError:
                self._send(400, "application/json", '{"ok":false,"err":"bad json"}')
                return
            with LOCK:
                cand = dict(S["prm"])
                for k, v in patch.items():
                    if k not in BND:
                        continue
                    lo, hi = BND[k]
                    try:
                        v = float(v)
                    except (TypeError, ValueError):
                        self._send(400, "application/json",
                                   '{"ok":false,"err":"out of range"}')
                        return
                    if not (lo <= v <= hi):
                        self._send(400, "application/json",
                                   '{"ok":false,"err":"out of range"}')
                        return
                    cand[k] = v
                if cand["thr_deadband"] >= cand["thr_span"] or \
                   cand["x_deadband"] >= cand["x_span"] or \
                   cand["stb_min_us"] >= cand["stb_max_us"]:
                    self._send(400, "application/json",
                               '{"ok":false,"err":"out of range"}')
                    return
                S["prm"] = cand
            self._send(200, "application/json", '{"ok":true}')
        elif self.path == "/api/erase":
            with LOCK:
                S["prm"] = dict(DEF)
            self._send(200, "application/json", '{"ok":true}')
        elif self.path == "/api/calibrate":
            with LOCK:
                if S["state"] == "READY":
                    S["state"] = "CAL_MANUAL"
                    S["cal_until"] = time.time() + 15
            self._send(200, "application/json", '{"ok":true}')
        elif self.path == "/api/close":
            with LOCK:
                S["web_active"] = False
            self._send(200, "application/json", '{"ok":true}')
        else:
            self._send(404, "text/plain", "not found")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    threading.Thread(target=tick, daemon=True).start()
    srv = ThreadingHTTPServer(("127.0.0.1", port), H)
    print(f"BoatControl preview: http://localhost:{port}  (Ctrl+C — выход)")
    srv.serve_forever()


if __name__ == "__main__":
    main()
