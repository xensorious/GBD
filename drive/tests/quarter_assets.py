#!/usr/bin/env python3
"""Check the directional full-body additions without changing the legacy cast.

The approval fixture pins the published cast-overhaul archive, the exact 142
curated source crops and each source's retained connected silhouette. Alpha
oracles are constructed independently from the original RGBA sources. Facing
labels are authored art-review data; mathematical checks cannot judge a pose's
anatomical direction, which is reviewed separately in the source contact sheet.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from story_script import CATALOG, PEOPLE, POSES, load_story

BASELINE_REF = '3cb4900f97717e70cafa33ea4c54834560348d12'
BASELINE_ROM_SHA = '83b7badf01773cb67ce61bcd0703bbe6745ff49238be0d4b851d0b9bb1a69453'
LEGACY_PORTRAITS = 76
QUARTER_PORTRAITS = 142
FRAME = (256, 264)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def archive(data):
    magic, events_at, count, assets_at, assets, total, unused = struct.unpack_from('<8s6I', data)
    assert magic == b'CSTORY02' and count == 125 and total == len(data) and not unused
    records = []
    for index in range(assets):
        at, width, height, size, kind = struct.unpack_from('<IHHII', data, assets_at + index * 16)
        assert size == width * height and at + 512 + size <= total
        payload = data[at:at + 512 + size]
        records.append({'sha256': sha(payload), 'width': width, 'height': height,
                        'kind': kind, 'palette': payload[:512], 'pixels': payload[512:]})
    scenes = []
    for event in range(count):
        at, size, lines, reserved = struct.unpack_from('<IHHI', data, events_at + event * 12)
        actual_lines, pad = struct.unpack_from('<HH', data, at)
        assert lines == actual_lines and not reserved and not pad and at + size <= total
        encoded = []
        for line in range(lines):
            ptr = struct.unpack_from('<I', data, at + 4 + line * 4)[0]
            length = data[ptr + 6]
            assert 0 < length < 192 and ptr + 8 + length <= total
            encoded.append(data[ptr:ptr + 8 + length].hex())
        scenes.append(encoded)
    return records, scenes


def rom_archive(data):
    fat, size = struct.unpack_from('<II', data, 0x48)
    assert size == 8
    start, end = struct.unpack_from('<II', data, fat)
    return data[start:end]


def components(mask, width):
    remaining = bytearray(bool(value) for value in mask)
    areas, largest = [], []
    for seed in range(len(remaining)):
        if not remaining[seed]:
            continue
        remaining[seed] = 0
        todo, component = [seed], []
        while todo:
            at = todo.pop()
            component.append(at)
            x = at % width
            for neighbor in ((at - 1 if x else -1),
                             (at + 1 if x + 1 < width else -1),
                             at - width, at + width):
                if 0 <= neighbor < len(remaining) and remaining[neighbor]:
                    remaining[neighbor] = 0
                    todo.append(neighbor)
        areas.append(len(component))
        if len(component) > len(largest):
            largest = component
    return sorted(areas, reverse=True), largest


def source_oracle(spec, review_images=False):
    path = ROOT / 'art/story' / spec['sheet']
    raw = path.read_bytes()
    with Image.open(path) as opened:
        assert 'A' in opened.getbands(), spec
        image = opened.convert('RGBA')
    left, top, right, bottom = spec['bounds']
    assert 0 <= left < right <= image.width and 0 <= top < bottom <= image.height, spec
    crop = image.crop(spec['bounds'])
    alpha = crop.getchannel('A')
    assert alpha.getextrema()[0] == 0 and alpha.getextrema()[1] >= 240, spec
    areas, figure = components(alpha.point(lambda value: 255 if value >= 96 else 0).tobytes(), crop.width)
    assert areas and areas[0] > 500, spec
    original = alpha.tobytes()
    retained = bytearray(len(original))
    for at in figure:
        retained[at] = original[at]
    silhouette = Image.frombytes('L', crop.size, bytes(retained))
    bounds = silhouette.getbbox()
    scaled = ImageOps.contain(silhouette.crop(bounds), (FRAME[0] - 4, FRAME[1] - 2), Image.Resampling.LANCZOS)
    native = Image.new('L', FRAME)
    native.paste(scaled, ((FRAME[0] - scaled.width) // 2, FRAME[1] - scaled.height))
    metadata = {'sheet': spec['sheet'], 'bounds': spec['bounds'],
                'source_sha256': sha(raw), 'rgba_sha256': sha(crop.tobytes()),
                'source_size': list(image.size),
                'source_components': areas, 'retained_source_bounds': list(bounds),
                'retained_source_pixels': len(figure),
                'discarded_source_pixels': sum(areas[1:]),
                'retained_alpha_sha256': sha(silhouette.tobytes())}
    mask = bytes(value >= 96 for value in native.tobytes())
    metadata['native_alpha_sha256'] = sha(mask)
    if not review_images:
        return metadata, mask
    retained = crop.copy()
    retained.putalpha(silhouette)
    discarded = crop.copy()
    discarded.putalpha(Image.frombytes('L', crop.size, bytes(
        value if value >= 96 and not retained_value else 0
        for value, retained_value in zip(original, silhouette.tobytes()))))
    retained_native = Image.new('RGBA', FRAME)
    resized = ImageOps.contain(retained.crop(bounds), (FRAME[0] - 4, FRAME[1] - 2), Image.Resampling.LANCZOS)
    retained_native.alpha_composite(resized, ((FRAME[0] - resized.width) // 2, FRAME[1] - resized.height))
    return metadata, mask, (crop, retained_native, discarded)


def native_image(record):
    colors = struct.unpack('<256H', record['palette'])
    return Image.frombytes('RGBA', FRAME, bytes(
        channel for index in record['pixels'] for channel in (
            (colors[index] & 31) * 255 // 31,
            ((colors[index] >> 5) & 31) * 255 // 31,
            ((colors[index] >> 10) & 31) * 255 // 31,
            255 if index else 0)))


def review_pages(sources, entries, records, first, output):
    """Render evidence only; approval still requires viewing every page."""
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    pages, metadata = {}, {}
    characters = list(dict.fromkeys(spec['character'] for spec in sources.values()))
    for character in characters:
        specs = [spec for spec in sources.values() if spec['character'] == character]
        poses = [pose for pose in POSES if any(spec['pose'] == pose for spec in specs)]
        page = Image.new('RGB', (8 * FRAME[0], 40 + len(poses) * (FRAME[1] + 35)), '#14232c')
        draw = ImageDraw.Draw(page)
        draw.text((8, 8), f'{character}: source / retained alpha oracle / discarded components / ROM import, LEFT then RIGHT', fill='white')
        for row, pose in enumerate(poses):
            for facing_index, facing in enumerate(('LEFT', 'RIGHT')):
                name = f'{character}.{pose}.{facing}'
                spec = sources[name]
                metadata[name], _, images = source_oracle(spec, review_images=True)
                imported = native_image(records[first + entries[name]['portrait']])
                for column, (label, panel) in enumerate(zip(('SOURCE', 'RETAINED', 'DISCARDED', 'ROM'), (*images, imported))):
                    x = (facing_index * 4 + column) * FRAME[0]
                    y = 40 + row * (FRAME[1] + 35)
                    draw.text((x + 4, y), f'{pose} {facing} {label}', fill='white')
                    background = Image.new('RGBA', FRAME, '#64727a')
                    checker = ImageDraw.Draw(background)
                    for cy in range(0, FRAME[1], 16):
                        for cx in range(0, FRAME[0], 16):
                            if (cx // 16 + cy // 16) % 2:
                                checker.rectangle((cx, cy, cx + 15, cy + 15), fill='#89969b')
                    fitted = ImageOps.contain(panel, FRAME, Image.Resampling.LANCZOS)
                    background.alpha_composite(fitted, ((FRAME[0] - fitted.width) // 2, FRAME[1] - fitted.height))
                    page.paste(background.convert('RGB'), (x, y + 18))
        path = output / f'{character.lower()}.png'
        page.save(path)
        pages[character] = {'path': str(path.relative_to(ROOT)), 'sha256': sha(path.read_bytes())}
    return pages, metadata


def key(spec):
    return '.'.join(spec[field] for field in ('character', 'pose', 'facing'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/quarter-staging')
    parser.add_argument('--approval', type=Path, default=ROOT / 'tests/fixtures/quarter-approval.json')
    parser.add_argument('--review-output', type=Path, default=ROOT / 'build/quarter-staging/source-review')
    parser.add_argument('--freeze-approval', action='store_true', help='Pin reviewed art sources against the published baseline; does not mark fragment review approved.')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
    catalog = json.loads((ROOT / 'art/story/quarter/catalog.json').read_text())
    current = (ROOT / 'build/nitro/story.bin').read_bytes()
    records, scenes = archive(current)
    assert sum(map(len, scenes)) == 923
    backgrounds, authored = load_story(ROOT)
    first = len(backgrounds)
    assert first == 54 and manifest['people'] == len(PEOPLE) == 14
    assert manifest['quarter_portraits'] == len(manifest['quarter_entries']) == QUARTER_PORTRAITS
    assert manifest['portraits'] == LEGACY_PORTRAITS + QUARTER_PORTRAITS == 218
    assert len(records) == manifest['assets'] == 272
    assert manifest['gallery_count'] == len(manifest['gallery']) == 28
    assert [entry['id'] for entry in manifest['gallery'][:14]] == PEOPLE
    assert CATALOG['people'] == [[entry['id'], entry['name']] for entry in manifest['gallery'][:14]]
    ids = [entry['id'] for entry in manifest['gallery']]
    assert len(set(ids)) == 28
    sources = {}
    for person in catalog['characters']:
        assert person['character'] in ids
        poses = POSES if person['character'] == 'NIA' else POSES[:4] if person['character'] in PEOPLE else POSES[:1]
        assert {(spec['pose'], spec['facing']) for spec in person['portraits']} == {(pose, facing) for pose in poses for facing in ('LEFT', 'RIGHT')}
        for spec in person['portraits']:
            value = {'character': person['character'], **spec}
            assert key(value) not in sources
            sources[key(value)] = value
    assert len(sources) == 142
    entries = {key(spec): spec for spec in manifest['quarter_entries']}
    assert len(entries) == 142 and set(entries) == set(sources)
    quarter_ids = {spec['portrait'] for spec in entries.values()}
    assert quarter_ids == set(range(76, 218)) and max(quarter_ids) < 256
    for name, source in sources.items():
        assert all(entries[name][field] == source[field] for field in ('character', 'pose', 'facing', 'sheet', 'bounds'))

    if args.freeze_approval:
        baseline_dir = ROOT / 'build/quarter-before'
        baseline_rom = (baseline_dir / 'Coastline.nds').read_bytes()
        assert sha(baseline_rom) == BASELINE_ROM_SHA
        previous = (baseline_dir / 'story.bin').read_bytes()
        assert previous == rom_archive(baseline_rom)
        old_records, old_scenes = archive(previous)
        assert len(old_records) == 54 + 76
        old_manifest = json.loads((baseline_dir / 'story-manifest.json').read_text())
        pages, source_metadata = review_pages(sources, entries, records, first, args.review_output)
        approval = {'baseline_ref': BASELINE_REF, 'baseline_rom_sha256': BASELINE_ROM_SHA,
                    'baseline_archive_sha256': sha(previous),
                    'legacy_payloads': [record['sha256'] for record in old_records],
                    'legacy_scenes': old_scenes,
                    'legacy_manifest': {field: old_manifest[field] for field in ('people', 'portrait_map', 'fullbody_map', 'gallery', 'gallery_count', 'ordinary_portraits')},
                    'authored_files': {str(path.relative_to(ROOT)): sha(path.read_bytes()) for path in [ROOT / 'story/rising_tide.txt', ROOT / 'story/staging.json', ROOT / 'story/cast.json']},
                    'source_approvals': {},
                    'review_pages': pages}
        for name, spec in sources.items():
            metadata = source_metadata[name]
            metadata['native_payload_sha256'] = records[first + entries[name]['portrait']]['sha256']
            metadata['visual_review'] = 'PENDING independent source/native review of facing and complete head, hands and feet'
            if len(metadata['source_components']) > 1 and metadata['source_components'][1] > 32:
                metadata['discarded_fragment_review'] = 'PENDING independent visual review of retained/discarded components'
            approval['source_approvals'][name] = metadata
        args.approval.parent.mkdir(parents=True, exist_ok=True)
        args.approval.write_text(json.dumps(approval, indent=2) + '\n')
        print('Pinned published baseline and 142 source crops; wrote 28 source/retained/discarded/ROM review pages. All visual review markers must be resolved before validation.')
        return

    approval = json.loads(args.approval.read_text())
    assert approval['baseline_ref'] == BASELINE_REF and approval['baseline_rom_sha256'] == BASELINE_ROM_SHA
    assert len(approval['legacy_payloads']) == 130
    assert [record['sha256'] for record in records[:130]] == approval['legacy_payloads'], 'A background or legacy portrait payload changed'
    assert scenes == approval['legacy_scenes'], 'Encoded story text, speaker or shot cues changed'
    for path, expected in approval['authored_files'].items():
        assert sha((ROOT / path).read_bytes()) == expected, ('Authored file changed', path)
    for field, previous in approval['legacy_manifest'].items():
        assert manifest[field] == previous, ('Legacy asset bindings changed', field)
    assert set(approval['source_approvals']) == set(sources)
    assert set(approval['review_pages']) == set(ids)
    for character, evidence in approval['review_pages'].items():
        assert evidence['path'].endswith(f'/{character.lower()}.png')
        assert len(evidence['sha256']) == 64
        page = ROOT / evidence['path']
        if page.exists():
            assert sha(page.read_bytes()) == evidence['sha256'], ('Visual review evidence changed', character)

    imported = []
    discarded = []
    for name, spec in sources.items():
        metadata, mask = source_oracle(spec)
        expected = approval['source_approvals'][name]
        for field, value in metadata.items():
            assert expected[field] == value, ('Approved source crop changed', name, field)
        review = expected.get('visual_review', '')
        assert review and 'PENDING' not in review, ('Source direction/full-body needs visual review', name)
        if len(metadata['source_components']) > 1 and metadata['source_components'][1] > 32:
            review = expected.get('discarded_fragment_review', '')
            assert review and 'PENDING' not in review, ('Source fragment needs visual review', name, metadata['source_components'][:5])
            discarded.append({'pose': name, 'review': review})
        record = records[first + entries[name]['portrait']]
        assert record['sha256'] == expected['native_payload_sha256'], ('Reviewed native colors or pixels changed', name)
        assert (record['width'], record['height'], record['kind']) == (*FRAME, 1)
        errors = sum(bool(actual) != bool(wanted) for actual, wanted in zip(record['pixels'], mask))
        assert len(record['pixels']) == len(mask) and errors == 0, ('Imported alpha differs from approved source', name, errors)
        areas, _ = components(record['pixels'], FRAME[0])
        assert len(areas) == 1 or areas[1] <= 32, ('Visible adjacent atlas fragment', name, areas[:5])
        imported.append({'pose': name, 'portrait': entries[name]['portrait'], 'source_sha256': metadata['source_sha256'], 'crop_sha256': metadata['rgba_sha256'], 'alpha_errors': errors, 'payload_sha256': record['sha256']})
    assert len({entry['payload_sha256'] for entry in imported}) == 142, 'Repeated directional frame payload'
    quarter_map, gallery_map = manifest['quarter_map'], manifest['gallery_quarters']
    assert len(quarter_map) == 2 and len(gallery_map) == 28
    for actor, character in enumerate(ids):
        assert len(gallery_map[actor]) == len(POSES)
        for pose, pose_name in enumerate(POSES):
            assert len(gallery_map[actor][pose]) == 2
            for facing, facing_name in enumerate(('LEFT', 'RIGHT')):
                wanted = entries.get(f'{character}.{pose_name}.{facing_name}', entries[f'{character}.DEFAULT.{facing_name}'])['portrait']
                assert gallery_map[actor][pose][facing] == wanted
                if actor < 14:
                    assert len(quarter_map[facing]) == len(POSES) and len(quarter_map[facing][pose]) == 14
                    assert quarter_map[facing][pose][actor] == wanted
    assert {value for person in gallery_map for row in person for value in row} == quarter_ids
    assert not any(line['speaker'] >= len(PEOPLE) + 2 for scene in authored for line in scene['lines'])

    rom = (ROOT / 'build/Coastline.nds').read_bytes()
    assert rom_archive(rom) == current and sha(current) == manifest['archive_sha256']
    result = {'rom_sha256': sha(rom), 'archive_sha256': sha(current),
              'baseline_ref': BASELINE_REF, 'baseline_rom_sha256': BASELINE_ROM_SHA,
              'legacy_background_payloads_preserved': 54, 'legacy_portrait_payloads_preserved': 76,
              'encoded_story_events_preserved': 125, 'encoded_story_lines_preserved': sum(map(len, scenes)),
              'story_speaker_ids_preserved': True, 'directions': ['LEFT', 'RIGHT'],
              'quarter_portraits': 142, 'characters': 28, 'archive_portraits': 218,
              'archive_assets': 272, 'maximum_portrait_id': max(quarter_ids),
              'independent_source_alpha_oracles_passed': len(imported),
              'source_and_native_visual_reviews': len(imported),
              'alpha_extraction_uses_source_alpha_only': True,
              'quarter_maps_valid': True, 'original_gallery_bindings_preserved': True,
              'source_crops': imported, 'discarded_fragment_reviews': discarded,
              'visual_review_pages': approval['review_pages'],
              'review_limit': 'Facing/anatomy are reviewed visually; source/crop hashes pin that reviewed artwork.'}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'quarter-assets-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key not in ('source_crops', 'discarded_fragment_reviews')}, indent=2))


if __name__ == '__main__':
    main()
