"""Explicit local ROM and ELF inputs for the code-only native checks."""
import hashlib
import os
from pathlib import Path
import struct
import sys

PROJECT = PROJECT_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(PROJECT))
from tools.build_release import EXPECTED, sections

name = os.environ.get('AMETHYST_ROM')
if not name:
    raise SystemExit('Set AMETHYST_ROM to the original Amethyst v1.4.0 ROM path.')
ROM_PATH = Path(name).expanduser()
try:
    ROM = ROM_PATH.read_bytes()
except OSError as exc:
    raise SystemExit(f'Cannot read AMETHYST_ROM: {exc}') from exc
if hashlib.sha256(ROM).hexdigest() != EXPECTED:
    raise SystemExit('AMETHYST_ROM does not match the supported v1.4.0 SHA-256.')
ELF = PROJECT / 'build' / 'code_only.elf'
if not ELF.is_file():
    raise SystemExit('Missing build/code_only.elf. Compile the code-only runtime first.')
PARTS = PAYLOAD = sections(ELF)
layout = {'.readkeys': (0x0200cecc, 8), '.text': (0x0200d100, 0x0f00),
          '.entrymode': (0x0200e940, 0x1b0),
          '.heapentry': (0x02010a38, 16), '.guards': (0x02010b00, 20),
          '.callbacks': (0x02010b20, 8)}
if len(PARTS) != len(layout) or set(n for n, _, _ in PARTS) != set(layout):
    raise SystemExit('Unexpected code-only ELF sections; rebuild the current runtime.')
for section, address, data in PARTS:
    expected, capacity = layout[section]
    if address != expected or not 0 < len(data) <= capacity:
        raise SystemExit(f'Invalid code-only ELF section layout: {section}')
    if section not in ('.text','.entrymode') and len(data) != capacity:
        raise SystemExit(f'Invalid code-only ELF section size: {section}')
text = next(data for name, _, data in PARTS if name == '.text')
for name, _, data in PARTS:
    if name in ('.guards', '.callbacks'):
        for pointer in struct.unpack('<' + 'I' * (len(data) // 4), data):
            if not pointer & 1 or not 0x0200d100 <= (pointer & ~1) < 0x0200d100 + len(text):
                raise SystemExit(f'Invalid Thumb pointer in {name}; rebuild the runtime.')
