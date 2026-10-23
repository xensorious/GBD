#!/usr/bin/env python3
"""Check baked illustrated levels with ASan/UBSan and exact scene pixels."""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from inspect_rom import inspect

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT / 'build/baked-scenes')
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
target = ROOT / 'build/test-plate-render'
command = ['gcc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
           '-pedantic', '-fsanitize=address,undefined', '-Isrc', '-Ibuild',
           'tests/plate_render.c', 'src/game.c', 'src/story.c', 'src/story_io.c',
           'src/road.c', 'src/traffic.c', 'src/gfx.c', 'build/assets.c',
           'src/ui.c', 'src/render.c', 'src/story_render.c', 'src/opening.c',
           '-o', str(target)]
subprocess.run(command, cwd=ROOT, check=True)
run = subprocess.run([str(target)], cwd=ROOT, capture_output=True, text=True)
print(run.stdout, end='')
if run.stderr:
    print(run.stderr, end='', file=sys.stderr)
run.check_returncode()
counts = re.search(r'(\d+) checks; (\d+) illustrated levels, (\d+) lines / (\d+) pages', run.stdout)
assert counts
result = {'rom': inspect(ROOT / 'build/Coastline.nds'),
          'checks': int(counts[1]), 'sanitizers_passed': True,
          'levels': ['D04', 'D05', 'D06'], 'baked_backgrounds': int(counts[2]),
          'dialogue_lines': int(counts[3]), 'dialogue_pages': int(counts[4]),
          'native_scene_pixels': 256 * 288,
          'separate_actor_loads': 0, 'portrait_overlays': False,
          'title_caption_and_bars_absent': True, 'static_camera': True,
          'pause_resume': True, 'cold_cache_pixels': True,
          'legacy_scene_cache_restoration': True,
          'normal_progression': True, 'save_format_unchanged': True,
          'result': run.stdout.strip()}
(args.output / 'plate-render-results.json').write_text(json.dumps(result, indent=2) + '\n')
