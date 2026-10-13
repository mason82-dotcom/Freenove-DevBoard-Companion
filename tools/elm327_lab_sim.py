#!/usr/bin/env python3
"""LAB-ONLY ELM327 TCP simulator. No vehicle, CAN, Bluetooth or USB access.

Answers the Freenove companion's read-only allowlist with fixed, plausible
values (RPM and speed vary slowly) and logs every received command. Any
command outside the allowlist is answered with "?" and reported as
"POLICY VIOLATION", so a lab run proves what the firmware actually sent.
"""
import argparse
import math
import signal
import socketserver
import sys
import threading
import time

ADAPTER = {
    "ATZ": "\r\rELM327 v1.5 (LAB SIM)",
    "ATE0": "OK", "ATL0": "OK", "ATS0": "OK", "ATH0": "OK", "ATSP0": "OK",
}
# 0100: 04 05 0C 0D 0F 11 20 supported; 0120: 40; 0140: 42.
STATIC = {
    "0100": "41 00 18 1A 80 01",
    "0120": "41 20 00 00 00 01",
    "0140": "41 40 40 00 00 00",
    "0104": "41 04 4C",     # 29.8 %
    "0105": "41 05 82",     # 90 C
    "010F": "41 0F 46",     # 30 C
    "0111": "41 11 20",     # 12.5 %
    "0142": "41 42 34 BC",  # 13.500 V
}
ALLOWED = set(ADAPTER) | set(STATIC) | {"010C", "010D"}


def dynamic(cmd, t):
    if cmd == "010C":
        raw = int((850 + 400 * (1 + math.sin(t / 3))) * 4)
        return "41 0C %02X %02X" % (raw >> 8, raw & 0xFF)
    if cmd == "010D":
        return "41 0D %02X" % int(25 + 25 * math.sin(t / 7))
    return None


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        peer = "%s:%d" % self.client_address
        print("connect", peer, flush=True)
        searched = False
        buf = b""
        while True:
            try:
                chunk = self.request.recv(256)
            except OSError:  # board closed with RST (page left)
                break
            if not chunk:
                break
            buf += chunk
            while b"\r" in buf:
                line, buf = buf.split(b"\r", 1)
                cmd = line.decode("ascii", "replace").strip().upper().replace(" ", "")
                if not cmd:
                    continue
                self.server.counts[cmd] = self.server.counts.get(cmd, 0) + 1
                if cmd not in ALLOWED:
                    self.server.violations += 1
                    print("POLICY VIOLATION:", repr(cmd), flush=True)
                    reply = "?"
                elif cmd in ADAPTER:
                    reply = ADAPTER[cmd]
                    if cmd == "ATZ":
                        searched = False
                else:
                    reply = dynamic(cmd, time.monotonic()) or STATIC[cmd]
                    if not searched:
                        time.sleep(self.server.search_delay)
                        reply = "SEARCHING...\r" + reply
                        searched = True
                time.sleep(self.server.reply_delay)
                try:
                    self.wfile.write((reply + "\r\r>").encode("ascii"))
                except OSError:
                    return
        print("disconnect", peer, "counts:", dict(sorted(self.server.counts.items())),
              "violations:", self.server.violations, flush=True)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def make_server(bind, port, search_delay=1.0, reply_delay=0.03):
    server = Server((bind, port), Handler)
    server.counts = {}
    server.violations = 0
    server.search_delay = search_delay
    server.reply_delay = reply_delay
    return server


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--bind", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=35000)
    parser.add_argument("--search-delay", type=float, default=1.0)
    parser.add_argument("--reply-delay", type=float, default=0.03)
    parser.add_argument("--duration", type=float, default=0,
                        help="stop after N seconds (0 = run until Ctrl+C)")
    args = parser.parse_args()
    server = make_server(args.bind, args.port, args.search_delay, args.reply_delay)
    print("ELM327 LAB SIM on %s:%d" % (args.bind, args.port), flush=True)
    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stop.set())
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    stop.wait(args.duration or None)
    server.shutdown()
    server.server_close()
    print("SUMMARY counts:", dict(sorted(server.counts.items())),
          "violations:", server.violations, flush=True)
    return 1 if server.violations else 0


if __name__ == "__main__":
    sys.exit(main())
