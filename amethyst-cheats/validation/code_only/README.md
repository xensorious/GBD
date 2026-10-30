# Optional checks for the Delta-only runtime

Use Python3.10+, Zig0.13.0 and Unicorn2.1.4. Supply your original matching ROM locally. No ROM is included.

From the project directory:

```sh
python3 -m venv .venv
. .venv/bin/activate
python3 -m pip install -r validation/code_only/requirements.txt
export AMETHYST_ROM=/absolute/path/to/original-amethyst.gba
ZIG=/absolute/path/to/zig sh tools/build_code_only.sh "$AMETHYST_ROM"
python3 validation/code_only/test_compiled_native_input.py
python3 validation/code_only/test_compiled_native_effects.py
python3 validation/code_only/test_disabled_guards.py
python3 validation/code_only/test_mixed_spread_bar.py
python3 validation/code_only/test_dispatch.py
python3 validation/code_only/test_clones.py
python3 validation/code_only/test_heap.py
python3 validation/code_only/test_walk_return.py
python3 validation/code_only/test_shiny_persistence.py
python3 validation/code_only/test_entry_mode.py
```

The shared loader checks the ROM SHA-256 and current ELF layout, including the entry-mode helper section. Scripts are separate from the dependency-free tests/ suite. The shiny persistence regression executes the actual cleanup loop, flag routines and generation code with the compiled handler; it stubs only RTC/keypad handling. Entry-mode checks exercise the actual compiled capture/debounce, key masking, callback restore, and foot/bike/battle slot selection; ordinary keypad updates are simulated there.

Input checks stub inventory/shiny calls; effects checks stub controller plumbing and original faint/healthbar callbacks as documented. Spread checks execute the original spread routine with its classification/substitute/controller dependencies stubbed. Callback checks compare original and relocated code with external callees stubbed. Collision checks stub map/stair/lower collision calls and exercise the actual result instruction. Heap checks run the actual allocator and exact patched entry, preserving registers and verifying every protected interval.

See [the validation report](../../CODE_ONLY_VALIDATION.md) for results, full-game evidence and limits. These checks do not run the actual iPhone app.
