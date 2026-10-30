# Requested behavior

## Shared input rules

- Use deliberate, GTA-style **sequences** of button inputs, rather than only holding a button combination.
- Each cheat must have a sequence unlikely to occur accidentally during normal play.
- The user's example `L, R, L, R, Left, Right, Up, Down` became the shared prefix; six endings are listed in README.md.
- The intended interface is Delta's custom cheat menu for Pokémon Amethyst v1.4.0.
- The current implementation is one Action Replay code entry for the original game, without a ROM patch. Direct iPhone testing remains outstanding.

## Six effects

| # | Effect | Required behavior |
| --- | --- | --- |
| 1 | +1 Master Ball | Completing its sequence once adds exactly one Master Ball to the player's inventory, then stops. No further setting or activation step. |
| 2 | +1 Max Revive | The same one-item, one-activation rules as the Master Ball cheat. |
| 3 | +1 Rare Candy | The same one-item, one-activation rules as the Master Ball cheat. |
| 4 | Walk on any tile | Allow walking over trees, houses, walls, mountains, water, and other normally blocked tiles. The user requested this effect; making it toggle on/off with repeated sequence entry is an assistant-proposed assumption. |
| 5 | 100% shiny encounter rate | Completing its sequence switches normal shiny encounter rates to 100%. Completing it again restores normal rates; each subsequent completion alternates the state. This toggle behavior is explicitly required by the user. |
| 6 | Invulnerable player Pokémon in battle | All of the player's Pokémon in battle become invulnerable. Making this effect toggle on/off with repeated sequence entry is an assistant-proposed assumption. |

## Implementation checks

These checks follow from the requested behavior. Game effects are implemented; tested scope and gaps are recorded in VALIDATION.md.

- Each completed item sequence grants exactly one item, including on repeated uses. Holding the final button must not repeatedly add items.
- Item grants must preserve existing inventory contents; continuously refilling or overwriting a slot does not satisfy the request.
- Shiny toggle deactivation must restore normal encounter behavior.
- Walking and invulnerability must be tested against the exact game version. Refilling HP periodically alone does not establish invulnerability.
- Inventory grants use the game's own rejection behavior. Ordinary heap resets preserve toggles; full resets start fresh walking/protection state. Shiny uses a saveable game flag. Earlier save states lack the runtime and must not be used for setup. Special battle paths remain documented testing limits.

## Implementation decisions

- Walking and invulnerability use sequence-controlled toggles, based on the assistant's proposed behavior.
- A master-code installer and protected RAM intervals handle heap resets. The final artifact is the requested Delta-only entry; the earlier patch approach is superseded.

The builder enforces the exact source checksum. These hooks must not be reused with another release.
