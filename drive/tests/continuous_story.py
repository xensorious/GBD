#!/usr/bin/env python3
"""Button-driven continuous scene and page captures on the actual DS cartridge.

The later scene saves are explicit visual fixtures made by the native save codec.
The emulator is observed through read-only RAM and actual LCD screenshots.
"""
import argparse
import ctypes as C
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

from emulator import Emulator, ROOT
from fixtures import encode_profile, write_battery
from game_snapshot import Game, find_symbol, symbol_size
from playthrough import DriveThrough


class Line(C.LittleEndianStructure):
    _fields_ = [(name, C.c_uint8) for name in
                ('speaker', 'companion', 'stage', 'background')]
    _fields_ += [('focus', C.c_uint16), ('length', C.c_uint8),
                ('pose_flags', C.c_uint8), ('text', C.c_char * 192)]


class Story(C.LittleEndianStructure):
    _fields_ = [('offsets', C.c_uint32 * 16), ('revision', C.c_uint32),
                ('current', Line), ('previous', Line)]
    _fields_ += [(name, C.c_uint16) for name in ('revealed', 'ticks', 'age')]
    _fields_ += [(name, C.c_uint8) for name in
                 ('active', 'chapter', 'event', 'line', 'count', 'phase',
                  'phase_tick', 'replay', 'pause', 'cursor', 'failed',
                  'page_start', 'page_end', 'page_index')]


def quarter_portrait_bounds(rom, manifest):
    """Read silhouette extents from the archive actually shipped in the ROM."""
    if not manifest.get('quarter_portraits'):
        return {}
    from PIL import Image
    data = rom.read_bytes()
    fat, size = struct.unpack_from('<II', data, 0x48)
    assert size == 8
    start, end = struct.unpack_from('<II', data, fat)
    archive = data[start:end]
    assert hashlib.sha256(archive).hexdigest() == manifest['archive_sha256']
    magic, _, _, table, count, total, unused = struct.unpack_from('<8s6I', archive)
    assert magic == b'CSTORY02' and total == len(archive) and not unused
    result = {}
    ids = {portrait for direction in manifest['quarter_map']
           for poses in direction for portrait in poses if portrait >= 0}
    for portrait in ids:
        index = manifest['backgrounds'] + portrait
        assert index < count
        offset, width, height, pixels, kind = struct.unpack_from('<IHHII', archive, table + index * 16)
        assert (width, height, pixels, kind) == (256, 264, 256 * 264, 1)
        mask = archive[offset + 512:offset + 512 + pixels]
        assert len(mask) == pixels
        bounds = Image.frombytes('L', (width, height), mask).getbbox()
        assert bounds
        result[portrait] = (bounds[0], bounds[2])
    return result


