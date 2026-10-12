#!/usr/bin/env python3
"""Install exactly the verified Freenove FNK0086 CFG1 TFT_eSPI setup.

No profile switch, autodetection, or fallback to CFG2 is supported.
"""
import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "include" / "VerifiedDisplaySetup.h"
CFG1_NAME = "FNK0086A_2P8_CFG1_240x320_ST7789.h"
OTHER_SETUP = re.compile(r"FNK0086A_2P8_CFG\d+_240x320_ST7789\.h$", re.I)
INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')


def resolve_profile(source: Path) -> Path:
    source = source.expanduser().resolve(strict=True)
    if source.name == CFG1_NAME:
        return source
    if OTHER_SETUP.fullmatch(source.name):
        raise ValueError("Only CFG1 is permitted for this board")
    if source.name != "User_Setup_Select.h":
        raise ValueError("Source must be the CFG1 header or active User_Setup_Select.h")
    active = []
    content = re.sub(r"/\*.*?\*/", "", source.read_text(encoding="utf-8", errors="replace"), flags=re.S)
    for line in content.splitlines():
        if line.lstrip().startswith("//"):
            continue
        m = INCLUDE.match(line)
        if not m:
            continue
        name = Path(m.group(1)).name
        if OTHER_SETUP.fullmatch(name):
            active.append((name, source.parent / m.group(1)))
    if len(active) != 1:
        raise ValueError(f"Expected exactly one active FNK0086 setup, found {len(active)}")
    name, path = active[0]
    if name != CFG1_NAME:
        raise ValueError("Only CFG1 is permitted; active vendor setup is not CFG1")
    return path.resolve(strict=True)


def import_setup(source: Path, destination: Path = OUT) -> Path:
    selected = resolve_profile(source)
    content = selected.read_text(encoding="utf-8", errors="replace")
    required = [
        ("ST7789_DRIVER", None),
        ("TFT_WIDTH", "240"),
        ("TFT_HEIGHT", "320"),
    ]
    for macro, value in required:
        expr = rf"^\s*#\s*define\s+{macro}\b" + (rf"\s+{value}\b" if value else "")
        if not re.search(expr, content, re.M):
            raise ValueError(f"CFG1 header missing required {macro}" + (f"={value}" if value else ""))
    header = (
        "// ONLY FNK0086 CFG1; imported from vendor setup.\n"
        f"// Source: {CFG1_NAME}\n"
        "// Generated locally; never commit to source control.\n"
    )
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(header + content, encoding="utf-8")
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Known-good CFG1 header or User_Setup_Select.h")
    args = parser.parse_args()
    output = import_setup(args.source)
    print(f"Verified FNK0086 CFG1 setup installed at {output}")
    print("No profile selection and no automatic firmware upload.")


if __name__ == "__main__":
    main()
