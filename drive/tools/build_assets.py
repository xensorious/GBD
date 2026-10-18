"""Pack Coastline's original artwork, typography and music for the Nintendo DS.

The source atlases are original generated artwork. Curated boxes retain the
whole bus, gantry and other tall/wide shapes where the painted grid varies.
Runtime assets use transparent RGB555 and compact .incbin assembly declarations.
"""
from __future__ import annotations

import array
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
PROP_SIDE_IDS = (0, 1, 2, 7, 8, 14, 16)
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
)
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


def artwork() -> None:
    cars = atlas_crops(ART / "vehicles.png", VEHICLE_BOXES, (1383, 1137))
    expansion = atlas_crops(ART / "vehicles-expansion.png", EXPANSION_BOXES, (1269, 1240))
    player_cars = cars[:6] + expansion
    poses = [vehicle_pose(car, offset) for car in player_cars for offset in YAW_FRONT_OFFSETS]
    raw("vehicle_art", b"".join(rgb555(im) for im in poses), "uint16_t", 10 * 5 * VEHICLE_W * VEHICLE_H)
    raw("vehicle_bounds", silhouette_bounds(poses), "uint8_t", 10 * 5 * 4)
    traffic = [contain(car, TRAFFIC_W, TRAFFIC_H) for car in cars[6:]]
    raw("traffic_art", b"".join(rgb555(im) for im in traffic), "uint16_t", 6 * TRAFFIC_W * TRAFFIC_H)
    raw("traffic_bounds", silhouette_bounds(traffic), "uint8_t", 6 * 4)
    vehicle_geometry(poses, traffic)
    prop_sources = atlas_crops(ART / "scenery.png", PROP_BOXES, (1383, 1137))
    prop_sources += atlas_crops(ART / "scenery-expansion.png", PROP_EXPANSION_BOXES, (1536, 1024))
    grounded = atlas_crops(ART / "scenery-grounded.png", PROP_GROUNDED_BOXES, (1312, 1199))
    for prop_id, source in zip(PROP_GROUNDED_IDS, grounded[:7]):
        prop_sources[prop_id] = source
    props = [contain(im, PROP_W, PROP_H, bottom=True) for im in prop_sources]
    frontage = (0, 1, 2, 14, 16)
    anchors = [ground_contact(im, inner_corner=i in frontage or i == 7)
               for i, im in enumerate(props)]
    side_props = [props[i].transpose(Image.Transpose.FLIP_LEFT_RIGHT) for i in PROP_SIDE_IDS]
    side_anchors = [(PROP_W - 1 - anchors[i][0], anchors[i][1]) for i in PROP_SIDE_IDS]
    raw("prop_art", b"".join(rgb555(im) for im in props), "uint16_t", len(props) * PROP_W * PROP_H)
    raw("prop_bounds", silhouette_bounds(props), "uint8_t", len(props) * 4)
    raw("prop_anchor", bytes(v for xy in anchors for v in xy), "uint8_t", len(props) * 2)
    raw("prop_side_art", b"".join(rgb555(im) for im in side_props), "uint16_t", len(side_props) * PROP_W * PROP_H)
    raw("prop_side_bounds", silhouette_bounds(side_props), "uint8_t", len(side_props) * 4)
    raw("prop_side_anchor", bytes(v for xy in side_anchors for v in xy), "uint8_t", len(side_props) * 2)
    boat = contain(grounded[7].transpose(Image.Transpose.ROTATE_270), 128, 64)
    pier = contain(grounded[8], 64, 128)
    raw("boat_ground", rgb555(boat), "uint16_t", 128 * 64)
    raw("pier_ground", rgb555(pier), "uint16_t", 64 * 128)
    faces = Image.open(ART / "building-elevations.png").convert("RGBA")
    if faces.size != (971, 1619):
        raise ValueError("Building elevation atlas dimensions changed")
    wall_faces, end_faces, roof_faces = [], [], []
    for wall_box, end_box, roof_box in BUILDING_FACE_BOXES:
        wall_faces.append(faces.crop(wall_box).resize((64, 64), Image.Resampling.LANCZOS))
        end_faces.append(faces.crop(end_box).resize((32, 64), Image.Resampling.LANCZOS))
        roof_faces.append(faces.crop(roof_box).resize((32, 32), Image.Resampling.LANCZOS))
    raw("building_wall_art", b"".join(rgb555(im) for im in wall_faces), "uint16_t", 5 * 64 * 64)
    raw("building_end_art", b"".join(rgb555(im) for im in end_faces), "uint16_t", 5 * 32 * 64)
    raw("building_roof_art", b"".join(rgb555(im) for im in roof_faces), "uint16_t", 5 * 32 * 32)
    # These opaque faces keep the full geometry while reducing texture working
    # sets in the ARM9's small data cache. Distant facades also gain the same
    # offline filtering as foliage, rather than dropping windows unpredictably.
    for height in (32, 16):
        wall_lod = [im.resize((height, height), Image.Resampling.LANCZOS) for im in wall_faces]
        end_width = height // 2
        end_lod = [im.resize((end_width, height), Image.Resampling.LANCZOS) for im in end_faces]
        raw(f"building_wall_lod{height}", b"".join(rgb555(im) for im in wall_lod),
            "uint16_t", 5 * height * height)
        raw(f"building_end_lod{end_width}", b"".join(rgb555(im) for im in end_lod),
            "uint16_t", 5 * end_width * height)
        preview(f"building-walls-lod{height}.png", wall_lod, 5, 4)
        preview(f"building-ends-lod{end_width}.png", end_lod, 5, 4)
    # A raised broadside cabin and hull sit above an explicit waterline, as an
    # upright billboard. The old overhead image is not reused as a profile.
    boat_profile = contain(Image.open(ART / "boat-profile.png"), 64, 48, bottom=True)
    raw("boat_profile", rgb555(boat_profile), "uint16_t", 64 * 48)
    raw("boat_profile_anchor", bytes(ground_contact(boat_profile)), "uint8_t", 2)
    # Only the uninterrupted central planking is reused. Cropping away the
    # old end caps makes every segment meet the next with zero alpha gap.
    dock = grounded[8]
    dock = dock.crop((round(dock.width * .22), round(dock.height * .18),
                      round(dock.width * .78), round(dock.height * .82)))
    dock = dock.transpose(Image.Transpose.ROTATE_270).resize((64, 32), Image.Resampling.LANCZOS)
    raw("dock_segment", rgb555(dock), "uint16_t", 64 * 32)
    preview("building-walls-preview.png", wall_faces, 5, 3)
    preview("building-ends-preview.png", end_faces, 5, 3)
    preview("building-roofs-preview.png", roof_faces, 5, 4)
    boat_profile.save(OUT / "boat-profile-preview.png")
    dock.save(OUT / "dock-segment-preview.png")
    # Offline filtering avoids high-frequency scintillation when scenery scales
    # down and streams past at speed. Resize the entire framed sprite so its
    # normalized ground contact and padding stay consistent across all levels.
    for width in (48, 32, 16):
        height = width * PROP_H // PROP_W
        lod = [im.resize((width, height), Image.Resampling.LANCZOS) for im in props]
        raw(f"prop_lod{width}", b"".join(rgb555(im) for im in lod), "uint16_t", len(props) * width * height)
        raw(f"prop_lod{width}_bounds", silhouette_bounds(lod), "uint8_t", len(props) * 4)
        side_lod = [im.resize((width, height), Image.Resampling.LANCZOS) for im in side_props]
        raw(f"prop_side_lod{width}", b"".join(rgb555(im) for im in side_lod), "uint16_t", len(side_lod) * width * height)
        raw(f"prop_side_lod{width}_bounds", silhouette_bounds(side_lod), "uint8_t", len(side_lod) * 4)
        preview(f"scenery-lod{width}.png", lod, 6, 3)
    preview("vehicle-poses.png", poses, 5)
    preview("traffic-preview.png", traffic, 6)
    preview("scenery-preview.png", props, 6)
    preview("scenery-side-preview.png", side_props, 7, 3)
    boat.save(OUT / "boat-ground-preview.png")
    pier.save(OUT / "pier-ground-preview.png")


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
    lines.append(f"#define ASSET_PROP_SIDE_COUNT {len(PROP_SIDE_IDS)}")
    lines.extend(f"#define ASSET_PROP_SIDE_{PROP_NAMES[prop_id]} {index}"
                 for index, prop_id in enumerate(PROP_SIDE_IDS))
    lines.extend(("#define ASSET_BOAT_GROUND_W 128", "#define ASSET_BOAT_GROUND_H 64",
                  "#define ASSET_PIER_GROUND_W 64", "#define ASSET_PIER_GROUND_H 128"))
    lines.extend(("#define ASSET_BUILDING_COUNT 5", "#define ASSET_BUILDING_WALL_W 64",
                  "#define ASSET_BUILDING_WALL_H 64", "#define ASSET_BUILDING_END_W 32",
                  "#define ASSET_BUILDING_END_H 64", "#define ASSET_BUILDING_ROOF_W 32",
                  "#define ASSET_BUILDING_ROOF_H 32", "#define ASSET_BOAT_PROFILE_W 64",
                  "#define ASSET_BOAT_PROFILE_H 48", "#define ASSET_DOCK_SEGMENT_W 64",
                  "#define ASSET_DOCK_SEGMENT_H 32"))
    lines.append("#define ASSET_VEHICLE_COUNT 10")
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
