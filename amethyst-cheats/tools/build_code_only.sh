#!/usr/bin/env sh
# Zig 0.13.0 and Python3; outputs only Delta cheat text and its manifest.
set -eu
if [ "$#" -ne 1 ]; then
    echo 'Usage: tools/build_code_only.sh /path/to/original-amethyst.gba' >&2
    exit 2
fi
rom_path=$1
case "$rom_path" in /*) ;; *) rom_path=$PWD/$rom_path ;; esac
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build
"${ZIG:-zig}" cc -target thumb-freestanding-eabi -mcpu=arm7tdmi -Oz \
    -fno-lto -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
    -Wl,-T,native/code_only.ld -Wl,-e,read_keys_entry -Wl,--gc-sections \
    native/code_only.c -o build/code_only.elf
python3 tools/build_code_only.py "$rom_path"
