#!/usr/bin/env python3
"""Verify released/revised cartridges preserve the 204-byte v2 fixture."""
import argparse
import ctypes as C
import json
from pathlib import Path
from emulator import Emulator
from fixtures import coastal_profile
from game_snapshot import Save,find_symbol,read_game

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,required=True)
p.add_argument('--elf',type=Path,required=True)
p.add_argument('--save-dir',type=Path,required=True)
p.add_argument('--current',action='store_true',help='Also validate the revised cartridge Game load flags')
p.add_argument('--output',type=Path)
p.add_argument('--expected',type=Path,help='Write the observed profile with only its checksum cleared')
a=p.parse_args();address=find_symbol(a.elf,'game')
with Emulator(a.rom,a.save_dir) as e:
    e.run(180)
    if a.current:
        g=read_game(e,address)
        assert g.mode==0 and g.has_save and g.save_ok and g.save.version==2
        old=g.save
    else:old=Save.from_buffer_copy(e.read_ram(address,C.sizeof(Save)))
    assert bytes(old)==bytes(coastal_profile()),'The cartridge rejected or changed the v2 fixture'
    if a.output:e.screenshot(a.output)
    if a.expected:
        old.checksum=0;a.expected.write_bytes(bytes(old))
    print(json.dumps({'current_v2_profile_preserved' if a.current else 'previous_v2_rom_accepted_fixture':True,'cash':old.cash,
                      'save_bytes':C.sizeof(old),'selected_vehicle':old.selected_vehicle}),flush=True)
