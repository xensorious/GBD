#!/usr/bin/env python3
"""Prepare a trusted released cartridge/ELF pair for reproducible save checks.

Reuse an existing verified pair, or extract only drive/ from the selected pinned
release commit and rebuild it in ignored build storage. This never creates a
worktree or changes the current checkout. A rebuild must match the published
original ROM byte for byte before its ELF is accepted.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
ORIGINAL_COMMIT = "9f6293ad23181e95787f23269ae71b77a1e5a9d9"
ORIGINAL_SHA256 = "610acd38ab739711cb25233a48c2b14348f57078799d147b7ff4cf336223770f"
COASTAL_COMMIT = "27eab74da2dd4b9f09c37ecf7d6ed79eb3e7f1dd"
COASTAL_SHA256 = "7238c37ee85789e5b7d86c6170f28488ac6f80e635e57b1cc61529a56b0fd242"
TURBO_COMMIT = "32b53218e546ca0bf37967215459f1a6ed393e4e"
TURBO_SHA256 = "676dc118cb0c621be9388d3b98acc5c19a65f33713999fc611f0e5c0b16e4822"
HANDLING_COMMIT = "ffeacfb08bb39fb220293829ece0705204ceb4ee"
HANDLING_SHA256 = "a8fc2e74be2bd4ab2fa39ce6549d3bea6461f222f9ad79e47b447bee0d8d75b0"
EXPANSION_COMMIT = "b4c46f60fc706964c6ab3e3c6b40de0f1b1eddb5"
EXPANSION_SHA256 = "cb93c5ec187aabe74543bd346a89435bf6975263eaa925f5f59613ce52a10f4e"
RELEASES = {"original":(ORIGINAL_COMMIT,ORIGINAL_SHA256),
            "coastal":(COASTAL_COMMIT,COASTAL_SHA256),
            "turbo":(TURBO_COMMIT,TURBO_SHA256),
            "handling":(HANDLING_COMMIT,HANDLING_SHA256),
            "expansion":(EXPANSION_COMMIT,EXPANSION_SHA256)}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_pair(rom_path: Path, elf_path: Path, expected_sha256: str = ORIGINAL_SHA256) -> None:
    """Check the trusted ROM hash and every ARM9 ELF load byte against it."""
    rom = rom_path.read_bytes()
    if hashlib.sha256(rom).hexdigest() != expected_sha256:
        raise ValueError(f"{rom_path} is not the trusted published cartridge ({expected_sha256})")
    elf = elf_path.read_bytes()
    if elf[:7] != b"\x7fELF\x01\x01\x01" or len(elf) < 52:
        raise ValueError(f"{elf_path} is not a little-endian ELF32 image")
    rom_offset, rom_entry, load_address, rom_size = struct.unpack_from("<4I", rom, 0x20)
    entry, program_offset = struct.unpack_from("<II", elf, 24)
    program_size, program_count = struct.unpack_from("<HH", elf, 42)
    if entry != rom_entry or program_size < 32:
        raise ValueError("The baseline ELF entry point does not match the original ROM")
    intervals = []
    for index in range(program_count):
        offset = program_offset + index * program_size
        if offset + 32 > len(elf):
            raise ValueError("Truncated ELF program header")
        kind, file_offset, _, physical_address, size, _, _, _ = struct.unpack_from("<8I", elf, offset)
        if kind != 1 or not size:
            continue
        relative = physical_address - load_address
        if relative < 0 or relative + size > rom_size or file_offset + size > len(elf):
            raise ValueError("A baseline ELF load segment is outside the original ARM9 image")
        expected = rom[rom_offset + relative:rom_offset + relative + size]
        if elf[file_offset:file_offset + size] != expected:
            raise ValueError("The baseline ELF executable bytes do not match the original ROM")
        intervals.append((relative, relative + size))
    covered = 0
    for begin, end in sorted(intervals):
        if begin != covered:
            raise ValueError("Baseline ELF load segments do not cover the complete original ARM9 image")
        covered = end
    if covered != rom_size:
        raise ValueError("Baseline ELF does not cover the complete original ARM9 image")


def verify_battery(path: Path) -> None:
    data = path.read_bytes()
    marker = data.find(b"|<--Snip")
    if marker < 204 or marker not in (512, 8192, 65536, 131072, 262144, 524288, 1048576):
        raise ValueError(f"{path} is not a supported DeSmuME battery-save template")
    if b"|-DESMUME SAVE-|" not in data[marker:]:
        raise ValueError(f"{path} has no DeSmuME battery-save footer")


def git_bytes(*args: str) -> bytes:
    return subprocess.check_output(["git", "-C", str(REPO), *args])


def rebuild(source_root: Path, jobs: int, commit: str = ORIGINAL_COMMIT,
            expected_sha256: str = ORIGINAL_SHA256) -> Path:
    try:
        git_bytes("cat-file", "-e", commit + "^{commit}")
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(
            f"Release commit is missing. Run git fetch origin {commit}, then retry."
        ) from exc
    if source_root.exists():
        # Remove only our generated extraction directory, never a repository.
        allowed = [(ROOT / "build" / name).resolve() for name in
                   ("legacy-source", "coastal-source", "turbo-source", "handling-source", "expansion-source")]
        if source_root.resolve() not in allowed:
            raise ValueError("Refusing to replace an extraction directory outside the dedicated baseline source directories")
        shutil.rmtree(source_root)
    source_root.mkdir(parents=True)
    archive = git_bytes("archive", "--format=tar", commit, "drive")
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:") as bundle:
        for member in bundle.getmembers():
            target = (source_root / member.name).resolve()
            if not target.is_relative_to(source_root.resolve()) or member.issym() or member.islnk():
                raise ValueError("Unexpected path or link in the trusted source archive")
        bundle.extractall(source_root, filter="data")
    project = source_root / "drive"
    subprocess.run(["make", "-C", str(project), f"-j{jobs}"], check=True)
    built = project / "build" / "Coastline.nds"
    published = project / "dist" / "Coastline.nds"
    if sha256(published) != expected_sha256 or sha256(built) != expected_sha256:
        raise RuntimeError(
            "The original source rebuild differs from its published cartridge. "
            "The ROM/ELF pair was not installed. Check the SDK/compiler, Pillow "
            "and DejaVu font versions against the original setup; generated "
            f"files remain in {project / 'build'} for diagnosis."
        )
    verify_pair(built, project / "build" / "arm9.elf", expected_sha256)
    return project / "build"


def prepare(output: Path, source_baseline: Path | None, battery_template: Path | None, jobs: int,
            release: str = "original") -> Path:
    if release not in RELEASES:
        raise ValueError("Choose a pinned original, coastal, turbo, handling or expansion release")
    commit,expected_sha256 = RELEASES[release]
    output = output.resolve()
    if output in (REPO.resolve(), ROOT.resolve(), (ROOT / "build").resolve()):
        raise ValueError("Choose a dedicated baseline subdirectory; the current project/build cannot be replaced")
    if source_baseline is not None:
        source_baseline = source_baseline.resolve()
        verify_pair(source_baseline / "Coastline.nds", source_baseline / "arm9.elf", expected_sha256)
        source = source_baseline
        origin = "existing verified baseline"
    elif (output / "Coastline.nds").is_file() and (output / "arm9.elf").is_file():
        verify_pair(output / "Coastline.nds", output / "arm9.elf", expected_sha256)
        source = output
        origin = "existing verified baseline"
    else:
        source_name = "legacy-source" if release == "original" else release+"-source"
        print(f"Rebuilding the pinned {release} release in drive/build/{source_name}...", flush=True)
        source = rebuild(ROOT / "build" / source_name, jobs, commit, expected_sha256)
        origin = "byte-identical rebuild of trusted release"
    if battery_template is None:
        candidates = [source / "validation" / "save" / "Coastline.dsv",
                      output / "validation" / "save" / "Coastline.dsv",
                      ROOT / "build" / "validation" / "save" / "Coastline.dsv"]
        battery_template = next((candidate for candidate in candidates if candidate.is_file()), None)
    if battery_template is None:
        raise FileNotFoundError(
            "No battery-save template found. Run python3 drive/tests/playthrough.py first, "
            "or pass --battery-template /path/to/Coastline.dsv."
        )
    battery_template = battery_template.resolve()
    verify_battery(battery_template)
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="legacy-setup-", dir=output.parent) as temporary:
        stage = Path(temporary)
        shutil.copy2(source / "Coastline.nds", stage / "Coastline.nds")
        shutil.copy2(source / "arm9.elf", stage / "arm9.elf")
        shutil.copy2(battery_template, stage / "Coastline.dsv")
        (output / "validation" / "save").mkdir(parents=True, exist_ok=True)
        os.replace(stage / "Coastline.nds", output / "Coastline.nds")
        os.replace(stage / "arm9.elf", output / "arm9.elf")
        os.replace(stage / "Coastline.dsv", output / "validation" / "save" / "Coastline.dsv")
    manifest = {
        "commit": commit, "rom_sha256": expected_sha256,
        "elf_sha256": sha256(output / "arm9.elf"),
        "elf_load_bytes_match_rom": True, "source": origin,
        "battery_template": str(battery_template),
        "battery_template_note": "Only the emulator footer is reused; migration.py writes its documented legacy profile fixture.",
    }
    (output / "SETUP.json").write_text(json.dumps(manifest, indent=2) + "\n")
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, help="Dedicated generated baseline directory")
    parser.add_argument("--release", choices=tuple(RELEASES), default="original")
    parser.add_argument("--source-baseline", type=Path, help="Reuse an existing original ROM/ELF pair without rebuilding")
    parser.add_argument("--battery-template", type=Path, help="Existing emulator battery save; only its footer is reused")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not 1 <= args.jobs <= 16:
        parser.error("--jobs must be between 1 and 16")
    try:
        destination = args.output or ROOT / "build" / ("legacy-baseline" if args.release == "original" else args.release+"-baseline")
        output = prepare(destination, args.source_baseline, args.battery_template, args.jobs, args.release)
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"Legacy setup failed: {exc}", file=sys.stderr)
        return 1
    print(f"Verified {args.release} cartridge, matching ELF and battery template: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
