# Delta-only validation

Supported original ROM: SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`, 33,554,432 bytes. Version **1.1.1** text: **3,775 lines**, SHA-256 `1725361d1435cf7740b856b77967dac523dd6c06416fc508b4394123491c94de`. No ROM patch is applied.

## Current release checks

The v1.1.0 timing regression was reproduced during battle initialization: a40-frame L+R hold encountered only29 ReadKeys samples, below the30-sample toggle threshold. The old code nevertheless froze both main callbacks during that partial hold, while the command decoder remained disabled. A90-frame hold entered correctly. This establishes a partial-hold defect; it does not prove the cause of every failure reported in iPhone Delta.

Version1.1.1 masks an incomplete chord's game keys without freezing callbacks. It also supports ten distinct Up presses as the user-requested alternative entry/exit method. Compiled regressions cover on/off, single counting of held keys, interruption, timeout, full release after toggles, and commands rejected outside capture. All three items are still emitted with quantity1.

On the rebuilt runtime, native input16, heap, shiny persistence, effects162, relocated callbacks260 plus action override, dispatch46, and dependency-free41 checks passed. Collision behavior was left unchanged for this release.

The updated real-game check explicitly advanced past the battle intro to the visible Fight/Bag/Pokémon/Run menu, rather than treating an intro text screen as the action menu. Up10 entry/exit granted all three items once (`0→1`); L+R entry/exit on that same ready action menu granted each once again (`1→2`). Navigation then opened the actual battle Bag (callback08107EE1); Up10 capture granted each once more (`2→3`) and restored that same Bag callback. Battle-script pointer, all battler data and frozen callback pointers stayed unchanged during each set of grants. Entry during the intro was also checked with slower Up taps; the game can miss taps during a busy loading interval before ReadKeys runs again.

## Live-game harness

The original game executes in mGBA0.10.5 with the actual examined Delta-core VBA-M ARv3 parser/executor. A CPU hook schedules the master-code pass at InitHeap, before instruction execution. These are game/engine tests, not a run of the iPhone application.

- The current release's battle checks are described above. The following live-game checks were completed on v1.1.0 and retained as baseline evidence.
- Holding L+R entered capture mode. Ordinary A/B/Start/Select/directions/shoulders did not move the player or operate the game. Normal callbacks resumed after exit and release.
- In an actual Party menu (callback0811EBA1), capture mode added one Rare Candy and restored the same menu. A/B in the command did not dismiss or select it.
- During a battle intro, the three actual bag quantities changed `0 → 1` respectively for Master Ball, Max Revive and Rare Candy. Menu cursors, battle script pointer, player HP and frozen callback pointers stayed unchanged while commands were typed. The battle remained active and its normal callbacks resumed after exit.
- On foot, west movement was blocked at(10,13); enabling walking crossed the obstacle to(6,13); disabling blocked again at(10,13).
- On the bicycle, movement was blocked at(10,13); enabling bypass crossed to(7,13); disabling blocked again at(10,13). The actual bike instruction080BD3F6 switched0600→2000→0600. The earlier code only patched the foot routine.
- The immutable entry helper occupies protected RAM at0200E940. Reserved heap intervals remain unchanged at8,936bytes including their headers.

The field fixture predates the code. Test setup preserves existing allocations, reserves the same intervals in its verified free tail, and refreshes the installer. Cold boot has separate prior checks. Test Pokémon and battle setup use real game routines. ROMs/saves/fixtures remain private.

## Focused regressions on the current compiled runtime

| Scope | Result |
| --- | --- |
| Compiled command decoder, one-shot quantities, toggles and shiny recovery |16 scenarios passed|
| L+R/Up10 toggles, debounce, interruption/timeout, capture-only commands, partial-chord callback progress, key masking, callback restore and hook removal |Passed|
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

Earlier releases also passed real item quantities0→1→2, a held-final-button single grant, shiny normal/on/off/on creation, single-battle protection18→18 then18→8 after disabling, both player positions protected in a four-battler battle, and a direct SaveMoveBlocks_ResetHeap call preserving toggles/items. The v1.1.0 release reran its affected capture/collision paths. The v1.1.1 release reran entry/items and the native checks listed above; those older complete battle/reset/collision scenarios were not repeated wholesale.

The normal foot-collision result was previously checked across256behavior bytes,11lower collision results and4directions (22,528cases) with external map/stair/collision calls stubbed. This covers the normal shared result path, not every warp or scripted movement.

## Shiny fix retained from1.0.1

The game clears temporary shiny flag0913 in its real end-battle cleanup loop088F033A–034C. The handler restores it while an explicitly activated shiny toggle is on. Toggle-off and disable restoration remain intact; an untouched transient game flag is not silently promoted into a persistent cheat.

## Limits

The actual iPhone Delta app and touchscreen paste/timing remain untested. Water and mountain tiles were not directly visited in live-map tests. Specialized subsystems that do not return to the normal ReadKeys loop, loading periods, every script/warp, unusual battle outcome, late-game allocation peak and long-term save are not exhaustively covered. The code can only process buttons while the game is running; it cannot receive them while Delta itself is paused.

If a bag list was already built before the grant, it can require closing/reopening to show a newly added item. The inventory change itself is immediate.
