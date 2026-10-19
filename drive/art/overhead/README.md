Free Roam overhead vehicles
==========================

The three `source-*.png` atlases contain new, original image-generated plan-view
paintings of all 48 garage vehicles, in vehicle-ID order. These are independent
overhead designs rather than flattened chase-camera sprites. Crops retain full
hoods, roofs, open cabins, truck beds and model-specific bodywork.

`vehicles.png` holds the reviewed 48x72 RGBA sources, eight columns by six rows.
`materials.png` is a matching indexed material mask: 0 protected art/cabin/glass,
1 enamel or customizable equipment, 2 finishable metal, 3 original lamp lens,
4 rubber/dark protected trim. Its palette is for editing only. The accompanying
JSON binds each mask to the exact source RGBA hash. Normal builds consume these
checked-in masks; they do not infer materials from source hues.

`tools/author_overhead.py` and `tools/overhead_materials.py` reproduce the offline
crops and measured openings. Review both artwork and masks after using them.
`tools/overhead_assets.py` fits the four existing whole-vehicle livery paths to
enamel and authors three front-axle poses. Paint never crosses a protected hole.
Each model has a full 256-entry RGB555 material palette. All three indexed poses
share one compressed model bank; the runtime decodes one bank and retains twelve
finished, rotated sprites. Four source samples per destination pixel stabilize
small details through continuous heading changes without upscaling the world.

The native city suite validates every source-pose material, every body color and
livery-color combination, cold/warm steering caches and world-fixed lane paint.
`overhead-review.ppm` is a native-renderer diagnostic, not an emulator screenshot.
