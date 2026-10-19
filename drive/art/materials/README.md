This is an indexed semantic atlas, not vehicle color artwork.

- 0: original pixels (glass, rubber, interiors, riders, trim).
- 1: painted panels or the novelty vehicle's customizable equipment.
- 2: wheel/exposed metal finish.
- 3: original lamp lens, including white reverse lamps.

Each cell is exactly 48x72; IDs run left to right in 8 columns and 6 rows. The
packer reads palette indices directly, validates the source hashes in the JSON,
and carries the same labels through all five steering poses. It never creates
a material region from a color threshold or a bounding rectangle.

To intentionally re-author a mask, edit its measured shapes in
`../../tools/vehicle_material_shapes.py`, run
`python3 tools/author_vehicle_materials.py` from `drive`, and inspect dark-paint
and livery renders before building. This tool's cold contour/edge refinements
are baked into the checked-in PNG; normal builds do not execute them.
