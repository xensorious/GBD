#!/usr/bin/env bash
# Local, non-root DS toolchain setup. Debian packages are verified by APT.
set -euo pipefail
nds_project_dir="$(cd "$(dirname "$0")/.." && pwd)"
nds_tools_dir="${NDS_TOOLS_ROOT:-/workspace/nds-tools}"
mkdir -p "$nds_tools_dir/src" "$nds_tools_dir/sysroot"
nds_cc="$nds_tools_dir/sysroot/usr/bin/arm-none-eabi-gcc"
if [[ ! -x "$nds_cc" && -x /workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-gcc ]]; then
    nds_cc=/workspace/gba-tools/sysroot/usr/bin/arm-none-eabi-gcc
fi
if [[ ! -x "$nds_cc" || ! -f "$nds_tools_dir/sysroot/usr/lib/arm-none-eabi/newlib/libc.a" ]]; then
    mkdir -p "$nds_tools_dir/apt/lists/partial" "$nds_tools_dir/apt/archives/partial"
    cat > "$nds_tools_dir/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
    nds_apt_args=(-o Debug::NoLocking=1 -o "Dir::Etc::sourcelist=$nds_tools_dir/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$nds_tools_dir/apt/lists" -o "Dir::Cache::archives=$nds_tools_dir/apt/archives" -o APT::Install-Recommends=false)
    apt-get "${nds_apt_args[@]}" update
    apt-get "${nds_apt_args[@]}" --download-only -y install gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-dev libnewlib-arm-none-eabi
    for nds_package in "$nds_tools_dir"/apt/archives/*.deb; do
        dpkg-deb -x "$nds_package" "$nds_tools_dir/sysroot"
    done
    nds_cc="$nds_tools_dir/sysroot/usr/bin/arm-none-eabi-gcc"
fi

nds_fetch_source() {
    local nds_source_name="$1" nds_source_url="$2" nds_source_commit="$3"
    local nds_source_dir="$nds_tools_dir/src/$nds_source_name"
    if [[ ! -d "$nds_source_dir/.git" ]]; then
        git init -q "$nds_source_dir"
        git -C "$nds_source_dir" remote add origin "$nds_source_url"
    fi
    if ! git -C "$nds_source_dir" cat-file -e "$nds_source_commit^{commit}" 2>/dev/null; then
        git -C "$nds_source_dir" fetch -q --depth 1 origin "$nds_source_commit"
    fi
    git -C "$nds_source_dir" checkout -q --detach "$nds_source_commit"
}
nds_fetch_source libnds https://github.com/devkitPro/libnds.git 04ff07cd2ac0f3f0c13adc7d54fd2c5b850341d8
nds_fetch_source devkitarm-crtls https://github.com/devkitPro/devkitarm-crtls.git 1c0c10257c44bbb5a433453bb6bba91582825492
nds_fetch_source ndstool https://github.com/devkitPro/ndstool.git 76e8b681bb225d945a48852821e03114e6c7ce1c
NDS_CC="$nds_cc" python3 "$nds_project_dir/platform/build_sdk.py" "$nds_tools_dir"
printf '%s\n' "$nds_cc" > "$nds_tools_dir/compiler.path"
"$nds_cc" --version | head -1
printf 'DS SDK ready in %s\n' "$nds_tools_dir"
