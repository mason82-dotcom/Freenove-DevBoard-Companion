"""Host-side C++ constexpr checks for the exact FNK0086 landscape mapping."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = r'''
#include "TouchTransform.h"
using TouchTransform::rawToLandscape;
constexpr auto topLeft = rawToLandscape(234, 27);
static_assert(topLeft.x == 27 && topLeft.y == 5, "top left");
constexpr auto topRight = rawToLandscape(226, 318);
static_assert(topRight.x == 318 && topRight.y == 13, "top right");
constexpr auto bottomLeft = rawToLandscape(0, 46);
static_assert(bottomLeft.x == 46 && bottomLeft.y == 239, "bottom left");
constexpr auto bottomRight = rawToLandscape(0, 287);
static_assert(bottomRight.x == 287 && bottomRight.y == 239, "bottom right");
constexpr auto limit0 = rawToLandscape(0, 0);
static_assert(limit0.x == 0 && limit0.y == 239, "origin");
constexpr auto limit1 = rawToLandscape(239, 319);
static_assert(limit1.x == 319 && limit1.y == 0, "opposite corner");
constexpr auto outOfBounds = rawToLandscape(500, 900);
static_assert(outOfBounds.x == 319 && outOfBounds.y == 0, "clamp bounds");
using TouchTransform::isNear;
static_assert(isNear(topLeft, {24, 24}), "physical top-left target");
static_assert(isNear(topRight, {296, 24}), "physical top-right target");
static_assert(isNear(bottomLeft, {24, 216}), "physical bottom-left target");
static_assert(isNear(bottomRight, {296, 216}), "physical bottom-right target");
static_assert(isNear({160, 120}, {160, 120}), "centre target");
static_assert(isNear({100, 100}, {140, 100}, 40), "inclusive tolerance");
static_assert(!isNear({100, 100}, {141, 100}, 40), "reject misses");
static_assert(!isNear({0, 0}, {319, 239}), "reject distant point");
int main() { return 0; }
'''


class LandscapeTouchTests(unittest.TestCase):
    def test_cpp_transform_compiles_and_matches_physical_measurements(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("No host C++ compiler installed")
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "touch_test.cpp"
            source.write_text(CPP, encoding="utf-8")
            result = subprocess.run(
                [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                 "-pedantic", "-fsyntax-only", "-I", str(ROOT / "include"),
                 str(source)],
                capture_output=True, text=True, timeout=30, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
