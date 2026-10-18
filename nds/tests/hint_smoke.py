#!/usr/bin/env python3
"""Check the final hint-label format against an existing real battery save."""
import json
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import ImageOps

from emulator import Emulator, ROOT
from game_snapshot import find_symbol
from inspect_rom import inspect
from playthrough import Playthrough


def main():
    output = ROOT / "build" / "validation"
    runtime = output / "hint-runtime"
    runtime.mkdir(exist_ok=True)
    rom, elf = runtime / "Asterwild.nds", runtime / "arm9.elf"
    shutil.copy2(ROOT / "build" / "Asterwild.nds", rom)
    shutil.copy2(ROOT / "build" / "arm9.elf", elf)
    save_dir = output / "hint-save"
    if save_dir.exists():
        shutil.rmtree(save_dir)
    shutil.copytree(output / "visual-save", save_dir)
    with Emulator(rom, save_dir) as emu:
        run = Playthrough(emu, find_symbol(elf, "game"), output / "screens")
        emu.run(180)
        assert run.game().has_save
        run.press("a"); run.settle()
        assert run.game().mode == 1
        run.walk(21); run.wait_ticks(24)
        before = run.game().frame
        emu.run(120)
        field_fps = (run.game().frame - before) / 2
        assert field_fps >= 25
        run.shot("single-button-hint")
        assert b"A  %s\0" not in rom.read_bytes()
        # OCR is an independent observation of the actual displayed caption.
        advances = emu.read_ram(find_symbol(elf, "font_advance"), 96)
        label = "A: Gather a lantern blossom"
        width = sum(advances[ord(ch) - 32] for ch in label)
        left = (256 - width - 16) // 2 + 8
        caption = emu.image().crop((left - 1, 171, left + width + 1, 187))
        caption = caption.resize(((width + 2) * 5, 80)).convert("L")
        caption = ImageOps.invert(caption.point(lambda value: 255 if value > 160 else 0))
        caption_path = output / "hint-caption-ocr.png"
        caption.save(caption_path)
        text = subprocess.run(["tesseract", str(caption_path), "stdout", "--psm", "7", "-c",
                               "tessedit_char_whitelist=ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz: "],
                              check=True, capture_output=True, text=True).stdout.strip()
        assert text.lower().startswith("a:"), text
        assert text.count("A:") == 1, text
        assert emu.audio_nonzero > 0
    subprocess.run([sys.executable, str(ROOT / "tests" / "playthrough.py"), "--resume",
                    "--rom", str(rom), "--elf", str(elf), "--output", str(output)], check=True)
    report_path = output / "final-visual-results.json"
    report = json.loads(report_path.read_text())
    report.setdefault("visual_suite_rom", report["rom"])
    report["rom"] = inspect(rom)
    report["final_hint_fix"] = {"ocr": text, "field_fps": field_fps,
                                "screenshot": "single-button-hint.png",
                                "completed_save_coldboot": "passed",
                                "scope": "Removed the repeated A prefix; other rules and visuals unchanged"}
    report_path.write_text(json.dumps(report, indent=2))
    print(json.dumps({"rom": report["rom"], "hint": report["final_hint_fix"]}, indent=2))


if __name__ == "__main__":
    main()
