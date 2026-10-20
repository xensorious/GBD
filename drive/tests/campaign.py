#!/usr/bin/env python3
"""Actual cartridge Career UI tour. Physical buttons; read-only emulated RAM.
Preview progress is intentionally transient and never treated as earned progress.
"""
import argparse
import ctypes as C
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from game_snapshot import Game,find_symbol,symbol_size
from playthrough import DriveThrough
class Campaign(C.LittleEndianStructure):
 _fields_=[(k,C.c_uint8) for k in ('tab','chapter','event','modal','page','garage_return','drive_column')]+[('preview_done',C.c_uint8*5),('revision',C.c_uint32)]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build/career-release');p.add_argument('--reload-check',action='store_true');p.add_argument('--native-only',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build/Coastline.nds';elf=ROOT/'build/arm9.elf';address=find_symbol(elf,'game');uiaddr=find_symbol(elf,'campaign');assert symbol_size(elf,'campaign')==C.sizeof(Campaign)==16
save=out/'campaign-save'
if '--reload-check' in sys.argv:
 with Emulator(rom,save) as e:
  e.run(180);d=DriveThrough(e,address,out)
  assert not any(Campaign.from_buffer_copy(e.read_ram(uiaddr,16)).preview_done)
  assert d.game().save.owned==1 and d.game().save.cash==0
 print('Fresh process: transient preview resets; original fleet and cash preserved.')
 raise SystemExit(0)
# Compile the exact shared game/UI sources with runtime bounds/UB checks.
target=ROOT/'build/test-campaign'
subprocess.run(['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined','-Isrc','-Ibuild','tests/campaign.c','src/game.c','src/road.c','src/traffic.c','src/gfx.c','build/assets.c','src/ui.c','src/render.c','src/story_render.c','-o',str(target)],cwd=ROOT,check=True)
run=subprocess.run([str(target)],cwd=ROOT,check=True,capture_output=True,text=True)
print(run.stdout,end='',flush=True)
(out/'campaign-native-results.json').write_text(json.dumps({'sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),'checks':int(re.search(r'(\d+) checks',run.stdout)[1]),'sanitizers_passed':True,'result':run.stdout.strip()},indent=2)+'\n')
if a.native_only:raise SystemExit(0)
shutil.rmtree(save,ignore_errors=True)
with Emulator(rom,save) as e:
 e.run(180);d=DriveThrough(e,address,out)
 def state():return Campaign.from_buffer_copy(e.read_ram(uiaddr,16))
 initial=bytes(d.game().save)
 d.menu(0);d.press('a');d.press('down');assert d.game().cursor==2;d.shot('career-drive-selector')
 d.ticks(2,touch=(40,150));assert d.game().mode==11 and d.game().cursor==2
 d.press('a');assert d.game().mode==12;d.shot('career-route-d')
 for chapter in range(5):
  assert state().chapter==chapter
  if chapter:d.shot('career-route-'+str(chapter))
  d.press('down')
  for event in range(25):
   assert state().event==event
   if event in (0,11,24):
    d.press('a');assert state().modal==1
    for page in range(3):
     assert state().page==page
     if chapter in (0,4):d.shot(f'career-brief-{chapter}-{event}-{page}')
     d.press('a')
    d.press('b');assert not state().modal
   d.press('right')
  d.press('up');d.press('right')
 assert state().chapter==0
 for tab in range(1,5):
  d.press('r');assert state().tab==tab;d.shot('career-tab-'+str(tab))
  if tab<4:
   rows=3 if tab==2 else 5
   for row in range(rows):
    d.press('a');assert state().modal==tab+1
    for page in range(3):
     assert state().page==page
     if row==0:d.shot(f'career-reader-{tab}-{page}')
     d.press('a')
    d.press('b');d.press('down')
 d.press('down')
 for n in range(1,26):
  d.press('right');assert state().preview_done[0]==n
  if n in (11,12,24,25):d.shot('career-progress-'+str(n))
 assert bytes(d.game().save)==initial
 # Return from the real garage must preserve the Career page, not Home.
 d.press('up');d.press('a');assert d.game().mode==3 and state().garage_return
 d.shot('career-shared-garage');d.press('b');assert d.game().mode==12 and state().tab==4
 d.press('down');d.press('down');d.press('a');assert not any(state().preview_done)
 d.shot('career-preview-reset');d.press('l');assert state().tab==3;d.press('r');assert state().tab==4
 d.ticks(3,touch=(30,60));assert d.game().mode==12 and state().tab==4
 assert bytes(d.game().save)==initial
 d.press('b');assert d.game().mode==11 and d.game().cursor==2
 d.press('up');d.press('a');assert d.game().mode==1;d.shot('career-existing-endless')
 # A fresh boot cannot mistake the preview illustration for saved campaign progress.
subprocess.run([sys.executable,__file__,'--output',str(out),'--reload-check'],check=True)
result={'sha256':hashlib.sha256(rom.read_bytes()).hexdigest(),'chapters':5,'events':125,'tabs':['Route','Story','Rivals','Plans','Office'],'button_only':True,'touch_ignored':True,'read_only_emulated_ram':True,'preview_does_not_modify_save':True,'shared_garage_return':True,'existing_endless_launch':True,'preview_resets_on_boot':True,'race_gameplay':False}
(out/'campaign-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
