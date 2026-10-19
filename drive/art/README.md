Coastline source artwork
=======================

All source sprites and architectural textures in this directory are original
art generated for this project with OpenAI image generation. They contain no
licensed vehicle badges or third-party game art. Source files retain their
original resolution; the packer crops, resizes, filters and quantizes them for
the Nintendo DS. Native review sheets are written to ignored `build/` files.

The vehicle library
-------------------

`vehicles.png` supplies the original six player vehicles and six traffic models;
`vehicles-expansion.png` adds the four original garage expansion vehicles.
`vehicles-roster.png` adds fifteen distinct silhouettes, including a compact
hatch, coupe, panel van, armored SUV, sedan, long GT, rally coupe, wedge supercar,
streamlined GT, roadster, offroader, hypercar, luxury GT, electric track car and
performance truck. `vehicles-novelty.png` adds a roller skater, bicycle and rider,
moped and rider, Segway rider, skateboarder, and horse with rider. All 31 models
face north with their rear at the bottom for the highway chase camera.

Five ground-plane yaw poses use offsets `-9, -5, 0, 5, 9`. Normal steering uses
the shallow poses and committed drifts use the stronger poses. Sharing lateral
displacement between nose and rear keeps the same 48x72 canvas without banking
or clipping. Curated source crops exclude neighboring sprites: the roster's
middle row ends before the next row's roofs, and the moped/horse use independent
vertical bounds rather than a uniform grid boundary.

Each vehicle owns a 256-entry RGB555 palette and five indexed 8-bit poses.
Index zero is transparent. Separate palette partitions preserve material tags:
protected glass, skin, hair and horse fur; paintable body/clothing/frame areas;
and finishable rims, trim or novelty hardware. The latter includes scooter
trim, skates, bicycle hardware, skateboard trucks and the rider's stirrups.
Only the current pose is decoded to a small RGB555 runtime bank. The ignored
`build/vehicle_art.bin` is an exact decoded native-test oracle, not a cartridge
asset. `vehicle_geometry.h` contains per-row spans and exact occupancy masks
from the final decoded alpha, so collision follows the visible vehicle.

Architecture and water
----------------------

`building-elevations.png` supplies the original five flat front/end elevations
and overhead roofs. Four new atlases add twenty architectural styles:

- `buildings-neon-jungle.png`: arcade, noodle bar, neon office, nightclub, lodge.
- `buildings-jungle-snow.png`: jungle hut, bayou cabin, boathouse, chalet,
  observatory pavilion.
- `buildings-snow-coast.png`: snow lodge, salt pavilion, solar service station,
  coastal duplex, pool villa.
- `buildings-coastal-civic.png`: motel, cafe, marina shed, apartment, tall hotel.

Every style packs a 64x64 front/long wall, 32x64 end wall, and 32x32 roof.
Filtered walls at 32x32/16x16 and ends at 16x32/8x16 reduce distant shimmer and
texture working sets. All faces share one indexed palette per building. Wall
crops exclude source gables/sky: actual box and pitched-roof geometry supplies
position-dependent one-point perspective, while vertical edges remain vertical.

`boats-neon-palm.png` supplies four broadside boat designs. `cabin-boat.png`
is the isolated final cabin-boat source; `neon-palm.png` isolates the complete
illuminated palm to avoid neighboring atlas fragments. Each boat is a 64x48
indexed upright profile with an explicit waterline anchor. Hulls and cabins
rise above the water; no embedded water, wake, reflection or shadow is packed.
`scenery-grounded.png` supplies uninterrupted central pier planking for the
opaque 64x32 dock module; one or two modules can meet without transparent gaps.

Layered roadside objects
------------------------

`scenery.png`, `scenery-expansion.png` and `scenery-grounded.png` supply the
original trees, shrubs, rocks, lamps, mangrove, citrus and reeds.
`biome-props.png` adds vending machine, neon sculpture, utility pole, banana,
bamboo, fern, jungle canopy, rainflowers, vines, cypress, dead tree, rushes,
fallen log, spruce, birch, snow rock/bush/bank, crystals, solar array, salt mound,
scrub and flamingo. The isolated neon palm completes the 24 new logical props.

Logical IDs remain stable across all 42 props. Only 33 active upright sprites
are packed; unused building cards, overhead signs, billboard, overhead boat and
old pier cards consume no runtime texture space. `prop_slot` maps logical IDs
to compact sprite slots. Rock and lamp retain two mirrored indexed variants;
their palette is shared with the original. Root/pole contacts and exact bounds
remain separate metadata, independent of canopy centers or transparent padding.
The 64x96 base and filtered 48x72, 32x48 and 16x24 levels share each object's
palette. All transparent palette entries remain index zero.

The complete packed art, typography and original synthesized music total
1,456,312 bytes. Source-only references such as `scenery-original.png` and the
old `boat-profile.png` remain available without being linked into the game.
