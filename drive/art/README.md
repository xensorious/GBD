Coastline source artwork
=======================

`vehicles.png`, `vehicles-expansion.png`, `scenery.png`,
`scenery-expansion.png`, `scenery-grounded.png` and `boat-profile.png` are original transparent
sprite atlases generated for this project with OpenAI image generation. They
contain no licensed vehicle
badges or third-party game art. All vehicles face north, with their rear at the
bottom, for the highway chase camera.

`../tools/build_assets.py` contains the curated atlas crop rectangles and converts
the artwork into transparent RGB555 sprites. It also prepares five vehicle yaw
poses, two DejaVu Sans font faces and an original synthesized music loop. Turning
poses shear the body in the ground plane while keeping tire contact level,
rather than rotating the whole upright picture. The five offsets are
`-9, -5, 0, 5, 9`: normal steering uses the shallow poses and committed drifts
use the stronger poses. Sharing the lateral shift between nose and rear keeps
the body inside the same 48x72 canvas. The vehicle atlases have been cleaned
with image generation while preserving their original vehicle identities.
Curated crops exclude adjacent vehicles, including the blue-coach fragment
previously caught beneath the white traffic sedan. Genuine dark tires, glass,
and grilles remain intact. Source
art is kept at its original resolution; small native-resolution contact sheets
are written to the ignored `build/` directory for visual review. The refined
world art emphasizes dimensional buildings, roofs and foliage silhouettes.
`scenery-original.png` retains the first scenery set for reference.

`scenery-grounded.png` supplies low-camera street-facing buildings, an upright
street light, a low rock cluster, and true overhead boat and pier textures.
Buildings have shallow roof bands and level ground footprints. Their left-side
versions face inward toward the road; mirrored right-side versions face inward
from the opposite shoulder. `prop_anchor` and `prop_side_anchor` identify each
facade's road-facing ground corner, each lamp's pole foot, and each tree's root
contact. These anchors are independent of the picture's centre and canopy.
The earlier overhead yacht and pier remain packed as `boat_ground` (128x64)
and `pier_ground` (64x128) for compatibility with rendering checks; the world
uses the new broadside `boat_profile` (64x48) with a waterline anchor and an
opaque `dock_segment` (64x32). The boat profile includes raised hull and cabin
detail, with no embedded water, wake, reflection, or cast shadow. The dock
texture reuses only uninterrupted central planking, so one or two modules can
join at their ends without transparent gaps.

`building-elevations.png` is a new original opaque texture atlas containing
five sets of straight orthographic front/end elevations and overhead roofs.
It packs as `building_wall_art` (64x64), `building_end_art` (32x64), and
`building_roof_art` (32x32) in glass/brick/warehouse/hotel/villa order.
These flat faces contain no isometric side walls. The renderer supplies their
position-dependent one-point perspective using actual box and roof geometry.
Offline filtered wall levels at 32x32 and 16x16, and matching end levels at
16x32 and 8x16, preserve the same geometry while reducing distant shimmer and
the texture working set in the ARM9's small data cache. They add 19,200 bytes.

The generated `vehicle_geometry.h` contains exact per-row spans and occupancy
masks from the final packed vehicle alpha. Collision therefore follows the
same visible pixels, including narrow motorcycle gaps. Native graphics checks
validate every pose and NPC silhouette, require transparent canvas edges and a
single connected body (to catch stray crop fragments), preserve dark details,
and compare the masks against the final RGB555 data.

Three offline filtered scenery levels (48x72, 32x48 and 16x24) use the same
normalized framing as the 64x96 base sprites to reduce shimmer in motion.
The seven opposite-shoulder variants include the same filtered levels.

Player vehicles: Pacer hatchback, Vela GT coupe, Nomad pickup, Kestrel motorcycle,
Vector supercar and Sundown muscle car; the expansion adds Mariner roadster,
Regent luxury sedan, Rallye sport wagon and Tempest exotic. Traffic: blue/white sedans, red van,
yellow taxi, white delivery truck and blue coach.

Scenery: glass office, brick building, warehouse, palm, pine, deciduous tree,
tropical shrub, sandstone rock, street lamp, gantry, billboard and yacht.
Expansion scenery: mangrove, citrus tree, neon hotel, wetland reeds, beach villa
and wooden harbor pier.
