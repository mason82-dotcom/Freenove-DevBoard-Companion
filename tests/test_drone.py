"""Contracts for the read-only drone telemetry page (AeroNexus osd via MQTT)."""
import importlib.util
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def arduinojson_include():
    """ArduinoJson from a PlatformIO libdeps folder, or None (CI host job)."""
    candidates = [Path(os.environ["PLATFORMIO_LIBDEPS_DIR"])] if "PLATFORMIO_LIBDEPS_DIR" in os.environ else []
    candidates += [ROOT / ".pio" / "libdeps", ROOT.parent / ".platformio-shared" / "libdeps"]
    for base in candidates:
        for header in sorted(base.glob("*/ArduinoJson/src/ArduinoJson.h")):
            return header.parent
    return None


def compile_and_run(test, sources, includes, prefix):
    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise unittest.SkipTest("Native C++ compiler unavailable")
    with tempfile.TemporaryDirectory(prefix=prefix) as tmp:
        binary = Path(tmp) / "smoke"
        cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic"]
        cmd += ["-I" + str(p) for p in includes] + [str(s) for s in sources] + ["-o", str(binary)]
        compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        test.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
        ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
        test.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


def load_sim():
    spec = importlib.util.spec_from_file_location("drone_osd_sim", ROOT / "tools" / "drone_osd_sim.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class DroneTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = code_only(read("src/DroneClient.cpp"))
        cls.mqtt = code_only(read("src/DroneMqtt.cpp"))
        cls.mqtt_header = code_only(read("include/DroneMqtt.h"))
        cls.osd = code_only(read("src/DroneOsd.cpp"))
        cls.gui = read("src/CompanionUi.cpp")

    def test_subscribe_only_by_construction(self):
        # No PUBLISH encoder exists; the only packets the board can send are
        # CONNECT, SUBSCRIBE, PINGREQ and DISCONNECT.
        for source in (self.mqtt, self.mqtt_header, self.client):
            self.assertNotIn("encodePublish", source)
        self.assertNotRegex(self.mqtt, r"0x3[0-9A-Fa-f]")  # no PUBLISH type byte is ever written
        self.assertIn("out[0] = 0xC0;", self.mqtt)
        self.assertIn("out[0] = 0xE0;", self.mqtt)
        self.assertIn("putHeader(out, capacity, 0x10,", self.mqtt)
        self.assertIn("putHeader(out, capacity, 0x82,", self.mqtt)
        sends = re.findall(r"send\(out, DroneMqtt::(\w+)\(", self.client)
        self.assertEqual(sorted(set(sends)),
                         ["encodeConnect", "encodeDisconnect", "encodePingreq", "encodeSubscribe"])
        self.assertIn('kTopicFilter[] = "thing/product/+/osd"', self.client)
        self.assertNotIn("WiFi.begin(", self.client)
        self.assertNotIn("setInsecure(", self.client)

    def test_positions_are_never_parsed_or_logged(self):
        for source in (self.osd, self.client, code_only(read("src/DroneDisplay.cpp"))):
            for field in ("latitude", "longitude"):
                with self.subTest(field=field):
                    self.assertNotIn(field, source)
        self.assertIn("DeserializationOption::Filter(filter)", self.osd)

    def test_session_only_while_page_open(self):
        self.assertIn("ulTaskNotifyTake(pdTRUE, portMAX_DELAY);", self.client)
        self.assertIn("if (showing(CompanionModules::Id::Drone)) DroneTelemetry::requestActive(false);",
                      self.gui)
        self.assertIn("if (showing(CompanionModules::Id::Drone)) DroneTelemetry::requestActive(true);",
                      self.gui)
        self.assertIn("kAuthRetryDelayMs = 60000", self.client)
        self.assertIn("MALLOC_CAP_SPIRAM", self.client)

    def test_local_config_is_ignored_and_template_uses_companion(self):
        ignore = read(".gitignore").splitlines()
        self.assertIn("include/DroneLocalConfig.h", ignore)
        example = read("include/DroneLocalConfig.example.h")
        self.assertIn('#define DRONE_MQTT_USER "companion"', example)
        self.assertIn('"YOUR_COMPANION_PASSWORD"', example)
        self.assertIn("#define DRONE_MQTT_PORT 1884", example)

    def test_native_codec_and_formatter(self):
        compile_and_run(self, [ROOT / "tests" / "drone_codec_smoke.cpp", ROOT / "src" / "DroneMqtt.cpp",
                               ROOT / "src" / "DroneDisplay.cpp"], [ROOT / "include"], "fnk0086-drone-")

    def test_native_osd_parser_and_selection(self):
        include = arduinojson_include()
        if include is None:
            self.skipTest("ArduinoJson not installed (run pio run -e fnk0086 once)")
        compile_and_run(self, [ROOT / "tests" / "drone_osd_smoke.cpp", ROOT / "src" / "DroneOsd.cpp"],
                        [ROOT / "include", include], "fnk0086-droneosd-")

    def test_simulator_sends_osd_without_positions(self):
        sim = load_sim()
        msg = sim.aircraft_osd("RC1", 0, "m3t")
        self.assertEqual(msg["gateway"], "RC1")
        self.assertIn("mode_code", msg["data"])
        self.assertEqual(msg["data"]["cameras"][0]["payload_index"], "67-0-0")
        self.assertNotIn("latitude", json.dumps(msg))
        self.assertIn("wireless_link", sim.rc_osd("RC1", 0)["data"])
        # Wire format: decode the packets the simulator would send.
        connect = sim.encode_connect("sim", "pilot", "pw", 30)
        self.assertEqual(connect[0], 0x10)
        self.assertEqual(connect[1], len(connect) - 2)
        payload = b"x" * 300
        packet = sim.encode_publish("thing/product/RC1/osd", payload)
        self.assertEqual(packet[0], 0x30)
        self.assertEqual(packet[1:3], bytes([0xC3, 0x02]))  # 300 + 2 + 21 = 323 = 67 + 2 * 128
        topic_length = struct.unpack("!H", packet[3:5])[0]
        self.assertEqual(packet[5:5 + topic_length], b"thing/product/RC1/osd")
        self.assertEqual(packet[5 + topic_length:], payload)

    def test_simulator_against_fake_broker(self):
        sim = load_sim()
        server = socket.socket()
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        received = []

        def broker():
            conn, _ = server.accept()
            with conn:
                conn.recv(1)
                conn.recv(256)
                conn.sendall(b"\x20\x02\x00\x00")
                data = b""
                conn.settimeout(5)
                while b"\xe0\x00" not in data[-2:]:
                    chunk = conn.recv(65536)
                    if not chunk:
                        break
                    data += chunk
                received.append(data)

        thread = threading.Thread(target=broker, daemon=True)
        thread.start()
        env = dict(os.environ, DRONE_SIM_USER="pilot", DRONE_SIM_PASSWORD="pw")
        port = server.getsockname()[1]
        ran = subprocess.run(["python3", str(ROOT / "tools" / "drone_osd_sim.py"), "--host", "127.0.0.1",
                              "--port", str(port), "--duration", "0.5"],
                             env=env, capture_output=True, text=True, timeout=30)
        thread.join(5)
        server.close()
        self.assertEqual(ran.returncode, 0, ran.stderr)
        self.assertTrue(received and b"thing/product/SIMAC0000A1B/osd" in received[0])
        self.assertIn(b"thing/product/SIMRC0000001/osd", received[0])


if __name__ == "__main__":
    unittest.main()
