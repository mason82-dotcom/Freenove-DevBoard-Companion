"""Contracts for the read-only Hermes HTTPS status module."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class HermesStatusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.worker = (ROOT / "src" / "HermesStatus.cpp").read_text(encoding="utf-8")
        cls.gui = (ROOT / "src" / "CompanionUi.cpp").read_text(encoding="utf-8")

    def test_local_credentials_are_never_tracked(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("include/HermesLocalConfig.h", ignore)
        self.assertTrue((ROOT / "include" / "HermesLocalConfig.example.h").exists())
        self.assertIn('__has_include("HermesLocalConfig.h")', self.worker)

    def test_tls_is_verified_and_bounded(self):
        for expected in ("client->setCACert(HERMES_CA_PEM)",
                         "client->setHandshakeTimeout(kHandshakeTimeoutS)",
                         "constexpr unsigned long kHandshakeTimeoutS = 8",
                         "http->setConnectTimeout(kConnectTimeoutMs)",
                         "http->setTimeout(kReadTimeoutMs)",
                         "http->setReuse(false)",
                         "HTTPC_DISABLE_FOLLOW_REDIRECTS",
                         "constexpr size_t kMaxBody = 16384",
                         "NestingLimit(6)"):
            with self.subTest(symbol=expected):
                self.assertIn(expected, self.worker)
        for forbidden in ("setInsecure(", "client->setTimeout(", "http->POST(",
                          "http->PUT(", "WiFi.reconnect("):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, self.worker)

    def test_tls_memory_is_routed_to_psram_at_boot(self):
        # Hermes failed with -32512 whenever an HTTPS radio stream held a
        # second TLS session in internal RAM.
        tls = (ROOT / "src" / "TlsMemory.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("mbedtls_platform_set_calloc_free(tlsCalloc, free)", tls)
        self.assertIn("MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT", tls)
        self.assertIn("MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT", tls)
        setup = main.split("void setup()", 1)[1]
        self.assertLess(setup.index("TlsMemory::begin()"), setup.index("CompanionUi::begin("))

    def test_silent_close_only_after_complete_body(self):
        self.assertIn("bodyComplete = true;", self.worker)
        self.assertEqual(self.worker.count("client->stop()"), 1)
        self.assertIn("if (bodyComplete) client->stop();", self.worker)
        self.assertLess(self.worker.index("if (bodyComplete) client->stop();"),
                        self.worker.index("http->end();"))
        self.assertLess(self.worker.index("bodyComplete = true;"),
                        self.worker.index("deserializeJson("))

    def test_last_valid_values_survive_failures(self):
        self.assertIn("HermesStatus::Snapshot state;", self.worker)
        self.assertIn("poll(state, quietPolls)", self.worker)
        self.assertIn("if (fetch(s, endMs)) s.failures = 0;", self.worker)
        self.assertIn("HermesStatus::format(HermesStatus::read(), millis(), text, sizeof(text))",
                      self.gui)

    def test_native_formatter(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-hermes-") as tmp:
            binary = Path(tmp) / "hermes_format_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "include"),
                   str(ROOT / "tests" / "hermes_format_smoke.cpp"),
                   str(ROOT / "src" / "HermesDisplay.cpp"), "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
