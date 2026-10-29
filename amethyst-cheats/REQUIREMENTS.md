# Requested behavior

## Shared input rules

- Use deliberate, GTA-style **sequences** of button inputs, rather than only holding a button combination.
- Each cheat must have a sequence unlikely to occur accidentally during normal play.
- The user's example was `L, R, L, R, Left, Right, Up, Down`; this is illustrative, not an assigned or implemented sequence.
- The intended interface is Delta's custom cheat menu for Pokémon Amethyst v1.4.0.
- No specific code format or implementation approach has been validated yet.

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

These checks follow from the requested behavior. Host tests cover command emission; game effects have not been implemented or tested in the target game.

- Each completed item sequence grants exactly one item, including on repeated uses. Holding the final button must not repeatedly add items.
- Item grants must preserve existing inventory contents; continuously refilling or overwriting a slot does not satisfy the request.
- Shiny toggle deactivation must restore normal encounter behavior.
- Walking and invulnerability must be tested against the exact game version. Refilling HP periodically alone does not establish invulnerability.
- Inventory-full handling, toggle persistence across resets/save states, and the treatment of special battle effects remain implementation details to resolve during development.

## Unverified assumptions

- Walking and invulnerability will use sequence-controlled toggles unless the user changes that preference.
- A Delta-compatible implementation is preferred. A game patch is a possible fallback, not a confirmed requirement.

No sequence, address, hook, or code in this project should be treated as working until validated against the exact game file.
