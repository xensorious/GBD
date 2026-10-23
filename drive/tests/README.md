# Coastline cartridge validation

Run the focused cast checks after `make -C drive` against the final ROM:

- `python3 drive/tests/dialogue_pixels.py` compares native 256x264 actor canvases
  with an independent pixel oracle, including clipping, fades, lighting, reduced
  secondary portraits, the continuous 256x288 LCD seam and lower 96 dialogue rows.
  ASan/UBSan guards the shared 302,592-byte one-background/two-actor lease inside
  the existing 307,200-byte driving cache.
- `python3 drive/tests/campaign.py --native-only` traverses all 125 scenes,
  923 messages and 929 pages. It checks exact font wrapping, reveal/advance rules,
  stable shot/speaker across pages, pause, replay, abort, progress and saves.
- `python3 drive/tests/story_assets.py` checks the 54 backgrounds and 218 portrait
  records: the preceding 76 portraits and 142 authored directional full bodies.
  Authored references and gallery availability
  are checked separately; unused review poses are not claimed as story content.
- `python3 drive/tests/character_replacement.py` retains Nia's source/crop
  approvals and checks the intentional restyles against their own sources. The
  previous native archive pins all 54 backgrounds and ten Nia payloads.
- `python3 drive/tests/quarter_assets.py` pins all 76 old portrait payloads and
  54 backgrounds to the published cast-overhaul ROM. It checks all 142 explicit
  source/crop approvals against independent alpha masks, source direction maps,
  complete gallery coverage and the unchanged 923 encoded dialogue/shot records.
  Every frame has an explicit source/native visual review and a pinned complete
  native palette/pixel payload hash; large discarded
  components have separate notes. `--freeze-approval` creates pending review
  markers and 28 source/retained/discarded/ROM contact pages; it does not approve
  the art. Alpha extraction uses the original alpha channel at threshold 96,
  keeps the largest connected figure and never color-keys red clothing.
- `python3 drive/tests/newquarter_staging.py` checks stable speaker sides, shared
  floors, silhouette fitting and the transient facing/comparison controls. Its
  `--geometry-only` mode is explicitly a pre-art check, not complete validation.
- `python3 drive/tests/cast_gallery.py` checks the 28-person Cast Studio browser,
  pose cycling, unchanged saves/story record, guarded drawing and cache release.

`python3 drive/tests/continuous_story.py` is the actual-cartridge scene runner
for D01, D04, D05, D24 and S17, including a real page flip and pause/resume.
Controller input drives DeSmuME; RAM is read only. Later-scene batteries are
labelled review fixtures, not earned progression. The focused bundle's
VALIDATION.json identifies reports for its exact ROM. The final directional build
passes 38,934 native campaign checks, 4,873 dialogue pixel checks, 4,632,846 Cast
Studio checks and 246,580,775 real-art standing-staging checks across 2,145
configurations on eight floors. The actual DeSmuME gallery tour captures 213
views, confirms passive touch and returns to both driving modes. The five-scene
story sample passes 48 messages on 49 pages, one real page flip and pause/resume;
both tours report zero archive failures.
All 142 quarter source alpha oracles and reviewed native payload hashes pass.
All 54 backgrounds, 76 legacy portraits and 923 encoded message/shot records
match the published cast build byte for byte. Nia's earlier approvals and all
66 wardrobe/concept alpha oracles also pass. Results are in `build/quarter-update`
and the [focused preview](../previews/three-quarter-cast/README.md).
The broader driving release matrix was not rerun, and physical Delta/DS testing is not claimed.

`python3 drive/tests/playthrough.py --native-only` builds portable rules with
GCC AddressSanitizer and UndefinedBehaviorSanitizer. Fifteen suites cover driving,
score receipts, six-level parts, effective tuning, mastery/career/perks, traffic
following and lane safety, thirteen-region road continuity, settings and v1/v2/v3/v4
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

For the interface/garage release, use this exact-ROM sequence:

```sh
python3 drive/tests/playthrough.py --output drive/build/studio-validation
python3 drive/tests/studio.py
python3 drive/tests/opening.py
python3 drive/tests/migration.py --output drive/build/studio-validation
python3 drive/tests/studio_migration.py
python3 drive/tools/package.py
```

`studio.c` checks atomic cosmetic payments, per-vehicle repeat-install discounts,
paid preset loads, owned-only browsing, five Dealer class lists and all menu/room
renders under ASan/UBSan. Fusion additionally checks every tile's directional
neighbors and boundaries, all nine settings rows, L/R page cycling, value
editing, controls/pause return paths, battery persistence, and pixel-exact
Midnight styling outside the two hybrid layouts. It can also produce native
diagnostic PPM images by passing an output directory; those are not represented as emulator screenshots.

