#!/usr/bin/env python3
"""Lab simulator: DJI Cloud API osd telemetry for the Drohne page.

Publishes remote-controller and aircraft osd messages (QoS 0, 1 Hz) to the
AeroNexus EMQX broker with a *pilot* account, which the broker ACL allows to
publish osd as a device. Serials are fake (SIM...), so the AeroNexus backend,
which subscribes osd only for registered online devices, ignores them.
No positions are sent.

    export DRONE_SIM_USER=pilot
    export DRONE_SIM_PASSWORD=...     # MQTT_PILOT_PASSWORD from AeroNexus edge/.env
    python3 tools/drone_osd_sim.py --host 192.168.178.63 --port 1884 --duration 120

--second-aircraft also publishes a second aircraft: the board must stay on the
first one while it reports and switch only after it went silent (--stop-first).
"""
import argparse
import json
import math
import os
import socket
import struct
import sys
import time

MODEL_PAYLOAD = {"m3e": "66-0-0", "m3t": "67-0-0", "m3m": "68-0-0", "m4e": "88-0-0", "m4t": "89-0-0"}


def remaining_length(n):
    out = bytearray()
    while True:
        digit, n = n % 128, n // 128
        out.append(digit | (0x80 if n else 0))
        if not n:
            return bytes(out)


def mqtt_string(text):
    raw = text.encode()
    return struct.pack("!H", len(raw)) + raw


def encode_connect(client_id, user, password, keepalive=30):
    body = mqtt_string("MQTT") + bytes([4, 0xC2]) + struct.pack("!H", keepalive)
    body += mqtt_string(client_id) + mqtt_string(user) + mqtt_string(password)
    return bytes([0x10]) + remaining_length(len(body)) + body


def encode_publish(topic, payload):
    body = mqtt_string(topic) + payload
    return bytes([0x30]) + remaining_length(len(body)) + body


def envelope(gateway, data, t):
    return {"tid": f"sim-{int(t * 1000)}", "bid": "sim", "timestamp": int(t * 1000),
            "gateway": gateway, "data": data}


def aircraft_osd(rc_sn, step, model="m3t", mode_code=3):
    phase = step / 10.0
    data = {
        "mode_code": mode_code,
        "elevation": round(20 + 15 * math.sin(phase), 1),
        "height": 0,  # ellipsoid height: not shown, no real position is sent
        "home_distance": round(40 + 30 * (1 + math.sin(phase / 2)), 1),
        "horizontal_speed": round(abs(5 * math.cos(phase)), 1),
        "vertical_speed": round(1.5 * math.cos(phase), 1),
        "battery": {"capacity_percent": max(20, 95 - step // 6),
                    "remain_flight_time": max(0, 1500 - step * 5),
                    "batteries": [{"capacity_percent": max(20, 95 - step // 6)}]},
        "position_state": {"gps_number": 17, "rtk_number": 0, "is_fixed": 0, "quality": 4},
        # Pilot 2 reports the camera in cameras[] and as a "<type>-0-0" key
        # (RC Pro + Mavic 3M, 2026-10-14), not in the demo's payloads[].
        "cameras": [{"camera_mode": 0, "payload_index": MODEL_PAYLOAD[model]}],
        MODEL_PAYLOAD[model]: {"payload_index": MODEL_PAYLOAD[model], "gimbal_pitch": -30},
    }
    return envelope(rc_sn, data, time.time())


def rc_osd(rc_sn, step):
    data = {"capacity_percent": max(30, 88 - step // 10),
            "wireless_link": {"sdr_quality": 5 if step % 20 < 15 else 4, "sdr_link_state": True}}
    return envelope(rc_sn, data, time.time())


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1884)
    ap.add_argument("--model", choices=sorted(MODEL_PAYLOAD), default="m3t")
    ap.add_argument("--duration", type=float, default=120)
    ap.add_argument("--rc-sn", default="SIMRC0000001")
    ap.add_argument("--aircraft-sn", default="SIMAC0000A1B")
    ap.add_argument("--second-aircraft", action="store_true")
    ap.add_argument("--stop-first", type=float, default=0,
                    help="seconds after which the first aircraft stops reporting")
    args = ap.parse_args()
    user, password = os.environ.get("DRONE_SIM_USER"), os.environ.get("DRONE_SIM_PASSWORD")
    if not user or not password:
        sys.exit("DRONE_SIM_USER and DRONE_SIM_PASSWORD must be set (pilot account)")

    sock = socket.create_connection((args.host, args.port), timeout=5)
    sock.sendall(encode_connect("drone-osd-sim", user, password))
    connack = sock.recv(4)
    if len(connack) < 4 or connack[0] != 0x20 or connack[3] != 0:
        sys.exit(f"CONNACK refused: {connack.hex()}")
    print(f"connected to {args.host}:{args.port}, model {args.model}", flush=True)

    start, step = time.time(), 0
    try:
        while time.time() - start < args.duration:
            elapsed = time.time() - start
            messages = [(args.rc_sn, rc_osd(args.rc_sn, step))]
            if not args.stop_first or elapsed < args.stop_first:
                messages.append((args.aircraft_sn, aircraft_osd(args.rc_sn, step, args.model)))
            if args.second_aircraft:
                messages.append(("SIMAC0000C2D", aircraft_osd("SIMRC0000002", step, "m4t", 0)))
            for sn, msg in messages:
                sock.sendall(encode_publish(f"thing/product/{sn}/osd", json.dumps(msg).encode()))
            if step % 10 == 0:
                print(f"t={elapsed:5.1f}s sent {len(messages)} osd", flush=True)
            step += 1
            if step % 15 == 0:
                sock.sendall(b"\xc0\x00")  # PINGREQ keeps the 30 s keepalive
            time.sleep(1)
    finally:
        sock.sendall(b"\xe0\x00")
        sock.close()


if __name__ == "__main__":
    main()
