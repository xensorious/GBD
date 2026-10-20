# Free Roam optimization

The target is the same game, sprites, lighting, traffic, camera and controls at
lower cost. No draw-distance, resolution, animation-rate or traffic reductions
are part of this change. The simulation still advances at 30 updates per second.

## Cause

The previous build uploaded a new full-color player texture whenever steering
changed. Uploading remapped texture VRAM and waited for a safe LCD blanking
window. These waits combined with scene preparation and dashboard work to miss
occasional presentation deadlines. A read-only trace of the old cartridge found
six of eight long preparation gaps immediately following an upload; none
coincided with a shadow-cache rebuild. The separate screen-output audit recorded
nine 66.7 ms intervals on its extended route.

Static lot geometry was also recomputed repeatedly, including road clipping for
collision corners and decorative footprints. Cars outside the final camera view
could still run their 35-point shadow sampling before their polygons were
rejected. Many identical texture, palette, polygon and color commands were sent
again for successive polygons.

## Changes

- Keep all three original 48×72 steering drawings in VRAM. Their source indices
  share the exact painted RGB555 palette, preserving every color and transparent
  pixel. Steering selects an atlas region without decoding, uploading or
  remapping texture memory.
- Decode NPC indices directly during the initial upload. The old path also
  expanded every NPC into an unused full-color image.
- Memoize immutable lot bounds in a compact 5,120-byte table. It fills on demand
  and preserves the original bounds and boundary rules.
- Reject fully off-screen vehicle silhouettes before lighting work. Conservative
  bounds include the car, its raised body and its projected shadow.
- Cache graphics attributes within each scene; send only changes. Solid polygons
  skip irrelevant texture-coordinate commands. Geometry order, vertices, UVs,
  tint values, alpha and polygon IDs retain their original meaning.

The pose atlas fits in the existing texture banks. Replacing the old temporary
full-color image also frees enough main RAM for the lot cache; this does not
introduce a larger streamed working set.

## Verification

| Extended route measurement | Previous build | Optimized build |
| --- | ---: | ---: |
| Steering texture uploads | 695 | 0 |
| Displayed-frame 95th percentile | 50.0 ms | 33.3 ms |
| Worst displayed-frame interval | 66.7 ms | 50.0 ms |
| Intervals of 50 ms or longer | 219 / 4,083 | 16 / 4,071 |
| Render preparation 95th percentile | 28.6 ms | 18.7 ms |
| Resident RAM headroom | 25,384 bytes | 29,160 bytes |

The four additional short routes, covering a compact car, heavy SUV, novelty
vehicle and motorcycle, never exceed 33.3 ms. The long route still has sixteen
50 ms intervals; this is a measured improvement, not a claim of perfectly flat
timing in every view. Approximately 80.4% of repeated graphics-attribute writes
are eliminated. The pose uploads and rendering budgets are measured in emulated
DS time, not desktop wall-clock throughput. Waypoint routes use the same
controller-driven policy; later adaptive-route positions can differ slightly.

`PERFORMANCE.json` in the download contains exact ROM hashes, timing results and
comparison evidence. Tests use DS emulator execution and read-only observations;
physical Delta/iPhone/DS performance is not claimed.

The native before/after comparison checks 248 scenes, 24,379,392 pixels and exact
simulation/save bytes. The dedicated VRAM comparison checks all three poses for
eight representative vehicles with different paint, livery and wheel colors,
against the previous cartridge's full-color textures. Five paused DS top-screen
captures also match their previous-build pixels exactly. The lighting audit checks
192 camera/district cases for visibility and static-light flicker. Extended and
short button-driven routes retain the strict timing assertions. General checks
cover city geometry, shadows, vehicles, saves, interfaces, both driving modes
and the story renderer's shared memory lease.