`studio.py` creates explicitly funded review profiles for Apex, Signal,
Midnight and Fusion. Each actual-cartridge case visits all menus, browses all 31 models in
class lists, buys a car, applies parts/builds, installs all four cosmetic kinds,
switches away and back at half price, saves and reloads a paid preset, drives,
pauses, banks a run and reloads its real battery file in a fresh process. All
13 departure interiors are captured through the route library. Touch input is
verified passive. The Fusion case also exercises four-direction navigation,
page wraparound, continuous list scrolling and selection through Settings.
Fixture progress is never described as earned gameplay.

`migration.py` verifies v1 and three published v2 cartridges accept their own
fixtures first, then checks the new cartridge preserves them on the original
512-byte source chip before importing Save 1 into the new 8 KiB bank. `studio_migration.py` additionally creates a rich
31-vehicle v3 profile using the pinned previous release, boots that released
ROM before migration, preserves every existing field except the retired
color-only skin selection, initializes new history/smoke/preset fields, and
reloads the new battery file in another process. Source/ROM/ELF baselines are
verified against published hashes, with reproducible setup via `setup_legacy.py`.
The current profile is 708 bytes in RAM and 500 bytes on disk, inside independently checked 512-byte records in an 8 KiB three-profile bank.

Packaging requires matching SHA-256 values for the final ROM, all fifteen native
suites, the stock playthrough, four interface tours, all five prior-release migration cases, and the opening
and three-profile persistence checks. Images in the package come from this ROM's emulator output.
The broader handling/scenery/expansion/transition tours remain supplemental;
they are not claimed as rerun evidence for this menu-focused update.

The interpreter is intentional: Debian's old DeSmuME core has an ARM7 FIFO/JIT
regression. `setup_emulator.sh` installs the local core without system package
changes. No copyrighted BIOS or firmware files are needed for homebrew boot.

`opening.c` checks independent slot records, corruption containment, legacy Slot 1
import, button gating, guarded tall-camera drawing and a pixel-exact transition
endpoint against the existing menu. The tall framebuffer uses spare LCDC VRAM
on the actual DS rather than reserving another 192 KiB of main RAM.

`opening.py` boots the actual cartridge without bypassing its title, verifies
touch is passive and X/Y both open the profile selector, captures the two-screen
animation and eased slide, earns a run on Save 1, configures distinct preferences
in all three slots, switches back to the first profile and reloads all three in
fresh emulator processes. No RAM writes or save-state restores are used.


The Free Roam visual update adds exact overlap comparisons for 288 camera
translations across road endpoints and junctions, including the old 47/48-pixel
dash-phase seam. White highway dashes must remain inside drivable pavement.
The city suite checks all 144 overhead source poses and all existing finish
combinations against native-only material/index oracles, and checks finished
steering sprites on cold and warm cache paths. Review sheets show factory,
Black, Graphite, White and all four liveries for all 48 painted plan views.


The living-city scenery tests add all sixteen building and twenty-four garden
frames to the independent graphics oracle, including 80x94/96x96 cold and warm
cache draws, edge clipping and 97px fallback paths. Garden acceptance is
compared against individual drivable/water/solid pixels. All architectural
styles must be used, wind poses retain their roots, and the repeated lawn
rows and opposite texture edges must match. Six native district scenes are
explicit diagnostic views; cartridge screenshots still come from button-only
DS emulator playthroughs. Source hashes and both DS memory budgets are checked
in `city_environment.py`.


Rotating Free Roam adds a shared fixed-point 3D mesh and a native depth-buffered
reference backend (excluded from the ROM). The city suite verifies all 256
heading projections, upward vehicle alignment, world-translation cancellation,
vertical heights and pixel-exact ground overlap at 24 rotated cardinal views.
Sixteen positioned native views show walls and pitched roofs around a full turn.
The button-only cartridge tour additionally captures diagonal junction turns,
a complete reverse circle including the 255/0 heading wrap, DS polygon budgets
and latched underflow/overflow flags. It re-enters the same customized car,
returns to the bitmap highway and reloads the resulting shared battery profile.
The mesh asset test checks every texture region, source hashes, opaque straight
wall/roof coverage, palette limits, texture-bank layout and DS main/ITCM bounds.

The Free Roam refinement additionally checks 1,161,216 oriented contact cases
against independent polygon clipping, lane gaps for cars/trucks/motorcycles,
and smaller per-model drawing envelopes. The city_pixels.py helper inspects
actual emulator output at successive camera poses: it erodes road boundaries,
excludes raised objects and detects sidewalk/curb colors leaking into asphalt.
The material classifier must also find those colors on the real sidewalks,
so an empty or incorrectly calibrated classifier cannot silently pass.
The tour includes a junction sway near a formerly wrapping street corner.
