# Building and extending the project

The release uses a ROM-resident ARM7TDMI Thumb handler and a four-line Action Replay heartbeat. A pure RAM installer was rejected because normal gameplay resets its heap. The examined Delta AR v3 engine also has only four reversible halfword ROM patch slots.

## Build

Requires Python 3.10+ and Zig **0.13.0**. Supply your own matching original ROM; any other SHA-256 is rejected.

```sh
ZIG=/path/to/zig sh tools/build.sh /path/to/original-amethyst.gba
python3 -m unittest discover -s tests -v
```

This writes `build/runtime.elf`, the BPS patch, activation text and manifest. It leaves the input untouched and emits no patched ROM by default. For private local testing:

```sh
python3 tools/build_release.py /path/to/original-amethyst.gba \
  --patched-copy /path/to/private-patched-amethyst.gba
```

Keep ROMs, saves and credentials outside version control.

## Runtime layout

The payload occupies verified-unused ROM near `09FF0000`; entry thunks and guard pointers occupy `09FF1000–09FF1073`. The builder checks exact hook bytes, requires injected regions to have been FF, and validates ELF layout and Thumb pointers. Its BPS output has source/target/patch CRCs and is independently applied for a byte-for-byte round trip.

InitHeap reduces the standard heap from `1C000` to `1BE00` bytes. State occupies the excluded 512 bytes at `0201BE00`. The hook updates original allocator globals `03000A38/03000A3C` and calls the original free-header initializer. Heap resets preserve state; an EWRAM reset initializes fresh state.

ReadKeys chains the game's RTC/keypad routine, consumes one heartbeat, and recognizes single-button taps. The maximum gap is 120 frames. Item commands call the game's inventory function with quantity one. Shiny uses an existing game flag. Walking returns no collision while active and replays the original entry when off.

Battle commands 0B, 0C, 19, 79 and F4 route through guards for positive HP damage, zeroing, direct subtraction and faint checks. Foe damage and healing remain normal. Position parity identifies both player banks. Faint tracking checks party identity before restoring a previously living bank. This is engine-path protection, not blanket healing each frame.

The Delta code checks the patched ReadKeys pointer and installed-state magic, then writes a heartbeat. Turning it off clears walking/protection and restores the prior shiny option. It consumes no AR ROM patch slots.

See [VALIDATION.md](VALIDATION.md) and [validation/README.md](validation/README.md) for tests and stub boundaries. Device testing remains outstanding.
