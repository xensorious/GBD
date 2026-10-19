"""Pack Coastline's original artwork, typography and music for the Nintendo DS.

The source atlases are original generated artwork. Curated boxes retain the
whole bus, gantry and other tall/wide shapes where the painted grid varies.
Runtime sprites and faces use indexed 8-bit textures with RGB555 palettes;
traffic and dock planking stay RGB555. Compact .incbin declarations keep
the complete expanded library within Nintendo DS resident memory.
"""
from __future__ import annotations

import array
import json
import math
from pathlib import Path
import random
import sys

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "art"
OUT = ROOT / "build"
OUT.mkdir(exist_ok=True)
FONT_ROOT = Path("/usr/share/fonts/truetype/dejavu")
ALPHA = 110
VEHICLE_W, VEHICLE_H = 48, 72
TRAFFIC_W, TRAFFIC_H = 40, 64
PROP_W, PROP_H = 64, 96
# Front offsets emulate a shallow road-plane turn while keeping the rear tire
# contact and rear bumper level. Rotating the whole picture would tilt its
# vertical body height and make the vehicle appear to bank off the pavement.
YAW_FRONT_OFFSETS = (-9, -5, 0, 5, 9)

# Curated cells of the cleaned 1383x1137 vehicle atlas.
VEHICLE_BOXES = (
    (35, 18, 324, 370), (390, 20, 680, 370),
    (723, 8, 1008, 370), (1080, 50, 1365, 370),
    (25, 386, 327, 728), (389, 386, 679, 728),
    (720, 388, 1008, 728), (1082, 388, 1368, 712),
    (32, 732, 325, 1120), (390, 770, 681, 1120),
    (722, 730, 1008, 1120), (1082, 715, 1368, 1125),
)
# Four new garage vehicles, in ID order 6..9, in the 1269x1240 atlas.
EXPANSION_BOXES = (
    (215, 105, 595, 622), (705, 40, 1080, 656),
    (210, 675, 598, 1198), (667, 695, 1115, 1206),
)
PROP_BOXES = (
    (25, 8, 343, 402), (378, 24, 656, 405),
    (677, 65, 1076, 408), (1074, 6, 1383, 414),
    (32, 403, 333, 749), (339, 401, 680, 748),
    (699, 505, 1040, 748), (1041, 442, 1372, 724),
    (40, 752, 262, 1104), (271, 760, 728, 1091),
    (729, 760, 1067, 1093), (1105, 721, 1317, 1126),
)
PROP_EXPANSION_BOXES = (
    (0, 4, 545, 514), (566, 67, 990, 488),
    (1035, 4, 1530, 489), (4, 526, 500, 1004),
    (493, 515, 1058, 996), (1116, 495, 1462, 1011),
)
# Grounded roadside set: low camera, narrow roof bands, inward-facing facades.
# The lower boat/pier crops are true top views for water-plane projection.
PROP_GROUNDED_BOXES = (
    (0, 0, 440, 396), (440, 0, 863, 398), (864, 70, 1312, 396),
    (0, 396, 451, 797), (456, 405, 911, 794), (965, 390, 1265, 793),
    (0, 860, 530, 1135), (610, 790, 802, 1199), (972, 792, 1200, 1199),
)
PROP_GROUNDED_IDS = (0, 1, 2, 14, 16, 8, 7)
PROP_SIDE_IDS = (7, 8)
# Each style is packed as independent flat faces. Perspective is supplied by
# the renderer's geometry; no diagonal/isometric wall edges are baked here.
BUILDING_IDS = (0, 1, 2, 14, 16)
BUILDING_FACE_BOXES = (
    ((4, 7, 365, 321), (375, 8, 629, 321), (643, 12, 963, 316)),
    ((4, 332, 365, 646), (375, 332, 629, 646), (640, 331, 967, 646)),
    ((4, 660, 365, 965), (375, 660, 629, 965), (641, 660, 679, 963)),
    ((5, 981, 364, 1295), (375, 980, 629, 1295), (643, 982, 963, 1288)),
    ((4, 1310, 365, 1613), (375, 1310, 629, 1613), (640, 1306, 967, 1612)),
)
PROP_NAMES = (
    "GLASS", "BRICK", "WAREHOUSE", "PALM", "PINE", "TREE",
    "SHRUB", "ROCK", "LAMP", "GANTRY", "BILLBOARD", "BOAT",
    "MANGROVE", "CITRUS", "HOTEL", "REEDS", "VILLA", "PIER",
    "VENDING", "NEON_SCULPTURE", "UTILITY_POLE", "NEON_PALM",
    "BANANA", "BAMBOO", "FERN", "CANOPY", "RAINFLOWER", "VINE",
    "CYPRESS", "DEADWOOD", "RUSH", "FALLEN_LOG", "SPRUCE", "BIRCH",
    "SNOW_ROCK", "SNOW_BUSH", "SNOW_BANK", "CRYSTAL", "SOLAR_ARRAY",
    "SALT_MOUND", "SCRUB", "FLAMINGO",
)
VEHICLE_COUNT = 31
BUILDING_COUNT = 25
BOAT_COUNT = 4
ASSETS: list[tuple[str, str, int]] = []


