#!/usr/bin/env python3
"""Buy, equip, style, drive and save all 17 additions using cartridge buttons.

The fixture is explicitly funded and owns only the old roster. It does not
represent earned cash. No game RAM is written by this test.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from fixtures import encode_profile,write_battery,assert_compact_battery
from game_snapshot import Game,find_symbol,catalogue,verify_layout
from inspect_rom import inspect
from playthrough import DriveThrough
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'premium-final')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf';verify_layout(ROOT)
library=ROOT/'build'/'libpremium-qa.so'
subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild','src/game.c','src/story.c','src/story_io.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_dealer_count.argtypes=[C.c_int];lib.game_dealer_vehicle.argtypes=[C.c_int,C.c_int]
vehicles,_=catalogue(lib);profile=Game();lib.game_init(C.byref(profile),0x5045524d)
profile.save.cash=1000000;profile.save.owned=0x7fffffff;profile.save.paints=63;profile.save.unlocked_locations=8191
profile.save.selected_location=1;profile.save.best_near=20;profile.save.best_drift=3000;profile.save.career_xp=50000
for v in range(31):profile.save.mastery_xp[v]=10000
directory=a.output/'premium-save';shutil.rmtree(directory,ignore_errors=True)
write_battery(a.output/'save'/'Coastline.dsv',directory/'Coastline.dsv',encode_profile(lib,profile))
reviewed=[]
with Emulator(rom,directory) as e:
 e.run(180);d=DriveThrough(e,find_symbol(elf,'game'),a.output)
 for v in range(31,48):
  car=vehicles[v];d.menu(1);d.press('a');assert d.game().mode==3
  while d.game().garage_tab!=3:d.press('r')
  while d.game().garage_filter!=car['class_id']:d.press('right')
  row=next(i+1 for i in range(lib.game_dealer_count(car['class_id'])) if lib.game_dealer_vehicle(car['class_id'],i)==v)
  d.menu(row);assert d.game().selection==v
  cash=d.game().save.cash;d.press('a');assert d.game().save.cash==cash-car['price'] and d.game().save.selected_vehicle==v
  d.shot(f'premium-{v}-dealer')
  d.press('a');assert d.game().garage_tab==0
  d.shot(f'premium-{v}-garage')
  # Style and restore the newest high ID through ordinary menu services.
  if v==47:
   d.press('r');d.press('r');assert d.game().garage_tab==2
   for row in (0,1,3,4):d.menu(row);d.press('right')
   d.menu(5);d.press('a');assert d.game().save.paint[v]==1 and d.game().save.smoke[v]==1
   while d.game().garage_tab!=0:d.press('l')
   d.menu(4);d.press('a');assert ((d.game().save.presets[0]&31)|((d.game().save.presets[0]>>21)&32))==47
   d.shot('premium-preset')
  d.press('b');d.menu(0);d.press('a');assert d.game().mode==11;d.press('a');assert d.game().vehicle==v
  d.autopilot(45,('r',));assert d.game().mode==1 and d.game().speed_q8>0
  if car['bike']:
   # Observe the actual wheelie while holding A. A collision later in a long
   # fixed interval correctly cancels a wheelie, and must not erase evidence
   # that the input worked before that collision.
   for _ in range(60):
    d.autopilot(2,('r','a'));g=d.game()
    if g.mode!=1 or g.wheelie:break
   assert g.mode==1 and g.wheelie,('New motorcycle must support A-wheelies',v,g.speed_q8,g.crash_timer,g.health)
  d.shot(f'premium-{v}-driving',('r',))
  d.press('start');d.menu(1);d.press('a');assert d.game().mode==2
  d.press('a');d.ticks(4);reviewed.append({'id':v,'name':car['name'],'class':('D','C','B','A','S')[car['class_id']],'price':car['price']})
 assert d.game().save.owned==0xffffffff and d.game().save.owned_hi==0xffff
 g=d.game();g.save.checksum=0;expected=a.output/'premium-expected.bin';expected.write_bytes(bytes(g.save))
 geometry=assert_compact_battery(directory/'Coastline.dsv')
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(rom),'--elf',str(elf),'--save-dir',str(directory),'--expected',str(expected),'--output',str(a.output/'premium-reloaded.png')],check=True)
result={'rom':inspect(rom),'vehicles':reviewed,'purchases':17,'all_new_vehicles_driven':True,'motorcycle_wheelies':True,'high_id_preset':True,'fresh_process_reload':True,'profile_source':'explicitly funded old-roster fixture; cartridge joypad inputs only',**geometry}
(a.output/'premium-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
