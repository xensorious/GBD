# Building the Delta-only release

The current deliverable is `dist/amethyst-v1.4.0-delta-code.txt`. It is one Action Replay v3 entry for the unchanged source ROM. The earlier BPS workflow is superseded.

## Build

Use Python 3.10+ and Zig 0.13.0. Supply your own exact original ROM:

```sh
ZIG=/path/to/zig sh tools/build_code_only.sh /path/to/original-amethyst.gba
python3 -m unittest discover -s tests -v
```

The builder checks the complete source SHA-256, allocated ELF layout, every exported Thumb pointer, payload overlap and reserved data bounds. It emits cheat text and a manifest; it never modifies the ROM or creates a patched game.

[Optional native checks](validation/code_only/README.md) additionally need Unicorn 2.1.4. Keep game files and test saves outside version control.

## Source

- `native/code_only.c`: input decoder, inventory/shiny operations, heap reservation, HP guards and entry thunks.
- `native/code_only_dispatch.h`: battle callbacks, including public callback identity preservation.
- `native/code_only.ld`: fixed RAM placement.
- `tools/code_only_relocate.py`: bounded local relocation of the supplied game's two battle callbacks, including literal and external-call fixups.
- `tools/build_code_only.py`: AR v3 encoding, guarded ROM hook pairs and packaging.
- `tools/cheat_encoding.py`: encrypted code operations.
- `tools/build_release.py`: shared checksum/ELF helpers and the older patch generator; it is not the current installer.

## Execution and memory

The AR master code runs at InitHeap entry `08002B80`. It restores immutable RAM code before the game enters its heap trampoline, including cold boot and reset paths. ReadKeys calls a zero-argument InitHeap no-op to request the same safe refresh once per main-loop iteration. This avoids rewriting the save scratch buffer while save-block copies are in progress.

Two ROM halfwords route InitHeap through a small ARM entry; a third redirects the existing ReadKeys literal. The fourth AR patch slot serves the main battle-command table during battles and the foot-collision result while walking is enabled outside battle. Conditional skips gate entire patch pairs, because this engine does not apply global codes-off conditions to its outer ROM-patch switch.

The handler protects eight allocated intervals rather than reserving a continuous 22 KiB hole. The intervals total 8,936 bytes including their allocated headers. Free gaps remain available to the original allocator. State resides at `02010C00` and survives ordinary heap resets. A full game reset starts fresh state.

Two exact game callbacks execute from RAM copies, with external BL veneers preserving argument registers. Wrappers preserve the original public callback identity while game logic executes, including multiple logical battle steps in one video frame. This covers the extra dispatch paths without spending another ROM patch slot.

Payload writes use halfword FILL operations so the engine's DeleteAll operation does not replace live RAM code with stale words before ROM hooks are undone. Effect toggles should be switched off with their sequences before removing the entire entry; restart afterward.

See [CODE_ONLY_VALIDATION.md](CODE_ONLY_VALIDATION.md) for measured results and limits.
