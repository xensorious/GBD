# Delta-only validation

Final source ROM: SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`, 33,554,432 bytes. Delivered text: **3,451 lines**, SHA-256 `06a5ccfc0de41c715fc2d4e1a9103243480fec94727f91cc310589cdd857388b`. No ROM patch was applied.

## Full-game integration

The original game executed in mGBA 0.10.5. A bridge compiled from the examined Delta core's actual VBA-M Cheats.cpp parsed and executed the complete AR v3 entry. A CPU hook scheduled it at the same InitHeap program-counter trigger as VBA-M master codes, before execution of the affected instruction. No extra VBlank cheat passes were used while that master code was active.

- Cold boot installed the runtime and processed ordinary ReadKeys samples.
- Real inventory quantities for Master Ball, Max Revive and Rare Candy changed `0 → 1 → 2` after repeated respective entries.
- Holding a final key for 90 frames granted only one item.
- Walking, shiny and protection flags each alternated `1 → 0 → 1`.
- Live foot movement crossed a blocked bedroom obstacle when enabled and stopped again after disabling.
- Real CreateMon generation alternated normal/shiny/normal/shiny. Trainer ownership was preserved.
- A real Magikarp/Rattata battle started and retained player HP at 18 through protected turns. Reentering the command disabled protection; subsequent ordinary attacks lowered HP from 18 to 8.
- A real double-battle fixture initialized four battlers. Both player positions retained 18 HP through eight fight cycles. Heap links remained valid, with 6,512 free bytes and a largest free block of 5,312 bytes.
- The real MoveSaveBlocks_ResetHeap routine returned with walking/protection enabled, item quantity unchanged and runtime magic intact. Heap links remained valid after 60 more frames. This tests the actual reset function, not the entire menu-driven save process.

Integration field fixtures were captured from the original game before installation. Test preparation reserved the same eight intervals inside a verified free heap tail and explicitly refreshed the installer. Cold boot was tested separately without that preparation. Test Pokémon and battle startup used real game routines. Fixtures/saves/ROMs remain private.

The full-game automation's direction inputs initially selected Run in the battle menu after a cheat sequence. The focused final battle check deliberately selected Fight before each ordinary action; this was a test-control issue, not a change to cheat behavior.

## Parser and teardown

The complete final file parses with zero errors in the actual VBA-M engine. Its size is below that core's 16,384-line limit. It contains no trailing blank line, matching Delta's bridge validation.

All four combinations of version-signature and requested-mode conditions were tested: the ROM patch occurs only when both match, and every other ROM byte stays unchanged. Actual cheatsDeleteAll(true) clears the master code and preserves payload RAM. The following normal engine pass restores all ROM hooks. Sequence-controlled off/on behavior is separate from removing the entire entry.

## Source and ARM checks

The dependency-free project suite passes **41 tests**. Optional [ROM-backed scripts](validation/code_only/README.md) exercise the final ELF:

| Scope | Result |
| --- | --- |
| Compiled input/quantity-one requests and toggles | 14 scenarios passed |
| HP guards, both sides, healing, waits, faint state and party identity | 162 cases passed |
| Missing-hook fail-open behavior | 32 cases passed |
| Mixed-target spread HP bars | 48 cases passed |
| Dispatch wrappers and callback public identity | 46 cases passed |
| Relocated game callbacks | 260 differential cases plus one action-table override passed |
| Foot-collision return path | 22,528 cases passed |
| Sparse heap | Cold/warm init, six resets, all nine free gaps, allocation exhaustion, split/coalesce, nonstandard heaps, no-op refresh and missing-state cases passed |

The walking check runs the actual function and patch across 256 behavior bytes, 11 lower collision results and four directions, with map/stair/collision dependencies stubbed. It verifies the normal shared result path; it does not establish every warp or script behavior. Unicorn's translated-code cache is invalidated after toggling the ROM instruction.

Other native tests stub external calls as stated in each script. Input tests stub inventory/shiny operations; full-game tests above execute those real operations. Battle tests execute original HP routines while stubbing controller plumbing, and stub faint callbacks where documented. Callback differential tests compare registers, call traces and RAM with external callees stubbed. These are not hundreds of complete battles.

## Limits

The actual iPhone Delta app and touchscreen paste/timing were not exercised. Live map visits did not include water or mountain terrain. Every unusual battle mode, scripted result, map transition, late-game allocation peak and long-term save has not been exhaustively tested. Ordinary map boundaries, warps, forced movement, other game options and already-existing Pokémon remain governed by game behavior.

These limits describe testing scope; the delivered artifact is the original requested Delta-only code.
