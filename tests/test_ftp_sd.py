"""Contracts for the global SD card and the local FTP server."""
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def code_only(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def read(path):
    return code_only((ROOT / path).read_text(encoding="utf-8"))


class FtpSdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.ftp = read("src/FtpServer.cpp")
        cls.sd = read("src/SdCard.cpp")
        cls.gui = read("src/CompanionUi.cpp")
        cls.main = read("src/main.cpp")

    def test_sd_is_global_and_never_formats(self):
        self.assertIn("SD_MMC.setPins(BoardPins::kSdClk, BoardPins::kSdCmd, BoardPins::kSdD0)", self.sd)
        self.assertIn('SD_MMC.begin("/sdcard", true, false)', self.sd)
        self.assertIn("SdCard::begin();", self.main)
        self.assertNotIn("format", self.sd.lower())
        # usedBytes() scans the FAT: only in the background task.
        usage = self.sd.split("void usageTask(", 1)[1].split("namespace SdCard", 1)[0]
        self.assertIn("SD_MMC.usedBytes()", usage)
        self.assertEqual(self.sd.count("usedBytes()"), 1)
        self.assertNotIn("usedBytes()", self.gui)

    def test_ftp_is_off_until_enabled_and_home_wlan_only(self):
        self.assertIn("std::atomic<bool> wantEnabled{false};", self.ftp)
        self.assertIn("ulTaskNotifyTake(pdTRUE, portMAX_DELAY)", self.ftp)
        self.assertIn("WifiLink::connectedTo(WifiLink::Profile::Home)", self.ftp)
        self.assertIn("kIdleOffMs = 10UL * 60UL * 1000UL", self.ftp)
        self.assertIn("FtpProtocol::credentialsAcceptable(FTP_USER, FTP_PASSWORD)", self.ftp)
        self.assertIn("FtpServer::setEnabled(!FtpServer::enabled())", self.gui)

    def test_ftp_access_restrictions(self):
        self.assertIn("FtpProtocol::sameSubnet(ip, status.localIp, mask)", self.ftp)
        self.assertIn("toHostOrder(candidate.remoteIP()) == s.clientIp", self.ftp)
        self.assertIn("kMaxFailedLogins = 3", self.ftp)
        self.assertIn("kLockoutMs = 30000", self.ftp)
        self.assertIn('reply(s, "502 Nur Passivmodus.\\r\\n")', self.ftp)
        self.assertIn('if (!s.loggedIn) { reply(s, "530 Bitte anmelden.\\r\\n"); return true; }', self.ftp)
        # Every file-system path goes through the sandboxing resolver.
        self.assertNotRegex(self.ftp, r"fs\(\)\.\w+\(arg")
        self.assertEqual(self.ftp.count("FtpProtocol::resolve("), 1)
        # Neither the configured password nor a received PASS argument is logged.
        logs = re.findall(r"Serial\.print\w*\(([^;]*)\);", self.ftp)
        self.assertTrue(logs)
        for call in logs:
            self.assertNotIn("FTP_PASSWORD", call)
        pass_branch = self.ftp.split('if (is("PASS")) {', 1)[1].split('if (is("NOOP"))', 1)[0]
        for call in re.findall(r"Serial\.print\w*\(([^;]*)\);", pass_branch):
            self.assertNotIn("arg", call)

    def test_local_credentials_are_ignored(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8").splitlines()
        self.assertIn("include/FtpLocalConfig.h", ignore)
        example = (ROOT / "include" / "FtpLocalConfig.example.h").read_text(encoding="utf-8")
        self.assertIn('#define FTP_PASSWORD "YOUR_FTP_PASSWORD_MIN_8_CHARS"', example)

    def test_native_protocol(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("Native C++ compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="fnk0086-ftp-") as tmp:
            binary = Path(tmp) / "ftp_smoke"
            cmd = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
                   "-I" + str(ROOT / "include"), str(ROOT / "tests" / "ftp_smoke.cpp"),
                   str(ROOT / "src" / "FtpProtocol.cpp"), "-o", str(binary)]
            compiled = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            ran = subprocess.run([str(binary)], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == "__main__":
    unittest.main()
