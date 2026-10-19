#!/usr/bin/env python3
"""Import four published single-profile batteries into Save 1 of the 8 KiB bank.

Each old fixture must first boot in its released cartridge. The genuine 512B
file then boots in the revised cartridge, which imports its expanded 1000B v5
profile into the expanded bank and reloads all 1032 RAM bytes in a fresh process. No RAM writes or
emulator checkpoints are used.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from game_snapshot import enter_opening
from emulator import Emulator,ROOT
from fixtures import (legacy_profile,coastal_profile,write_battery,
                      assert_imported_fields,assert_compact_battery,battery_size)
from game_snapshot import Game,find_symbol,read_game,symbol_size
from inspect_rom import inspect
from setup_legacy import verify_pair,RELEASES

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--baseline',type=Path,default=ROOT/'build'/'legacy-baseline')
p.add_argument('--v2-baseline',type=Path,default=ROOT/'build'/'coastal-baseline')
p.add_argument('--turbo-baseline',type=Path,default=ROOT/'build'/'turbo-baseline')
p.add_argument('--handling-baseline',type=Path,default=ROOT/'build'/'handling-baseline')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)

def prepare(release,directory):
    if not (directory/'Coastline.nds').exists() or not (directory/'arm9.elf').exists():
        subprocess.run([sys.executable,str(ROOT/'tests'/'setup_legacy.py'),
                        '--release',release,'--output',str(directory)],check=True)
    verify_pair(directory/'Coastline.nds',directory/'arm9.elf',RELEASES[release][1])
    return directory

def fixture(name,baseline,payload):
    directory=a.output/(name+'-fixture-save');shutil.rmtree(directory,ignore_errors=True)
    write_battery(baseline/'validation'/'save'/'Coastline.dsv',directory/'Coastline.dsv',payload)
    assert battery_size(directory/'Coastline.dsv')==512
    return directory

def reload(directory,expected,image):
    subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
        '--elf',str(a.elf),'--save-dir',str(directory),'--expected',str(expected),
        '--output',str(a.output/image)],check=True)
    assert_compact_battery(directory/'Coastline.dsv')

baseline=prepare('original',a.baseline);legacy=legacy_profile()
directory=fixture('legacy',baseline,bytes(legacy))
subprocess.run([sys.executable,str(ROOT/'tests'/'legacy_boot.py'),
    '--rom',str(baseline/'Coastline.nds'),'--elf',str(baseline/'arm9.elf'),
    '--save-dir',str(directory)],check=True)
address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
with Emulator(a.rom,directory) as e:
    e.run(180);enter_opening(e,a.elf);g=read_game(e,address)
    assert g.mode==0 and g.has_save and g.save_ok
    assert_imported_fields(g.save,legacy)
    e.screenshot(a.output/'legacy-migrated.png')
    g.save.checksum=0;expected=bytes(g.save)
expected_path=a.output/'expected-migrated-profile.bin';expected_path.write_bytes(expected)
geometry=assert_compact_battery(directory/'Coastline.dsv')
reload(directory,expected_path,'legacy-reloaded.png')
result={'rom':inspect(a.rom),'baseline_rom':inspect(baseline/'Coastline.nds'),
        'profile_source':'100-byte legacy fixture accepted by original released ROM on unchanged 512B chip',
        'legacy_bytes':100,'migrated_bytes':len(expected),'migrated_ram_bytes':len(expected),
        'legacy_fields_preserved':True,'new_expansion_defaults':True,'fresh_process_reload':True,
        **geometry,'inputs':'ordinary cartridge boot; no RAM writes, checkpoint restore or chip enlargement',
        'screenshots':['legacy-migrated.png','legacy-reloaded.png']}

for release,baseline,key in (
    ('coastal',a.v2_baseline,'previous_v2_compatibility'),
    ('turbo',a.turbo_baseline,'previous_turbo_v2_compatibility'),
    ('handling',a.handling_baseline,'previous_handling_v2_compatibility')):
    baseline=prepare(release,baseline);old=coastal_profile()
    directory=fixture(release,baseline,bytes(old))
    subprocess.run([sys.executable,str(ROOT/'tests'/'coastal_boot.py'),
        '--rom',str(baseline/'Coastline.nds'),'--elf',str(baseline/'arm9.elf'),
        '--save-dir',str(directory)],check=True)
    # The old core cannot safely re-initialize inside this parent process.
    # Every additional cartridge boot uses an independent child process.
    expected_path=a.output/('expected-'+release+'-profile.bin')
    kept_image=release+'-save-preserved.png';reload_image=release+'-save-reloaded.png'
    subprocess.run([sys.executable,str(ROOT/'tests'/'coastal_boot.py'),
        '--rom',str(a.rom),'--elf',str(a.elf),'--save-dir',str(directory),
        '--current','--output',str(a.output/kept_image),
        '--expected',str(expected_path)],check=True)
    geometry=assert_compact_battery(directory/'Coastline.dsv')
    reload(directory,expected_path,reload_image)
    assert len(expected_path.read_bytes())==1032
    result[key]={'baseline_rom':inspect(baseline/'Coastline.nds'),
        'profile_source':'detailed 204-byte fixture accepted by released '+release+' ROM',
        'source_ram_bytes':204,'migrated_ram_bytes':1032,
        'legacy_fields_preserved':True,'new_expansion_defaults':True,
        'fresh_process_reload':True,**geometry,
        'preserved':'all ten old vehicles, parts, mastery, tuning, career, money, records, departures, paint and settings',
        'defaults':'38 new vehicles, recovery/style arrays, stock cosmetic masks, no selected Dispatch',
        'screenshots':[kept_image,reload_image]}
(a.output/'migration-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
