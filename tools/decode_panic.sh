#!/usr/bin/env bash
# Symbolicate the original FNK0086 GUI crash against the exact built ELF.
# Does not build, flash, edit, or connect to hardware.
set -euo pipefail

ELF="${COMPANION_ELF:-.pio/build/fnk0086/firmware.elf}"
if [[ $# -gt 0 && "$1" == *.elf ]]; then
    ELF="$1"
    shift
fi
if [[ ! -f "$ELF" ]]; then
    printf 'ERROR: firmware ELF not found: %s\n' "$ELF" >&2
    exit 2
fi

TOOL=""
if command -v xtensa-esp32s3-elf-addr2line >/dev/null 2>&1; then
    TOOL="$(command -v xtensa-esp32s3-elf-addr2line)"
else
    for root in "$HOME/.platformio" "$PWD/.pio"; do
        if [[ -d "$root" ]]; then
            TOOL="$(find "$root" -type f -name xtensa-esp32s3-elf-addr2line -print -quit 2>/dev/null || true)"
            if [[ -n "$TOOL" ]]; then break; fi
        fi
    done
fi
if [[ -z "$TOOL" ]]; then
    printf 'ERROR: xtensa-esp32s3-elf-addr2line missing from PATH/PlatformIO\n' >&2
    exit 3
fi

printf 'ELF: %s\nTool: %s\n' "$ELF" "$TOOL"
sha256sum "$ELF" || true

if (( $# > 0 )); then
    addresses=("$@")
else
    # First reported 2026-10-12 LVGL startup panic. Corrupted frames may show ??.
    addresses=(
        0x40378c0e 0x4205374f 0x4205374c 0x420537c4
        0x40375e72 0x40375f0e 0x42032243 0x42031f00
        0x42031f1d 0x42053a9f 0x42031eb5 0x40375e01
        0x403760a8 0x4038576b
    )
fi
"$TOOL" -a -f -C -i -p -e "$ELF" "${addresses[@]}"
