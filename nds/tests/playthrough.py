#!/usr/bin/env python3
"""Validate native rules, then play the actual DS cartridge using only buttons.

RAM is observed read-only for assertions. No cheats, direct state writes, test
hooks, touchscreen input, or emulator state restores are used in this run.
"""
from __future__ import annotations

import argparse
import array
import json
import shutil
import subprocess
import sys
from collections import deque
from pathlib import Path

from emulator import Emulator, ROOT
from game_snapshot import MODES, find_symbol, symbol_size, read_game, read_world
from inspect_rom import inspect


def native_tests():
    for name, sources in (("gameplay", ["game.c"]), ("movement", ["game.c"]),
                          ("routes", ["game.c", "world.c"])):
        target = ROOT / "build" / ("test-" + name)
        command = ["gcc", "-std=c99", "-O2", "-g", "-Wall", "-Wextra", "-Werror",
                   "-pedantic", "-fsanitize=address,undefined", "-I", str(ROOT / "src"),
                   str(ROOT / "tests" / (name + ".c"))]
        command += [str(ROOT / "src" / source) for source in sources]
        subprocess.run(command + ["-o", str(target)], check=True)
        subprocess.run([str(target)], check=True)


class Playthrough:
    def __init__(self, emulator, address, output):
        self.e, self.address, self.output = emulator, address, output
        self.shots = []
        self.steps = []
        self.world = None
        self.solids = None
        self.mask = None

    def game(self):
        return read_game(self.e, self.address)

    def wait_ticks(self, count=1, keys=()):
        before = self.game().frame
        for _ in range(max(30, count * 12)):
            if self.game().frame - before >= count:
                return
            self.e.run(max(2, min(12, (count - (self.game().frame - before)) * 2)), keys)
        raise AssertionError("Game update stalled")

    def press(self, key):
        # Keep a button down through an actual game tick so a busy rendering
        # frame cannot turn a simulated press into a missed frontend event.
        self.wait_ticks(1, (key,))
        self.wait_ticks(1)

    def shot(self, name):
        self.wait_ticks(2)
        path = self.e.screenshot(self.output / (name + ".png"))
        self.shots.append(path.name)
        g = self.game()
        print(f"Screenshot {name}: {MODES[g.mode]}, frame {g.frame}", flush=True)

    def settle(self, growth=1, limit=80):
        for _ in range(limit):
            g = self.game()
            if g.mode == 2 or (g.mode == 4 and g.battle.phase == 1):
                self.press("a")
            elif g.mode == 9:
                self.shot("growth-" + str(g.growth_slot))
                if growth == 2 and not g.cursor:
                    self.press("right")
                self.press("a")
            else:
                self.wait_ticks(12)
                return
        raise AssertionError("Dialogue did not terminate")

    def build_mask(self):
        from PIL import Image, ImageDraw
        self.world, self.solids = read_world(self.e, self.game())
        mask = Image.new("L", (1024, 768), 0)
        draw = ImageDraw.Draw(mask)
        draw.rectangle((8, 8, 1016, 760), fill=1)
        for solid in self.solids:
            draw.rectangle((solid.x - 5, solid.y - 1,
                            solid.x + solid.w + 5, solid.y + solid.h + 6), fill=0)
        for point in self.world.npcs:
            for dy in range(-12, 13):
                for dx in range(-12, 13):
                    if dx * dx + dy * dy < 144:
                        x, y = point.x + dx, point.y + dy
                        if 0 <= x < 1024 and 0 <= y < 768:
                            mask.putpixel((x, y), 0)
        self.mask = mask.tobytes()

    def nearest(self, x, y, g):
        best, distance = -1, 46 * 46
        for i, p in enumerate(self.world.npcs):
            d = (x - p.x) ** 2 + (y - p.y) ** 2
            if d < distance:
                best, distance = i, d
        if g.save.flags & 2:
            for i, p in enumerate(self.world.wilds):
                if g.save.wild_taken & (1 << i) or g.wild_cooldown[i]:
                    continue
                d = (x - p.x) ** 2 + (y - p.y) ** 2
                if d < distance and d < 40 * 40:
                    best, distance = 10 + i, d
            for i, p in enumerate(self.world.flowers):
                if g.save.flowers & (1 << i):
                    continue
                d = (x - p.x) ** 2 + (y - p.y) ** 2
                if d < distance and d < 32 * 32:
                    best, distance = 20 + i, d
            p = self.world.camp
            if (x - p.x) ** 2 + (y - p.y) ** 2 < distance:
                best = 30
        return best

    def walk(self, target, destination=None):
        if self.mask is None:
            self.build_mask()
        g = self.game()
        assert g.mode == 1 and not g.ui_focus
        x0, y0 = g.save.player_x, g.save.player_y
        assert x0 % 16 == 8 and y0 % 16 == 8 and not g.move_remaining, (x0, y0, g.move_remaining)
        width, height = 64, 48
        start = (y0 // 16) * width + x0 // 16
        previous = array.array("i", [-1]) * (width * height)
        previous[start] = start
        queue = deque([start])
        goal = None
        while queue:
            cell = queue.popleft()
            x, y = (cell % width) * 16 + 8, (cell // width) * 16 + 8
            arrived = (x, y) == destination if destination else self.nearest(x, y, g) == target
            if arrived:
                goal = cell
                break
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = cell % width + dx, cell // width + dy
                if not (0 <= nx < width and 0 <= ny < height):
                    continue
                neighbor = ny * width + nx
                if previous[neighbor] >= 0:
                    continue
                if all(self.mask[(y + dy * distance) * 1024 + x + dx * distance]
                       for distance in range(1, 17)):
                    previous[neighbor] = cell
                    queue.append(neighbor)
        assert goal is not None, f"No route to {destination or target}"
        path = []
        while goal != start:
            path.append(goal)
            goal = previous[goal]
        path.reverse()
        # Group whole cardinal tiles. Observe endpoints rather than assuming
        # a fixed ratio between hardware VBlanks and rendered game updates.
        last = (x0, y0)
        segments = []
        for cell in path:
            point = ((cell % width) * 16 + 8, (cell // width) * 16 + 8)
            key = "left" if point[0] < last[0] else "right" if point[0] > last[0] else "up" if point[1] < last[1] else "down"
            if segments and segments[-1][0] == key:
                segments[-1] = (key, point)
            else:
                segments.append((key, point))
            last = point
        for key, (tx, ty) in segments:
            for _ in range(1000):
                now = self.game()
                assert now.mode == 1
                remaining = abs(tx - now.save.player_x) + abs(ty - now.save.player_y)
                if remaining == 0:
                    break
                before = (now.save.player_x, now.save.player_y)
                # At most 4 walking pixels can move in one hardware frame;
                # this bound cannot overshoot the requested tile endpoint.
                self.e.run(min(120, max(1, remaining // 4)), (key,))
                after = self.game()
                if (after.save.player_x, after.save.player_y) == before:
                    continue  # Rendering may occupy a complete short run.
                assert abs(tx - after.save.player_x) + abs(ty - after.save.player_y) < remaining, (target, key, before, (tx, ty))
            else:
                raise AssertionError("Movement did not reach waypoint")
        self.e.run(2)
        now = self.game()
        if destination:
            assert (now.save.player_x, now.save.player_y) == destination
        else:
            assert self.nearest(now.save.player_x, now.save.player_y, now) == target
        assert not now.move_remaining
        self.steps.append({"target": destination or target, "cardinal_tiles": len(path)})
        print(f"Walked to {destination or target} with buttons ({len(path)} cardinal tiles)", flush=True)

    def camera(self, elf):
        return tuple(int.from_bytes(self.e.read_ram(find_symbol(elf, name), 4),
                                    "little", signed=True)
                     for name in ("cam_x", "cam_y"))

    @staticmethod
    def color_matches(pixel, color):
        expected = ((color & 31) * 255 // 31,
                    ((color >> 5) & 31) * 255 // 31,
                    ((color >> 10) & 31) * 255 // 31)
        return all(abs(a - b) <= 8 for a, b in zip(pixel, expected))

    def stable_pose(self):
        # ARM9 can already be preparing the next frame when the frontend
        # exposes its last presented image. Sample inside both animation
        # phases so a normal one-frame presentation delay is unambiguous.
        for _ in range(48):
            frame = self.game().frame
            if 4 <= frame % 36 <= 31 and 4 <= frame % 24 <= 19:
                return
            self.wait_ticks(1)
        raise AssertionError("Could not sample a stable animation pose")

    def depth_proof(self, elf):
        """Displayed pixels prove the rail hides the hero, then falls behind."""
        depth = self.e.read_ram(find_symbol(elf, "world_depth"), 1024 * 768)
        hero = array.array("H")
        hero.frombytes(self.e.read_ram(find_symbol(elf, "hero_idle_art"), 4 * 2 * 24 * 32 * 2))
        world = array.array("H")
        world.frombytes(self.e.read_ram(find_symbol(elf, "world_art"), 1024 * 768 * 2))
        results = {}
        for name, (x, y) in (("behind", (616, 296)), ("front", (616, 344))):
            self.walk(None, (x, y))
            self.wait_ticks(24)
            self.stable_pose()
            g = self.game()
            assert not g.walking and not g.move_remaining
            cam_x, cam_y = self.camera(elf)
            screen = self.e.image()
            frame = (g.facing * 2 + ((g.frame // 36) & 1)) * 24 * 32
            hidden = hidden_match = visible = visible_match = overlap = overlap_match = 0
            for j in range(30):  # Exclude the sprite's contact/shadow row.
                for i in range(24):
                    color = hero[frame + j * 24 + i]
                    if not color:
                        continue
                    wx, wy = x - 12 + i, y - 32 + j
                    sx, sy = wx - cam_x, wy - cam_y
                    if not (0 <= sx < 256 and 28 <= sy < 144):
                        continue
                    pixel = screen.getpixel((sx, sy))
                    scenery = world[wy * 1024 + wx]
                    plane = depth[wy * 1024 + wx]
                    if plane > y // 4:
                        hidden += 1
                        hidden_match += self.color_matches(pixel, scenery)
                    else:
                        visible += 1
                        visible_match += self.color_matches(pixel, color)
                        if plane and 592 <= wx < 656 and 280 <= wy < 328 and color != scenery:
                            overlap += 1
                            overlap_match += self.color_matches(pixel, color)
            assert visible >= 35 and visible_match / visible >= .9, (name, visible, visible_match)
            if name == "behind":
                assert hidden >= 20, ("Rail does not overlap the hero", hidden)
                assert hidden_match / hidden >= .9, ("Rail did not hide hero", hidden_match, hidden)
            else:
                assert hidden == 0, ("Hero remained behind railing after passing it", hidden)
                assert overlap >= 10 and overlap_match / overlap >= .9, (overlap, overlap_match)
            self.shot("stairs-" + name)
            results[name] = {"position": [x, y], "occluded_pixels": hidden,
                             "occluded_pixels_match_scenery": hidden_match,
                             "visible_pixels": visible, "visible_pixels_match_hero": visible_match,
                             "hero_pixels_in_front_of_railing": overlap,
                             "railing_overlap_pixels_match_hero": overlap_match}
        print("Displayed stair-rail depth proof: " + json.dumps(results), flush=True)
        return results

    def animation_proof(self, elf):
        """Two stationary displayed poses move foliage and the hero, not rules."""
        from PIL import ImageChops
        # This ordinary path puts tree crowns and shrubs above the player's
        # unobstructed silhouette; no test teleport or special renderer path.
        self.walk(None, (568, 296))
        self.wait_ticks(24)
        self.stable_pose()
        baseline = self.game()
        assert not baseline.walking and not baseline.move_remaining
        save = bytes(baseline.save)
        collision = self.e.read_ram(self.world.solids, len(self.solids) * 8)
        camera = self.camera(elf)
        first = self.e.image().crop((0, 0, 256, 192))
        # Choose the opposite pose for both periodic idle and wind animation.
        for _ in range(150):
            current = self.game()
            if ((current.frame // 36) & 1) != ((baseline.frame // 36) & 1) and \
               ((current.frame // 24) & 1) != ((baseline.frame // 24) & 1) and \
               4 <= current.frame % 36 <= 31 and 4 <= current.frame % 24 <= 19:
                break
            self.wait_ticks(1)
        else:
            raise AssertionError("Idle/wind phases did not advance")
        current = self.game()
        second = self.e.image().crop((0, 0, 256, 192))
        assert bytes(current.save) == save, "Ambient animation changed progress or position"
        assert camera == self.camera(elf), "Stationary camera shifted"
        assert collision == self.e.read_ram(self.world.solids, len(self.solids) * 8), "Animation changed collision"
        x, y = current.save.player_x - camera[0], current.save.player_y - camera[1]
        silhouette = (x - 12, y - 32, x + 12, y - 2)
        idle_diff = ImageChops.difference(first.crop(silhouette), second.crop(silhouette))
        idle_data = idle_diff.get_flattened_data() if hasattr(idle_diff, "get_flattened_data") else idle_diff.getdata()
        idle_pixels = sum(pixel != (0, 0, 0) for pixel in idle_data)
        assert idle_pixels >= 8, ("Hero has no visible idle animation", idle_pixels)
        # Read the authored animation rectangles and inspect displayed pixels
        # where the base and alternate foliage artwork actually differ.
        import ctypes as C
        class Animation(C.LittleEndianStructure):
            _fields_ = [("x", C.c_int16), ("y", C.c_int16),
                        ("w", C.c_uint16), ("h", C.c_uint16), ("offset", C.c_uint32)]
        table_bytes = self.e.read_ram(find_symbol(elf, "world_animations"), symbol_size(elf, "world_animations"))
        entries = (Animation * (len(table_bytes) // C.sizeof(Animation))).from_buffer_copy(table_bytes)
        art_length = max(patch.offset + patch.w * patch.h for patch in entries)
        alternate = array.array("H")
        alternate.frombytes(self.e.read_ram(find_symbol(elf, "world_animation_art"), art_length * 2))
        base = array.array("H")
        base.frombytes(self.e.read_ram(find_symbol(elf, "world_art"), 1024 * 768 * 2))
        foliage = foliage_match = 0
        for patch in entries:
            for j in range(patch.h):
                for i in range(patch.w):
                    wx, wy = patch.x + i, patch.y + j
                    sx, sy = wx - camera[0], wy - camera[1]
                    if not (0 <= sx < 256 and 28 <= sy < 140):
                        continue
                    if silhouette[0] <= sx < silhouette[2] and silhouette[1] <= sy < silhouette[3]:
                        continue
                    normal = base[wy * 1024 + wx]
                    windy = alternate[patch.offset + j * patch.w + i]
                    if normal == windy:
                        continue
                    before = first.getpixel((sx, sy))
                    after = second.getpixel((sx, sy))
                    source0, source1 = (windy, normal) if (baseline.frame // 24) & 1 else (normal, windy)
                    foliage += 1
                    foliage_match += self.color_matches(before, source0) and self.color_matches(after, source1)
        assert foliage >= 20 and foliage_match / foliage >= .8, ("Foliage did not animate on screen", foliage, foliage_match)
        first.save(self.output / "living-world-pose-1.png")
        second.save(self.output / "living-world-pose-2.png")
        first.save(self.output / "living-world.gif", save_all=True,
                   append_images=[second], duration=800, loop=0)
        self.shots.extend(("living-world-pose-1.png", "living-world-pose-2.png"))
        result = {"position": [current.save.player_x, current.save.player_y],
                  "frames": [baseline.frame, current.frame], "idle_changed_pixels": idle_pixels,
                  "foliage_changed_source_pixels": foliage, "foliage_display_matches": foliage_match,
                  "camera_unchanged": True, "progress_unchanged": True, "collision_unchanged": True}
        print("Stationary animation proof: " + json.dumps(result), flush=True)
        return result

    def presentation_tour(self, elf):
        """Capture ordinary world play after the opening's rules have passed."""
        from PIL import Image
        self.walk(None, (568, 296))
        self.wait_ticks(24)
        save, camera = bytes(self.game().save), self.camera(elf)
        frames = []
        for _ in range(24):
            self.wait_ticks(4)
            frames.append(self.e.image().crop((0, 0, 256, 192)))
        assert save == bytes(self.game().save) and camera == self.camera(elf)

        def gif(name, images, duration):
            palette = images[0].quantize(colors=256)
            encoded = [image.quantize(palette=palette, dither=Image.Dither.NONE)
                       .resize((512, 384), Image.Resampling.NEAREST) for image in images]
            encoded[0].save(self.output / name, save_all=True, append_images=encoded[1:],
                            duration=duration, loop=0, optimize=False)

        gif("living-world.gif", frames, 133)
        self.walk(None, (536, 296))
        self.wait_ticks(12)
        frames, positions = [], []
        for index in range(12):
            self.wait_ticks(2, ("right",))
            g = self.game()
            assert (g.save.player_x, g.save.player_y) == (544 + index * 8, 296)
            positions.append([g.save.player_x, g.save.player_y])
            frames.append(self.e.image().crop((0, 0, 256, 192)))
        self.wait_ticks(2)
        assert not self.game().move_remaining
        gif("walking-stairs.gif", frames, 67)
        self.walk(None, (888, 360))
        self.wait_ticks(24)
        self.shot("forest-path")
        before = self.game().frame
        self.e.run(120)
        forest_fps = (self.game().frame - before) / 2
        assert forest_fps >= 25, "Animated forest exceeded the field frame budget"
        result = {"stationary_animation_updates": 96, "stationary_frames": 24,
                  "walking_frames": 12, "walking_positions": positions,
                  "forest_fps": forest_fps,
                  "gifs": ["living-world.gif", "walking-stairs.gif"]}
        print("Presentation tour: " + json.dumps(result), flush=True)
        return result

    def meet(self, target):
        self.walk(target)
        self.wait_ticks(12)
        self.press("a")

    def move(self, move):
        g = self.game()
        assert g.mode == 4 and g.battle.phase == 0
        if (g.cursor ^ move) & 1:
            self.press("right")
        if (self.game().cursor ^ move) & 2:
            self.press("down")
        assert self.game().cursor == move
        self.press("a")
        self.settle()


def fresh_resume(rom, elf, save_dir, expected):
    address = find_symbol(elf, "game")
    with Emulator(rom, save_dir) as emu:
        run = Playthrough(emu, address, save_dir.parent / "screens")
        emu.run(180)
        g = run.game()
        assert g.mode == 0 and g.has_save
        assert g.save.flags & 64 and g.save.party_count >= 2
        actual = bytearray(bytes(g.save))
        actual[8:12] = b"\0" * 4
        assert actual.hex() == expected["save_hex"], "Fresh-process progress differs from saved journey"
        run.shot("continue-journey")
        run.press("a")
        assert run.game().mode == 1
        run.shot("resumed-world")
    print("Fresh-process cartridge save/continue verified.", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=ROOT / "build" / "Asterwild.nds")
    parser.add_argument("--elf", type=Path, default=ROOT / "build" / "arm9.elf")
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "validation")
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    if args.resume:
        expected = json.loads((args.output / "expected.json").read_text())
        fresh_resume(args.rom, args.elf, args.output / "save", expected)
        return
    if not args.skip_native:
        native_tests()
    metadata = inspect(args.rom)
    runtime = args.output / "runtime"
    runtime.mkdir(exist_ok=True)
    rom, elf = runtime / "Asterwild.nds", runtime / "arm9.elf"
    shutil.copy2(args.rom, rom)
    shutil.copy2(args.elf, elf)
    save_dir = args.output / "save"
    if save_dir.exists():
        shutil.rmtree(save_dir)
    screenshots = args.output / "screens"
    screenshots.mkdir(exist_ok=True)
    address = find_symbol(elf, "game")
    results = {"rom": metadata, "screenshots": [], "routes": [], "input": "buttons only"}
    with Emulator(rom, save_dir) as emu:
        run = Playthrough(emu, address, screenshots)
        try:
            emu.run(180)
            g = run.game()
            assert g.save.magic == 0x41535452 and g.mode == 0
            assert (emu.width, emu.height) == (256, 384)
            assert len(emu.image().crop((0, 0, 256, 192)).getcolors(50000)) > 32, "Title framebuffer is blank or corrupt"
            run.shot("title")
            before = run.game().frame
            emu.run(120)
            results["title_fps"] = (run.game().frame - before) / 2
            assert results["title_fps"] >= 25, "Title updates fell below the playable frame budget"
            run.press("a")
            run.shot("opening-dialogue")
            run.settle()
            assert run.game().mode == 1
            run.shot("harbor-arrival")
            before = run.game().frame
            emu.run(120)
            results["field_fps"] = (run.game().frame - before) / 2
            assert results["field_fps"] >= 25, "Field updates fell below the playable frame budget"
            # Select focuses the notebook; movement is frozen until returning.
            run.press("select")
            position = (run.game().save.player_x, run.game().save.player_y)
            run.wait_ticks(8, ("right",))
            assert position == (run.game().save.player_x, run.game().save.player_y)
            assert run.game().ui_focus and run.game().ui_tab == 0
            run.shot("focused-map")
            for tab, name in ((1, "empty-team"), (2, "island-map"), (3, "field-guide"), (4, "system-menu")):
                run.press("r")
                assert run.game().ui_tab == tab
                run.shot(name)
            run.press("l")
            assert run.game().ui_tab == 3
            run.press("select")
            assert not run.game().ui_focus and run.game().mode == 1
            run.meet(0)
            run.shot("mira-welcome")
            run.settle()
            assert run.game().mode == 3
            run.shot("starter-ferro")
            run.press("right"); run.shot("starter-solkin")
            run.press("right"); run.shot("starter-nimu")
            run.press("left"); run.press("left"); run.press("a")
            run.settle()
            assert run.game().save.party_count == 1 and run.game().save.starter == 0
            run.shot("first-companion")
            run.walk(1)
            run.wait_ticks(24)
            run.shot("town-commons")
            run.meet(1)
            run.shot("jun-field-lesson")
            run.settle()
            assert run.game().save.flags & 4
            run.meet(10)
            run.settle()
            assert run.game().mode == 4
            run.shot("wild-battle")
            run.move(3)
            assert run.game().battle.calm == 1
            run.shot("battle-intent")
            run.move(3)
            assert run.game().save.party_count == 2
            run.shot("new-friend")
            results["depth_proof"] = run.depth_proof(elf)
            results["animation_proof"] = run.animation_proof(elf)
            run.press("x")
            run.shot("team-notebook")
            run.press("select")
            assert run.game().mode == 1 and not run.game().ui_focus
            run.meet(2)
            run.shot("edda-request")
            run.settle()
            for target in (20, 21, 22):
                run.meet(target)
                run.shot("lantern-blossom-" + str(target - 19))
                run.settle()
            run.meet(2)
            run.settle()
            assert run.game().save.flags & 16
            run.meet(30)
            run.settle()
            run.shot("lantern-camp")
            run.meet(3)
            run.shot("vale-trial-invitation")
            run.settle()
            assert run.game().mode == 4 and run.game().battle.is_trial
            run.shot("keeper-trial")
            # Exercise the actual X button and party switch within battle.
            run.press("x")
            run.shot("trial-party-switch")
            run.press("down"); run.press("a"); run.settle()
            assert run.game().battle.active == 1
            for turn in range(50):
                g = run.game()
                if g.mode != 4:
                    break
                active = g.save.party[g.battle.active]
                run.move(2 if g.battle.intent == 1 else 1 if active.energy >= 2 else 0)
            else:
                raise AssertionError("Trial did not terminate")
            assert run.game().mode == 10 and run.game().save.flags & 64
            run.shot("lantern-crest")
            run.press("a"); run.settle(growth=2)
            assert run.game().mode == 1
            run.shot("chapter-complete")
            results["presentation"] = run.presentation_tour(elf)
            run.press("start")
            for _ in range(3):
                run.press("down")
            assert run.game().cursor == 3
            run.press("a")
            run.wait_ticks(6)
            save = bytearray(bytes(run.game().save))
            save[8:12] = b"\0" * 4
            (args.output / "expected.json").write_text(json.dumps({"save_hex": save.hex()}))
            assert emu.audio_nonzero > 0
            results.update({"screenshots": run.shots, "routes": run.steps,
                            "game_frames": run.game().frame, "hardware_frames": emu.frames,
                            "audio_nonzero_batches": emu.audio_nonzero,
                            "party_count": run.game().save.party_count,
                            "flags": run.game().save.flags})
        except BaseException:
            emu.screenshot(args.output / "failure.png")
            g = run.game()
            print(f"Failure at mode={g.mode} frame={g.frame} position={g.save.player_x},{g.save.player_y}", file=sys.stderr)
            raise
    subprocess.run([sys.executable, str(Path(__file__).resolve()), "--resume", "--rom", str(rom),
                    "--elf", str(elf), "--output", str(args.output)], check=True)
    results["fresh_process_save"] = "passed"
    (args.output / "results.json").write_text(json.dumps(results, indent=2))
    print(json.dumps(results, indent=2), flush=True)


if __name__ == "__main__":
    main()
