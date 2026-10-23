#!/usr/bin/env python3
"""Cast studio validation: sanitized native pixels plus a button-only ROM tour."""
import argparse
import ctypes as C
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

from emulator import Emulator, ROOT
from game_snapshot import Game, find_symbol, symbol_size
from inspect_rom import inspect
from playthrough import DriveThrough


class Campaign(C.LittleEndianStructure):
    _fields_ = [(name, C.c_uint8) for name in
                ('tab', 'chapter', 'event', 'modal', 'page', 'garage_return',
                 'drive_column', 'gallery_pose')] + [('revision', C.c_uint32)]


def native(output):
    target = ROOT / 'build/test-cast-gallery'
    command = ['gcc', '-std=c11', '-O2', '-g', '-Wall', '-Wextra', '-Werror',
               '-pedantic', '-fsanitize=address,undefined', '-Isrc', '-Ibuild',
               'tests/cast_gallery.c', 'src/game.c', 'src/story.c',
               'src/story_io.c', 'src/road.c', 'src/traffic.c', 'src/gfx.c',
               'build/assets.c', 'src/ui.c', 'src/render.c', 'src/story_render.c',
               'src/dialogue_render.c', 'src/opening.c', '-o', str(target)]
    subprocess.run(command, cwd=ROOT, check=True)
    run = subprocess.run([str(target)], cwd=ROOT, capture_output=True, text=True)
    print(run.stdout, end='')
    if run.stderr:
        print(run.stderr, end='', file=sys.stderr)
    run.check_returncode()
    directional = 142 if json.loads((ROOT / 'build/story-manifest.json').read_text()).get('quarter_portraits') else 0
    result = {'rom': inspect(ROOT / 'build/Coastline.nds'),
              'checks': int(re.search(r'(\d+) checks', run.stdout)[1]),
              'sanitizers_passed': True, 'characters': 28, 'pose_views': 71,
              'directional_views': directional, 'total_gallery_views': 71 + directional,
              'story_ids_unchanged': True, 'saved_state_unchanged': True,
              'native_pixels': True, 'dual_lcd_seam': True,
              'exact_cache_restoration': ['story', 'endless_drive', 'free_roam'],
              'result': run.stdout.strip()}
    (output / 'cast-gallery-native-results.json').write_text(
        json.dumps(result, indent=2) + '\n')


