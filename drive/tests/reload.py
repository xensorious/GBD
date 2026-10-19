#!/usr/bin/env python3
"""Fresh-process cartridge boot to check a genuine EEPROM save."""
import argparse
import json
from pathlib import Path
from game_snapshot import enter_opening
from emulator import Emulator
from game_snapshot import find_symbol,read_game

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',required=True,type=Path)
p.add_argument('--elf',required=True,type=Path)
p.add_argument('--save-dir',required=True,type=Path)
p.add_argument('--expected',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
a=p.parse_args()
with Emulator(a.rom,a.save_dir) as e:
    e.run(180)
    enter_opening(e,a.elf)
    g=read_game(e,find_symbol(a.elf,'game'))
    assert g.mode==0 and g.has_save
    g.save.checksum=0
    assert bytes(g.save)==a.expected.read_bytes()
    e.screenshot(a.output)
    print(json.dumps({'fresh_process_load':True,'total_runs':g.save.total_runs,
                      'cash':g.save.cash,'save_bytes':len(bytes(g.save))}))
