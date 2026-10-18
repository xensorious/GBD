#!/usr/bin/env bash
# Debian cloud setup. Packages are authenticated by APT and extracted locally.
set -euo pipefail
gba_tools_dir="${GBA_TOOLS_ROOT:-/workspace/gba-tools}"
mkdir -p "$gba_tools_dir/apt/lists/partial" "$gba_tools_dir/apt/archives/partial" "$gba_tools_dir/sysroot"
cat > "$gba_tools_dir/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
apt_args=(-o Debug::NoLocking=1 -o "Dir::Etc::sourcelist=$gba_tools_dir/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$gba_tools_dir/apt/lists" -o "Dir::Cache::archives=$gba_tools_dir/apt/archives" -o APT::Install-Recommends=false)
apt-get "${apt_args[@]}" update
apt-get "${apt_args[@]}" --download-only -y install gcc-arm-none-eabi binutils-arm-none-eabi libmgba-dev
for gba_package in "$gba_tools_dir"/apt/archives/*.deb; do
    dpkg-deb -x "$gba_package" "$gba_tools_dir/sysroot"
done
"$gba_tools_dir/sysroot/usr/bin/arm-none-eabi-gcc" --version
python3 -c 'from PIL import Image; print("Pillow available")'
