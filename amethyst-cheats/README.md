# Amethyst button cheats for Delta

An experimental button-sequence system for **Pokémon Amethyst v1.4.0**: one-shot items, walking through obstacles, shiny generation, and player-side battle protection.

**Install the BPS patch first. The four-line Delta code alone does nothing to the original ROM.** The handler needs permanent ROM storage because the game resets its heap during normal play. No game or save files are distributed.

## Install on iPhone

1. Export a backup of your current in-game save from Delta.
2. Download [the BPS patch](dist/amethyst-v1.4.0-button-cheats.bps). Apply it to your original Amethyst v1.4.0 `.gba` with a BPS patcher such as [ROM Patcher JS](https://www.marcrobledo.com/RomPatcher.js/). It produces a new `.gba`. A checksum mismatch means a different ROM; do not force the patch.
3. Import the patched game into Delta. Import your exported in-game save for it and start with **Continue**. Do not load a save state made with the original ROM: it has the old heap layout.
4. Add one cheat named `Amethyst button cheats`, choose **Action Replay**, paste all four lines, save it, and leave it enabled.

```text
5D7E1F8B 453ED2BE
79B40C0C E894258B
47A4EA49 35AB8BC4
1E27D83B BBFB6923
```

Also available as [delta-action-replay.txt](dist/delta-action-replay.txt). A new game works too; items become available once its inventory is initialized.

## Button sequences

Begin every sequence with:

**L, R, L, R, Left, Right, Up, Down**

Then enter the ending:

| Ending | Effect |
| --- | --- |
| A, A | Add exactly 1 Master Ball |
| B, B | Add exactly 1 Max Revive |
| A, B | Add exactly 1 Rare Candy |
| B, A | Toggle walking through obstacles |
| L, L | Toggle 100% shiny generation |
| R, R | Toggle player Pokémon battle protection |

L/R are shoulder buttons; Left/Right are directions. Tap one button at a time and release it before the next tap, including repeated buttons. Tap deliberately, leaving less than about two seconds between taps at normal speed. Avoid entering a sequence during loading or transitions. Holding the last button does not repeat an item grant. Each full item sequence requests quantity one; a full bag can reject it.

Repeat a toggle sequence to turn its effect off, then again to turn it on. Walking and protection start off. Shiny starts from the game's current shiny option and affects newly generated Pokémon; existing Pokémon stay unchanged. Buttons still reach the game, so use a safe position. There is no visual toggle indicator.

## Tested scope and limits

The exact game ran in headless mGBA with the activation code processed by the actual VBA-M cheat implementation used by the examined Delta core. Tests covered real items, shiny creation, a battle, bedroom obstacles, outdoor trees, a house roof, and map transitions. Another 243 compiled ARM checks cover input, heap boundaries, and battle paths. **The iPhone Delta app itself has not been tested.** See [VALIDATION.md](VALIDATION.md).

Walking bypasses player collision. It does not create tiles beyond map boundaries or remove scripts and warps. Water and mountains have not been tested directly. Return to an ordinary accessible tile before turning it off. Battle protection covers verified HP-loss and fainting paths; unusual scripted outcomes remain unverified.

Disabling the Delta code turns walking/protection off and restores the shiny option recorded before the first shiny toggle. Walking/protection are session state. Shiny uses a game flag that can be saved: turn it off before saving if the next session should start with normal rates. Other game options can also affect those rates. Save states made after patching retain their recorded state.

This build accepts only the original ROM with SHA-256:

```text
89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430
```

## Source and checks

- [native/runtime.c](native/runtime.c): actual ARM/Thumb runtime.
- [tools/build_release.py](tools/build_release.py): checked BPS and Action Replay generator.
- [DEVELOPMENT.md](DEVELOPMENT.md): build instructions.
- [validation/README.md](validation/README.md): optional ROM-backed native checks.
- [REQUIREMENTS.md](REQUIREMENTS.md): requested behavior.
- [SETUP.md](SETUP.md): repository and conversation handoff.

From this directory:

```sh
python3 -m unittest discover -s tests -v
```

Game files, saves, and private download links stay outside this repository. The conversation is preserved as project notes, not an automatically synchronized transcript.
