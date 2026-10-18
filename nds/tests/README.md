# Validation

`make test` builds the cartridge and runs `python3 tests/playthrough.py`.

The test suite first compiles the portable rules with AddressSanitizer and
UndefinedBehaviorSanitizer. It checks all three starters, recruitment by trust
and by lowering health, shared experience, both growth choices, the optional
blossom quest, battle switching, defeat recovery, notebook controls, save
round-tripping, and corruption rejection. A second native test follows real
`game_update` button movement to all 14 world interactions.

The integration run executes the actual ARM9/ARM7 cartridge in DeSmuME. It uses
only joypad input to play the opening through the keeper trial, opens every
notebook tab, completes the optional quest, saves, and resumes in a separate
emulator process. Game RAM is observed read-only. There are no cheats, state
writes, test hooks, touchscreen input, or emulator state restores in the run.

The renderer check walks to an accessible canopy and compares displayed pixels:
part of the hero must remain visible while the tree hides the overlapping
pixels. The suite also checks the cartridge header, audio output, and a minimum
field/title update rate. Screenshots and a JSON report are written to
`build/validation/`.

After a presentation-only change, `python3 tests/visual_smoke.py` uses the
completed suite's battery save as a compatibility fixture. It also starts a
fresh journey, checks that a visible blossom disappears when collected,
reaches the revised ending through a real trial, and checks frame rate and
audio. Its final ROM hash and results are kept separately in
`build/validation/final-visual-results.json` so they cannot be confused with
the full suite's baseline artifact.

The locally extracted Debian emulator can be installed with:

```sh
bash tests/setup_emulator.sh
```

It uses `/workspace/nds-qa-tools` by default. Set `NDS_LIBRETRO_CORE` and
`NDS_LIBRETRO_XML` to use another compatible core and dependency location.
Pillow and a native C compiler are also required.

The Debian 2016 DeSmuME core has an ARM7 IPC regression in JIT mode; the test
frontend uses its interpreter, which correctly runs audio and input. Tests use
the core's homebrew direct boot and require no Nintendo BIOS files.

These checks verify the actual Nintendo DS ROM in DeSmuME. They do not claim a
physical iPhone/Delta test. Cartridge EEPROM behavior in Delta's melonDS core
is documented separately in the platform notes.
