# Delta-only validation

Supported original ROM: SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`, 33,554,432 bytes. Version **1.1.0** text: **3,711 lines**, SHA-256 `50c1d5f0b0c10834a7609ae95f7981941388a095ce7788eb4ad180fe1821cd53`. No ROM patch is applied.

## Current live-game checks

The original game executes in mGBA0.10.5 with the actual examined Delta-core VBA-M ARv3 parser/executor. A CPU hook schedules the master-code pass at InitHeap, before instruction execution. These are game/engine tests, not a run of the iPhone application.

- Holding L+R enters capture mode. Ordinary A/B/Start/Select/directions/shoulders do not move the player or operate the game. Normal callbacks resume after exit and release.
- In an actual Party menu (callback0811EBA1), capture mode added one Rare Candy and restored the same menu. A/B in the command did not dismiss or select it.
- During a real battle, the three actual bag quantities changed `0 → 1` respectively for Master Ball, Max Revive and Rare Candy. Menu cursors, battle script pointer, player HP and frozen callback pointers stayed unchanged while commands were typed. The battle remained active and its normal callbacks resumed after exit.
- On foot, west movement was blocked at(10,13); enabling walking crossed the obstacle to(6,13); disabling blocked again at(10,13).
- On the bicycle, movement was blocked at(10,13); enabling bypass crossed to(7,13); disabling blocked again at(10,13). The actual bike instruction080BD3F6 switched0600→2000→0600. The earlier code only patched the foot routine.
- The new immutable entry helper occupies protected RAM at0200E940. Reserved heap intervals remain unchanged at8,936bytes including their headers.

The field fixture predates the code. Test setup preserves existing allocations, reserves the same intervals in its verified free tail, and refreshes the installer. Cold boot has separate prior checks. Test Pokémon and battle setup use real game routines. ROMs/saves/fixtures remain private.

## Focused regressions on the current compiled runtime

| Scope | Result |
| --- | --- |
| Compiled command decoder, one-shot quantities, toggles and shiny recovery |16 scenarios passed|
| Capture hold/release debounce, key masking, watched-key preservation, callback freeze/restore and hook removal |Passed|
| Foot/bicycle selection and battle slot priority |Passed|
| Actual shiny flag cleanup and actual Pokémon generation |ON survives3cleanup loops/9generations; OFF and ON again passed|
| Player/foe HP guards, healing, controller waits, fainting and party identity |162cases passed|
| Mixed-target spread HP bars |48cases passed|
| Dispatch wrappers and public callback identity |46cases passed|
| Relocated battle callbacks |260differential cases plus1action override passed|
| Sparse heap allocator, reset preservation, all free gaps, exhaustion, split/coalesce and zero refresh |Passed|
| Dependency-free source/package tests |41tests passed|

Input-unit checks stub external item/shiny operations. The shiny persistence regression runs the actual FlagGet/Set/Clear, CreateMon and exact original battle-cleanup loop; only RTC/keypad is stubbed. Battle native tests stub controller plumbing and some original callbacks as documented. Callback differential tests compare registers, calls and RAM with external callees stubbed.

## Retained baseline evidence

Earlier releases also passed real item quantities0→1→2, a held-final-button single grant, shiny normal/on/off/on creation, single-battle protection18→18 then18→8 after disabling, both player positions protected in a four-battler battle, and a direct SaveMoveBlocks_ResetHeap call preserving toggles/items. The v1.1.0 changes rerun the affected capture/collision paths and native regressions above; those older complete battle/reset scenarios were not repeated wholesale.

The normal foot-collision result was previously checked across256behavior bytes,11lower collision results and4directions (22,528cases) with external map/stair/collision calls stubbed. This covers the normal shared result path, not every warp or scripted movement.

## Shiny fix retained from1.0.1

The game clears temporary shiny flag0913 in its real end-battle cleanup loop088F033A–034C. The handler restores it while an explicitly activated shiny toggle is on. Toggle-off and disable restoration remain intact; an untouched transient game flag is not silently promoted into a persistent cheat.

## Limits

The actual iPhone Delta app and touchscreen paste/timing remain untested. Water and mountain tiles were not directly visited in live-map tests. Specialized subsystems that do not return to the normal ReadKeys loop, loading periods, every script/warp, unusual battle outcome, late-game allocation peak and long-term save are not exhaustively covered. The code can only process buttons while the game is running; it cannot receive them while Delta itself is paused.

If a bag list was already built before the grant, it can require closing/reopening to show a newly added item. The inventory change itself is immediate.
