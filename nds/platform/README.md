# Native DS platform

The game executes native ARM9 and ARM7 code. It does not run HTML, a browser,
or an interpreter. The ARM9 owns gameplay and two 256 × 192 RGB555 render
surfaces. The ARM7 supplies calibrated system input and 8-bit PCM sound over
libnds FIFO channels. Gameplay uses buttons only.

Run `bash tools/setup_nds.sh` from `nds/`, then `make -j2`. Installation is
local to `/workspace/nds-tools`, without administrator privileges. The setup
script obtains GCC/binutils/newlib through authenticated Debian APT indexes and
pins the upstream source revisions for libnds, devkitARM startup files, and
ndstool. It can reuse the earlier local ARM compiler.

The platform API is in `platform.h`. Pixel bit 15 must be set for visible
RGB555 bitmap pixels. `ds_vblank` synchronizes to the real LCD scanline counter.
The main screen renders directly into the inactive A/B VRAM page; `ds_present`
flips that page at vblank. The lower-screen surface stays in main RAM and is
transferred with DMA only when its contents change.

Save data uses two-address-byte cartridge EEPROM, up to 8192 bytes. Writes are
page aligned, have bounded busy polling, and are verified by reading them back.
No DLDI disk image or host filesystem is required. The ROM's original `KAWE`
identifier and ARM9 ROM offset `0x4000` deliberately select the standard
cartridge path in melonDS, whose default for an unknown cartridge is 64 Kbit
EEPROM. The DeSmuME direct-boot tests exercise button input, sound, and saves;
testing on an actual iPhone running Delta remains a separate device check.

This cartridge classification was also checked against Delta's own pinned
[melonDS 0.9.5 core](https://github.com/rileytestut/melonDS/blob/a53f9bab7b5c06efcbc6ecdf30ac0367555995aa/src/NDSCart.cpp).
The [Delta bridge](https://github.com/rileytestut/MelonDSDeltaCore/blob/c212a2d74dea32e780f41d3044eeb12bc4e18dab/MelonDSDeltaCore/Bridge/MelonDSEmulatorBridge.mm)
forwards cartridge EEPROM changes to its normal save mechanism. No existing
commercial game's identifier is reused.

Upstream components retain their original licenses:

- libnds: [zlib-style license](https://github.com/devkitPro/libnds/blob/v1.8.3/libnds_license.txt).
- devkitARM startup/linker sources: [MPL 2.0 notices](https://github.com/devkitPro/devkitarm-crtls).
- ndstool: [upstream project](https://github.com/devkitPro/ndstool).
- GCC, binutils, newlib: upstream licenses included in their Debian packages.

Generated dependencies and downloaded toolchains are kept outside the source
repository. `platform/arm7.c`, `platform/runtime.c`, and the game-facing
`platform/platform.c` are game-specific integration code; the adapted libnds
initialization retains its original attribution and license notice.
