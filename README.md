# Bramblebound: First Light

An original, playable Game Boy Advance creature-catching RPG opening. Choose a partner, explore Mossbell and Fernway, recruit wild friends, choose a growth path, help a garden, and challenge your first rival.

The release is a standalone `.gba` ROM: no existing game or patch is required.

This edition redraws the world, characters, creatures, battle scene and menus.
Tall scenery overlaps characters according to where they stand, giving the
overhead view visible walls, tree canopies and foreground depth. Gameplay,
collision maps, controls and battery-save format match the original prototype.

- [Play and controls](docs/PLAY.md)
- [Design, research decisions, and first-draft boundaries](docs/DESIGN.md)
- Release files: `dist/Bramblebound.gba` and `dist/Bramblebound-First-Light.zip`

## Build

Requires Python 3 with Pillow, GNU Make, and an ARM bare-metal GCC toolchain with libgcc. It does not require newlib or a proprietary SDK.

```sh
python3 -m pip install -r requirements-dev.txt
make TOOLCHAIN=/path/to/arm-none-eabi-
```

For example, a devkitARM installation can use `TOOLCHAIN=/opt/devkitpro/devkitARM/bin/arm-none-eabi-`. In the prepared cloud environment, plain `make` uses the locally extracted toolchain under `/workspace/gba-tools`.

To recreate the Debian cloud tools without installing system packages:

```sh
bash tools/setup.sh
make
```

APT verifies signed Debian repository metadata and package hashes. The setup script downloads and extracts GCC, binutils, and mGBA under `/workspace/gba-tools`; it does not need root or disable signature/TLS verification. A native C compiler is required for the emulator tests. Pillow is already available in the prepared instance.

## Validation

```sh
make test
python3 tests/visual_validation.py
```

The tests use the real mGBA core, press game buttons, and observe game state through ELF symbols. They never write game RAM. They cover all three starter playthroughs, calm/half-health recruitment, duplicate protection, development, garden help, safe loss recovery, the rival endpoint, non-silent audio output, SRAM restoration after a fresh boot, and recovery from a corrupted primary save. Test saves and screenshots stay under ignored `build/`.

Visual validation also checks the original gameplay/data fingerprints and collision
maps, captures the redesigned screens, and verifies actual foreground/background
overlap pixels while walking around a house. Use `--old-saves /path/to/fixtures`
with the original `test-battery.sav` and `test-backup.sav` to check upgrade loading.

Run `python3 tools/package.py` after a successful build/test to assemble the ROM, instructions, preview, and checksums in `dist/`.

## Source map

- `src/game.c`: exploration, creatures, combat, dialogue, saves, and PSG audio.
- `src/world_render.inc`, `src/ui_render.inc`: world depth, scene rendering and UI.
- `src/start.s`, `src/gba.ld`: GBA startup and memory layout.
- `tools/assets.py`, `tools/art_overhaul.py`: original pixel artwork, font, and scenery.
- `tools/fix_rom.py`: cartridge header and size validation.
- `tests/`: headless mGBA playthrough harness.

This first draft ends at the first Trail Pin. See the design notes for deliberately unimplemented full-game features. No claim of physical-hardware testing is made.
