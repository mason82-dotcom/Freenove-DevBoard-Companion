import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("imp_profile", ROOT / "tools" / "import_verified_display_setup.py")
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
HEADER = "\n".join([
    "#define ST7789_DRIVER", "#define TFT_WIDTH 240",
    "#define TFT_HEIGHT 320", "#define TFT_MOSI 20", "#define TFT_SCLK 21",
    "#define TFT_CS -1", "#define TFT_DC 0",
    "#define TFT_RGB_ORDER TFT_BGR", "#define TFT_INVERSION_OFF",
])


class FixedCfg1Tests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "User_Setups").mkdir()
        self.cfg1 = self.root / "User_Setups" / mod.CFG1_NAME
        self.cfg1.write_text(HEADER, encoding="utf-8")
        self.other = self.root / "User_Setups" / "FNK0086A_2P8_CFG2_240x320_ST7789.h"
        self.other.write_text(HEADER, encoding="utf-8")
        self.out = self.root / "VerifiedDisplaySetup.h"

    def test_cfg1_header_is_accepted(self):
        mod.import_setup(self.cfg1, self.out)
        self.assertIn("ONLY FNK0086 CFG1", self.out.read_text())
        self.assertTrue(self.out.read_text().endswith(HEADER))

    def test_active_cfg1_select_is_accepted(self):
        select = self.root / "User_Setup_Select.h"
        select.write_text('// #include <User_Setups/FNK0086A_2P8_CFG2_240x320_ST7789.h>\n'
                          '#include <User_Setups/FNK0086A_2P8_CFG1_240x320_ST7789.h>\n')
        mod.import_setup(select, self.out)
        self.assertIn("CFG1", self.out.read_text())

    def test_vendor_cfg1_filename_and_macro_select_are_accepted(self):
        vendor = self.root / "User_Setups" / mod.VENDOR_CFG1_NAME
        vendor.write_text(HEADER, encoding="utf-8")
        select = self.root / "User_Setups" / "User_Setup_Select.h"
        select.write_text(
            "#define FNK0086A_2P8_CFG1_240x320_ST7789\n"
            "//#define FNK0086A_2P8_CFG2_240x320_ST7789\n"
            "#ifdef FNK0086A_2P8_CFG1_240x320_ST7789\n"
            "#include <FNK0086A_2.8_CFG1_240x320_ST7789.h>\n"
            "#elif defined FNK0086A_2P8_CFG2_240x320_ST7789\n"
            "#include <FNK0086A_2.8_CFG2_240x320_ST7789.h>\n"
        )
        mod.import_setup(select, self.out)
        self.assertIn("Source: " + mod.VENDOR_CFG1_NAME, self.out.read_text())

    def test_vendor_cfg2_filename_is_rejected(self):
        other = self.root / "User_Setups" / "FNK0086A_2.8_CFG2_240x320_ST7789.h"
        other.write_text(HEADER, encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Only CFG1"):
            mod.import_setup(other, self.out)

    def test_changed_cfg1_pin_is_rejected(self):
        self.cfg1.write_text(HEADER.replace("TFT_MOSI 20", "TFT_MOSI 15"), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "TFT_MOSI"):
            mod.import_setup(self.cfg1, self.out)

    def test_other_profile_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "Only CFG1"):
            mod.import_setup(self.other, self.out)

    def test_other_active_profile_is_rejected(self):
        select = self.root / "User_Setup_Select.h"
        select.write_text('#include <User_Setups/FNK0086A_2P8_CFG2_240x320_ST7789.h>\n')
        with self.assertRaisesRegex(ValueError, "Only CFG1"):
            mod.import_setup(select, self.out)

    def test_multiple_active_profiles_are_rejected(self):
        select = self.root / "User_Setup_Select.h"
        select.write_text('#include <User_Setups/FNK0086A_2P8_CFG1_240x320_ST7789.h>\n'
                          '#include <User_Setups/FNK0086A_2P8_CFG2_240x320_ST7789.h>\n')
        with self.assertRaisesRegex(ValueError, "exactly one"):
            mod.import_setup(select, self.out)

    def test_missing_driver_rejected(self):
        self.cfg1.write_text(HEADER.replace("#define ST7789_DRIVER", "// #define ST7789_DRIVER"))
        with self.assertRaisesRegex(ValueError, "ST7789_DRIVER"):
            mod.import_setup(self.cfg1, self.out)


if __name__ == "__main__":
    unittest.main()
