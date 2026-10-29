# Amethyst cheats for Delta

Project notes for developing custom button-sequence cheats for Pokémon Amethyst v1.4.0 in the Delta emulator.

**Status: project started. No working GBA cheat code or game patch has been created or tested.**

The requested system recognizes deliberate sequences of button presses, grants one item per completed item sequence, and supports reversible cheat effects. See [REQUIREMENTS.md](REQUIREMENTS.md) for the six requested effects and [RESEARCH.md](RESEARCH.md) for findings and remaining work.

The host-side Python model in `prototype/sequence_input.py` specifies sequence recognition and emits commands for later game integration. It does not modify a game, add items to a real inventory, or generate Delta cheat codes. Candidate sequences are development defaults, not final controls.

## Project files

- [REQUIREMENTS.md](REQUIREMENTS.md): the six requested cheats and input rules.
- [RESEARCH.md](RESEARCH.md): source links and unverified game details.
- [DEVELOPMENT.md](DEVELOPMENT.md): implementation milestones and validation plan.
- [SETUP.md](SETUP.md): repository and conversation handoff.
- `prototype/sequence_input.py`: reference input handler, independent of the ROM.
- `tests/test_sequence_input.py`: host-side behavior tests.

## Run the reference tests

Use Python 3.10 or later; no third-party dependencies are required. From this project's folder (`amethyst-cheats/` in the repository):

```sh
python3 -m unittest discover -s tests -v
```

Passing these tests verifies only the reference model. The game effects still require a matching Amethyst v1.4.0 binary and testing in Delta.

The initial run passed all 14 reference-model tests. See [VALIDATION.md](VALIDATION.md) for the scope of that result.

## Current inputs

- Emulator: Delta on iPhone, using its GBA cheat menu.
- Game: Pokémon Amethyst v1.4.0; user reports a roughly 33 MB game file.
- The exact game binary has not been obtained or inspected.
- No button sequences have been finalized.

## Next steps

1. Obtain and identify the exact v1.4.0 game file, including its checksum.
2. Verify the relevant game functions, memory layout, and Delta cheat behavior.
3. Determine whether the complete system can run as Delta cheat codes or needs a game patch.
4. Implement and test each effect, including repeated activations and restoring normal behavior.

Keep game files, saves, save states, archives, and credentials outside version control. The repository's `.gitignore` helps exclude these files.

These documents preserve the project context from the conversation. Connecting a repository does not automatically copy the conversation transcript or synchronize future files.

See [SETUP.md](SETUP.md) for creating the GitHub repository and optionally preparing a Codex Cloud environment.
