These scripts execute the compiled ARM Thumb runtime in Unicorn against an original, checksum-checked Amethyst v1.4.0 ROM supplied locally. No ROM or save file is included. They are independent scripts, separate from the synthetic unit tests in `tests/`.

Use Python 3.10 or newer, Zig 0.13.0, and Unicorn 2.1.4. From the project directory:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r validation/requirements.txt
export AMETHYST_ROM=/absolute/path/to/original-amethyst.gba
ZIG=/absolute/path/to/zig tools/build.sh "$AMETHYST_ROM"
python3 validation/test_compiled_native_input.py
python3 validation/test_compiled_native_effects.py
python3 validation/test_mixed_spread_bar.py
python3 validation/test_heap_and_wrappers.py
```

The scripts derive project paths from their own location and load `build/runtime.elf`. `AMETHYST_ROM` must match source SHA-256 `89f92414af401a3c20068224e4c937c1b4380be5d0997e9202f4f788916b1430`. With the tested runtime, they report 14 input scenarios, 162 effect cases, 48 spread healthbar cases, and 19 heap/ABI cases: 243 total.

| Script | Executed code and limits |
| --- | --- |
| `test_compiled_native_input.py` | Runs the compiled input handler and command dispatcher. Original key/RTC handling, inventory additions, and shiny flag operations are stubbed; checks item IDs and quantity, repetitions, held keys, chords, timeouts, toggles, and disable/re-enable behavior. |
| `test_compiled_native_effects.py` | Runs compiled battle guards and original ROM HP-update and direct self-HP commands. Four downstream routines are stubbed: `0x088D3B80`, `0x088EDB8C`, `0x0800DFF0`, and `0x08017248`. The ordinary healthbar and faint callbacks are stubbed to check routing and restoration; spread-move detection is stubbed here. Covers both player banks, opponents, damage/healing, busy execution, fainted slots, and party-slot changes. |
| `test_mixed_spread_bar.py` | Runs the compiled healthbar guard and original ROM `DoublesHPBarReduction` and `EmitHealthBarUpdate`. Spread-move detection, substitute/block queries, buffer copying, and controller marking are stubbed. Checks protected player targets alongside opponents and negative healing values. |
| `test_heap_and_wrappers.py` | Runs the compiled heap replacement, original allocator and heap initialization, and entry wrappers. Compares heap headers with original initialization and checks allocation boundaries and preserved ARM registers. Collision dependencies are stubbed to compare the enabled bypass and disabled original route; key/RTC and flag calls are stubbed for the readkeys wrapper. The out-of-memory callback is stubbed. |

Passing these checks establishes the tested runtime behavior with those boundaries. It does not substitute for complete gameplay coverage or an on-device Delta test.
