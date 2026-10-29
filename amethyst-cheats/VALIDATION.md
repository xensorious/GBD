# Validation — experimental build

Checks on October 29, 2026 used the supplied 32 MiB ROM, SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`. Patched SHA-256: `be1804e482d04b5a5284c427d9625ccd6c051c5e30595715dd395c6efb0b62f6`. Neither ROM nor saves are distributed.

## Reproducible source checks

`python3 -m unittest discover -s tests -v` passes **32 tests**: 14 reference input tests, 8 encoding tests, and 10 release-builder tests. These cover input behavior, Action Replay encoding, BPS round trips/checksums, and ELF rejection. They do not themselves prove game effects.

The [optional native scripts](validation/README.md), using the final ELF and checksum-matched original ROM, pass **243 cases**:

| Cases | Scope |
| --- | --- |
| 14 | Compiled decoder, quantity-one item requests, chords, timeout, held final key, shiny alternation and re-enabling |
| 162 | Player/foe banks, damage/healing, busy controllers, HP subtraction/zeroing, faint protection and stale party-slot rejection |
| 48 | Mixed-target spread damage; foe damage and player healing preserved |
| 19 | Real heap allocator boundaries, original InitHeap comparison, entry thunks, callee registers and stack |

External calls are stubbed where stated in the scripts. Input tests stub inventory/shiny calls; faint tests stub the original faint handler. Spread tests run the actual ROM routine but stub spread classification, substitute queries and controller plumbing. These are not 243 full battles.

Allocator checks prove a maximum allocation ends at `0201BE00`, the reserved state's first byte; oversize requests fail without touching the 512-byte state.

## Full-game checks

The game ran in mGBA 0.10.5's libretro core. Activation text ran through a bridge compiled from the VBA-M Cheats.cpp revision used by the examined Delta core. Host memory/system access was replaced; parsing, decryption and cheat conditions were original. This combined setup is **not the Delta iPhone app**.

- Cold boot initialized state and reserved the heap tail; heap size was `1BE00`.
- The activation code was inert on the original ROM, even with RAM magic prefilled.
- Each item sequence changed its real bag quantity `0 → 1 → 2`. Holding a final key for 90 frames added only one.
- Three toggle flags alternated `1 → 0 → 1`.
- Disabling the code cleared walking/protection, restored the original shiny option, and blocked item entries. Re-enabling allowed a fresh shiny activation.
- Real CreateMon calls alternated normal/shiny/normal/shiny. Shiny XOR used this game's threshold of 16 and retained trainer ownership.
- A real level-5 Magikarp/Rattata battle kept player HP at 18 while protected. Ordinary damage returned after disabling protection. Battle entry reset the heap without erasing code/state.
- Walking off blocked a bedroom obstacle; on crossed it; off blocked it again. Equivalent tests passed through outdoor trees and a house wall/roof. Natural stairs and house-exit transitions worked.

The field fixture predates the patch. Its preparation shortened a verified free heap tail by 512 bytes and initialized state. Cold boot was tested separately without that preparation. Test Pokémon and battle startup used real game routines. This was controlled integration testing, not a complete normal playthrough.

## Remaining validation

- Actual Delta iPhone version, touchscreen timing, and importing a user's save.
- Water, mountains, every terrain/elevation, map edge and transition.
- Every battle variant, unusual move/ability/item interaction and scripted loss.
- A full playthrough, long-term saves and heap-heavy late-game features.

Collision coverage uses the common player function. Battle guards cover verified paths and both player-position parities, including self-KO and direct subtraction. Special scripts outside those paths may behave differently. Existing fainted party members are not intentionally revived. No visual toggle indicator is implemented.
