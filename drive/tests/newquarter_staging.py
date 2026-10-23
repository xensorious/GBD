#!/usr/bin/env python3
"""Check standing conversations, authored facing variants and preview controls."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

from inspect_rom import inspect

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT / 'build/quarter-staging')
parser.add_argument('--geometry-only', action='store_true', help='Pre-art geometry/input check; not a complete asset validation.')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
target = ROOT / 'build/test-quarter-staging'
command = ['gcc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
           '-pedantic', '-fsanitize=address,undefined', '-Isrc', '-Ibuild',
           'tests/newquarter_staging.c', 'src/game.c', 'src/story.c',
           'src/story_io.c', 'src/road.c', 'src/traffic.c', 'src/gfx.c',
           'build/assets.c', 'src/ui.c', 'src/render.c', 'src/story_render.c',
           'src/opening.c', '-o', str(target)]
subprocess.run(command, cwd=ROOT, check=True)
run = subprocess.run([str(target)] + (['--geometry-only'] if args.geometry_only else []),
                     cwd=ROOT, capture_output=True, text=True)
print(run.stdout, end='')
if run.stderr:
    print(run.stderr, end='', file=sys.stderr)
run.check_returncode()
manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
result = {'rom': inspect(ROOT / 'build/Coastline.nds'),
          'checks': int(re.search(r'(\d+) checks', run.stdout)[1]),
          'scope': 'synthetic_geometry' if args.geometry_only else 'real_directional_art',
          'sanitizers_passed': True, 'complete_directional_assets': not args.geometry_only,
          'native_scale_when_pair_fits': True, 'individual_character_scales': True, 'speaker_sides_stable': True,
          'right_partner_floor_offset': -4, 'geometry_clips_no_opaque_pixels': True,
          'orientation_uses_existing_transient_byte': True,
          'result': run.stdout.strip()}
if not args.geometry_only:
    assert manifest['quarter_portraits'] == 142 and manifest['portraits'] == 218
    profile = json.loads((ROOT / 'art/story/proportions.json').read_text())
    proportions = manifest['proportions']
    expected_scales = [profile['characters'][person['id']] for person in manifest['gallery']]
    assert proportions['character_scales'] == expected_scales
    assert expected_scales[0] == 256 and expected_scales[13] == 196
    owners = [-1] * manifest['portraits']
    for actor, person in enumerate(manifest['gallery']):
        for portrait, full in zip(person['portraits'], person['full_body']):
            if full:
                owners[portrait] = actor
    names = [person['id'] for person in manifest['gallery']]
    for entry in manifest['quarter_entries']:
        owners[entry['portrait']] = names.index(entry['character'])
    assert proportions['portrait_characters'] == owners
    assert proportions['portrait_scales'] == [expected_scales[actor] if actor >= 0 else 256 for actor in owners]
    counts = re.search(r'real art: (\d+) pairs across (\d+) floor scenes; (\d+) native / (\d+) reduced pairs', run.stdout)
    assert counts, 'Missing real-art pair coverage'
    stable = re.search(r'stable poses: (\d+) comparisons across (\d+) character pairs; (\d+) pixel-derived pose envelopes verified', run.stdout)
    assert stable and int(stable[1]) == 2054 and int(stable[2]) == 91
    assert int(stable[3]) == manifest['portraits'] == len(manifest['pose_envelopes'])
    result.update({'authored_directional_gallery_views': 142,
                   'real_art_pair_configurations': int(counts[1]),
                   'compatible_floor_scenes': int(counts[2]),
                   'native_art_pairs': int(counts[3]), 'reduced_art_pairs': int(counts[4]),
                   'speaker_swap_reloads': 0, 'pair_pixels_across_both_lcds': True,
                   'opaque_actors_do_not_overlap': True, 'nia_vale_reference_sizes': [256, 196],
                   'nia_vale_camera_scale': 256,
                   'pose_changes_keep_pair_scale_and_positions': True,
                   'pose_stability_comparisons': int(stable[1]),
                   'stable_character_pairs': int(stable[2]),
                   'pixel_derived_pose_envelopes_verified': int(stable[3]),
                   'story_archive_sha256': manifest['archive_sha256'],
                   'manifest_sha256': hashlib.sha256((ROOT / 'build/story-manifest.json').read_bytes()).hexdigest()})
(args.output / ('quarter-geometry-results.json' if args.geometry_only else 'quarter-staging-results.json')).write_text(
    json.dumps(result, indent=2) + '\n')
