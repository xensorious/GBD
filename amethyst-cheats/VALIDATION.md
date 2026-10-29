# Initial validation

On October 29, 2026, the host-side reference model passed **14 unittest tests** using:

```sh
python3 -m unittest discover -s tests -v
```

Run that command from `amethyst-cheats/` using Python 3.10 or later.

## What was verified

- All six candidate sequences emit their expected command.
- Holding keys, including the final button, does not repeatedly activate a command.
- Repeated taps of the same button require a release.
- Reentering each item sequence emits another command with quantity exactly one.
- The shiny command alternates enabled, disabled, enabled.
- Proposed walking and invulnerability toggles keep independent states.
- Wrong inputs, overlapping prefixes, simultaneous buttons, timeouts, and invalid input samples are handled.

## What has not been verified

These tests only exercise Python command generation. They do not grant an item, change a shiny encounter rate, bypass collision, or prevent battle damage. No Amethyst game binary has been inspected, no Delta cheat text has been generated, and no target game or emulator integration has been tested.

Candidate sequences and the 120-frame maximum gap between accepted taps are configurable prototype defaults. Walk-anywhere and invulnerability toggles are proposed assumptions; shiny toggling is an explicit user requirement. A new decoder starts with all toggle states off. Reset/save-state persistence remains unspecified for the eventual target implementation.
