#!/usr/bin/env python3
"""Regression-test the exact player-reply pages that changed apparent builds."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from inspect_rom import inspect

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT / 'build/proportions-update')
parser.add_argument('--prove-old-fails', action='store_true', help='Also run the same regression against the preceding published renderer.')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def compile_and_run(target, renderer=None):
    command = ['gcc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
               '-pedantic', '-fsanitize=address,undefined', '-Isrc', '-Ibuild']
    if renderer is not None:
        command += ['-Wno-unused-const-variable',
                    '-DSHOT_CONTINUITY_RENDERER=' + json.dumps(str(renderer))]
    command += ['tests/shot_continuity.c', 'src/game.c', 'src/story.c',
                'src/story_io.c', 'src/road.c', 'src/traffic.c', 'src/gfx.c',
                'build/assets.c', 'src/ui.c', 'src/render.c', 'src/story_render.c',
                'src/opening.c', '-o', str(target)]
    subprocess.run(command, cwd=ROOT, check=True)
    return subprocess.run([str(target)], cwd=ROOT, capture_output=True, text=True)


old_failure = None
if args.prove_old_fails:
    with tempfile.TemporaryDirectory(prefix='shot-continuity-old-', dir=ROOT / 'build') as temporary:
        directory = Path(temporary)
        old = subprocess.check_output(['git', 'show',
                                      '33f1fdd487a31978fdee3d048bae4f019e2ecd9e:drive/src/dialogue_render.c'], cwd=ROOT)
        renderer = directory / 'dialogue_render.c'
        renderer.write_bytes(old)
        run = compile_and_run(directory / 'old-renderer-test', renderer)
        assert run.returncode != 0 and 'same_scene(&baseline,&screen)' in run.stderr, run.stdout + run.stderr
        old_failure = {'baseline_revision': '33f1fdd487a31978fdee3d048bae4f019e2ecd9e',
                       'renderer_sha256': hashlib.sha256(old).hexdigest(),
                       'failed_at_exact_d01_reply_visual_comparison': True,
                       'diagnostic': run.stderr.strip()}
        print('Preceding renderer fails the exact D01 reply visual regression, as expected.')

run = compile_and_run(ROOT / 'build/test-shot-continuity')
print(run.stdout, end='')
if run.stderr:
    print(run.stderr, end='', file=sys.stderr)
run.check_returncode()
assert '6 authored reply comparisons, 6 cold-cache replies' in run.stdout
result = {'rom': inspect(ROOT / 'build/Coastline.nds'),
          'checks': int(re.search(r'(\d+) checks', run.stdout)[1]),
          'sanitizers_passed': True,
          'cases': ['D01 pages 6/7', 'D03 pages 3/4', 'C16 pages 2/3',
                    'D01 pages 3/4', 'D14 pages 2/3'],
          'authored_reply_comparisons': 6,
          'cold_cache_reply_comparisons': 6,
          'whole_scene_pixel_identity': True,
          'correct_directional_portraits_retained': True,
          'reply_portrait_reloads': 0,
          'three_person_scene_preserves_visible_pair': True,
          'skeptical_pose_preserved': True,
          'pause_replay_abort': True,
          'previous_shot_fade_uses_previous_line_cue': True,
          'save_and_story_state_unchanged_by_rendering': True,
          'native_fixture_note': 'Native codec review profiles have a manufactured completed frontier; input advances the actual authored archive without patching StoryState.',
          'old_renderer_regression_proof': old_failure,
          'result': run.stdout.strip()}
(args.output / 'shot-continuity-results.json').write_text(json.dumps(result, indent=2) + '\n')
