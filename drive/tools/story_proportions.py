"""Compile fixed character sizes and conservative, pose-stable camera framing."""
from functools import lru_cache
from PIL import Image, ImageChops


def compile_proportions(profile, gallery, quarter_entries, assets, backgrounds, envelopes, people):
    ids = [person['id'] for person in gallery]
    assert set(profile['characters']) == set(ids)
    scales = [profile['characters'][name] for name in ids]
    assert all(isinstance(scale, int) and 128 <= scale <= 256 for scale in scales)
    assert profile['reference_character'] == 'NIA' and scales[ids.index('NIA')] == 256
    owners = [-1] * (len(assets) - backgrounds)
    for actor, person in enumerate(gallery):
        for portrait, full in zip(person['portraits'], person['full_body']):
            if full:
                owners[portrait] = actor
    masks = {}
    for entry in quarter_entries:
        actor = ids.index(entry['character'])
        portrait = entry['portrait']
        owners[portrait] = actor
        width, height, payload = assets[backgrounds + portrait]
        mask = Image.frombytes('L', (width, height), payload[512:]).point(lambda v: 255 if v else 0)
        key = (actor, entry['facing'])
        masks[key] = ImageChops.lighter(masks[key], mask) if key in masks else mask
    portrait_scales = [scales[owner] if owner >= 0 else 256 for owner in owners]
    margin = 2

    @lru_cache(maxsize=None)
    def stage(actor, facing, camera):
        scale = scales[actor] * camera // 256
        width, height = scale, 264 * scale // 256
        original = masks[(actor, facing)]
        raw = original.tobytes()
        columns = [x * 256 // width for x in range(width)]
        rows = [y * 264 // height for y in range(height)]
        # Match the cartridge's integer sampler, rather than Pillow's different
        # nearest-neighbour pixel-centre rounding.
        resized = Image.frombytes('L', (width, height), bytes(raw[y * 256 + x] for y in rows for x in columns))
        left, _, right, _ = original.getbbox()
        x = margin - (left * width + 255) // 256 if facing == 'RIGHT' else 256 - margin - (right * width + 255) // 256
        floor = 284 if facing == 'RIGHT' else 280
        result = Image.new('L', (256, 288))
        result.paste(resized, (x, floor - height))
        return result

    cameras = [[256] * people for _ in range(people)]
    for left in range(people):
        for right in range(left + 1, people):
            # Different heights can share horizontal space at different rows.
            # Fit actual pose silhouettes, not two full-height rectangles. This
            # lets native-size Nia stand beside shorter Vale without shrinking
            # Nia merely because Vale has an outstretched hand at waist level.
            for camera in range(256, 63, -1):
                overlap = ImageChops.multiply(stage(left, 'RIGHT', camera), stage(right, 'LEFT', camera))
                if overlap.getbbox() is None:
                    cameras[left][right] = cameras[right][left] = camera
                    break
            else:
                raise AssertionError(('Standing pair cannot fit', ids[left], ids[right]))
    assert cameras[ids.index('NIA')][ids.index('VALE')] == 256
    return {'character_scales': scales, 'portrait_characters': owners,
            'portrait_scales': portrait_scales, 'pair_camera_scales': cameras,
            'standing_margin': margin}
