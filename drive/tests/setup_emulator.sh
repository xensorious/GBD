#!/usr/bin/env bash
# Install the QA core locally. This does not change system packages.
set -euo pipefail
qa_root="${NDS_QA_TOOLS_ROOT:-/workspace/nds-qa-tools}"
mkdir -p "$qa_root/archives" "$qa_root/lists/partial" "$qa_root/sysroot"
cat > "$qa_root/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
apt_args=(-o Debug::NoLocking=1 -o "Dir::Etc::sourcelist=$qa_root/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$qa_root/lists")
apt-get "${apt_args[@]}" update
cd "$qa_root/archives"
# Download the two actual dependencies, not the RetroArch frontend/font bundle.
apt-get "${apt_args[@]}" download libretro-desmume libtinyxml2.6.2v5
for package in libretro-desmume_*.deb libtinyxml2.6.2v5_*.deb; do
    dpkg-deb -x "$package" "$qa_root/sysroot"
done
python3 -c 'from PIL import Image; print("Headless Nintendo DS emulator and screenshot support ready")'
