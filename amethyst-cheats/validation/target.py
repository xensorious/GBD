"""Load an explicitly supplied, checksum-checked ROM and the local native build."""
import hashlib
import os
from pathlib import Path
import sys

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT))
from tools.build_release import EXPECTED, sections, validate_payload

rom_name = os.environ.get('AMETHYST_ROM')
if not rom_name:
    raise SystemExit('Set AMETHYST_ROM to your original Amethyst v1.4.0 ROM path.')
rom_path = Path(rom_name).expanduser()
try:
    ROM = rom_path.read_bytes()
except OSError as exc:
    raise SystemExit(f'Cannot read AMETHYST_ROM: {exc}') from exc
if hashlib.sha256(ROM).hexdigest() != EXPECTED:
    raise SystemExit('AMETHYST_ROM does not match the supported v1.4.0 SHA-256.')
elf_path = PROJECT_ROOT / 'build' / 'runtime.elf'
if not elf_path.is_file():
    raise SystemExit('Missing build/runtime.elf. Compile the native runtime first.')
PAYLOAD = sections(elf_path)
validate_payload(PAYLOAD)
