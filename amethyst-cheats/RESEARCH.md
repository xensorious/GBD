# Delta-only implementation findings

The supplied Amethyst v1.4.0 ROM is 33,554,432 bytes, header title POKEMON FIRE, BPRE revision 0. The builder identifies the complete file using SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`.

The examined [GBADeltaCore bridge](https://github.com/rileytestut/GBADeltaCore/blob/master/GBADeltaCore/Bridge/GBAEmulatorBridge.mm) sends Action Replay and GameShark to AR v3. Its [VBA-M revision](https://raw.githubusercontent.com/visualboyadvance-m/visualboyadvance-m/453fa0decf179360926fb417725a794aa496d6e3/src/gba/Cheats.cpp) has four reversible AR v3 halfword ROM patch slots. Code Breaker does not supply an unrestricted ROM patch operation.

Those limits initially led to a BPS workaround. A code-only route was subsequently implemented: an InitHeap master-code installer, a two-halfword ARM/Thumb heap entry, one ReadKeys literal change, and a fourth slot shared between battle dispatch and walking. The master point also prevents installer writes during save scratch copying.

Two battle callbacks are copied and relocated locally from the supplied ROM. RAM wrappers preserve public callback identity and catch dispatch changes inside fast battle loops. Reserving only eight necessary RAM intervals avoids the out-of-memory failure caused by the earlier continuous reservation.

| Location | Purpose in this exact ROM |
| --- | --- |
| 08002B80 | InitHeap/master-code trigger |
| 08002B84 / 08002B96 | Two-halfword ARM-entry redirect |
| 03000A38 / 03000A3C | Original heap globals |
| 080005EC | ReadKeys destination literal; original 0891CECD chained |
| 08014C1E | Main command-table literal halfword |
| 0805BB8C | Common normal foot-collision result |
| 0809A085 | AddBagItem: item IDs1/25/68, quantity1 |
| 0806E681 / 0806E6A9 / 0806E6D1 | Shiny flag0913 set/clear/get |
| 02010C00 | Runtime state inside a protected interval |

Addresses are version-specific. [CODE_ONLY_VALIDATION.md](CODE_ONLY_VALIDATION.md) separates actual game results from stubbed ARM checks and direct iPhone testing.

[Delta cheat instructions](https://faq.deltaemulator.com/using-delta/cheat-codes).
