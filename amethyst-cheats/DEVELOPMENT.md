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
- `tools/code_only_relocate.py`: bounded local relocation of the supplied game's fast battle callback, including literal and external-call fixups.
- `tools/build_code_only.py`: AR v3 encoding, guarded ROM hook pairs and packaging.
- `tools/cheat_encoding.py`: encrypted code operations.
- `tools/build_release.py`: shared checksum/ELF helpers and the older patch generator; it is not the current installer.

## Execution and memory

The AR master code runs at InitHeap entry `08002B80`. It restores immutable RAM code before the game enters its heap trampoline, including cold boot and reset paths. ReadKeys calls a zero-argument InitHeap no-op to request the same safe refresh once per main-loop iteration. This avoids rewriting the save scratch buffer while save-block copies are in progress.

Two ROM halfwords route InitHeap through a small ARM entry; a third redirects the existing ReadKeys literal. The fourth AR patch slot serves either the selection-script command table or the original ExecuteActions action table during battles, and the appropriate collision result while walking outside battle. Conditional skips gate entire patch pairs, because this engine does not apply global codes-off conditions to its outer ROM-patch switch.

The handler protects six allocated intervals totaling 6,192 bytes including their headers. Most movable code is grouped near the heap's upper end, leaving larger contiguous gaps for the original allocator. The previous eight-interval layout could leave only 6,032 contiguous free bytes when the Pokédex requested an 8,192-byte sprite buffer, causing the game's allocator assertion. State remains at `02010C00` and survives ordinary heap resets. A full game reset starts fresh state.

Capture retains earlier state offsets, including the Up counter, release detector and timeout at offsets124–127. Ten distinct Up presses with no other buttons and at most120samples between presses toggle capture; holding Up counts once. The L+R30-sample alternative remains. Before its threshold, L+R masks ordinary keys but leaves callbacks running. After a real toggle, entry masks all five ordinary key halfwords, preserves the pre-read watched-key latch, resets repeat timing, and saves/replaces both normal main callbacks with a gate. This freezes gameplay while the main ReadKeys loop and interrupts continue. Exit restores callbacks after release; the gate also restores them if hooks disappear. The command decoder runs only in active capture mode after all toggle buttons release.

Outside battle, the fourth ROM slot selects bike collision at080BD3F6 when avatar flags contain2/4, otherwise foot collision at0805BB8C. This behavior is retained from earlier releases.

ExecuteActions now runs from its original ROM function. A native wrapper claims the fourth ROM slot for its action-table literal at088CE782, routing action10 through the guarded RAM handler. The native battle callback claims that same slot for the selection-script table at08014C1E during other phases. It refreshes at a phase change before calling the original game function, preserving both dispatch routes. The fast CB2 loop still executes from a 304-byte relocated copy, with external BL veneers preserving argument registers and wrappers preserving the public callback identity.

Current placement: `.text`0201AE60, `.entrymode`0201BD10, CB2 clone0201BE60, guard exports0201BFA0 and callback exports0201BFC0. Their shared protected block is0201AE50–0201BFE0. Fixed ReadKeys, InitHeap, action-table and state reservations remain separate. Restart from the title-screen Continue option after updating; old states retain the previous allocation chain and callback addresses.

Payload writes use halfword FILL operations so the engine's DeleteAll operation does not replace live RAM code with stale words before ROM hooks are undone. Effect toggles should be switched off with their sequences before removing the entire entry; restart afterward.

See [CODE_ONLY_VALIDATION.md](CODE_ONLY_VALIDATION.md) for measured results and limits.
