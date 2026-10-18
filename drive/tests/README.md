# Coastline cartridge validation

`python3 drive/tests/playthrough.py --native-only` builds the portable rules with
GCC AddressSanitizer and UndefinedBehaviorSanitizer and exercises road continuity,
vehicle controls, awards, receipts, progression and save integrity.

`python3 drive/tests/playthrough.py` additionally boots the compiled ARM9/ARM7
cartridge in the DeSmuME libretro interpreter. All navigation and driving use
joypad callbacks. The only pointer input asserts that touch is ignored. RAM is
observed read only through the cartridge's ELF symbols. No RAM writes, cheats,
emulator checkpoint restores or simulated screenshots are used. The runner
captures real dual-screen output and a top-screen motion GIF under
`drive/build/validation/`, ends a run through collisions, checks the itemized
receipt, and reloads the resulting battery save on a fresh cartridge boot.

The interpreter is intentional: Debian's old DeSmuME core has an ARM7 FIFO/JIT
regression. `setup_emulator.sh` installs the local test core without system
package changes. No copyrighted BIOS or firmware files are required for this
homebrew direct boot.
