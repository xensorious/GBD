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
YAW_FRONT_OFFSETS = (-4, -2, 0, 2, 4)

# Pixel coordinates refer to the original 1382x1138 vehicle atlas.
VEHICLE_BOXES = (
    (35, 18, 324, 370), (390, 20, 680, 370),
    (723, 8, 1008, 370), (1080, 50, 1365, 370),
    (25, 386, 327, 728), (389, 386, 679, 728),
    (720, 388, 1008, 728), (1082, 388, 1368, 728),
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
            (1, slope, -front_offset - slope * top, 0, 1, 0),
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
    cars = atlas_crops(ART / "vehicles.png", VEHICLE_BOXES, (1382, 1138))
    expansion = atlas_crops(ART / "vehicles-expansion.png", EXPANSION_BOXES, (1269, 1240))
    player_cars = cars[:6] + expansion
    poses = [vehicle_pose(car, offset) for car in player_cars for offset in YAW_FRONT_OFFSETS]
    raw("vehicle_art", b"".join(rgb555(im) for im in poses), "uint16_t", 10 * 5 * VEHICLE_W * VEHICLE_H)
    raw("vehicle_bounds", silhouette_bounds(poses), "uint8_t", 10 * 5 * 4)
    traffic = [contain(car, TRAFFIC_W, TRAFFIC_H) for car in cars[6:]]
    raw("traffic_art", b"".join(rgb555(im) for im in traffic), "uint16_t", 6 * TRAFFIC_W * TRAFFIC_H)
    raw("traffic_bounds", silhouette_bounds(traffic), "uint8_t", 6 * 4)
    prop_sources = atlas_crops(ART / "scenery.png", PROP_BOXES, (1383, 1137))
    prop_sources += atlas_crops(ART / "scenery-expansion.png", PROP_EXPANSION_BOXES, (1536, 1024))
    props = [contain(im, PROP_W, PROP_H, bottom=True) for im in prop_sources]
    raw("prop_art", b"".join(rgb555(im) for im in props), "uint16_t", len(props) * PROP_W * PROP_H)
    # Offline filtering avoids high-frequency scintillation when scenery scales
    # down and streams past at speed. Resize the entire framed sprite so its
    # normalized ground contact and padding stay consistent across all levels.
    for width in (48, 32, 16):
        height = width * PROP_H // PROP_W
        lod = [im.resize((width, height), Image.Resampling.LANCZOS) for im in props]
        raw(f"prop_lod{width}", b"".join(rgb555(im) for im in lod), "uint16_t", len(props) * width * height)
        preview(f"scenery-lod{width}.png", lod, 6, 3)
    preview("vehicle-poses.png", poses, 5)
    preview("traffic-preview.png", traffic, 6)
    preview("scenery-preview.png", props, 6)


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