def raw(name: str, data: bytes, ctype: str, count: int) -> None:
    size = {"uint8_t": 1, "int8_t": 1, "uint16_t": 2}[ctype]
    if len(data) != count * size:
        raise ValueError(f"{name}: byte length does not match declaration")
    (OUT / f"{name}.bin").write_bytes(data)
    ASSETS.append((name, ctype, count))


def pixels(im: Image.Image):
    return im.get_flattened_data() if hasattr(im, "get_flattened_data") else im.getdata()


def rgb555(im: Image.Image) -> bytes:
    values = array.array("H")
    for r, g, b, a in pixels(im.convert("RGBA")):
        values.append(0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10) if a >= ALPHA else 0)
    if sys.byteorder != "little":
        values.byteswap()
    return values.tobytes()


def silhouette_bounds(images: list[Image.Image]) -> bytes:
    """Exact visible bounds for chassis-grounded shadows; right/bottom exclusive."""
    result = bytearray()
    for im in images:
        box = im.getchannel("A").point(lambda a: 255 if a >= ALPHA else 0).getbbox()
        if box is None:
            raise ValueError("Cannot bound an empty sprite")
        result.extend(box)
    return bytes(result)


def vehicle_geometry(poses: list[Image.Image], traffic: list[Image.Image]) -> None:
    """Portable collision metadata from the final packed alpha silhouettes.

    The gameplay rules deliberately do not link megabytes of sprite artwork.
    Keeping row spans in a generated header gives rendering and collision the
    same visible shape without a second hand-maintained vehicle hitbox.
    """
    lines = ["#ifndef COASTLINE_VEHICLE_GEOMETRY_H",
             "#define COASTLINE_VEHICLE_GEOMETRY_H", "#include <stdint.h>"]
    for name, images in (("vehicle_body_spans", poses), ("traffic_body_spans", traffic)):
        height = images[0].height
        lines.append(f"static const uint8_t {name}[{len(images)}][{height}][2] = {{")
        for im in images:
            mask = im.getchannel("A").point(lambda a: 255 if a >= ALPHA else 0)
            rows = []
            for y in range(height):
                box = mask.crop((0, y, im.width, y + 1)).getbbox()
                rows.append("{0,0}" if box is None else f"{{{box[0]},{box[2]}}}")
            lines.append(" {" + ",".join(rows) + "},")
        lines.append("};")
    # A row span is a cheap broad phase, but the motorcycle's handlebar/body
    # gaps are real transparent pixels. Retain exact occupancy for the final
    # collision test so a bounding span cannot bridge an invisible corner.
    for name, images in (("vehicle_body_masks", poses), ("traffic_body_masks", traffic)):
        height = images[0].height
        lines.append(f"static const uint32_t {name}[{len(images)}][{height}][2] = {{")
        for im in images:
            mask = im.getchannel("A")
            rows = []
            for y in range(height):
                words = [0, 0]
                for x in range(im.width):
                    if mask.getpixel((x, y)) >= ALPHA:
                        words[x // 32] |= 1 << (x % 32)
                rows.append("{" + ",".join(f"UINT32_C(0x{v:08x})" for v in words) + "}")
            lines.append(" {" + ",".join(rows) + "},")
        lines.append("};")
    lines.extend(("#endif", ""))
    (OUT / "vehicle_geometry.h").write_text("\n".join(lines))


def content(im: Image.Image) -> Image.Image:
    im = im.convert("RGBA")
    box = im.getchannel("A").point(lambda a: 255 if a >= ALPHA else 0).getbbox()
    if box is None:
        raise ValueError("Empty atlas crop")
    return im.crop(box)


def contain(im: Image.Image, w: int, h: int, *, bottom: bool = False) -> Image.Image:
    im = content(im)
    im.thumbnail((w - 2, h - 2), Image.Resampling.LANCZOS)
    result = Image.new("RGBA", (w, h))
    result.alpha_composite(im, ((w - im.width) // 2, h - im.height - 1 if bottom else (h - im.height) // 2))
    return result


def ground_contact(im: Image.Image, *, inner_corner: bool = False) -> tuple[int, int]:
    """Ground reference in the final framed sprite, never the canopy centre.

    Frontage uses its road-facing corner; foliage and street lights use the
    centre of their actual foot in the lowest few opaque rows. An anchor lets
    the renderer project the contact point independently of the upright art.
    """
    mask = im.getchannel("A").point(lambda a: 255 if a >= ALPHA else 0)
    box = mask.getbbox()
    if box is None:
        raise ValueError("Cannot anchor an empty sprite")
    left, _, right, bottom = box
    if inner_corner:
        return right - 2, bottom - 1
    foot = mask.crop((left, max(0, bottom - 4), right, bottom)).getbbox()
    assert foot is not None
    return left + (foot[0] + foot[2] - 1) // 2, bottom - 1


def atlas_crops(path: Path, boxes: tuple, expected_size: tuple[int, int]) -> list[Image.Image]:
    source = Image.open(path).convert("RGBA")
    if source.size != expected_size:
        raise ValueError(f"{path.name}: curated crop atlas has unexpected size {source.size}")
    return [content(source.crop(box)) for box in boxes]


def vehicle_pose(source: Image.Image, front_offset: int) -> Image.Image:
    # All poses retain their physical height, fixed rear contact and body scale.
    # The neutral pose matches the original car art exactly.
    body = source.copy()
    body.thumbnail((36, 60), Image.Resampling.LANCZOS)
    result = Image.new("RGBA", (VEHICLE_W, VEHICLE_H))
    top = (VEHICLE_H - body.height) // 2
    result.alpha_composite(body, ((VEHICLE_W - body.width) // 2, top))
    if front_offset:
        slope = front_offset / (body.height - 1)
        # Pillow uses the inverse mapping from destination to source pixels.
        result = result.transform(
            result.size, Image.Transform.AFFINE,
            # Share the yaw displacement between nose and rear. A nine-pixel
            # drift sweep then fits the existing48px canvas without clipping.
            (1, slope, -front_offset / 2 - slope * top, 0, 1, 0),
            resample=Image.Resampling.BICUBIC,
        )
    return result


def preview(name: str, images: list[Image.Image], cols: int, scale: int = 2) -> None:
    width, height = images[0].size
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * width, rows * height), "#233b46")
    for i, im in enumerate(images):
        sheet.alpha_composite(im, (i % cols * width, i // cols * height))
    sheet.resize((sheet.width * scale, sheet.height * scale), Image.Resampling.NEAREST).save(OUT / name)


def grid_crops(path: Path, cols: int, rows: int) -> list[Image.Image]:
    source = Image.open(path).convert("RGBA")
    return [content(source.crop((round(x * source.width / cols), round(y * source.height / rows),
                                 round((x + 1) * source.width / cols), round((y + 1) * source.height / rows))))
            for y in range(rows) for x in range(cols)]


def material_class(vehicle: int, r: int, g: int, b: int, x: float, y: float) -> int:
    """Material metadata only; never alter the authored silhouette or alpha.

    Separate palette partitions allow a gray shirt and gray chrome to remain
    different materials even when their final RGB555 color is identical.
    Human skin/hair and horse fur are protected. Novelty finish means hardware,
    such as skateboard trucks, scooter trim or saddle fittings.
    """
    hi, lo = max(r, g, b), min(r, g, b)
    sat = (hi - lo) / max(1, hi)
    if vehicle >= 25 or vehicle == 3:
        if vehicle == 3 and r > 95 and g > 65 and b < min(r, g) * .55 and .10 < y < .85:
            return 1
        skin_or_fur = r > b * 1.15 and r >= g * 1.03 and g > b * 1.03
        if skin_or_fur:
            return 0
        # Saturated clothing and vehicle frames are safe; helmets, skin and
        # hair stay original. Horse blue riding jacket is separately bounded.
        if sat > .34 and hi > 65 and .16 < y < (.53 if vehicle == 30 else .91):
            return 1
        if vehicle == 28 and .24 < y < .59 and .20 < x < .80 and sat < .30 and hi > 18:
            return 1
        if vehicle == 30:
            return 2 if .30 < y < .49 and (x < .25 or x > .75) and sat < .50 and hi > 12 else 0
        if y > .70 and sat < .24 and hi > 65:
            return 2
        return 0
    # Spatially limited neutral trim is finishable; dark glass and black tire
    # rubber remain protected. Saturated painted body panels use partition1.
    if ((x < .25 or x > .75) and y > .30 or y > .86 and (x < .40 or x > .60)) and sat < .50 and 25 < hi < 235:
        return 2
    if sat > .30 and hi > 65:
        return 1
    if sat > .28 and hi > 35 and g > r * 1.15 and g > b * 1.15:
        return 1
    if .24 < x < .76 and y < .86 and hi > 165 and sat < .22:
        return 1
    return 0


def indexed_group(images: list[Image.Image], vehicle: int | None = None):
    """Quantize one logical object, keeping index0 transparent in every LOD."""
    palettes = [0] * 256
    flags = bytearray(256)
    buffers = [bytearray(im.width * im.height) for im in images]
    classes = (0, 1, 2) if vehicle is not None else (0,)
    budgets = (128, 96, 31) if vehicle is not None else (255,)
    cursor = 1
    for material, budget in zip(classes, budgets):
        values, positions = [], []
        for frame, im in enumerate(images):
            bounds = im.getchannel("A").point(lambda a: 255 if a >= ALPHA else 0).getbbox()
            assert bounds is not None
            for p, (r, g, b, a) in enumerate(pixels(im.convert("RGBA"))):
                if a < ALPHA:
                    continue
                x, y = p % im.width, p // im.width
                tag = material_class(vehicle, r, g, b, (x - bounds[0]) / max(1, bounds[2] - bounds[0] - 1),
                                     (y - bounds[1]) / max(1, bounds[3] - bounds[1] - 1)) if vehicle is not None else 0
                if tag == material:
                    values.append((r, g, b)); positions.append((frame, p))
        if not values:
            cursor += budget
            continue
        strip = Image.new("RGB", (len(values), 1)); strip.putdata(values)
        quantized = strip.quantize(colors=budget, method=Image.Quantize.MEDIANCUT,
                                   dither=Image.Dither.NONE)
        rgb = quantized.getpalette()
        for index in set(pixels(quantized)):
            r, g, b = rgb[index * 3:index * 3 + 3]
            palettes[cursor + index] = 0x8000 | (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
            flags[cursor + index] = material
        for (frame, p), index in zip(positions, pixels(quantized)):
            buffers[frame][p] = cursor + index
        cursor += budget
    words = array.array("H", palettes)
    if sys.byteorder != "little": words.byteswap()
    return [bytes(data) for data in buffers], words.tobytes(), bytes(flags)


def decode_indexed(data: bytes, palette: bytes, width: int, height: int) -> Image.Image:
    words = array.array("H"); words.frombytes(palette)
    if sys.byteorder != "little": words.byteswap()
    im = Image.new("RGBA", (width, height))
    im.putdata([((words[i] & 31) * 255 // 31, ((words[i] >> 5) & 31) * 255 // 31,
                 ((words[i] >> 10) & 31) * 255 // 31, 255) if i else (0, 0, 0, 0) for i in data])
    return im


def artwork() -> None:
    cars = atlas_crops(ART / "vehicles.png", VEHICLE_BOXES, (1383, 1137))
    expansion = atlas_crops(ART / "vehicles-expansion.png", EXPANSION_BOXES, (1269, 1240))
    roster = Image.open(ART / "vehicles-roster.png").convert("RGBA")
    additions = []
    for top, bottom in ((0, 324), (326, 630), (638, 972)):
        additions += [content(roster.crop((round(x * 1619 / 5), top,
                                          round((x + 1) * 1619 / 5), bottom))) for x in range(5)]
    novelty = Image.open(ART / "vehicles-novelty.png").convert("RGBA")
    additions += [content(novelty.crop((round(x * 1145 / 3), 0,
                                        round((x + 1) * 1145 / 3), 620))) for x in range(3)]
    additions += [content(novelty.crop((round(x * 1145 / 3), 615,
                                        round((x + 1) * 1145 / 3), 1374))) for x in range(3)]
    player_cars = cars[:6] + expansion + additions
    assert len(player_cars) == VEHICLE_COUNT
    poses, indexed, palettes, materials = [], [], [], []
    material_counts = []
    for vehicle, car in enumerate(player_cars):
        authored = [vehicle_pose(car, offset) for offset in YAW_FRONT_OFFSETS]
        frames, palette, flags = indexed_group(authored, vehicle)
        decoded = [decode_indexed(frame, palette, 48, 72) for frame in frames]
        poses.extend(decoded); indexed.extend(frames); palettes.append(palette); materials.append(flags)
        material_counts.append({"vehicle": vehicle, "paint": sum(flags[i] == 1 for frame in frames for i in frame),
                                "hardware": sum(flags[i] == 2 for frame in frames for i in frame)})
    raw("vehicle_indexed", b"".join(indexed), "uint8_t", VEHICLE_COUNT * 5 * 48 * 72)
    raw("vehicle_palette", b"".join(palettes), "uint16_t", VEHICLE_COUNT * 256)
    raw("vehicle_palette_flags", b"".join(materials), "uint8_t", VEHICLE_COUNT * 256)
    # Oracle only: never linked into the cartridge; indexed decode is exact.
    (OUT / "vehicle_art.bin").write_bytes(b"".join(rgb555(im) for im in poses))
    raw("vehicle_bounds", silhouette_bounds(poses), "uint8_t", VEHICLE_COUNT * 5 * 4)
    traffic = [contain(car, TRAFFIC_W, TRAFFIC_H) for car in cars[6:]]
    raw("traffic_art", b"".join(rgb555(im) for im in traffic), "uint16_t", 6 * TRAFFIC_W * TRAFFIC_H)
    raw("traffic_bounds", silhouette_bounds(traffic), "uint8_t", 6 * 4)
    vehicle_geometry(poses, traffic)

    prop_sources = atlas_crops(ART / "scenery.png", PROP_BOXES, (1383, 1137))
    prop_sources += atlas_crops(ART / "scenery-expansion.png", PROP_EXPANSION_BOXES, (1536, 1024))
    grounded = atlas_crops(ART / "scenery-grounded.png", PROP_GROUNDED_BOXES, (1312, 1199))
    for prop_id, source in zip(PROP_GROUNDED_IDS, grounded[:7]):
        prop_sources[prop_id] = source
    sheet = Image.open(ART / "biome-props.png").convert("RGBA")
    boxes = ((30, 0, 200, 263), (270, 0, 458, 263), (505, 0, 667, 263),
             (687, 0, 1022, 263), (1028, 0, 1280, 263), (1280, 40, 1536, 263),
             (0, 263, 326, 535), (290, 265, 568, 535), (548, 265, 782, 535),
             (782, 265, 1025, 542), (1030, 265, 1283, 542), (1286, 270, 1536, 542),
             (0, 545, 330, 750), (339, 525, 568, 782), (572, 533, 775, 782),
             (773, 620, 1024, 771), (1030, 577, 1276, 766), (1270, 644, 1534, 755),
             (0, 748, 246, 1024), (248, 800, 520, 1010), (519, 817, 772, 1024),
             (773, 810, 1028, 1015), (1034, 756, 1275, 1024), (1280, 760, 1536, 1024))
    biome = [content(sheet.crop(box)) for box in boxes]
    boats_sheet = Image.open(ART / "boats-neon-palm.png").convert("RGBA")
    boat_boxes = ((4, 322, 512, 596), (516, 88, 912, 598), (934, 453, 1370, 600),
                  (1368, 323, 1815, 602))
    boat_sources = [content(boats_sheet.crop(box)) for box in boat_boxes]
    boat_sources[3] = content(Image.open(ART / "cabin-boat.png"))
    # New logical IDs18..41, in the renderer's biome roster order.
    prop_sources += biome[:3] + [content(Image.open(ART / "neon-palm.png"))] + biome[3:23]
    assert len(prop_sources) == len(PROP_NAMES)
    props = [contain(im, PROP_W, PROP_H, bottom=True) for im in prop_sources]
    frontage = (0, 1, 2, 14, 16)
    anchors = [ground_contact(im, inner_corner=i in frontage or i == 7) for i, im in enumerate(props)]
    raw("prop_anchor", bytes(v for xy in anchors for v in xy), "uint8_t", len(props) * 2)
    unused = {0, 1, 2, 9, 10, 11, 14, 16, 17}
    active_ids = [i for i in range(len(props)) if i not in unused]
    slots = [255] * len(props)
    for slot, prop_id in enumerate(active_ids): slots[prop_id] = slot
    raw("prop_slot", bytes(slots), "uint8_t", len(props))
    side_ids = (7, 8)
    side_slots = [255] * len(props)
    for slot, prop_id in enumerate(side_ids): side_slots[prop_id] = slot
    raw("prop_side_slot", bytes(side_slots), "uint8_t", len(props))
    levels = (64, 48, 32, 16)
    by_level = {width: [] for width in levels}
    side_level = {width: [] for width in levels}
    prop_palettes = []
    for prop_id in active_ids:
        images = [props[prop_id].resize((w, w * 3 // 2), Image.Resampling.LANCZOS) for w in levels]
        frames, palette, _ = indexed_group(images)
        prop_palettes.append(palette)
        for w, frame in zip(levels, frames):
            by_level[w].append(frame)
            if prop_id in side_ids:
                side_level[w].append(Image.frombytes("L", (w, w * 3 // 2), frame).transpose(Image.Transpose.FLIP_LEFT_RIGHT).tobytes())
    raw("prop_palette", b"".join(prop_palettes), "uint16_t", len(active_ids) * 256)
    for w in levels:
        stem = "prop_idx" if w == 64 else f"prop_lod{w}_idx"
        side_stem = "prop_side_idx" if w == 64 else f"prop_side_lod{w}_idx"
        raw(stem, b"".join(by_level[w]), "uint8_t", len(active_ids) * w * (w * 3 // 2))
        raw(side_stem, b"".join(side_level[w]), "uint8_t", len(side_ids) * w * (w * 3 // 2))
        all_images = [im.resize((w, w * 3 // 2), Image.Resampling.LANCZOS) for im in props]
        bound_stem = "prop_bounds" if w == 64 else f"prop_lod{w}_bounds"
        side_bound_stem = "prop_side_bounds" if w == 64 else f"prop_side_lod{w}_bounds"
        raw(bound_stem, silhouette_bounds(all_images), "uint8_t", len(props) * 4)
        raw(side_bound_stem, silhouette_bounds([all_images[i].transpose(Image.Transpose.FLIP_LEFT_RIGHT) for i in side_ids]), "uint8_t", len(side_ids) * 4)
    side_anchors = [(63 - anchors[i][0], anchors[i][1]) for i in side_ids]
    raw("prop_side_anchor", bytes(v for xy in side_anchors for v in xy), "uint8_t", len(side_ids) * 2)

    face_sheet = Image.open(ART / "building-elevations.png").convert("RGBA")
    walls, ends, roofs = [], [], []
    for wall_box, end_box, roof_box in BUILDING_FACE_BOXES:
        walls.append(face_sheet.crop(wall_box).resize((64, 64), Image.Resampling.LANCZOS))
        ends.append(face_sheet.crop(end_box).resize((32, 64), Image.Resampling.LANCZOS))
        roofs.append(face_sheet.crop(roof_box).resize((32, 32), Image.Resampling.LANCZOS))
    batches = (("buildings-neon-jungle.png", (0, 675, 1005, 1536), (0, 200, 384, 580, 776, 1024)),
               ("buildings-jungle-snow.png", (0, 615, 922, 1536), (0, 200, 397, 594, 791, 1024)),
               ("buildings-snow-coast.png", (0, 720, 985, 1536), (0, 223, 411, 594, 792, 1024)),
               ("buildings-coastal-civic.png", (0, 774, 1025, 1536), (0, 204, 384, 575, 802, 1024)))
    for filename, xs, ys in batches:
        source = Image.open(ART / filename).convert("RGBA")
        for row in range(5):
            top, bottom = ys[row], ys[row + 1]
            # End-cell gables/sky are cropped away: geometry supplies the roof.
            wall_box = (xs[0] + 7, top + max(6, (bottom - top) // 10), xs[1] - 7, bottom - 7)
            end_box = (xs[1] + 7, top + (bottom - top) // 3, xs[2] - 7, bottom - 7)
            roof_box = (xs[2] + 7, top + 7, xs[3] - 7, bottom - 7)
            walls.append(source.crop(wall_box).resize((64, 64), Image.Resampling.LANCZOS))
            ends.append(source.crop(end_box).resize((32, 64), Image.Resampling.LANCZOS))
            roofs.append(source.crop(roof_box).resize((32, 32), Image.Resampling.LANCZOS))
    building_frames = {name: [] for name in ("building_wall_idx", "building_end_idx", "building_roof_idx",
                       "building_wall_lod32_idx", "building_wall_lod16_idx", "building_end_lod16_idx", "building_end_lod8_idx")}
    building_palettes = []
    for wall, end, roof in zip(walls, ends, roofs):
        images = [wall, end, roof, wall.resize((32, 32), Image.Resampling.LANCZOS),
                  wall.resize((16, 16), Image.Resampling.LANCZOS), end.resize((16, 32), Image.Resampling.LANCZOS),
                  end.resize((8, 16), Image.Resampling.LANCZOS)]
        frames, palette, _ = indexed_group(images)
        for name, frame in zip(building_frames, frames): building_frames[name].append(frame)
        building_palettes.append(palette)
    assert len(walls) == BUILDING_COUNT
    for name, frames in building_frames.items(): raw(name, b"".join(frames), "uint8_t", sum(map(len, frames)))
    raw("building_palette", b"".join(building_palettes), "uint16_t", BUILDING_COUNT * 256)
    boat_frames, boat_palettes, boat_anchors = [], [], []
    boats = [contain(source, 64, 48, bottom=True) for source in boat_sources[:4]]
    for boat in boats:
        frames, palette, _ = indexed_group([boat]); boat_frames.extend(frames); boat_palettes.append(palette)
        boat_anchors.extend(ground_contact(boat))
    raw("boat_profile_idx", b"".join(boat_frames), "uint8_t", BOAT_COUNT * 64 * 48)
    raw("boat_palette", b"".join(boat_palettes), "uint16_t", BOAT_COUNT * 256)
    raw("boat_profile_anchor", bytes(boat_anchors), "uint8_t", BOAT_COUNT * 2)
    dock = grounded[8]
    dock = dock.crop((round(dock.width * .22), round(dock.height * .18), round(dock.width * .78), round(dock.height * .82)))
    dock = dock.transpose(Image.Transpose.ROTATE_270).resize((64, 32), Image.Resampling.LANCZOS)
    raw("dock_segment", rgb555(dock), "uint16_t", 64 * 32)
    preview("vehicle-poses.png", poses, 5)
    preview("traffic-preview.png", traffic, 6)
    preview("scenery-preview.png", props, 7)
    preview("building-walls-preview.png", walls, 5, 3)
    preview("building-ends-preview.png", ends, 5, 3)
    preview("building-roofs-preview.png", roofs, 5, 4)
    preview("boats-preview.png", boats, 4, 4)
    (OUT / "artwork-manifest.json").write_text(json.dumps({"final_original_art": True,
        "vehicles": VEHICLE_COUNT, "buildings": BUILDING_COUNT, "logical_props": len(props),
        "packed_props": len(active_ids), "boat_profiles": BOAT_COUNT, "vehicle_material_pixels": material_counts}, indent=2) + "\n")


def fonts() -> None:
    for size, name in ((10, "font"), (9, "font_small")):
        face = ImageFont.truetype(str(FONT_ROOT / "DejaVuSans.ttf"), size)
        glyphs, advances = [], []
        for code in range(32, 128):
            ch = chr(code)
            im = Image.new("L", (12, 14))
            ImageDraw.Draw(im).text((0, -2), ch, font=face, fill=255)
            glyphs.append(im.tobytes())
            advances.append(max(2, round(face.getlength(ch))))
        raw(f"{name}_alpha", b"".join(glyphs), "uint8_t", 96 * 12 * 14)
        raw(f"{name}_advance", bytes(advances), "uint8_t", 96)


def music() -> tuple[int, int]:
    """Original 12.8-second coastal synth-pop loop, signed mono 8-bit PCM."""
    rate, beat = 12000, 0.4
    duration = beat * 32
    rng = random.Random(0xC0A571)
    # New melody and chord voicings, rather than recordings or commercial music.
    melody = (72, 76, 79, 83, 81, 79, 76, 74, 72, 76, 79, 81, 79, 76, 74, 71,
              69, 72, 76, 79, 81, 79, 76, 72, 71, 74, 79, 81, 79, 76, 74, 71)
    chords = ((48, 55, 59, 64), (45, 52, 57, 60), (53, 60, 64, 69), (43, 50, 55, 59))
    samples = bytearray()
    for i in range(round(rate * duration)):
        t = i / rate
        step = min(31, int(t / beat))
        pulse = t % beat
        note = melody[step]
        f = 440 * 2 ** ((note - 69) / 12)
        # Soft bell lead and an octave echo with a quiet sustained pad.
        v = 0.16 * math.sin(math.tau * f * t) * math.exp(-pulse * 5)
        v += 0.045 * math.sin(math.tau * f * 2 * t) * math.exp(-pulse * 9)
        chord = chords[step // 8]
        for n in chord:
            cf = 440 * 2 ** ((n - 69) / 12)
            v += 0.023 * math.sin(math.tau * cf * t)
        bass = 440 * 2 ** ((chord[0] - 12 - 69) / 12)
        v += 0.08 * math.sin(math.tau * bass * t) * (0.5 + 0.5 * math.exp(-pulse * 5))
        # Warm kick on whole beats, snare on two/four, gentle eighth-note hats.
        kick = 0.10 * math.sin(math.tau * (48 * pulse + 2.4 * (1 - math.exp(-pulse * 25)))) * math.exp(-pulse * 20)
        v += kick
        noise = rng.random() * 2 - 1
        if step % 4 in (1, 3):
            v += 0.035 * noise * math.exp(-pulse * 35)
        v += 0.018 * noise * math.exp(-(t % (beat / 2)) * 100)
        # The pad is faded at the seam to make restarting the small loop clickless.
        v *= min(1.0, t / 0.03, (duration - t) / 0.08)
        sample = max(-127, min(127, round(v * 127)))
        samples.append(sample & 255)
    raw("music_pcm", bytes(samples), "int8_t", len(samples))
    return len(samples), rate


def declarations(music_bytes: int, music_rate: int) -> None:
    lines = ["#ifndef COASTLINE_ASSETS_H", "#define COASTLINE_ASSETS_H", "#include <stdint.h>"]
    for name, ctype, count in ASSETS:
        lines.append(f"extern const {ctype} {name}[{count}];")
    for stem, width, height in (("VEHICLE", VEHICLE_W, VEHICLE_H), ("TRAFFIC", TRAFFIC_W, TRAFFIC_H), ("PROP", PROP_W, PROP_H)):
        lines.extend((f"#define ASSET_{stem}_W {width}", f"#define ASSET_{stem}_H {height}"))
    lines.extend(f"#define ASSET_PROP_{name} {index}" for index, name in enumerate(PROP_NAMES))
    lines.append(f"#define ASSET_PROP_COUNT {len(PROP_NAMES)}")
    lines.append("#define ASSET_PROP_SPRITE_COUNT 33")
    lines.append(f"#define ASSET_PROP_SIDE_COUNT {len(PROP_SIDE_IDS)}")
    lines.extend(f"#define ASSET_PROP_SIDE_{PROP_NAMES[prop_id]} {index}"
                 for index, prop_id in enumerate(PROP_SIDE_IDS))
    lines.extend((f"#define ASSET_BUILDING_COUNT {BUILDING_COUNT}", "#define ASSET_BUILDING_WALL_W 64",
                  "#define ASSET_BUILDING_WALL_H 64", "#define ASSET_BUILDING_END_W 32",
                  "#define ASSET_BUILDING_END_H 64", "#define ASSET_BUILDING_ROOF_W 32",
                  "#define ASSET_BUILDING_ROOF_H 32", "#define ASSET_BOAT_PROFILE_W 64",
                  "#define ASSET_BOAT_PROFILE_H 48", "#define ASSET_DOCK_SEGMENT_W 64",
                  "#define ASSET_DOCK_SEGMENT_H 32"))
    lines.append(f"#define ASSET_BOAT_PROFILE_COUNT {BOAT_COUNT}")
    lines.append(f"#define ASSET_VEHICLE_COUNT {VEHICLE_COUNT}")
    lines.extend((f"#define MUSIC_BYTES {music_bytes}", f"#define MUSIC_RATE {music_rate}", "#endif", ""))
    (OUT / "assets.h").write_text("\n".join(lines))
    with (OUT / "assets.c").open("w") as output:
        output.write("/* Original Coastline assets; generated by tools/build_assets.py. */\n")
        for name, _, _ in ASSETS:
            output.write('__asm__(".section .rodata\\n.balign 4\\n.global ' + name + '\\n' + name + ':\\n.incbin \\\"build/' + name + '.bin\\\"\\n.previous\\n");\n')


def main() -> None:
    artwork()
    fonts()
    declarations(*music())
    total = sum((OUT / f"{name}.bin").stat().st_size for name, _, _ in ASSETS)
    print(f"Prepared {len(ASSETS)} original Coastline assets; {total:,} bytes")


if __name__ == "__main__":
    main()
