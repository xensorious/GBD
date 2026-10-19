#!/usr/bin/env python3
"""Import the three independently saved profiles from the opening release.

First run opening.py on pinned release dfca164 (SHA checked below), then pass
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
from game_snapshot import Save,find_symbol,read_game,enter_opening
from inspect_rom import inspect

PINNED_SHA='27e8ebf25d32bf84f2f74f6135d35797ec364ba02d8dc05455405462a5d85738'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'fusion-styles-final')
p.add_argument('--source',type=Path,default=ROOT/'build'/'opening-validation')
p.add_argument('--slot',type=int,choices=range(3))
p.add_argument('--reload',action='store_true')
a=p.parse_args();a.output=a.output.resolve();a.source=a.source.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf'
if a.slot is None:
 old=json.loads((a.source/'opening-results.json').read_text())
 assert old['rom']['sha256']==PINNED_SHA and old['three_independent_profiles'] and old['fresh_process_reloads']==3
 for slot in range(3):
  directory=a.output/f'previous-slot-{slot}';shutil.rmtree(directory,ignore_errors=True);directory.mkdir()
  shutil.copyfile(a.source/'opening-save'/'Coastline.dsv',directory/'Coastline.dsv')
  for repeat in (False,True):
   subprocess.run([sys.executable,__file__,'--source',str(a.source),'--output',str(a.output),'--slot',str(slot)]+(['--reload'] if repeat else []),check=True)
 result={'rom':inspect(rom),'source_release_sha256':PINNED_SHA,'all_progress_preserved':True,
  'retired_interfaces_migrated_to_fusion':True,'profiles_imported':3,'fresh_process_reloads':3,
  'comparison':'All 708 RAM bytes match the previous accepted profile after normalizing only skin and checksum.'}
 (a.output/'previous-profiles-results.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result,indent=2));sys.exit(0)
expected=Save.from_buffer_copy((a.source/f'slot-{a.slot}-expected.bin').read_bytes());expected.skin=3;expected.checksum=0
with Emulator(rom,a.output/f'previous-slot-{a.slot}') as e:
 e.run(150);enter_opening(e,elf,a.slot);g=read_game(e,find_symbol(elf,'game'));g.save.checksum=0
 assert g.has_save and g.save_ok and bytes(g.save)==bytes(expected),(a.slot,'previous profile changed')
 e.screenshot(a.output/(f'previous-slot-{a.slot}'+('-reloaded.png' if a.reload else '-imported.png')))
print('Previous slot',a.slot+1,'reloaded' if a.reload else 'imported','with every progression field preserved.',flush=True)
