# Amethyst button cheats for Delta

**The current release is one Action Replay entry for the original Pokémon Amethyst v1.4.0 ROM. No ROM patch is required.**

[Open the complete code](dist/amethyst-v1.4.0-delta-code.txt) · [Raw copyable text](https://raw.githubusercontent.com/xensorious/GBD/main/amethyst-cheats/dist/amethyst-v1.4.0-delta-code.txt)

## Install

1. Use your original Amethyst v1.4.0 game. Replace the earlier four-line experimental cheat.
2. Open the code link, select all its text, and copy it. It contains **3,451 lines**; keep them together.
3. In Delta, add one cheat named `Amethyst button cheats`, choose **Action Replay**, and paste the entire code. Save and enable it.
4. Restart the game and choose **Continue** from its title screen. Use your normal in-game save. Avoid loading a save state made before this code was installed, because it lacks the handler's RAM state.

If you applied the earlier BPS patch, switch back to your original ROM and transfer your in-game save to it. This code targets the original file.

## Controls

Every command starts with:

**L, R, L, R, Left, Right, Up, Down**

Then add its ending:

| Ending | Effect |
| --- | --- |
| A, A | Add exactly 1 Master Ball |
| B, B | Add exactly 1 Max Revive |
| A, B | Add exactly 1 Rare Candy |
| B, A | Toggle walking through normally blocked tiles |
| L, L | Toggle 100% shiny generation |
| R, R | Toggle protection for your Pokémon in battle |

L/R are shoulder buttons. Left/Right/Up/Down are directions. Tap and release one button at a time, including repeated buttons. Leave less than about two seconds between taps at normal speed. Enter commands during ordinary gameplay, outside loading screens.

Each completed item command requests one item once. Holding the final button adds no extra items. A full bag can reject the grant; no slot is overwritten. Repeat a toggle command to switch it off, then again to switch it on. Walking and protection start off. Shiny starts from the game's current shiny flag and changes newly generated Pokémon.

Buttons also reach the normal game controls. A command can move the player or a battle-menu cursor; check your position/menu before the next ordinary action. There is no on-screen toggle indicator.

## Behavior

Walking bypasses the common foot-movement collision result. Map boundaries, warps, scripted movement, and other map scripts still exist. Return to an accessible ordinary tile before toggling walking off.

Shiny toggling uses the game's existing flag. It leaves existing Pokémon unchanged and preserves trainer ownership. Other game options can also affect normal shiny rates. The flag can be saved with the game; turn it off before saving if the next session should start with normal rates.

Battle protection guards HP-loss and fainting paths for both player positions, including double battles, while preserving enemy damage and healing. It does not intentionally revive already fainted party members. Unusual scripted outcomes outside those paths remain a validation limit.

Use the button commands to switch effects off before disabling/removing the entire Delta entry. Restart after changing/removing the entry so the game rebuilds its normal memory layout. Newly captured save states retain their recorded runtime state.

## Validation

The delivered code runs on the original ROM in headless mGBA, using the actual VBA-M AR v3 parser/executor and its CPU master-code timing. Real tests passed all three item grants, held-button behavior, toggles, shiny generation, walking, single-battle protection on/off, both player positions in a double battle, and a direct save-block/heap reset. Source and ARM checks also passed. **The actual iPhone Delta app has not been run here.**

Water and mountains were not directly visited in live-map tests. The shared collision-return patch was checked across all 256 behavior-byte inputs, 11 collision inputs and four directions; scripts/warps and every special terrain path remain outside that claim. See [the exact tested scope](CODE_ONLY_VALIDATION.md).

Only the supplied original ROM is supported:

```text
SHA-256: 89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430
Size: 33,554,432 bytes
```

[Build instructions](DEVELOPMENT.md) · [Native validation scripts](validation/code_only/README.md) · [Requirements](REQUIREMENTS.md)

The earlier patch release is superseded. No game binaries, saves, or private download links are stored in this repository.
