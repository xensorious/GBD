Coastal city environment
========================

Free Roam now uses the DS 3D engine with an orthographic, 60-degree-depression
camera. Its heading matches the player's heading, so the car always points
screen-up. The map, XY physics, collisions, traffic and progression are unchanged.
The lower map stays north-up. World height is separate from world Y and depth;
roofs, walls, streetlight poles and bridge rails are genuine geometry.

facades-source.png contains sixteen original orthographic wall elevations.
Their windows, trim, doors and awnings carry no baked isometric perspective.
tools/city_mesh_assets.py crops them to 64x32 facades and 32x32 return walls,
adds straight roof materials and uppercase caption glyphs, then packs them
alongside the existing overhead canopies, gardens, boats and ground materials.
A shared 256-color RGB555 palette and a 256x512 indexed atlas occupy exactly
128 KiB in texture bank A. Roof materials are mapped onto pitched or flat roofs;
gables close the ends, while directional wall shades stay fixed in world space.
Sixteen district-weighted styles retain the previous coastal building variety.

Texture bank D holds 48 downsampled 32x64 indexed NPC vehicle paintings
(98,304 bytes), plus a 64x128 direct-color player texture (16,384 bytes).
The latter retains the reviewed 48x72 painted source and steered-axle poses,
using the existing material-protected paint/livery/wheel renderer. Palette
bank E uses 512 bytes for the environment and 24,576 bytes for NPC palettes.
Texture uploads occur with banks mapped to LCDC during a vblank transfer
window. The platform restores normal bitmap/movie banks on leaving Free Roam;
a generation counter forces full texture reloads on re-entry. No GL heap
allocator or software rotating framebuffer is required on the cartridge.

The original buildings-source.png silhouettes remain available as source and
native asset diagnostics; they are no longer drawn as flat city buildings.
The original garden and grass sources still supply flowers, hedges, planters,
courtyards and world-fixed ground texture. Rooted wind poses move foliage;
raised canopy planes sit on simple trunks. Overhead boats lie in the canal,
attached docks have planking and raised end posts, and gulls have wingbeats.
Decorative footprints still reject roads, water and solid properties.

GPU street layers use explicit depth separation, including canal paving,
water, sidewalks, asphalt, lane paint and crosswalks. This avoids DS coplanar
striping without moving the paint away from its world coordinates. Asset
sections are individually garbage-collected, so unused diagnostic texture
banks do not consume cartridge runtime RAM.

city_mesh.inc shares its fixed-point vertices with a native depth-buffered
reference rasterizer, excluded from ARM9 builds. Native city tests cover all
256 camera headings, translation cancellation, vertical height, rotated ground
overlap at 24 cardinal views, map connectivity, material masks, gardens and
saves. Actual button-only cartridge tests drive neighborhood turns, cross the
bridge, turn a full reverse circle through the heading wrap, verify hardware
geometry budgets/overflow flags, switch rendering modes and reload a battery
save. Native review scenes are explicitly labelled separately from emulator
screenshots. No commercial game art is used.
