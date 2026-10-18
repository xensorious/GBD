#!/usr/bin/env python3
"""Focused real-ROM regression checks after a presentation-only change."""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

from emulator import Emulator, ROOT
from game_snapshot import find_symbol
from inspect_rom import inspect
from playthrough import Playthrough


def color_matches(pixel, color):
    expected = ((color & 31) * 255 // 31,
                ((color >> 5) & 31) * 255 // 31,
                ((color >> 10) & 31) * 255 // 31)
    return all(abs(a - b) <= 8 for a, b in zip(pixel, expected))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=ROOT / "build" / "Asterwild.nds")
    parser.add_argument("--elf", type=Path, default=ROOT / "build" / "arm9.elf")
    parser.add_argument("--output", type=Path, default=ROOT / "build" / "validation")
    args = parser.parse_args()
    runtime = args.output / "visual-runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    rom, elf = runtime / "Asterwild.nds", runtime / "arm9.elf"
    shutil.copy2(args.rom, rom)
    shutil.copy2(args.elf, elf)
    result = {"rom": inspect(rom), "input": "buttons only"}
    save_dir = args.output / "visual-save"
    if save_dir.exists():
        shutil.rmtree(save_dir)
    screenshots = args.output / "screens"
    screenshots.mkdir(exist_ok=True)
    address = find_symbol(elf, "game")
    with Emulator(rom, save_dir) as emu:
        run = Playthrough(emu, address, screenshots)
        try:
            emu.run(180)
            assert run.game().mode == 0
            before = run.game().frame
            emu.run(120)
            result["title_fps"] = (run.game().frame - before) / 2
            assert result["title_fps"] >= 25
            run.press("a"); run.settle()
            run.meet(0); run.settle()
            assert run.game().mode == 3
            run.press("a"); run.settle()
            assert run.game().save.party_count == 1
            run.walk(20)
            run.wait_ticks(24)
            flower = run.world.flowers[0]
            wx, wy = flower.x, flower.y - 14

            def pixel_at_flower():
                cam_x = int.from_bytes(emu.read_ram(find_symbol(elf, "cam_x"), 4), "little", signed=True)
                cam_y = int.from_bytes(emu.read_ram(find_symbol(elf, "cam_y"), 4), "little", signed=True)
                return emu.image().getpixel((wx - cam_x, wy - cam_y))

            assert not run.game().save.flowers & 1
            core_pixel = pixel_at_flower()
            assert color_matches(core_pixel, 0xDFDF), ("Missing lantern blossom cue", core_pixel)
            run.shot("blossom-before")
            before = run.game().frame
            emu.run(120)
            result["field_near_blossom_fps"] = (run.game().frame - before) / 2
            assert result["field_near_blossom_fps"] >= 25
            run.press("a"); run.settle()
            assert run.game().save.flowers & 1
            world_color = int.from_bytes(emu.read_ram(find_symbol(elf, "world_art") +
                                                     (wy * 1024 + wx) * 2, 2), "little")
            removed_pixel = pixel_at_flower()
            assert color_matches(removed_pixel, world_color), ("Collected cue remained visible", removed_pixel, world_color)
            run.shot("blossom-after")
            result["blossom"] = {"world_core": [wx, wy], "before_rgb": core_pixel,
                                  "after_rgb": removed_pixel, "removed": True}
            # Revisit the changed ending caption through a short actual trial,
            # without replaying the optional quest or the native rule suite.
            run.meet(10); run.settle()
            assert run.game().mode == 4
            run.move(3); run.move(3)
            assert run.game().save.party_count == 2
            run.meet(3); run.settle()
            assert run.game().mode == 4 and run.game().battle.is_trial
            for _ in range(50):
                g = run.game()
                if g.mode != 4:
                    break
                active = g.save.party[g.battle.active]
                run.move(2 if g.battle.intent == 1 else 1 if active.energy >= 2 else 0)
            assert run.game().mode == 10 and run.game().save.flags & 64
            run.shot("lantern-crest")
            content = rom.read_bytes()
            assert b"The Lantern Crest\0" in content and b"The Lantern Seal\0" not in content
            assert emu.audio_nonzero > 0
            result.update({"ending": "Lantern Crest displayed after actual trial",
                           "audio_nonzero_batches": emu.audio_nonzero,
                           "hardware_frames": emu.frames, "screenshots": run.shots})
        except BaseException:
            emu.screenshot(args.output / "visual-failure.png")
            raise
    # Use the fully completed prior journey as an unchanged battery-save fixture.
    subprocess.run([sys.executable, str(ROOT / "tests" / "playthrough.py"), "--resume",
                    "--rom", str(rom), "--elf", str(elf), "--output", str(args.output)], check=True)
    result["existing_completed_save"] = "exact progress preserved in a fresh process"
    baseline = json.loads((args.output / "results.json").read_text())
    result["full_suite_baseline_sha256"] = baseline["rom"]["sha256"]
    (args.output / "final-visual-results.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