def cartridge(output):
    rom = ROOT / 'build/Coastline.nds'
    elf = ROOT / 'build/arm9.elf'
    campaign_address = find_symbol(elf, 'campaign')
    assert symbol_size(elf, 'campaign') == C.sizeof(Campaign) == 12
    assert Campaign.gallery_pose.offset == 7 and Campaign.revision.offset == 8
    story_address = find_symbol(elf, 'story')
    story_size = symbol_size(elf, 'story')
    stats_address = find_symbol(elf, 'story_visual_stats')
    manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
    gallery = manifest['gallery']
    quarter_available = bool(manifest.get('quarter_portraits', 0))
    assert len(gallery) == 28
    sha = hashlib.sha256(rom.read_bytes()).hexdigest()
    captures = []

    save_directory = output / 'cast-studio-fresh-save'
    shutil.rmtree(save_directory, ignore_errors=True)
    with Emulator(rom, save_directory) as emulator:
        emulator.run(180)
        drive = DriveThrough(emulator, find_symbol(elf, 'game'), output)

        def state():
            return Campaign.from_buffer_copy(emulator.read_ram(campaign_address, 12))

        drive.menu(0)
        drive.press('a')
        assert drive.game().mode == 11
        drive.press('down')
        drive.press('a')
        assert drive.game().mode == 12
        drive.press('l')
        assert state().tab == 4
        drive.press('up')
        assert drive.game().cursor == 4
        original_save = bytes(drive.game().save)
        original_story = emulator.read_ram(story_address, story_size)
        drive.press('a')
        assert state().modal == 6 and state().page == state().gallery_pose == 0
        for entry, character in enumerate(gallery):
            assert state().page == entry and not state().gallery_pose
            ids = character['portraits']
            poses = len(set(ids))
            assert poses == (5 if entry == 0 else 4 if entry < 14 else 1)
            for pose in range(poses):
                assert state().gallery_pose == pose
                image = f'cast-{entry:02d}-{pose}'
                drive.shot(image)
                captures.append({'entry': entry, 'pose': pose,
                                 'name': character['name'],
                                 'view': 'quarter-right' if quarter_available else 'original',
                                 'image': image + '.png'})
                if quarter_available:
                    drive.press('x')
                    assert state().gallery_pose == (pose | 8)
                    image = f'cast-{entry:02d}-{pose}-left'
                    drive.shot(image)
                    captures.append({'entry': entry, 'pose': pose, 'name': character['name'],
                                     'view': 'quarter-left', 'image': image + '.png'})
                    assert bytes(drive.game().save) == original_save
                    assert emulator.read_ram(story_address, story_size) == original_story
                    drive.press('x')
                    assert state().gallery_pose == pose
                    drive.press('y')
                    assert state().gallery_pose == (pose | 16)
                    image = f'cast-{entry:02d}-{pose}-original'
                    drive.shot(image)
                    captures.append({'entry': entry, 'pose': pose, 'name': character['name'],
                                     'view': 'original', 'image': image + '.png'})
                    assert bytes(drive.game().save) == original_save
                    assert emulator.read_ram(story_address, story_size) == original_story
                    drive.press('y')
                    assert state().gallery_pose == pose
                assert bytes(drive.game().save) == original_save
                assert emulator.read_ram(story_address, story_size) == original_story
                drive.press('r')
            assert not state().gallery_pose
            drive.press('l')
            assert state().gallery_pose == poses - 1
            drive.press('a')
            assert not state().gallery_pose
            drive.press('right')
        assert len(captures) == (213 if quarter_available else 71)
        assert state().page == state().gallery_pose == 0
        drive.ticks(4, touch=(128, 120))
        assert state().page == state().gallery_pose == 0 and state().modal == 6
        drive.press('left')
        assert state().page == 27
        drive.press('down')
        assert not state().page
        assert bytes(drive.game().save) == original_save
        assert emulator.read_ram(story_address, story_size) == original_story
        drive.press('b')
        assert not state().modal and drive.game().cursor == 4
        drive.shot('cast-studio-office-return')
        drive.press('b')
        assert drive.game().mode == 11
        drive.press('up')
        drive.press('a')
        assert drive.game().mode == 1
        drive.ticks(25, ('r',))
        drive.shot('cast-studio-return-highway', ('r',))
        drive.press('start')
        drive.menu(1)
        drive.press('a')
        assert drive.game().mode == 2
        drive.press('a')
        drive.menu(0)
        drive.press('a')
        drive.press('right')
        drive.press('a')
        assert drive.game().mode == 1
        assert int.from_bytes(emulator.read_ram(find_symbol(elf, 'city'), 4), 'little') == 1
        drive.ticks(25, ('r',))
        drive.shot('cast-studio-return-city', ('r',))
        stats = list((C.c_uint32 * 5).from_buffer_copy(emulator.read_ram(stats_address, 20)))
        assert not stats[0]
        assert not drive.game().save.reserved[0]
    assert hashlib.sha256(rom.read_bytes()).hexdigest() == sha
    result = {'rom_sha256': sha, 'characters': 28, 'pose_views': 71,
              'directional_views': 142 if quarter_available else 0,
              'total_captures': len(captures),
              'fresh_profile': True, 'button_only': True, 'touch_ignored': True,
              'emulated_ram_read_only': True, 'saved_state_unchanged_while_browsing': True,
              'story_state_unchanged_while_browsing': True,
              'return_highway': True, 'return_city': True,
              'archive_failures': 0, 'captures': captures}
    (output / 'cast-gallery-cartridge-results.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(json.dumps({key: value for key, value in result.items() if key != 'captures'}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/cast-overhaul')
    parser.add_argument('--native-only', action='store_true')
    parser.add_argument('--emulator-only', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if not args.emulator_only:
        native(output)
    if not args.native_only:
        cartridge(output)


if __name__ == '__main__':
    main()
