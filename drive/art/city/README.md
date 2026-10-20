Coastal city environment
========================

These three original image-generated sources are separate from the existing
Endless Drive architecture. The sixteen building designs include stucco and
coastal homes, a cafe, florist, market, bakery, art-deco shop, apartment, office,
hotel, marina clubhouse, restaurant, garden center and workshop. Full roofs,
facades, glazing and awnings are painted into the artwork.

`tools/city_environment.py` finds complete building silhouettes in each source
column, removes disconnected generation specks and packs sixteen 64x80 indexed
sprites. They share one 256-color RGB555 palette. Building size on the map keeps
the sprite proportions; enclosed gardens fill surplus space in narrow lots.
The collision footprint remains the existing raised/fenced property boundary.

The garden source adds sixteen distinct flower bushes, hedge strips, planters,
parasols, benches, paths and water features. Eight extra 32x32 poses shear only
foliage by one texel; the outer/base rows remain fixed. Their palette is shared.
A smooth global breeze switches poses independently of camera movement.

The grass source becomes a seamless 128x32 RGB555 lawn with matched opposing
edges. Each packed row repeats three times, so an entire 256px scanline can be
copied in one span. It is sampled by world coordinates. Lawn hidden beneath a
complete street is skipped, then that exact region is filled by its sidewalk
and asphalt. The existing camera-translation tests verify the resulting image
is unchanged by the choice of clipped viewport.

District tables choose appropriate building mixes. Beds, benches and lamps
pass a road/water/solid-footprint test. Courtyards vary between garden, cafe,
pool and fountain arrangements. Butterflies, canal gulls, foliage and small
water glints provide animation without advancing gameplay randomness.

Packed source hashes, counts, crop boxes and layout information are recorded in
`build/city-environment-manifest.json`. Native city/graphics tests check every
bank, protected wind roots, garden footprints, wide caches and lawn seams;
actual cartridge tours measure both driving modes and verify saved profiles.
Native district review scenes are explicitly labelled separately from DS
emulator screenshots. No commercial game art is used.
