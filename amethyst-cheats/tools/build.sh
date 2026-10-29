#!/usr/bin/env sh
# Requires Zig 0.13.0 and Python 3. Run from any working directory.
# Usage: ZIG=/path/to/zig tools/build.sh /path/to/original-amethyst.gba
set -eu
if [ "$#" -ne 1 ]; then
    echo 'Usage: tools/build.sh /path/to/original-amethyst.gba' >&2
    exit 2
fi
rom_path=$1
case "$rom_path" in
    /*) ;;
    *) rom_path=$PWD/$rom_path ;;
esac
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build
"${ZIG:-zig}" cc -target thumb-freestanding-eabi -mcpu=arm7tdmi -Oz \
    -fno-lto -ffreestanding -fno-builtin -fno-stack-protector -nostdlib \
    -Wl,-T,native/runtime.ld -Wl,-e,read_keys_entry -Wl,--gc-sections \
    native/runtime.c -o build/runtime.elf
python3 tools/build_release.py "$rom_path"
