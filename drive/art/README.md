Coastline source artwork
=======================

`vehicles.png`, `vehicles-expansion.png`, `scenery.png`,
`scenery-expansion.png` and `scenery-grounded.png` are original transparent
sprite atlases generated for this project with OpenAI image generation. They
contain no licensed vehicle
badges or third-party game art. All vehicles face north, with their rear at the
bottom, for the highway chase camera.

`../tools/build_assets.py` contains the curated atlas crop rectangles and converts
the artwork into transparent RGB555 sprites. It also prepares five vehicle yaw
poses, two DejaVu Sans font faces and an original synthesized music loop. Turning
poses shift the front in the ground plane while keeping rear contact level,
rather than rotating the whole upright picture. Source
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
The yacht and pier are packed separately as `boat_ground` (128x64) and
`pier_ground` (64x128) for projection into the water plane. Neither texture
contains water, a cast shadow, or an upright backdrop.

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
