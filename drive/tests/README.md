# Coastline cartridge validation

`python3 drive/tests/playthrough.py --native-only` builds portable rules with
GCC AddressSanitizer and UndefinedBehaviorSanitizer. The suites cover driving,
score receipts, six-level parts, effective tuning, mastery/career/perks, traffic
following and lane safety, eight-region road continuity, settings and v1/v2
battery-save integrity. The finite-turbo suite verifies the exact eight-second
capacity, depletion latch, 10% activation threshold, bounded combo recharge,
near misses, sustained drifts/wheelies, precision recovery, collision rollback
and paused-state integrity. The graphics suite compares cached rendering, palettes,
clipping, scale/LOD and cache-eviction fallbacks with independent reference
images. `legacy_save.c` deliberately passes an exact 100-byte
legacy object so ASan catches migration code that reads a larger v2 save too
early.

`python3 drive/tests/playthrough.py` additionally boots the shipped ARM9/ARM7
cartridge in the DeSmuME libretro interpreter. Navigation and driving use
joypad callbacks. The only pointer input asserts that touch is ignored. RAM is
observed read only through ELF symbols. No RAM writes, cheats, checkpoints or
simulated screenshots are used. The runner captures real dual-screen output,
checks independent music/effects settings against the actual audio stream,
earns run XP/cash, checks the receipt and any affordable first part purchase,
and reloads the battery save in a separate process.

After that stock-profile run:

- `python3 drive/tests/scenery.py` uses a clearly labelled native-generated
  review profile to capture all eight regions and all 30 vehicle/skin dashboard
  combinations. Buttons exhaust the turbo, hold the depleted control, recharge
  through precision driving and a sustained drift, reactivate boost, and recover
  fuel through a motorcycle wheelie. Durable Nomad and Kestrel setups are
  explicitly fixture funded. Nearby broadside boat and horizontal pier photos
  are timed from deterministic world hashes during ordinary driving; their
  anchors and depths are recorded. The runner also applies parts/tunes/perks
  with buttons, verifies a fresh process reload, and measures actual update
  cadence for each region and cluster.
- `python3 drive/tests/migration.py` boots a generated v1 fixture in the original
  cartridge, then loads it through the revised cartridge's normal boot path and
  verifies the migrated v2 battery save reloads in a separate process. Missing
  baseline files are reproducibly prepared by `setup_legacy.py` from the trusted
  original release commit under `drive/build/legacy-baseline/`. An existing
  baseline may be supplied with `--baseline PATH`.
  The same runner also boots a detailed 204-byte v2 fixture in the previous
  coastal release, loads it unchanged in the new cartridge, and verifies another
  fresh-process reload. `setup_legacy.py --release coastal` prepares its pinned
  ROM/ELF pair in `drive/build/coastal-baseline/`; supply `--v2-baseline PATH` to
  reuse an existing verified pair. Money, all ten cars' parts/mastery/tuning,
  career perks, records, departures, paint and audio/interface settings must be
  byte-identical immediately after the upgrade.
- `python3 drive/tests/transition.py` drives the full coast-to-causeway boundary
  from distance zero with ordinary buttons. The touring vehicle's parts and
  perks are documented fixtures; its travel distance is never injected.

Captures, motion GIFs and matching-ROM JSON reports go under
`drive/build/validation/`. Fixture progress is never described as earned by the
stock-profile playthrough.

The interpreter is intentional: Debian's old DeSmuME core has an ARM7 FIFO/JIT
regression. `setup_emulator.sh` installs the local core without system package
changes. No copyrighted BIOS or firmware files are needed for homebrew boot.
