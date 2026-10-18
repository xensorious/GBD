#!/usr/bin/env python3
"""Boot released v1/v2 profiles, then verify migration and preservation.

The 100-byte fixture is accepted by the original ROM and migrated by the
revised ROM. A detailed 204-byte fixture is accepted by the previous coastal
ROM and loaded byte-exactly by this revision. Every extra boot uses a fresh
process. No emulated RAM writes, checkpoints or cheats are used.
"""
import argparse
import ctypes as C
import json
import shutil
import subprocess
import sys
from pathlib import Path
from emulator import Emulator,ROOT
from fixtures import legacy_profile,coastal_profile,write_battery
from game_snapshot import LegacySave,Game,find_symbol,read_game,symbol_size
from inspect_rom import inspect
from setup_legacy import verify_pair,COASTAL_SHA256

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--baseline',type=Path,default=ROOT/'build'/'legacy-baseline')
p.add_argument('--v2-baseline',type=Path,default=ROOT/'build'/'coastal-baseline')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
if not (a.baseline/'Coastline.nds').exists() or not (a.baseline/'arm9.elf').exists():
    subprocess.run([sys.executable,str(ROOT/'tests'/'setup_legacy.py'),
                    '--output',str(a.baseline)],check=True)
verify_pair(a.baseline/'Coastline.nds',a.baseline/'arm9.elf')
fixture=a.output/'legacy-fixture-save';shutil.rmtree(fixture,ignore_errors=True)
legacy=legacy_profile()
write_battery(a.baseline/'validation'/'save'/'Coastline.dsv',fixture/'Coastline.dsv',bytes(legacy))
subprocess.run([sys.executable,str(ROOT/'tests'/'legacy_boot.py'),
    '--rom',str(a.baseline/'Coastline.nds'),'--elf',str(a.baseline/'arm9.elf'),
    '--save-dir',str(fixture)],check=True)
address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
with Emulator(a.rom,fixture) as e:
    e.run(180);g=read_game(e,address)
    assert g.mode==0 and g.has_save and g.save_ok and g.save.version==2
    for name,_ in LegacySave._fields_:
        if name not in ('magic','version','checksum','paint','engine','grip','armor','reserved'):
            assert getattr(g.save,name)==getattr(legacy,name),name
    for name in ('paint','engine','grip','armor'):
        assert list(getattr(g.save,name))[:6]==list(getattr(legacy,name)),name
        assert list(getattr(g.save,name))[6:]==[0]*4,name
    assert g.save.music_on==1 and g.save.sfx_on==1
    assert 0<=g.save.skin<3 and 0<=g.save.units<2
    e.screenshot(a.output/'legacy-migrated.png')
    g.save.checksum=0;expected=bytes(g.save)
(a.output/'expected-migrated-profile.bin').write_bytes(expected)
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
    '--elf',str(a.elf),'--save-dir',str(fixture),
    '--expected',str(a.output/'expected-migrated-profile.bin'),
    '--output',str(a.output/'legacy-reloaded.png')],check=True)
result={'rom':inspect(a.rom),'baseline_rom':inspect(a.baseline/'Coastline.nds'),
        'profile_source':'documented native-generated 100-byte legacy fixture, accepted by original v1 ROM',
        'preserved':'cash, six-car parts and paint, selected motorcycle/departure, all run records and unlocks',
        'legacy_bytes':100,'migrated_bytes':len(expected),'fresh_process_reload':True,
        'inputs':'ordinary cartridge boot; no emulator RAM writes or state restores',
        'screenshots':['legacy-migrated.png','legacy-reloaded.png']}
if not (a.v2_baseline/'Coastline.nds').exists() or not (a.v2_baseline/'arm9.elf').exists():
    subprocess.run([sys.executable,str(ROOT/'tests'/'setup_legacy.py'),
                    '--release','coastal','--output',str(a.v2_baseline)],check=True)
verify_pair(a.v2_baseline/'Coastline.nds',a.v2_baseline/'arm9.elf',COASTAL_SHA256)
v2fixture=a.output/'coastal-fixture-save';shutil.rmtree(v2fixture,ignore_errors=True)
coastal=coastal_profile()
write_battery(a.output/'save'/'Coastline.dsv',v2fixture/'Coastline.dsv',bytes(coastal))
subprocess.run([sys.executable,str(ROOT/'tests'/'coastal_boot.py'),
    '--rom',str(a.v2_baseline/'Coastline.nds'),'--elf',str(a.v2_baseline/'arm9.elf'),
    '--save-dir',str(v2fixture)],check=True)
# The old DeSmuME core cannot safely re-initialize in this parent process.
# Each additional cartridge boot uses a fresh child, as the reload checks do.
subprocess.run([sys.executable,str(ROOT/'tests'/'coastal_boot.py'),
    '--rom',str(a.rom),'--elf',str(a.elf),'--save-dir',str(v2fixture),
    '--current','--output',str(a.output/'coastal-save-preserved.png'),
    '--expected',str(a.output/'expected-preserved-profile.bin')],check=True)
expected_v2=(a.output/'expected-preserved-profile.bin').read_bytes()
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
    '--elf',str(a.elf),'--save-dir',str(v2fixture),
    '--expected',str(a.output/'expected-preserved-profile.bin'),
    '--output',str(a.output/'coastal-save-reloaded.png')],check=True)
result['previous_v2_compatibility']={
    'baseline_rom':inspect(a.v2_baseline/'Coastline.nds'),
    'profile_source':'documented 204-byte v2 fixture accepted by previous coastal release',
    'bytes':len(expected_v2),'byte_exact_on_load':True,'fresh_process_reload':True,
    'preserved':'all ten cars, parts, mastery, tuning, career perks, money, records, departures, paint and settings',
    'screenshots':['coastal-save-preserved.png','coastal-save-reloaded.png']}
(a.output/'migration-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
