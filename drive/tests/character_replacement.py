#!/usr/bin/env python3
"""Check approved Nia art and the individual-cast wardrobe replacement.

Run after ``make -C drive``. Portraits intentionally reexport at a native
256x264 speaking frame; historical 168px packed hashes cannot establish source
preservation across that change. Original catalog/background source files and
specifications are pinned separately, alongside the approved Nia crop hashes.
Both ordinary and additional full-body imports use independent alpha oracles
derived from pinned approved source crops. Native background and Nia payloads
are also compared with the previous published cartridge, independently of IDs.
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


def source_overrides(catalog, width, height, fullbody=False):
    # Both spellings permit an explicit metadata file during art preparation.
    specs = catalog.get('fullbody_overrides', []) if fullbody else catalog.get(
        'overrides', catalog.get('portrait_overrides', []))
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
        crop_sha256 = digest(tile.tobytes())
        alpha = tile.getchannel('A')
        low, high = alpha.getextrema()
        assert low == 0 and high >= 240, ('Source crop lacks transparent silhouette', spec)
        # This explicitly approved export cleanup precedes component isolation
        # in the importer. The pinned source/crop hashes above remain raw.
        if spec.get('remove_export_matte'):
            alpha = Image.frombytes('L', tile.size, bytes(
                0 if (r > 220 and g < 60 and b < 60) or
                     (r > 220 and g > 220 and b < 60) else a
                for r, g, b, a in zip(*[iter(tile.tobytes())] * 4)))
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
                                       (width - 4, height - 2), Image.Resampling.LANCZOS)
        native_alpha = Image.new('L', (width, height))
        native_alpha.paste(figure_alpha, ((width - figure_alpha.width) // 2,
                                        height - figure_alpha.height))
        alpha_oracles[spec['pose']] = bytes(value >= 96 for value in native_alpha.tobytes())
        result.append({'pose': spec['pose'], 'source': str(source.relative_to(ROOT)),
                       'sha256': digest(source.read_bytes()), 'crop': box,
                       'crop_sha256': crop_sha256,
                       'source_components': areas[:5],
                       'alpha_bounds': bounds})
    return result, alpha_oracles


def preserve_sources(catalog, backgrounds, baseline):
    """Pin authored inputs, independently of the intentionally new export size."""
    approved = baseline['source_preservation']
    assert catalog['sheets'] == approved['catalog_sheets'], 'Original cast source specs changed'
    assert catalog['overrides'] == approved['catalog_overrides'], 'Approved Nia source specs changed'
    assert catalog['poses'] == approved['catalog_poses'], 'Authored pose names/order changed'
    assert backgrounds == approved['background_specs'], 'Original background source specs changed'
    for relative, expected in approved['authored_files'].items():
        assert digest((ROOT / relative).read_bytes()) == expected, ('Authored input changed', relative)
    for relative, expected in approved['source_files'].items():
        source = ROOT / relative
        assert digest(source.read_bytes()) == expected['sha256'], ('Original source artwork changed', relative)
        with Image.open(source) as image:
            assert list(image.size) == expected['size'], ('Original source dimensions changed', relative)
    return len(approved['source_files'])


def check_masks(assets, first_portrait, ids, alpha_oracles, width, height):
    masks = []
    for pose, portrait_id in zip(POSES, ids):
        asset = assets[first_portrait + portrait_id]
        assert (asset['width'], asset['height'], asset['kind']) == (width, height, 1)
        assert asset['palette'][:2] == b'\0\0'
        pixels = asset['pixels']
        assert 100 < pixels.count(0) < len(pixels) - 1000, pose
        areas, _ = components(pixels, width)
        assert len(areas) == 1 or areas[1] <= 32, ('Detached portrait fragments', pose, areas[:5])
        assert len(pixels) == len(alpha_oracles[pose]) == width * height
        alpha_errors = sum(bool(actual) != bool(expected)
                           for actual, expected in zip(pixels, alpha_oracles[pose]))
        assert not alpha_errors, ('Imported alpha differs from approved crop', pose, alpha_errors)
        xs = [at % width for at, value in enumerate(pixels) if value]
        ys = [at // width for at, value in enumerate(pixels) if value]
        assert min(xs) > 0 and max(xs) < width - 1 and min(ys) > 0, ('Portrait touches crop edge', pose)
        masks.append({'pose': pose, 'opaque_pixels': len(xs), 'components': areas[:5],
                      'source_alpha_mismatches': alpha_errors,
                      'bounds': [min(xs), min(ys), max(xs) + 1, max(ys) + 1]})
    return masks


def check_restyle_sources(catalog, manifest, assets, first_portrait, approval, width, height):
    """Check active wardrobe bindings and alpha against separately pinned art."""
    profiles = json.loads((ROOT / 'art/story/restyle/catalog.json').read_text())['characters']
    drafts = json.loads((ROOT / 'art/story/new-cast/catalog.json').read_text())['characters']
    people = [person[0] for person in catalog['people']]
    assert len({profile['character'] for profile in profiles}) == len(profiles) == 13
    assert {profile['character'] for profile in profiles} == set(approval['restyled_people'])
    assert [person['id'] for person in drafts] == approval['draft_people']
    assert len(profiles) == 13 and len(drafts) == 14
    assert len(manifest['restyled_people']) == 13
    assert set(manifest['restyled_people']) == set(approval['restyled_people'])
    gallery = manifest['gallery']
    assert manifest['gallery_count'] == len(gallery) == 28
    assert [entry['id'] for entry in gallery] == people + approval['draft_people']
    assert [entry['name'] for entry in gallery[:14]] == [person[1] for person in catalog['people']]
    assert all(not entry['draft'] for entry in gallery[:14])
    assert all(entry['draft'] for entry in gallery[14:])
    expected_keys = {profile['character'] + '.' + pose for profile in profiles for pose in POSES[:4]}
    expected_keys |= {'DRAFT.' + person['id'] for person in drafts}
    assert set(approval['source_approvals']) == expected_keys, 'Reviewed art approval pins are incomplete'
    results = []
    source_imports = []
    for profile in profiles:
        actor = people.index(profile['character'])
        assert actor != 0, 'The wardrobe replacement must not target Nia'
        assert profile['name'] == catalog['people'][actor][1]
        assert profile['style'].isascii() and profile['description'].isascii()
        assert len(profile['description']) <= 110
        portraits = {spec['pose']: spec for spec in profile['portraits']}
        assert len(profile['portraits']) == len(portraits) == 4 and set(portraits) == set(POSES[:4])
        ids = []
        for pose_index, pose in enumerate(POSES[:4]):
            portrait = manifest['portrait_map'][pose_index][actor]
            ids.append(portrait)
            key = profile['character'] + '.' + pose
            assert assets[first_portrait + portrait]['sha256'] != approval['previous_actor_payloads'][key], ('Wardrobe not replaced', key)
            source_imports.append((key, portraits[pose], portrait))
        assert len(set(ids)) == 4, ('Wardrobe/expressions share a portrait', profile['character'])
        assert manifest['portrait_map'][4][actor] == ids[0], 'Unprovided fifth expression must use the new default'
        assert gallery[actor]['portraits'] == ids + [ids[0]]
        assert gallery[actor]['full_body'] == [False] * 5
        assert gallery[actor]['style'] == profile['style'] and gallery[actor]['description'] == profile['description']
    for index, person in enumerate(drafts, len(people)):
        assert person['id'] not in people and person['age'] >= 18
        entry = gallery[index]
        assert person['full_body'] is True and entry['full_body'] == [True] * 5
        assert entry['name'] == person['name'] and entry['style'] == person['style']
        assert entry['description'] == person['description']
        assert entry['style'].isascii() and entry['description'].isascii()
        assert len(entry['description']) <= 110
        assert len(set(entry['portraits'])) == 1
        source_imports.append(('DRAFT.' + person['id'], person, entry['portraits'][0]))
    assert len(source_imports) == 66
    assert len({portrait for _, _, portrait in source_imports}) == 66
    for key, spec, portrait in source_imports:
        expected = approval['source_approvals'][key]
        assert expected['sheet'] == spec['sheet'] and expected['bounds'] == spec['bounds'], ('Source selection changed', key)
        path = ROOT / 'art/story' / spec['sheet']
        assert digest(path.read_bytes()) == expected['source_sha256'], ('Approved source changed', key)
        with Image.open(path) as opened:
            assert 'A' in opened.getbands(), ('Source needs explicit alpha', key)
            sheet = opened.convert('RGBA')
        left, top, right, bottom = spec['bounds']
        assert 0 <= left < right <= sheet.width and 0 <= top < bottom <= sheet.height, key
        tile = sheet.crop(spec['bounds'])
        assert digest(tile.tobytes()) == expected['rgba_sha256'], ('Approved crop pixels changed', key)
        alpha = tile.getchannel('A')
        assert alpha.getextrema()[0] == 0 and alpha.getextrema()[1] >= 240
        areas, figure = components(alpha.point(lambda value: 255 if value >= 96 else 0).tobytes(), tile.width)
        assert areas and areas[0] > 500
        assert areas == expected['source_components'], ('Approved source components changed', key)
        if len(areas) > 1 and areas[1] > 32:
            # A hand can cross an atlas cell boundary. Only a separately
            # reviewed and pinned crop may include a neighboring fragment;
            # the independent oracle below retains the intended largest actor.
            assert expected.get('discarded_fragment_review'), ('Unreviewed adjacent-cell fragment', key, areas[:5])
        raw = alpha.tobytes()
        isolated = bytearray(len(raw))
        for at in figure:
            isolated[at] = raw[at]
        silhouette = Image.frombytes('L', tile.size, bytes(isolated))
        trimmed = silhouette.crop(silhouette.getbbox())
        scaled = ImageOps.contain(trimmed, (width - 4, height - 2), Image.Resampling.LANCZOS)
        native = Image.new('L', (width, height))
        native.paste(scaled, ((width - scaled.width) // 2, height - scaled.height))
        actual = assets[first_portrait + portrait]
        assert (actual['width'], actual['height'], actual['kind']) == (width, height, 1)
        mask = bytes(value >= 96 for value in native.tobytes())
        errors = sum(bool(pixel) != bool(wanted) for pixel, wanted in zip(actual['pixels'], mask))
        assert len(actual['pixels']) == len(mask) and errors == 0, ('New portrait alpha differs from approved crop', key, errors)
        results.append({'key': key, 'portrait': portrait, 'source_sha256': expected['source_sha256'],
                        'crop_sha256': expected['rgba_sha256'], 'source_alpha_mismatches': errors,
                        'source_components': areas[:5]})
    return results


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
    approval = json.loads((ROOT / 'tests/fixtures/cast-restyle-approval.json').read_text())
    catalog = json.loads(args.metadata.read_text())
    backgrounds, scenes = load_story(ROOT)
    names = [background['name'] for background in backgrounds]
    manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
    assert catalog['people'] == baseline['people'], 'Character names or cast order changed'
    assert catalog['people'][0] == ['NIA', 'Nia']
    assert catalog['poses'] == POSES
    assert names == baseline['background_order']
    width, height = baseline['native_render_frame']['portrait_dimensions']
    assert (width, height) == (256, 264), 'Native speaking frame changed'
    assert catalog['portrait_size'] == manifest['portrait_size'] == width
    assert (catalog['portrait_width'], catalog['portrait_height']) == (width, height)
    assert (manifest['portrait_width'], manifest['portrait_height']) == (width, height)
    assert catalog['people'] == approval['story_people']
    quarter_count = manifest.get('quarter_portraits', 0)
    assert quarter_count in (0, 142)
    assert manifest['portraits'] == 76 + quarter_count and manifest['ordinary_portraits'] == 57
    assert manifest['fullbody_portraits'] == 5
    assert manifest['people'] == len(catalog['people'])
    preserved_source_files = preserve_sources(catalog, backgrounds, baseline)
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
    rom = (ROOT / 'build/Coastline.nds').read_bytes()
    fat, fat_size = struct.unpack_from('<II', rom, 0x48)
    assert fat_size == 8
    archive_start, archive_end = struct.unpack_from('<II', rom, fat)
    assert rom[archive_start:archive_end] == data, 'Cartridge archive differs from validated imports'
    magic, event_table, event_count, asset_table, asset_count, total, unused = struct.unpack_from(
        '<8s6I', data)
    assert magic == b'CSTORY02' and event_count == 125 and total == len(data) and not unused
    assert digest(data) == manifest['archive_sha256']
    assert asset_count == manifest['assets'] == len(backgrounds) + 76 + quarter_count
    assets = read_assets(data, asset_table, asset_count)
    for asset in assets[len(backgrounds):]:
        assert (asset['width'], asset['height'], asset['kind']) == (width, height, 1)
    for asset in assets[:len(backgrounds)]:
        assert (asset['width'], asset['height'], asset['kind']) == (512, 324, 0)
    for index, name in enumerate(names):
        assert assets[index]['sha256'] == approval['background_payloads'][name], ('Background payload changed', name)
    portrait_map = manifest['portrait_map']
    assert len(portrait_map) == 5
    assert all(len(row) == len(catalog['people']) for row in portrait_map)
    assert {value for row in portrait_map for value in row} == set(range(57))
    hashes = {}
    for pose_index, pose in enumerate(POSES):
        for actor_index, (actor, _) in enumerate(catalog['people']):
            asset = assets[len(backgrounds) + portrait_map[pose_index][actor_index]]
            key = actor + '.' + pose
            hashes[key] = asset['sha256']
            if actor == 'NIA':
                assert asset['sha256'] == approval['nia_payloads'][pose], ('Nia native portrait changed', pose)
                if pose in baseline['poses']:
                    assert asset['sha256'] != baseline['portraits'][key], ('Nia art unchanged', pose)
    nia_ids = [row[0] for row in portrait_map]
    assert len(set(nia_ids)) == len({hashes['NIA.' + pose] for pose in POSES}) == 5
    sources, alpha_oracles = source_overrides(catalog, width, height)
    masks = check_masks(assets, len(backgrounds), nia_ids, alpha_oracles, width, height)
    fullbody_map = manifest['fullbody_map']
    assert len(fullbody_map) == len(POSES)
    assert all(len(row) == len(catalog['people']) for row in fullbody_map)
    assert all(row[actor] == -1 for row in fullbody_map for actor in range(1, len(catalog['people'])))
    fullbody_ids = [row[0] for row in fullbody_map]
    assert set(fullbody_ids) == set(range(57, 62)), 'Story full bodies must be five additional Nia-only records'
    assert not set(fullbody_ids) & {value for row in portrait_map for value in row}
    fullbody_sources, fullbody_alpha = source_overrides(catalog, width, height, fullbody=True)
    fullbody_masks = check_masks(assets, len(backgrounds), fullbody_ids, fullbody_alpha, width, height)
    assert len({assets[len(backgrounds) + index]['sha256'] for index in fullbody_ids}) == 5
    for ordinary, full in zip(nia_ids, fullbody_ids):
        assert assets[len(backgrounds) + ordinary]['sha256'] != assets[len(backgrounds) + full]['sha256']
    for pose, full in zip(POSES, fullbody_ids):
        assert assets[len(backgrounds) + full]['sha256'] == approval['nia_fullbody_payloads'][pose], ('Nia native full body changed', pose)
    assert catalog['fullbody_overrides'] == baseline['fullbody_replacement']['overrides']
    for source in fullbody_sources:
        expected = baseline['fullbody_replacement']['crops'][source['pose']]
        assert source['sha256'] == expected['source_sha256'], ('Unapproved full-body source', source['pose'])
        assert source['crop'] == tuple(expected['bounds']), ('Full-body crop changed', source['pose'])
        assert source['crop_sha256'] == expected['rgba_sha256'], ('Full-body crop pixels changed', source['pose'])
    gallery = manifest['gallery']
    assert gallery[0]['portraits'] == fullbody_ids and gallery[0]['full_body'] == [True] * 5
    draft_ids = {entry['portraits'][0] for entry in gallery[14:]}
    assert draft_ids == set(range(62, 76))
    assert not draft_ids & {value for row in portrait_map for value in row}
    assert not draft_ids & set(fullbody_ids)
    gallery_ids = {portrait for entry in gallery for portrait in entry['portraits']}
    assert gallery_ids | set(nia_ids) == set(range(76))
    quarter_ids = {entry['portrait'] for entry in manifest.get('quarter_entries', [])}
    assert quarter_ids == set(range(76, 76 + quarter_count))
    assert gallery_ids | set(nia_ids) | quarter_ids == set(range(manifest['portraits']))
    new_sources = check_restyle_sources(catalog, manifest, assets, len(backgrounds), approval, width, height)

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
              'rom_sha256': digest(rom), 'archive_matches_nitrofs': True,
              'source_preservation_ref': baseline['source_preservation']['baseline_ref'],
              'portrait_size': width, 'portrait_width': width, 'portrait_height': height,
              'portraits': manifest['portraits'], 'ordinary_portraits': manifest['ordinary_portraits'],
              'fullbody_portraits': 5, 'legacy_portraits': 76,
              'additional_quarter_portraits': quarter_count, 'nia_poses': POSES,
              'all_previous_nia_portraits_replaced': True,
              'imported_alpha_matches_approved_source': True,
              'fullbody_alpha_matches_approved_source': True,
              'nia_native_payloads_preserved': 10,
              'background_native_payloads_preserved': len(backgrounds),
              'native_payload_baseline_ref': approval['baseline_ref'],
              'restyled_people': approval['restyled_people'],
              'new_draft_people': approval['draft_people'],
              'new_portrait_imports': new_sources,
              'new_portrait_alpha_matches_approved_sources': True,
              'draft_portraits_excluded_from_story': True,
              'gallery_covers_all_wardrobe_and_draft_portraits': True,
              'original_source_files_preserved': preserved_source_files,
              'original_background_source_specs_preserved': len(backgrounds),
              'names_and_story_preserved': True,
              'intentional_reexport': baseline['native_render_frame']['reason'],
              'intentional_wardrobe_replacement': approval['reason'],
              'workshop_family_photo_shots': family_shots,
              'fifth_pose_compiled_occurrences': occurrences, 'portrait_masks': masks,
              'fullbody_masks': fullbody_masks, 'source_overrides': sources,
              'fullbody_source_overrides': fullbody_sources}
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
