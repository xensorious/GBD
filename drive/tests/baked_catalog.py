"""Explicit approvals for the three intentionally replaced illustrated scenes.

Historical wardrobe/source fixtures remain immutable. This catalog authorizes
only D04--D06 and three appended backgrounds; callers must still compare every
other encoded event and all preceding artwork with those historical fixtures.
"""
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image
from story_script import CAST


REPLACED_EVENTS = ('D04', 'D05', 'D06')
BAKED_BACKGROUNDS = ('NIA_POOL_EVENING', 'NIA_REPLAY_NIGHT', 'NIA_SOFA_PLAN')
LEGACY_BACKGROUNDS = 54


def approved_baked_pages(root, backgrounds, scenes):
    root = Path(root)
    path = root / 'art/story/baked-pages/catalog.json'
    if not path.exists():
        assert len(backgrounds) == LEGACY_BACKGROUNDS
        return {'events': [], 'event_indices': set(), 'backgrounds': [], 'catalog_sha256': None}
    raw = path.read_bytes()
    catalog = json.loads(raw)
    assert catalog['replaced_events'] == list(REPLACED_EVENTS)
    images = catalog['backgrounds']
    events = catalog['events']
    assert [entry['name'] for entry in images] == list(BAKED_BACKGROUNDS)
    assert [entry['id'] for entry in events] == list(REPLACED_EVENTS)
    assert len(backgrounds) == LEGACY_BACKGROUNDS + len(images)
    assert [entry['name'] for entry in backgrounds[LEGACY_BACKGROUNDS:]] == list(BAKED_BACKGROUNDS)
    assert len(scenes) == 125
    indices = set()
    for position, (image, expected) in enumerate(zip(images, events)):
        assert image['sheet'].startswith('baked-pages/')
        assert backgrounds[LEGACY_BACKGROUNDS + position]['sheet'] == image['sheet']
        assert backgrounds[LEGACY_BACKGROUNDS + position]['plate'] is True
        source = root / 'art/story' / image['sheet']
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        assert image['source_sha256'] == digest, ('Unreviewed baked source', image['name'])
        with Image.open(source) as opened:
            assert opened.width >= 256 and opened.height >= 288
            assert opened.convert('RGBA').getchannel('A').getextrema() == (255, 255)
        assert expected['background'] == image['name']
        index = 'DCBAS'.index(expected['id'][0]) * 25 + int(expected['id'][1:]) - 1
        indices.add(index)
        scene = scenes[index]
        assert scene['id'] == expected['id'] and len(scene['shots']) == 1
        shot = scene['shots'][0]
        assert shot['stage'] == 4 and shot['present'] == [] and shot['move'] is False
        assert shot['background'] == LEGACY_BACKGROUNDS + position
        actual = [{'speaker': CAST[line['speaker']],
                   'text': line['text']} for line in scene['lines']]
        assert actual == expected['messages'], ('Baked scene differs from approved script', scene['id'])
        assert 4 <= len(actual) <= 16 and {line['speaker'] for line in actual} <= {'NIA', 'YOU'}
        assert all(line['stage'] == 4 and line['background'] == shot['background'] and
                   line['companion'] == 255 and line['move'] is False for line in scene['lines'])
    return {'events': list(REPLACED_EVENTS), 'event_indices': indices,
            'backgrounds': images, 'catalog_sha256': hashlib.sha256(raw).hexdigest()}


def check_baked_payloads(baked, records):
    """Pin the visually reviewed native painting, including its viewport crop."""
    checks = []
    border = 0x8000 | ((5 >> 3)) | ((10 >> 3) << 5) | ((16 >> 3) << 10)
    for position, expected in enumerate(baked['backgrounds']):
        record = records[LEGACY_BACKGROUNDS + position]
        assert (record['width'], record['height'], record['kind']) == (512, 324, 0)
        assert record['sha256'] == expected['native_payload_sha256'], ('Unreviewed native painting', expected['name'])
        colors = struct.unpack('<256H', record['palette'])
        assert all(color & 0x8000 for color in colors)
        pixels = record['pixels']
        # The central 256x288 painting is sampled natively across the two LCDs.
        # Far corners independently distinguish it from a whole-512px fit.
        assert all(colors[pixels[y * 512 + x]] == border
                   for x, y in ((0, 0), (511, 0), (0, 323), (511, 323), (64, 162), (448, 162)))
        viewport = bytes(pixels[y * 512 + x] for y in range(18, 306) for x in range(128, 384))
        assert len(viewport) == 256 * 288 and len({colors[index] for index in viewport}) > 32
        checks.append({'name': expected['name'], 'native_payload_sha256': record['sha256'],
                       'viewport': [128, 18, 384, 306], 'native_pixels': len(viewport),
                       'opaque_palette': True, 'source_and_reviewed_payload_pinned': True})
    return checks
