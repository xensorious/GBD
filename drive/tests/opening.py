#!/usr/bin/env python3
"""Actual dual-LCD movie, transition and isolated three-profile battery checks."""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from game_snapshot import Opening,find_symbol,read_game,enter_opening
from fixtures import battery_size
from inspect_rom import inspect
from playthrough import DriveThrough
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'opening-validation')
p.add_argument('--reload-slot',type=int,choices=range(3))
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf';directory=a.output/'opening-save'
address=find_symbol(elf,'game');opening_address=find_symbol(elf,'opening')
def state(e):return Opening.from_buffer_copy(e.read_ram(opening_address,C.sizeof(Opening)))
if a.reload_slot is not None:
 with Emulator(rom,directory) as e:
  e.run(150);enter_opening(e,elf,a.reload_slot);g=read_game(e,address);g.save.checksum=0
  assert bytes(g.save)==(a.output/f'slot-{a.reload_slot}-expected.bin').read_bytes()
  assert state(e).active_slot==a.reload_slot
  e.screenshot(a.output/f'slot-{a.reload_slot}-reloaded.png')
 print('Fresh cartridge boot preserved slot',a.reload_slot+1,flush=True);sys.exit(0)
shutil.rmtree(directory,ignore_errors=True)
with Emulator(rom,directory) as e:
 e.run(150);d=DriveThrough(e,address,a.output,enter_menu=False)
 assert state(e).phase==0 and not any(s.valid for s in state(e).slots)
 d.shot('opening-title');d.ticks(12,touch=(128,80));assert state(e).phase==0
 d.press('x');assert state(e).phase==1;d.shot('opening-slots-empty')
 d.press('b');assert state(e).phase==0;d.press('y');assert state(e).phase==1
 d.press('a');assert state(e).phase==2 and state(e).active_slot==0
 frames=[];phases=[]
 while state(e).phase==2:
  o=state(e);frames.append(e.image().resize((512,768)));phases.append(o.slide)
  if 12<=o.slide<15:d.shot('opening-slide-middle')
  d.ticks(2)
 assert state(e).phase==3 and d.game().mode==0
 frames.append(e.image().resize((512,768)))
 frames[0].save(a.output/'opening-transition.gif',save_all=True,append_images=frames[1:],duration=90,loop=0)
 d.shot('opening-home');assert d.game().save_ok and d.game().has_save
 # Profile one earns money normally; the other two stay separate, fresh profiles.
 d.menu(0);d.press('a');d.autopilot(180,('r',));assert d.game().mode==1
 d.press('start');d.menu(1);d.press('a');assert d.game().mode==2
 assert d.game().save.total_runs==1 and d.game().save.cash==d.game().run.total_cash
 d.press('a')
 expected=[]
 for slot,target_skin,volume in ((0,3,45),(1,4,60),(2,5,80)):
  if slot:
   d.press('b');assert state(e).phase==1;d.press('down');d.press('a')
   while state(e).phase==2:d.ticks(2)
   assert state(e).active_slot==slot and d.game().save.cash==0 and d.game().save.total_runs==0
   assert d.game().save.skin==3 and d.game().save.music_volume==70
  d.menu(4);d.press('a');d.menu(4)
  while d.game().save.skin!=target_skin:d.press('right')
  d.menu(1)
  while d.game().save.music_volume!=volume:d.press('left' if d.game().save.music_volume>volume else 'right')
  d.press('b');d.ticks(8);g=d.game();g.save.checksum=0;expected.append(bytes(g.save))
  (a.output/f'slot-{slot}-expected.bin').write_bytes(expected[-1])
  d.shot(f'opening-profile-{slot+1}')
 d.press('b');assert state(e).phase==1 and all(s.valid for s in state(e).slots)
 d.shot('opening-slots-filled');d.press('down');assert state(e).cursor==0;d.press('a')
 while state(e).phase==2:d.ticks(2)
 g=d.game();g.save.checksum=0;assert bytes(g.save)==expected[0]
 assert g.save.skin==3 and g.save.music_volume==45
 d.shot('opening-profile-one-restored')
assert battery_size(directory/'Coastline.dsv')==8192
for slot in range(3):subprocess.run([sys.executable,__file__,'--output',str(a.output),'--reload-slot',str(slot)],check=True)
result={'rom':inspect(rom),'dual_screen_cinematic':True,'horizon_at_pixel':128,
 'touch_ignored':True,'any_button_x_y_passed':True,'three_independent_profiles':True,
 'slot_one_earned_run':True,'fresh_process_reloads':3,'physical_save_bytes':8192,
 'transition_updates':phases,'transition_end_exact_native_check':True,
 'inputs':'joypad only; RAM read only; no emulator state restore',
 'screenshots':['opening-title.png','opening-slots-empty.png','opening-slide-middle.png','opening-home.png','opening-slots-filled.png'],
 'animation':'opening-transition.gif'}
(a.output/'opening-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2),flush=True)
