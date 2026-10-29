# Development plan

## First milestone: input behavior

Completed for the initial prototype: a host-side reference model emits one command per completed sequence and alternates the shiny toggle state. All 14 reference tests passed. It cannot change a running game, and it is not code that can be pasted into Delta.

The model will help specify press/release handling, rejection of simultaneous inputs, wrong-button recovery, and timeouts before implementing those rules inside the GBA game or emulator cheat engine. Candidate sequences and timing are development defaults, not finalized user controls. Walk-anywhere and invulnerability toggles remain proposed behavior; the user explicitly required toggling for the shiny effect.

## Second milestone: identify the target

- Obtain the user's exact Amethyst v1.4.0 game file in a local analysis workspace.
- Record its byte size, SHA-256, and GBA header identifiers. A filename or reported size alone cannot identify it.
- Establish the Delta version and the active GBA core's cheat semantics.
- Identify safe scratch storage and the relevant version-specific functions/hooks. Do not borrow addresses from another hack or release without checking the code.

## Third milestone: implement the game effects

| Effect | Implementation requirement |
| --- | --- |
| Add an item | Call a verified item-grant routine or implement equivalent inventory handling, preserving existing quantities and inventory contents. Emit one grant of exactly one item per completed sequence. A full bag can reject a grant; it must not corrupt or replace another item. |
| Walk anywhere | Verify collision, elevation, water, and map-transition behavior. Distinguish traversing blocked tiles from leaving valid map bounds. Preserve the original behavior so it can be restored if a toggle is implemented. |
| Shiny toggle | Change the relevant encounter-generation path only while enabled. Restore normal generation after the next activation. Test that disabling the effect restores normal behavior for new encounters. |
| Invulnerability | Prevent player-side damage/fainting in the relevant battle paths, including both player slots in double battles. Periodic healing alone does not prove invulnerability. Record any unsupported damage or instant-faint mechanics. |

Choose between a Delta cheat implementation and a game patch after inspecting the target. Both remain possible approaches. No build artifact should be called Delta-compatible until it has been tested in Delta.

## Validation and release

Test held buttons, repeated sequences, reset behavior, battle entry/exit, and item quantities. Verify game effects in a matching game/emulator setup and separately record host-model results. Only then publish installation steps, final sequences, known limitations, and a patch or cheat text. Keep game binaries, saves, and credentials outside version control.
