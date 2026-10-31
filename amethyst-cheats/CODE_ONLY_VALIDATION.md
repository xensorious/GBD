# Delta-only validation

Supported original ROM: SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`, 33,554,432 bytes. Version **1.1.2** text: **2,759 lines**, SHA-256 `3887135f426046e47f9f1d2923aad91bd656b571febaec90565f629d45ef0534`. No ROM patch is applied.

## Pokédex freeze: unresolved in the reported Delta case

The user reported the same Pokédex crash again after v1.1.2 was supplied. Their affected save/state and installed runtime have not been inspected. The passing controlled test below is insufficient to establish resolution of that report. No replacement code has been issued in response to the repeat crash.

The catch fixture directly creates Tornadus and calls ordinary wild-battle setup. It forces a one-member player party and starts from a controlled field snapshot. It does not run the actual roaming encounter and capture path from the user's save. Its successful8,192-byte request had only48bytes of contiguous headroom. That limited result does not establish the cause of the latest failure or adequate capacity in other game states.

The reported sequence was reproduced with the released v1.1.1 code: grant one Master Ball, explicitly enable shiny generation, generate a shiny Tornadus, catch it, and advance to its new Pokédex entry. The game stopped while drawing the entry's sprite, before reaching its A/B input state.

The original sprite routine requested8,192 contiguous heap bytes. Although more memory was free in total, the largest free block was6,032 bytes. The original allocator asserted at malloc.c line174, before returning NULL. This explains why simply watching for a NULL return did not reveal the failure.

Version1.1.2 removes the large ExecuteActions RAM clone, preserves its behavior using the original ROM function and a phase-specific action-table hook, and groups movable code near the heap's upper end. It reserves6,192bytes in six intervals, compared with8,936bytes in eight intervals in1.1.1. The tested capture path now has an8,240-byte block available for the8,192-byte request. No Pokédex allocation or graphics data is reduced or skipped.

The controlled sequence passed on the final packaged code: the generated shiny Tornadus was caught with the granted Master Ball in a regular wild battle, its entry reached the normal input state11, A dismissed it, and battle exit returned to the overworld with two party members instead of one. The new entry screen was visually inspected. The added party member was checked through the real GetMonData routine: species694/Tornadus, with a shiny personality/owner combination. The same controlled sequence previously stopped in drawing state8. This is a harness regression result, not confirmation that the user's roaming-capture crash is fixed.

The final code also passed a real battle protection test: HP stayed18 while enabled and fell to10 after disabling. Both player positions in a four-battler battle retained18HP through repeated turns. A real SaveMoveBlocks_ResetHeap call preserved the walking/protection toggles and granted Master Ball immediately and after60frames, with a valid reconstructed heap chain.

## Harness and scope

The original game runs in mGBA0.10.5 with the actual examined Delta-core VBA-M ARv3 parser/executor. A CPU hook schedules master-code execution at InitHeap before its first instruction. These are game/engine checks, not a run of the iPhone Delta application.

The field fixture predates the code. Setup preserves its five existing allocations, reserves the production intervals in the verified free tail, and refreshes the installer. Pokémon creation, battle setup, item insertion, capture, Pokédex tasks and battle exit use the actual game's routines. Cold boot and heap reset have separate compiled checks. ROMs, saves and private download links are not included.

A public regression replays169 capture/Pokédex allocation requests and their frees through the original ROM allocator after executing the compiled heap initializer. Its JSON contains only allocation sizes, lifetime IDs and a synthetic prefix; it contains no ROM or save bytes. Every allocation is filled, all protected code/tables/state are checked after each action, and all buffers remain independently freeable. This tests memory capacity and isolation; the separate live-game check establishes UI progress.

## Current compiled regressions

| Scope | Result |
| --- | --- |
| Decoder, one-shot item requests, toggles and shiny recovery |16 scenarios passed|
| L+R/Up10 entry/exit, release/debounce, interruption/timeout, capture-only commands and callback restoration |Passed|
| Sparse heap initialization, resets, all free intervals, exhaustion, split/coalesce and zero refresh |Passed|
| Capture/Pokédex allocation replay through original Alloc/Free |169 requests passed|
| Shiny toggle, real cleanup and real Pokémon generation |ON survives3cleanup loops/9generations; OFF and third ON passed|
| HP guards, healing, controller waits, fainting and party identity |162 cases passed|
| Missing-hook guard restoration |32 cases passed|
| Mixed-target spread HP bars |48 cases passed|
| Dispatcher wrappers |32 cases passed|
| Original-action mapping and disabled restoration |9 cases passed|
| Shared ROM-slot phase changes, selection and action dispatch |8 cases passed|
| Public callback identity, including nested fast-loop dispatch |5 cases passed|
| CB2/action-table differential cases |260 cases plus original action10 routing passed|
| Dependency-free source/package tests |41 tests passed|

Input tests stub external inventory/shiny effects. Shiny persistence executes the real flag routines, CreateMon and exact battle-cleanup loop; only RTC/keypad is stubbed. HP/dispatch tests stub controller plumbing and selected original callbacks as documented. Differential tests compare registers, calls and RAM with external callees stubbed. The allocation replay executes the actual allocator and stops on its assertion rather than treating assertion as a successful NULL return.

## Retained behavior and earlier evidence

All command combinations, Up10/L+R capture toggles, one-item quantities and shiny persistence are retained. Earlier live checks covered all three items added mid-battle and from the actual battle Bag, Party-menu callback restoration, foot/bicycle collision off/on/off, and capture-only command registration. Collision logic was not changed for this release.

Shiny mode uses game flag0913. Battle cleanup clears that flag; an explicitly activated toggle restores it. An untouched pre-existing game flag is not promoted into a persistent cheat. The flag itself is saved by the game, so saving while it is set can affect a later encounter after restart.

## Limits and updating

The actual iPhone app, touchscreen timing, every map/script, unusual battle outcome, late-game memory peak and long-term save are not exhaustively tested. The code receives inputs only while the game returns to ReadKeys; brief taps during loading can be missed. If a Bag list was already built before an item grant, close and reopen it to refresh the list.

This release changes the reserved heap layout and callback addresses. Replace the complete Action Replay entry, restart the game, and use title-screen Continue with an in-game save. Earlier Delta save states retain old memory links and must not be resumed with this version. An already crashed session is not repaired by replacing the cheat text.
