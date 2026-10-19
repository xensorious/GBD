# Coastline cartridge validation

`python3 drive/tests/playthrough.py --native-only` builds portable rules with
GCC AddressSanitizer and UndefinedBehaviorSanitizer. Ten suites cover driving,
score receipts, six-level parts, effective tuning, mastery/career/perks, traffic
following and lane safety, thirteen-region road continuity, settings and v1/v2/v3
battery-save integrity. The handling suite verifies exact quarter-step combos
over 48 seconds at full speed and 24 seconds of drift/wheelie, all Focus ranks,
controlled turn-in/countersteer/recovery for every chassis, launch grace,
stopped/reversing patrol capture, pause safety and one-time banked rewards.
Traffic contact is compared with an independent packed-pixel oracle for all 31
silhouettes over NPC types, yaw, depth and lateral offsets, including transparent gaps, then
tested for swept cut-ins between simulation updates. The renderer suite checks
one-point wall/roof geometry and separate frontage/plant/dock/boat spawn rules.
The style suite checks indexed assets, material-protected color changes,
per-vehicle livery/hardware combinations and cold/warm cache identity across
all 31 silhouettes and steering poses. The finite-turbo suite verifies the exact
eight-second
capacity, depletion latch, 10% activation threshold, bounded combo recharge,
near misses, sustained drifts/wheelies, precision recovery, collision rollback
and paused-state integrity. The graphics suite compares cached rendering, palettes,
clipping, scale/LOD and cache-eviction fallbacks with independent reference
images. Every RGB555 color under all six traffic paints is also compared with
the original color formula, independently of the optimized lookup tables.
`legacy_save.c` deliberately passes an exact 100-byte
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

- `python3 drive/tests/handling.py` uses a labelled battery profile with durable
  Pacer parts and Recovery, leaving its engine/grip/Focus stock. Ordinary buttons
  demonstrate launch grace, a rising patrol warning, pause freeze, a stopped-car
  capture and itemized ticket, safe Slow driving, controlled drifting and rapid
  recovery, quarter-step full-speed combos, an unharmed close traffic pass and
  genuine visible chassis contact. Collision evidence records the real health,
  projected body gap and screenshot; the independent native pixel oracle
  validates the geometry used to interpret these observations.
- `python3 drive/tests/scenery.py` uses a clearly labelled native-generated
  review profile to capture all 13 regions and all 31 showroom vehicles, then drive
  all 31 silhouettes and two extra skins for six representative dashboard families
  (43 driving/dashboard cases). Every novelty vehicle is driven. Buttons exhaust
  the turbo, hold the depleted control, recharge
  through precision driving and a sustained drift, reactivate boost, and recover
  fuel through a sustained motorcycle wheelie in a fresh run after spending
  boost. The proof requires 30 held ticks, the wheelie recharge source and a
  positive fuel delta. Durable Nomad and Kestrel setups are
  explicitly fixture funded. Nearby upright profile boat and shore-connected dock photos
  are timed from deterministic world hashes during ordinary driving; their
  anchors and depths are recorded. The runner also applies parts/tunes/perks
  with buttons, verifies a fresh process reload, and measures actual update
  cadence for each region and cluster. Read-only renderer material samples at
  ordinary Departures visits verify previews retain the selected biome before
  and after global frames 5250/10500, without adding an idle simulation tour.
- `python3 drive/tests/expansion.py`, after scenery, tests all five fleet-count
  class gates with actual prerequisite purchases, atomic per-vehicle style
  purchases, free reapplication, and the highest vehicle ID's style/recovery
  fields. A labelled fixture with two prior Dispatch completions earns one more
  through ordinary driving, banks its award exactly once, opens the third-
  completion location/Segway gates, and verifies optional free drive. The report
  also includes the matching-ROM scenery preview evidence. Every case reloads
  its battery profile in a separate process. To run these independent cases
  alongside the scenery tour, use `--cases-only`, then `--aggregate-only` after
  scenery completes; aggregation requires every same-ROM case and reload to pass.
- `python3 drive/tests/migration.py` boots a generated v1 fixture in the original
  cartridge, then loads it through the revised cartridge's normal boot path and
  verifies the migrated v3 battery save reloads in a separate process. Missing
  baseline files are reproducibly prepared by `setup_legacy.py` from the trusted
  original release commit under `drive/build/legacy-baseline/`. An existing
  baseline may be supplied with `--baseline PATH`.
  The same runner also boots a detailed 204-byte v2 fixture in the previous
  coastal release, preserves every old field in the expanded schema, and verifies another
  fresh-process reload. `setup_legacy.py --release coastal` prepares its pinned
  ROM/ELF pair in `drive/build/coastal-baseline/`; supply `--v2-baseline PATH` to
  reuse an existing verified pair. Money, all ten cars' parts/mastery/tuning,
  career perks, records, departures, paint and audio/interface settings must be
  unchanged immediately after the upgrade.
  It also repeats semantic preservation against the turbo and handling
  releases. `setup_legacy.py --release turbo` prepares that verified ROM/ELF pair;
  `--turbo-baseline PATH` may reuse an existing pair. Use `--release handling`
  and `--handling-baseline PATH` for the fourth published baseline. All imports
  retain the original 512-byte physical chip. New profiles use a 500-byte packed
  disk format and 596-byte RAM view; independent field/bit/checksum checks and
  fresh cartridge boots verify the profile's tail survives without chip enlargement.
- `python3 drive/tests/transition.py` drives the full coast-to-causeway boundary
  from distance zero with ordinary buttons. The touring vehicle's parts and
  perks are documented fixtures; its travel distance is never injected.

Captures, motion GIFs and matching-ROM JSON reports go under
`drive/build/validation/`. Fixture progress is never described as earned by the
stock-profile playthrough.

The interpreter is intentional: Debian's old DeSmuME core has an ARM7 FIFO/JIT
regression. `setup_emulator.sh` installs the local core without system package
changes. No copyrighted BIOS or firmware files are needed for homebrew boot.
