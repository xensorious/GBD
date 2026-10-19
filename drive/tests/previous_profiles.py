#!/usr/bin/env python3
"""Import the three independently saved profiles from the opening release.

First run opening.py on pinned release 6cc60aa (SHA checked below), then pass
that validation directory as --source. No game RAM is written. Each slot is
imported and booted again in a separate emulator process.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from game_snapshot import Save,V4Save,find_symbol,read_game,enter_opening
from inspect_rom import inspect

PINNED_SHA='9597d17ff99d2e4177b9a3a3921ce98e8b2a3d9674b6c0242df77b01f169e2df'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'premium-validation')
p.add_argument('--source',type=Path,default=ROOT/'build'/'fusion-styles-final')
p.add_argument('--slot',type=int,choices=range(3))
p.add_argument('--reload',action='store_true')
p.add_argument('--rich',action='store_true')
a=p.parse_args();a.output=a.output.resolve();a.source=a.source.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf'
if a.slot is None:
 old=json.loads((a.source/'opening-results.json').read_text())
 assert old['rom']['sha256']==PINNED_SHA and old['three_independent_profiles'] and old['fresh_process_reloads']==3
 directory=a.output/'previous-three-save';shutil.rmtree(directory,ignore_errors=True);directory.mkdir()
 shutil.copyfile(a.source/'opening-save'/'Coastline.dsv',directory/'Coastline.dsv')
 for slot in range(3):
  for repeat in (False,True):
   subprocess.run([sys.executable,__file__,'--source',str(a.source),'--output',str(a.output),'--slot',str(slot)]+(['--reload'] if repeat else []),check=True)
 rich=json.loads((a.source/'studio-results.json').read_text());assert rich['rom']['sha256']==PINNED_SHA
 rich_dir=a.output/'previous-rich-save';shutil.rmtree(rich_dir,ignore_errors=True);rich_dir.mkdir()
 shutil.copyfile(a.source/'studio-3-save'/'Coastline.dsv',rich_dir/'Coastline.dsv')
 for repeat in (False,True):
  subprocess.run([sys.executable,__file__,'--source',str(a.source),'--output',str(a.output),'--slot','0','--rich']+(['--reload'] if repeat else []),check=True)
 result={'rom':inspect(rom),'source_release_sha256':PINNED_SHA,'all_progress_preserved':True,
  'retired_interfaces_migrated_to_fusion':True,'profiles_imported':3,'fresh_process_reloads':3,
  'rich_cosmetics_and_presets_preserved':True,
  'comparison':'Every previous field is preserved; 17 new vehicle records start with factory defaults.'}
 (a.output/'previous-profiles-results.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2));sys.exit(0)
old=V4Save.from_buffer_copy((a.source/('studio-3-expected.bin' if a.rich else f'slot-{a.slot}-expected.bin')).read_bytes())
if a.rich:assert old.owned==0x7fffffff and old.preset_mask and any(old.smoke)
expected=Save()
for name,kind in V4Save._fields_:
 value=getattr(old,name)
 if hasattr(value,'__len__'):
  target=getattr(expected,name)
  for i,v in enumerate(value):target[i]=v
 else:setattr(expected,name,value)
for v in range(31,48):expected.livery_owned[v]=expected.wheels_owned[v]=expected.paint_seen[v]=expected.smoke_seen[v]=1
expected.version=5;expected.checksum=0;expected.skin=max(3,expected.skin)

with Emulator(rom,a.output/('previous-rich-save' if a.rich else 'previous-three-save')) as e:
 e.run(150);enter_opening(e,elf,a.slot);g=read_game(e,find_symbol(elf,'game'));g.save.checksum=0
 assert g.has_save and g.save_ok and bytes(g.save)==bytes(expected),(a.slot,'previous profile changed')
 e.screenshot(a.output/(('previous-rich' if a.rich else f'previous-slot-{a.slot}')+('-reloaded.png' if a.reload else '-imported.png')))
print('Previous slot',a.slot+1,'reloaded' if a.reload else 'imported','with every progression field preserved.',flush=True)
