# Verified target and implementation

The supplied v1.4.0 ROM is 33,554,432 bytes, title POKEMON FIRE, code BPRE, revision 0. Source SHA-256:

`89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`

Header identity alone cannot identify a hack/version. The builder verifies the complete hash and every original hook byte.

## Delta

The examined [GBADeltaCore bridge](https://github.com/rileytestut/GBADeltaCore/blob/master/GBADeltaCore/Bridge/GBAEmulatorBridge.mm) routes Action Replay and GameShark to AR v3. Its pinned [VBA-M implementation](https://raw.githubusercontent.com/visualboyadvance-m/visualboyadvance-m/453fa0decf179360926fb417725a794aa496d6e3/src/gba/Cheats.cpp) supports conditional RAM writes but only four reversible halfword ROM patches.

A RAM installer was rejected after live battle entry erased its heap storage. Other resets occur in title/save/reload paths. The release therefore uses a ROM patch plus a short enable heartbeat. [Delta cheat instructions](https://faq.deltaemulator.com/using-delta/cheat-codes).

## Version-specific anchors

| Location | Verified purpose |
| --- | --- |
| 080005EC | ReadKeys destination 0891CECD, chained by handler |
| 08002B80 | InitHeap, hooked to reserve 512 bytes |
| 03000A38/03000A3C | Heap start/size globals |
| 08002949 | First free-block initializer |
| 0805BB1C | Common player collision function |
| 0809A085 | AddBagItem: Master Ball1, Max Revive25, Rare Candy68 |
| 0806E681/0806E6A9/0806E6D1 | FlagSet/Clear/Get; shiny flag0913 |
| 0803DA55 | CreateMon, used for real generation tests |
| 0888B960 | Battle script command table |
| 02023BD6 | Battler positions; even positions are player-side |
| 0201BE00 | Reserved runtime state |

Shiny generation preserves trainer ownership and does not change existing Pokémon. Other game options can also influence shiny rates. Battle guards and exact addresses are recorded in source.

## Limits

Actual iPhone Delta, all terrain, all battle variants and a full playthrough remain untested. [VALIDATION.md](VALIDATION.md) separates stubbed checks from game tests.

Identification sources: [creator's v1.4.0 announcement](https://www.reddit.com/r/PokemonROMhacks/comments/1wq234b/pokemon_amethyst_v140_has_released_the_battle/) and [creator-linked HackDex page](https://www.hackdex.app/hack/pokemon-amethyst).