def actor_expectations(line, manifest, grounded, floor, bounds):
    """Describe each visible figure without changing story or emulator state."""
    people = manifest['people']
    pose = ((line.focus >> 9) & 3) | ((line.pose_flags & 15) << 2)
    companion_pose = ((line.focus >> 11) & 3) | ((line.pose_flags >> 4) << 2)
    speaking = line.speaker < people
    if line.stage != 0:
        return ([{'actor': line.speaker, 'pose': pose, 'full_body': False,
                  'placement': 'scene_badge',
                  'portrait': manifest['portrait_map'][pose][line.speaker]}]
                if speaking else [])
    face = line.speaker if speaking else line.companion
    selected = pose if speaking else companion_pose
    if face >= people:
        return []
    other = line.companion if speaking else 255
    distinct_other = other < people and other != face
    face_left = not distinct_other or face < other
    directions = manifest.get('quarter_map') if grounded and manifest.get('quarter_portraits') else None
    quarter = directions[1 if face_left else 0][selected][face] if directions else -1
    quarter_other = (directions[0 if face_left else 1][companion_pose][other]
                     if directions and distinct_other else -1)
    if quarter >= 0:
        actors = [(face, selected, quarter)]
        if quarter_other >= 0:
            actors.append((other, companion_pose, quarter_other))
            actors.sort()
            span = sum(bounds[portrait][1] - bounds[portrait][0] for _, _, portrait in actors)
            scale = 256 if span + 12 <= 240 else 228 * 256 // span
            while sum((bounds[portrait][1] * scale + 255) // 256 -
                      (bounds[portrait][0] * scale + 255) // 256
                      for _, _, portrait in actors) > 228:
                scale -= 1
            width, height = 256 * scale // 256, 264 * scale // 256
        else:
            scale, width, height = 256, 256, 264
        result = []
        for index, (actor, selected, portrait) in enumerate(actors):
            paired = len(actors) == 2
            actor_floor = floor - (4 if paired and index else 0)
            left, right = bounds[portrait]
            x = ((8 - (left * width + 255) // 256) if index == 0 else
                 (248 - (right * width + 255) // 256)) if paired else 0
            result.append({'actor': actor, 'pose': selected, 'full_body': True,
                           'portrait': portrait, 'placement': 'standing_pair' if paired else 'standing_single',
                           'side': ('left' if index == 0 else 'right') if paired else 'center',
                           'facing': ('RIGHT' if index == 0 else 'LEFT') if paired else
                                     ('RIGHT' if face_left else 'LEFT'),
                           'floor_y': actor_floor, 'position': [x, actor_floor - height],
                           'frame_size': [width, height], 'scale_q8': scale,
                           'native_scale': scale == 256})
        return result
    actors = [(face, selected)]
    if not grounded and distinct_other:
        actors.append((other, companion_pose))
    result = []
    for actor, selected in actors:
        fullbody = manifest['fullbody_map'][selected][actor]
        result.append({'actor': actor, 'pose': selected,
                       'full_body': grounded and fullbody >= 0,
                       'portrait': fullbody if grounded and fullbody >= 0 else
                                   manifest['portrait_map'][selected][actor]})
    return result


def contact_sheet(captures, output):
    from PIL import Image, ImageDraw
    columns = 4
    cell_height = 408
    sheet = Image.new('RGB', (columns * 256,
                              ((len(captures) + columns - 1) // columns) * cell_height),
                      '#12232d')
    draw = ImageDraw.Draw(sheet)
    for index, capture in enumerate(captures):
        x = index % columns * 256
        y = index // columns * cell_height
        with Image.open(output / capture['image']) as image:
            sheet.paste(image, (x, y + 24))
        draw.text((x + 4, y + 5),
                  f"{capture['scene']} line {capture['line'] + 1} page {capture['page'] + 1}",
                  fill='#f4dfb1')
    sheet.save(output / 'continuous-story-contact.png')


def run_case(args, manifest):
    elf = ROOT / 'build/arm9.elf'
    rom = ROOT / 'build/Coastline.nds'
    address = find_symbol(elf, 'game')
    story_address = find_symbol(elf, 'story')
    visual_address = find_symbol(elf, 'story_visual_stats')
    assert symbol_size(elf, 'story') == C.sizeof(Story) == 488
    assert (Story.page_start.offset, Story.page_end.offset, Story.page_index.offset) == (485, 486, 487)
    chapter = 'DCBAS'.index(args.case[0])
    event = int(args.case[1:]) - 1
    expected = chapter * 25 + event
    scene = manifest['events'][expected]
    assert scene['id'] == args.case
    shots = set()
    line_index = 0
    for shot in scene['shots']:
        shots.add(line_index)
        line_index += len(shot['lines'])
    authored_lines = [line for shot in scene['shots'] for line in shot['lines']]
    background_names = [item['name'] for item in
                        json.loads((ROOT / 'story/staging.json').read_text())['backgrounds']]
    floors = {item['name']: item.get('ground_y', 284) for item in
              json.loads((ROOT / 'art/story/tall-scenes.json').read_text())['backgrounds']}
    bounds = quarter_portrait_bounds(rom, manifest)
    captures = []
    page_flips = 0
    pause_checked = False
    text_by_line = {}

    with Emulator(rom, args.save_dir) as emulator:
        emulator.run(180)
        drive = DriveThrough(emulator, address, args.output)
        assert drive.game().save.reserved[0] == expected
        drive.menu(0)
        drive.press('a')
        assert drive.game().mode == 11
        drive.press('down')
        drive.press('a')
        assert drive.game().mode == 12
        drive.press('down')
        drive.press('a')
        assert drive.game().mode == 13

        def view():
            return Story.from_buffer_copy(emulator.read_ram(story_address, C.sizeof(Story)))

        for _ in range(1600):
            state = view()
            assert drive.game().mode == 13 and not state.failed
            if state.phase == 4:
                assert drive.game().save.reserved[0] == expected + 1
                drive.ticks(16)
                assert drive.game().save_ok and not drive.game().save_requested
                emulator.screenshot(args.output / f'continuous-{args.case.lower()}-clear.png')
                drive.press('a')
                assert drive.game().mode == 12
                break
            if state.phase != 1:
                drive.ticks(2)
                continue
            assert 0 <= state.page_start < state.page_end <= state.current.length < 192
            assert state.page_start <= state.revealed <= state.page_end
            text = bytes(state.current.text)
            assert text.decode('ascii') == authored_lines[state.line]['text']
            if state.revealed < state.page_end:
                drive.press('a')
                revealed = view()
                assert revealed.phase == 1 and revealed.line == state.line
                assert revealed.page_index == state.page_index and revealed.page_end == state.page_end
                assert revealed.revealed == revealed.page_end
                continue
            drive.ticks(max(0, 8 - state.age))
            state = view()
            key = (state.line, state.page_index)
            pages = text_by_line.setdefault(state.line, [])
            assert len(pages) == state.page_index, (args.case, key, len(pages))
            assert state.page_start == sum(len(page) for page in pages)
            pages.append(text[state.page_start:state.page_end])
            image = f'continuous-{args.case.lower()}-{state.line:02d}-page-{state.page_index + 1}.png'
            emulator.screenshot(args.output / image)
            assert (emulator.width, emulator.height) == (256, 384)
            pose = ((state.current.focus >> 9) & 3) | ((state.current.pose_flags & 15) << 2)
            companion_pose = ((state.current.focus >> 11) & 3) | ((state.current.pose_flags >> 4) << 2)
            background_name = background_names[state.current.background]
            grounded = background_name in manifest['fullbody_backgrounds']
            expected_actors = actor_expectations(state.current, manifest, grounded,
                                                 floors.get(background_name, 284), bounds)
            captures.append({'scene': args.case, 'line': state.line,
                             'page': state.page_index, 'start': state.page_start,
                             'end': state.page_end, 'length': state.current.length,
                             'speaker': state.current.speaker,
                             'companion': state.current.companion,
                             'background': state.current.background,
                             'background_name': background_name,
                             'stage': state.current.stage,
                             'pose': pose, 'companion_pose': companion_pose,
                             'expected_actors': expected_actors,
                             'shot_start': state.line in shots, 'image': image})
            if args.case == 'D01' and not pause_checked:
                drive.press('start')
                assert view().pause
                drive.ticks(8)
                paused = view()
                assert paused.line == state.line and paused.page_start == state.page_start
                emulator.screenshot(args.output / 'continuous-d01-paused.png')
                drive.press('b')
                resumed = view()
                assert not resumed.pause and resumed.line == state.line
                assert resumed.page_start == state.page_start and resumed.page_end == state.page_end
                pause_checked = True
            if state.page_end < state.current.length:
                current = bytes(state.current)
                previous = bytes(state.previous)
                drive.press('a')
                next_page = view()
                assert next_page.phase == 1 and next_page.line == state.line
                assert bytes(next_page.current) == current and bytes(next_page.previous) == previous
                assert next_page.page_index == state.page_index + 1
                assert next_page.page_start == state.page_end
                assert next_page.page_start <= next_page.revealed <= next_page.page_end
                assert next_page.age >= state.age
                page_flips += 1
            else:
                assert b''.join(pages) == text
                drive.press('a')
        else:
            raise AssertionError(('Scene did not finish', args.case))
        assert len(text_by_line) == len(authored_lines)
        visual = list((C.c_uint32 * 5).from_buffer_copy(emulator.read_ram(visual_address, 20)))
        assert not visual[0]
    result = {'scene': args.case, 'authored_lines': len(authored_lines),
              'pages': len(captures), 'page_flips': page_flips,
              'pause_resume_checked': pause_checked,
              'text_preserved': True, 'page_directions_preserved': True,
              'archive_failures': visual[0], 'captures': captures}
    (args.output / f'continuous-{args.case.lower()}-results.json').write_text(
        json.dumps(result, indent=2) + '\n')
    print(f"{args.case}: {len(authored_lines)} lines, {len(captures)} pages, {page_flips} page flips", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/continuous-story')
    parser.add_argument('--template', type=Path,
                        default=ROOT / 'build/character-integration/final-fresh-save/Coastline.dsv')
    parser.add_argument('--case')
    parser.add_argument('--save-dir', type=Path)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / 'build/story-manifest.json').read_text())
    if args.case:
        assert args.save_dir
        run_case(args, manifest)
        return
    rom = ROOT / 'build/Coastline.nds'
    sha = hashlib.sha256(rom.read_bytes()).hexdigest()
    library_path = args.output / 'libcontinuous-story.so'
    subprocess.run(['gcc', '-std=c11', '-O2', '-shared', '-fPIC', '-Isrc', '-Ibuild',
                    'src/game.c', 'src/story.c', 'src/story_io.c', 'src/road.c',
                    'src/traffic.c', '-o', str(library_path)], cwd=ROOT, check=True)
    library = C.CDLL(str(library_path))
    library.game_init.argtypes = [C.POINTER(Game), C.c_uint32]
    if not args.template.is_file():
        fresh = args.output / 'template-save'
        with Emulator(rom, fresh) as emulator:
            emulator.run(180)
            DriveThrough(emulator, find_symbol(ROOT / 'build/arm9.elf', 'game'), args.output)
            emulator.run(30)
        args.template = fresh / 'Coastline.dsv'
    cases = ['D01', 'D04', 'D05', 'D24', 'S17']
    results = []
    for case in cases:
        profile = Game()
        library.game_init(C.byref(profile), 123)
        profile.save.reserved[0] = 'DCBAS'.index(case[0]) * 25 + int(case[1:]) - 1
        directory = args.output / ('fixture-' + case.lower())
        write_battery(args.template, directory / 'Coastline.dsv', encode_profile(library, profile))
        subprocess.run([sys.executable, __file__, '--case', case, '--output', str(args.output),
                        '--save-dir', str(directory)], cwd=ROOT, check=True)
        results.append(json.loads((args.output / f'continuous-{case.lower()}-results.json').read_text()))
    assert hashlib.sha256(rom.read_bytes()).hexdigest() == sha
    assert sum(result['page_flips'] for result in results) > 0
    expected_actors = [actor for result in results for capture in result['captures']
                       for actor in capture['expected_actors']]
    assert any(actor['full_body'] for actor in expected_actors)
    assert any(actor['actor'] == 0 and not actor['full_body'] for actor in expected_actors)
    captures = [capture for result in results for capture in result['captures']
                if capture['shot_start'] or capture['page']]
    contact_sheet(captures, args.output)
    result = {'rom_sha256': sha, 'cases': cases, 'lcd_dimensions': [256, 384],
              'scene_rows': [0, 288], 'lcd_seam_y': 192, 'dialogue_rows': [288, 384],
              'button_only': True, 'emulated_ram_read_only': True,
              'fixture_note': 'Save frontiers are native codec visual fixtures, not earned progression.',
              'pagination_note': 'S17 exercises an authored line across the three-row dialogue pages.',
              'page_flips': sum(result['page_flips'] for result in results),
              'scenes': results}
    (args.output / 'continuous-story-results.json').write_text(json.dumps(result, indent=2) + '\n')


if __name__ == '__main__':
    main()
