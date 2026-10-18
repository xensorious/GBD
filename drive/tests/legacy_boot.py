#!/usr/bin/env python3
"""Original-cartridge acceptance check in its own emulator process."""
import argparse
import ctypes as C
import json
from pathlib import Path
from emulator import Emulator
from fixtures import legacy_profile
from game_snapshot import LegacySave,find_symbol
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--elf',type=Path,required=True)
p.add_argument('--save-dir',type=Path,required=True)
a=p.parse_args();address=find_symbol(a.elf,'game')
with Emulator(a.rom,a.save_dir) as e:
    e.run(180)
    old=LegacySave.from_buffer_copy(e.read_ram(address,C.sizeof(LegacySave)))
    assert bytes(old)==bytes(legacy_profile()),'Original cartridge rejected the v1 fixture'
    print(json.dumps({'original_v1_rom_accepted_fixture':True,'cash':old.cash,'save_bytes':C.sizeof(old)}),flush=True)
