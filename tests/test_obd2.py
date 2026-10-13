"""Contracts for the read-only OBD2 (ELM327 Wi-Fi/TCP) module."""
import importlib.util
import re
import shutil
import socket
import subprocess
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code_only(text):
    """Strip // and /* */ comments so documentation cannot satisfy or fail a contract."""
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def load_sim():
    spec = importlib.util.spec_from_file_location("elm327_lab_sim", ROOT / "tools" / "elm327_lab_sim.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Obd2Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.client = code_only((ROOT / "src" / "Obd2Client.cpp").read_text(encoding="utf-8"))
        cls.policy = code_only((ROOT / "include" / "Obd2Policy.h").read_text(encoding="utf-8"))
        cls.gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")

    def test_wire_strings_only_in_policy(self):
        wires = re.findall(r'return "([^"]+)";', self.policy)
        self.assertEqual(sorted(wires), sorted([
            "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0",
            "0100", "0120", "0140", "0104", "0105", "010C", "010D", "010F", "0111", "0142"]))
        for forbidden in ('"04"', '"03"', '"07"', '"0A"', "ATSH", "ATMA", "ATCRA", '"08'):
            self.assertNotIn(forbidden, self.policy)
        # The client transmits exactly the policy string plus CR, nothing else.
        self.assertEqual(re.findall(r"client\.(?:print|write|println)\(([^)]*)\)", self.client),
                         ["wire", "'\\r'"])
        self.assertIn("const char* wire = Obd2Policy::wireCommand(command);", self.client)
        self.assertNotRegex(self.client, r'"(?:AT[A-Z0-9]+|0[0-9A-F]{1,5})"')

    def test_session_only_while_page_open(self):
        poll = self.gui.split("void poll()", 1)[1]
        self.assertIn("if (showing(CompanionModules::Id::Obd2)) Obd2Status::requestActive(false);", poll)
        self.assertIn("if (showing(CompanionModules::Id::Obd2)) Obd2Status::requestActive(true);", poll)
        self.assertLess(poll.index("Obd2Status::requestActive(false)"),
                        poll.index("shownPage = requestedPage"))
        self.assertIn("ulTaskNotifyTake(pdTRUE, portMAX_DELAY)", self.client)
        self.assertIn("if (!desiredActive.load()) return Result::Aborted;", self.client)
        self.assertIn("kReplyCapacity = 512", self.client)
        self.assertNotIn("WiFi.begin(", self.client)
        self.assertNotIn("Obd2Response", self.gui)

    def test_local_config_is_ignored(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("include/Obd2LocalConfig.h", ignore)
        self.assertTrue((ROOT / "include" / "Obd2LocalConfig.example.h").exists())

    def test_native_parser_policy_formatter(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-obd2-") as tmp:
            binary = Path(tmp) / "obd2_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "include"), str(ROOT / "tests" / "obd2_smoke.cpp"),
                   str(ROOT / "src" / "Obd2Display.cpp"), "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_lab_simulator_answers_allowlist_and_flags_others(self):
        sim = load_sim()
        server = sim.make_server("127.0.0.1", 0, search_delay=0, reply_delay=0)
        threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            with socket.create_connection(server.server_address, timeout=5) as conn:
                def ask(cmd):
                    conn.sendall(cmd.encode() + b"\r")
                    data = b""
                    while not data.endswith(b">"):
                        data += conn.recv(256)
                    return data.decode()
                self.assertIn("ELM327", ask("ATZ"))
                self.assertIn("OK", ask("ATE0"))
                self.assertIn("SEARCHING...", ask("0100"))
                self.assertIn("41 00 18 1A 80 01", ask("0100"))
                self.assertRegex(ask("010C"), r"41 0C [0-9A-F]{2} [0-9A-F]{2}")
                self.assertIn("41 42 34 BC", ask("0142"))
                self.assertEqual(server.violations, 0)
                self.assertIn("?", ask("04"))
                self.assertEqual(server.violations, 1)
        finally:
            server.shutdown()
            server.server_close()


if __name__ == "__main__":
    unittest.main()
