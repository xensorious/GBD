#!/usr/bin/env python3
"""Short genuine-ARM update-rate check before the longer cartridge tour."""
import argparse
import ctypes as C
import json
from pathlib import Path
from emulator import Emulator,ROOT
from game_snapshot import find_symbol,read_game
from playthrough import DriveThrough
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--output',type=Path,default=ROOT/'build'/'perf')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
with Emulator(a.rom,a.output/'save') as e:
    address=find_symbol(a.elf,'game');e.run(180)
    d=DriveThrough(e,address,a.output)
    def measure(mode):
        old=d.game().frame;start=e.frames;e.run(240,('r',) if mode=='race' else ())
        hz=(d.game().frame-old)/(240/e.fps)
        report={'mode':mode,'game_updates':d.game().frame-old,'hardware_frames':e.frames-start,'update_hz':hz}
        try:
            prof=find_symbol(a.elf,'render_profile')
            report['render_profile']=list((C.c_uint32*5).from_buffer_copy(e.read_ram(prof,20)))
        except RuntimeError:
            pass
        print(json.dumps(report),flush=True)
        e.screenshot(a.output/(mode+'.png'))
        return hz
    menu_hz=measure('menu');d.menu(0);d.press('a');d.ticks(40,('r',))
    race_hz=measure('race')
    assert menu_hz>=28 and race_hz>=28,(menu_hz,race_hz)
