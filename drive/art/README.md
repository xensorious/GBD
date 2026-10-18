Coastline source artwork
=======================

`vehicles.png`, `vehicles-expansion.png`, `scenery.png` and
`scenery-expansion.png` are original transparent
sprite atlases generated for this project with OpenAI image generation. They
contain no licensed vehicle
badges or third-party game art. All vehicles face north, with their rear at the
bottom, for the high-angle overhead highway camera.

`../tools/build_assets.py` contains the curated atlas crop rectangles and converts
the artwork into transparent RGB555 sprites. It also prepares five vehicle yaw
poses, two DejaVu Sans font faces and an original synthesized music loop. Turning
poses shift the front in the ground plane while keeping rear contact level,
rather than rotating the whole upright picture. Source
art is kept at its original resolution; small native-resolution contact sheets
are written to the ignored `build/` directory for visual review. The refined
world art emphasizes dimensional buildings, roofs and foliage silhouettes.
`scenery-original.png` retains the first scenery set for reference.

Three offline filtered scenery levels (48x72, 32x48 and 16x24) use the same
normalized framing as the 64x96 base sprites to reduce shimmer in motion.

Player vehicles: Pacer hatchback, Vela GT coupe, Nomad pickup, Kestrel motorcycle,
Vector supercar and Sundown muscle car; the expansion adds Mariner roadster,
Regent luxury sedan, Rallye sport wagon and Tempest exotic. Traffic: blue/white sedans, red van,
yellow taxi, white delivery truck and blue coach.

Scenery: glass office, brick building, warehouse, palm, pine, deciduous tree,
tropical shrub, sandstone rock, street lamp, gantry, billboard and yacht.
Expansion scenery: mangrove, citrus tree, neon hotel, wetland reeds, beach villa
and wooden harbor pier.
