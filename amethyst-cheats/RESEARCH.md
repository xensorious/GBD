# Research and current limits

Research notes recorded from the conversation. These are leads for implementation, not evidence of a working cheat system.

## Delta

- Delta's GBA cheat interface offers Action Replay, Code Breaker, and GameShark. The selected format must match the supplied code.
- The examined VBA-M cheat-engine source contains conditional operations, memory writes, arithmetic operations, button checks, and ROM patch operations. These are building blocks for remembered sequence progress and toggles.
- This supports feasibility **in principle**; a complete sequential-input system has not been implemented or verified in Delta.
- A button sequence requires custom state handling. There is no verified ready-made sequence for this project.

Sources:

- [Delta cheat-code instructions](https://faq.deltaemulator.com/using-delta/cheat-codes)
- [Examined VBA-M Cheats.cpp revision](https://raw.githubusercontent.com/visualboyadvance-m/visualboyadvance-m/453fa0decf179360926fb417725a794aa496d6e3/src/gba/Cheats.cpp)
- [Delta issue discussing GBA code-format behavior](https://github.com/rileytestut/Delta/issues/347)

## Pokémon Amethyst

- The project concerns duck_caper's GBA Pokémon Amethyst v1.4.0. Other projects also use the Amethyst name, so identity and version must be checked.
- The creator's v1.4.0 announcement links to HackDex for the release and documentation, and describes a 32 MB game size. The user's reported roughly 33 MB file size is not enough to verify its contents.
- The exact v1.4.0 game binary has not been obtained, hashed, disassembled, or tested.
- No project-specific addresses, unused memory locations, item-grant functions, collision hooks, shiny-generation hooks, or invulnerability hooks have been verified.
- Whether the full requested system can be supplied through Delta's cheat menu, or requires a game patch, remains unresolved.

Sources:

- [Creator's v1.4.0 release announcement](https://www.reddit.com/r/PokemonROMhacks/comments/1wq234b/pokemon_amethyst_v140_has_released_the_battle/)
- [Creator-linked HackDex project page](https://www.hackdex.app/hack/pokemon-amethyst)

## Validation still needed

1. Obtain the exact game file privately and record its size and checksum without committing the binary.
2. Verify the active Delta GBA core and supported cheat semantics.
3. Identify version-specific functions and safe storage for sequence progress and toggle state.
4. Test one-shot inventory additions without overwriting items or repeatedly incrementing while a key is held.
5. Test collision bypass, shiny state restoration, and invulnerability in relevant game situations.
6. Document installation, assigned button sequences, limitations, and test results only after validation.

No working cheat code or patch has been created or tested.
