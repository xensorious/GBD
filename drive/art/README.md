Coastline source artwork
=======================

`vehicles.png` and `scenery.png` are original transparent sprite atlases generated
for this project with OpenAI image generation. They contain no licensed vehicle
badges or third-party game art. All vehicles face north, with their rear at the
bottom, for the high-angle overhead highway camera.

`../tools/build_assets.py` contains the curated atlas crop rectangles and converts
the artwork into transparent RGB555 sprites. It also prepares five vehicle yaw
poses, two DejaVu Sans font faces and an original synthesized music loop. Source
art is kept at its original resolution; small native-resolution contact sheets
are written to the ignored `build/` directory for visual review.

Player vehicles: Pacer hatchback, Vela GT coupe, Nomad pickup, Kestrel motorcycle,
Vector supercar and Sundown muscle car. Traffic: blue/white sedans, red van,
yellow taxi, white delivery truck and blue coach.

Scenery: glass office, brick building, warehouse, palm, pine, deciduous tree,
tropical shrub, sandstone rock, street lamp, gantry, billboard and yacht.
