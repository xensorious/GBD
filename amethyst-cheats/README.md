# Amethyst button cheats for Delta

**Version 1.1.0 is one Action Replay entry for the original Pokémon Amethyst v1.4.0 ROM. No ROM patch is required.** It adds a dedicated cheat-entry mode and fixes the bicycle collision bypass. The persistent shiny fix from 1.0.1 is included.

[Open the complete code](dist/amethyst-v1.4.0-delta-code.txt) · [Raw copyable text](https://raw.githubusercontent.com/xensorious/GBD/main/amethyst-cheats/dist/amethyst-v1.4.0-delta-code.txt)

## Install or update

1. Use your original Amethyst v1.4.0 game. Replace the entire previous cheat entry.
2. Open the code link, select all its text, and copy it. It contains **3,711 lines**; keep them together.
3. In Delta, add one cheat named `Amethyst button cheats`, choose **Action Replay**, and paste the entire code. Save and enable it.
4. Restart the game and choose **Continue** from its title screen using your in-game save. Do not load a save state captured with an earlier code version.

If you applied the old BPS patch, switch back to your original ROM and transfer your in-game save. This code targets the original file.

## Cheat-entry mode

1. **Hold L + R together for about one second, then release both.** This enters cheat mode and pauses ordinary gameplay and controls.
2. Enter any of the combinations below. You can enter multiple commands while remaining in this mode. Item grants happen immediately, including mid-battle.
3. **Hold L + R again for about one second, then release both** to resume the game.

The hold toggles only once; continuing to hold it does not repeatedly switch modes. Normal controls remain blocked until every held button is released. No ordinary A/B/menu/directional input is sent to the game while you type. Outside cheat-entry mode the command combinations are ignored.

This works from ordinary game menus and battle screens. It does not require closing the Party menu first. If the Bag was already open when an item was added, close and reopen it after resuming to refresh its list. Inventory must already exist; commands cannot grant items before a new game/save is initialized.

## Combinations

While in cheat-entry mode, begin every command with:

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

**Walking ends in B, A. R, R is battle protection.**

L/R are shoulder buttons. Left/Right/Up/Down are directions. Tap and release one button at a time during each command. Leave less than about two seconds between taps at normal emulator speed.

Each completed item command requests one item once, without overwriting another slot. Holding the final button adds no extra items. A full bag can reject the grant. Repeat a toggle command to switch its effect off, then again to switch it on. Leaving cheat-entry mode does not switch the effects off.

## Behavior

Walking selects the appropriate common collision bypass for foot or bicycle movement. Return to an accessible tile before switching it off. Map bounds, warps, scripted movement and special terrain rules still exist.

Shiny mode affects newly generated Pokémon and stays enabled across battle cleanup until toggled off. Existing Pokémon and trainer ownership are unchanged. Other game options can also affect normal shiny rates. Its game flag can be saved; turn it off before saving if the next session should start with normal rates.

Battle protection guards HP-loss and fainting paths for both player positions, while preserving enemy damage and healing. Existing fainted party members are not intentionally revived.

Exit cheat-entry mode and switch effects off with their combinations before removing the whole Delta entry. Restart after changing/removing the entry.

## Validation and supported ROM

Current live-game checks passed: normal controls and gameplay frozen during entry, Party menu preserved, all three items granted once mid-battle, normal battle callbacks restored on exit, and foot/bicycle collision off/on/off. Shiny persistence and native battle regressions also pass. The iPhone Delta app itself has not been run here. [Exact tested scope and limits](CODE_ONLY_VALIDATION.md).

Only the supplied original ROM is supported:

```text
SHA-256: 89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430
Size: 33,554,432 bytes
```

[Build instructions](DEVELOPMENT.md) · [Native checks](validation/code_only/README.md) · [Requirements](REQUIREMENTS.md)

No game binaries, saves, or private download links are stored in this repository.
