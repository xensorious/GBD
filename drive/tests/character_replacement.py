#!/usr/bin/env python3
"""Check the Nia replacement against a named, pre-replacement art baseline.

Run after ``make -C drive``. The baseline hashes include each asset's palette
and indexed pixels; logical actor/pose names keep this check independent of
asset numbering. Source overrides are read from cast.json, not inferred from
the archive's generated portrait map.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from story_script import load_story

POSES = ['DEFAULT', 'CASUAL', 'GEAR', 'QUIET', 'ANNOYED']
FAMILY_BACKGROUNDS = {'MEMORIES', 'LIGHTHOUSE'}
STORY_FIELDS = ('speaker', 'companion', 'text', 'background', 'stage', 'focus',
                'move', 'caption_right', 'shot')


def digest(value):
    return hashlib.sha256(value).hexdigest()


def story_digest(scene, backgrounds):
    lines = []
    for line in scene['lines']:
        stable = {key: line[key] for key in STORY_FIELDS}
        # Reframing the existing family picture also selects its caption side.
        if backgrounds[line['background']] == 'MEMORIES':
            del stable['focus']
            del stable['caption_right']
        lines.append(stable)
    return digest(json.dumps(lines, sort_keys=True, separators=(',', ':')).encode())


def components(mask, width):
    remaining = bytearray(bool(value) for value in mask)
    areas = []
    largest = []
    for seed in range(len(remaining)):
        if not remaining[seed]:
            continue
        remaining[seed] = 0
        todo = [seed]
        component = []
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


def read_assets(data, asset_table, count):
    assets = []
    for index in range(count):
        offset, width, height, size, kind = struct.unpack_from(
            '<IHHII', data, asset_table + index * 16)
        assert size == width * height and offset + 512 + size <= len(data), index
        payload = data[offset:offset + 512 + size]
        assets.append({'sha256': digest(payload), 'width': width, 'height': height,
                       'kind': kind, 'palette': payload[:512], 'pixels': payload[512:]})
    return assets


def source_overrides(catalog):
    # Both spellings permit an explicit metadata file during art preparation.
    specs = catalog.get('overrides', catalog.get('portrait_overrides', []))
    selected = [spec for spec in specs if spec.get('character') == 'NIA']
    assert len(selected) == 5, ('Expected five explicit Nia source overrides', selected)
    assert {spec['pose'] for spec in selected} == set(POSES)
    assert len(specs) == len(selected), 'Replacement overrides must only target Nia'
    result = []
    alpha_oracles = {}
    for spec in selected:
        source = ROOT / 'art/story' / spec.get('sheet', spec.get('file', ''))
        assert source.is_file(), ('Missing source portrait', source)
        with Image.open(source) as opened:
            assert 'A' in opened.getbands(), ('Portrait source has no alpha', source)
            sheet = opened.convert('RGBA')
        if 'bounds' in spec:
            box = tuple(spec['bounds'])
        else:
            columns, rows = spec['grid']
            tile = spec.get('tile', 0)
            assert columns > 0 and rows > 0 and 0 <= tile < columns * rows
            box = (round(tile % columns * sheet.width / columns),
                   round(tile // columns * sheet.height / rows),
                   round((tile % columns + 1) * sheet.width / columns),
                   round((tile // columns + 1) * sheet.height / rows))
        assert len(box) == 4 and 0 <= box[0] < box[2] <= sheet.width
        assert 0 <= box[1] < box[3] <= sheet.height
        tile = sheet.crop(box)
        alpha = tile.getchannel('A')
        low, high = alpha.getextrema()
        assert low == 0 and high >= 240, ('Source crop lacks transparent silhouette', spec)
        mask = alpha.point(lambda value: 255 if value >= 96 else 0)
        bounds = mask.getbbox()
        assert bounds and sum(bool(value) for value in mask.tobytes()) > 500, spec
        areas, figure = components(mask.tobytes(), tile.width)
        assert len(areas) == 1 or areas[1] <= 32, ('Large disconnected source fragment', spec, areas[:5])
        # Derive transparency from the approved image's alpha only. Color-key
        # cleanup must never erase opaque hair, lips, clothing or skin pixels.
        raw = alpha.tobytes()
        isolated = bytearray(len(raw))
        for at in figure:
            isolated[at] = raw[at]
        silhouette = Image.frombytes('L', tile.size, bytes(isolated))
        figure_alpha = ImageOps.contain(silhouette.crop(silhouette.getbbox()),
                                       (164, 166), Image.Resampling.LANCZOS)
        native_alpha = Image.new('L', (168, 168))
        native_alpha.paste(figure_alpha, ((168 - figure_alpha.width) // 2,
                                        168 - figure_alpha.height))
        alpha_oracles[spec['pose']] = bytes(value >= 96 for value in native_alpha.tobytes())
        result.append({'pose': spec['pose'], 'source': str(source.relative_to(ROOT)),
                       'sha256': digest(source.read_bytes()), 'crop': box,
                       'crop_sha256': digest(tile.tobytes()),
                       'source_components': areas[:5],
                       'alpha_bounds': bounds})
    return result, alpha_oracles


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path,
                        default=ROOT / 'tests/fixtures/character-replacement-baseline.json')
    parser.add_argument('--metadata', type=Path, default=ROOT / 'story/cast.json')
    parser.add_argument('--source', type=Path,
                        help='Optional original five-pose image to record in the result')
    parser.add_argument('--source-sha256', help='Expected SHA256 of --source')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/character-replacement')
    args = parser.parse_args()
    baseline = json.loads(args.baseline.read_text())
    catalog = json.loads(args.metadata.read_text())
    backgrounds, scenes = load_story(ROOT)
    names = [background['name'] for background in backgrounds]
    manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
    assert catalog['people'] == baseline['people'], 'Character names or cast order changed'
    assert catalog['people'][0] == ['NIA', 'Nia']
    assert catalog['poses'] == POSES
    assert names == baseline['background_order']
    assert catalog['portrait_size'] == manifest['portrait_size'] == baseline['portrait_size'] == 168
    assert manifest['portraits'] == 51 and manifest['people'] == len(catalog['people'])
    assert len(scenes) == len(baseline['story_events']) == 125
    for scene, expected in zip(scenes, baseline['story_events']):
        assert scene['id'] == expected['id']
        assert story_digest(scene, names) == expected['sha256'], ('Story changed', scene['id'])
    family_shots = []
    for scene in scenes:
        for shot in scene['shots']:
            if names[shot['background']] != 'MEMORIES' or shot['stage'] != 1:
                continue
            left = min(shot['focus'], 256)
            if shot['subject'] == 'SIGN' and left <= shot['subject_x'] < left + 256:
                family_shots.append({'scene': scene['id'], 'focus': left,
                                     'subject_x': shot['subject_x']})
    assert family_shots, 'The workshop family photograph has no unobscured camera shot'

    data = (ROOT / 'build/nitro/story.bin').read_bytes()
    magic, event_table, event_count, asset_table, asset_count, total, unused = struct.unpack_from(
        '<8s6I', data)
    assert magic == b'CSTORY02' and event_count == 125 and total == len(data) and not unused
    assert digest(data) == manifest['archive_sha256']
    assert asset_count == manifest['assets'] == len(backgrounds) + 51
    assets = read_assets(data, asset_table, asset_count)
    portrait_map = manifest['portrait_map']
    assert len(portrait_map) == 5
    assert all(len(row) == len(catalog['people']) for row in portrait_map)
    assert {value for row in portrait_map for value in row} == set(range(51))
    hashes = {}
    preserved = 0
    for pose_index, pose in enumerate(POSES):
        for actor_index, (actor, _) in enumerate(catalog['people']):
            asset = assets[len(backgrounds) + portrait_map[pose_index][actor_index]]
            key = actor + '.' + pose
            hashes[key] = asset['sha256']
            if actor == 'NIA':
                if pose in baseline['poses']:
                    assert asset['sha256'] != baseline['portraits'][key], ('Nia art unchanged', pose)
            else:
                previous = actor + '.' + (pose if pose in baseline['poses'] else 'DEFAULT')
                assert asset['sha256'] == baseline['portraits'][previous], ('Other actor changed', key)
                preserved += 1
    nia_ids = [row[0] for row in portrait_map]
    assert len(set(nia_ids)) == len({hashes['NIA.' + pose] for pose in POSES}) == 5
    sources, alpha_oracles = source_overrides(catalog)
    masks = []
    for pose, portrait_id in zip(POSES, nia_ids):
        asset = assets[len(backgrounds) + portrait_id]
        assert (asset['width'], asset['height'], asset['kind']) == (168, 168, 1)
        assert asset['palette'][:2] == b'\0\0'
        pixels = asset['pixels']
        assert 100 < pixels.count(0) < len(pixels) - 1000, pose
        areas, _ = components(pixels, 168)
        assert len(areas) == 1 or areas[1] <= 32, ('Detached portrait fragments', pose, areas[:5])
        alpha_errors = sum(bool(actual) != bool(expected)
                           for actual, expected in zip(pixels, alpha_oracles[pose]))
        assert not alpha_errors, ('Imported alpha differs from approved crop', pose, alpha_errors)
        xs = [at % 168 for at, value in enumerate(pixels) if value]
        ys = [at // 168 for at, value in enumerate(pixels) if value]
        assert min(xs) > 0 and max(xs) < 167 and min(ys) > 0, ('Portrait touches crop edge', pose)
        masks.append({'pose': pose, 'opaque_pixels': len(xs), 'components': areas[:5],
                      'source_alpha_mismatches': alpha_errors,
                      'bounds': [min(xs), min(ys), max(xs) + 1, max(ys) + 1]})

    changed_backgrounds = set()
    for index, name in enumerate(names):
        if assets[index]['sha256'] != baseline['backgrounds'][name]:
            changed_backgrounds.add(name)
    assert changed_backgrounds == FAMILY_BACKGROUNDS, ('Unexpected background changes', changed_backgrounds)

    occurrences = []
    for event, scene in enumerate(scenes):
        offset, size, count, reserved = struct.unpack_from('<IHHI', data, event_table + event * 12)
        encoded_count, pad = struct.unpack_from('<HH', data, offset)
        assert count == encoded_count == len(scene['lines']) and not reserved and not pad
        assert offset + size <= len(data)
        for line_index, authored in enumerate(scene['lines']):
            pointer = struct.unpack_from('<I', data, offset + 4 + line_index * 4)[0]
            speaker, companion, stage, background, focus, length, flags = struct.unpack_from(
                '<BBBBHBB', data, pointer)
            assert 0 < length < 192 and pointer + 8 + length <= len(data)
            pose = ((focus >> 9) & 3) | ((flags & 15) << 2)
            companion_pose = ((focus >> 11) & 3) | ((flags >> 4) << 2)
            assert pose < 5 and companion_pose < 5 and not focus & 0x2000
            text = data[pointer + 8:pointer + 8 + length].decode('ascii')
            actual = (speaker, companion, stage, background, focus & 511,
                      bool(focus & 0x8000), bool(focus & 0x4000), pose, companion_pose, text)
            expected = tuple(authored[key] for key in (
                'speaker', 'companion', 'stage', 'background', 'focus', 'move',
                'caption_right', 'pose', 'companion_pose', 'text'))
            assert actual == expected, ('Compiled direction differs', scene['id'], line_index)
            for role, actor, selected in (('speaker', speaker, pose),
                                          ('companion', companion, companion_pose)):
                if actor == 0 and selected == 4:
                    assert role == 'speaker' or stage == 0, ('Invisible fifth-pose companion', scene['id'])
                    occurrences.append({'scene': scene['id'], 'line': line_index, 'role': role,
                                        'portrait': portrait_map[selected][actor], 'flags': flags})
    assert occurrences, 'Fifth Nia pose is not used by any compiled story line'
    approved = baseline['replacement_source']
    for source in sources:
        expected = approved['crops'][source['pose']]
        assert source['sha256'] == approved['sha256'], ('Unapproved source image', source['pose'])
        assert source['crop'] == tuple(expected['bounds']), ('Pose crop changed', source['pose'])
        assert source['crop_sha256'] == expected['rgba_sha256'], ('Pose crop pixels changed', source['pose'])
    result = {'baseline_ref': baseline['baseline_ref'], 'archive_sha256': digest(data),
              'portrait_size': 168, 'portraits': 51, 'nia_poses': POSES,
              'all_previous_nia_portraits_replaced': True,
              'imported_alpha_matches_approved_source': True,
              'other_actor_pose_payloads_preserved': preserved,
              'names_and_story_preserved': True, 'changed_backgrounds': sorted(changed_backgrounds),
              'workshop_family_photo_shots': family_shots,
              'fifth_pose_compiled_occurrences': occurrences, 'portrait_masks': masks,
              'source_overrides': sources}
    assert not args.source_sha256 or args.source, '--source-sha256 requires --source'
    if args.source:
        sha = digest(args.source.read_bytes())
        assert sha == approved['sha256'], 'Original source differs from the approved artwork'
        assert not args.source_sha256 or sha == args.source_sha256, 'Original source SHA256 differs'
        with Image.open(args.source) as source:
            result['original_source'] = {'path': str(args.source), 'sha256': sha, 'size': source.size}
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'character-replacement-results.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
