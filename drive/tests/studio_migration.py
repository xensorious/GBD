#!/usr/bin/env python3
"""Preserve a full released v3 profile through the v4 interface update."""
import argparse
import ctypes as C
import json
import io
import re
import tarfile
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator, ROOT
from fixtures import write_battery, assert_compact_battery
from game_snapshot import enter_opening,Game, V3Save, find_symbol, read_game
from inspect_rom import inspect
from setup_legacy import verify_pair,RELEASES

class V3Game(C.LittleEndianStructure):
    _fields_=[('save',V3Save)]+Game._fields_[1:-4]
assert C.sizeof(V3Game)==1692
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'studio-validation')
p.add_argument('--phase',choices=('old','new'))
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(exist_ok=True)
baseline=ROOT/'build'/'v3-interface-baseline';directory=a.output/'v3-save'
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf'

if a.phase is None:
    if not (baseline/'Coastline.nds').exists():
        subprocess.run([sys.executable,str(ROOT/'tests'/'setup_legacy.py'),'--release','expansion',
            '--output',str(baseline),'--battery-template',str(a.output/'save'/'Coastline.dsv')],check=True)
    verify_pair(baseline/'Coastline.nds',baseline/'arm9.elf',RELEASES['expansion'][1])
    # Extract only trusted portable source files; no checkout or worktree.
    archive=subprocess.check_output(['git','archive',RELEASES['expansion'][0],'drive/src'],cwd=ROOT.parent)
    with tarfile.open(fileobj=io.BytesIO(archive)) as bundle:
        for member in bundle.getmembers():
            if member.isfile() and Path(member.name).parent==Path('drive/src'):
                target=baseline/'src'/Path(member.name).name;target.parent.mkdir(exist_ok=True)
                target.write_bytes(bundle.extractfile(member).read())
    # The old codec's traffic translation unit needs the unchanged original
    # 31-model geometry prefix, rather than the expanded 48-model dimensions.
    lines=[];vehicle=False;row=0
    for line in (ROOT/'build'/'vehicle_geometry.h').read_text().splitlines():
        if line.startswith('static const'):
            vehicle='vehicle_body_' in line;row=0
            if vehicle:line=re.sub(r'(vehicle_body_\w+\[)\d+',r'\g<1>155',line)
        elif vehicle and line.startswith(' {'):
            row+=1
            if row>155:continue
        lines.append(line)
    (baseline/'src'/'vehicle_geometry.h').write_text('\n'.join(lines)+'\n')
    libpath=baseline/'rules.so'
    subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-I'+str(baseline/'src'),
        '-Ibuild',*[str(baseline/'src'/s) for s in ('game.c','road.c','traffic.c')],
        '-o',str(libpath)],cwd=ROOT,check=True)
    lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(V3Game),C.c_uint32]
    lib.game_encode_save.argtypes=[C.POINTER(V3Game),C.c_void_p,C.c_uint]
    g=V3Game();lib.game_init(C.byref(g),0x334d4947)
    g.save.cash=543210;g.save.owned=0x7fffffff;g.save.unlocked_locations=8191;g.save.paints=63
    g.save.career_xp=50000;g.save.contracts_completed=13;g.save.dispatch_choice=1
    g.save.selected_vehicle=24;g.save.selected_location=10;g.save.total_runs=87
    g.save.skin=1;g.save.music_on=0;g.save.music_volume=35;g.save.sfx_volume=65;g.save.units=1;g.save.motion=2
    g.save.perks[:]=(2,3,1)
    for v in range(31):
        for name,value in (('paint',v%6),('engine',v%7),('grip',(v+1)%7),('armor',(v+2)%7),
            ('recovery',(v+3)%7),('mastery_xp',10000+v*123),('power_tune',v%3),
            ('handling_tune',(v+1)%3),('chassis_tune',(v+2)%3),('livery',v%5),('wheels',v%4),
            ('livery_owned',31),('wheels_owned',15)):
            getattr(g.save,name)[v]=value
    data=(C.c_uint8*500)();assert lib.game_encode_save(C.byref(g),data,500)
    shutil.rmtree(directory,ignore_errors=True)
    write_battery(ROOT/'build'/'coastal-baseline'/'validation'/'save'/'Coastline.dsv',directory/'Coastline.dsv',bytes(data))
    for phase in ('old','new'):
        subprocess.run([sys.executable,__file__,'--output',str(a.output),'--phase',phase],check=True)
    subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(rom),'--elf',str(elf),
        '--save-dir',str(directory),'--expected',str(a.output/'expected-v3-migrated.bin'),
        '--output',str(a.output/'v3-reloaded.png')],check=True)
    result={'rom':inspect(rom),'baseline_rom':inspect(baseline/'Coastline.nds'),
        'previous_v3_compatibility':True,'legacy_fields_preserved':True,
        'old_color_only_skins_migrate_to_fusion':True,'fresh_process_reload':True,
        'source':'rich 31-car fixture first accepted by previous published ROM',
        'old_ram_bytes':596,'new_ram_bytes':C.sizeof(Game().save),**assert_compact_battery(directory/'Coastline.dsv')}
    (a.output/'studio-migration-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2));sys.exit(0)

if a.phase=='old':
    with Emulator(baseline/'Coastline.nds',directory) as e:
        e.run(180);g=V3Game.from_buffer_copy(e.read_ram(find_symbol(baseline/'arm9.elf','game'),C.sizeof(V3Game)))
        assert g.has_save and g.save.version==3 and g.save.cash==543210
        (a.output/'source-v3-save.bin').write_bytes(bytes(g.save))
    print('Previous published ROM accepted the 31-car v3 profile.',flush=True)
else:
    old=V3Save.from_buffer_copy((a.output/'source-v3-save.bin').read_bytes())
    with Emulator(rom,directory) as e:
        e.run(180);enter_opening(e,elf);g=read_game(e,find_symbol(elf,'game'))
        assert g.mode==0 and g.has_save and g.save_ok and g.save.version==7
        for name,kind in V3Save._fields_:
            if name in ('version','checksum','skin','reserved'):continue
            actual,expected=getattr(g.save,name),getattr(old,name)
            if name=='paints':assert actual&63==expected;continue
            if isinstance(actual,C.Array):assert list(actual)[:len(expected)]==list(expected),name
            else:assert actual==expected,name
        assert g.save.skin==3 and not g.save.preset_mask and not any(g.save.presets)
        for v in range(31):
            assert g.save.paint_seen[v]==(1|(1<<old.paint[v]))
            assert g.save.smoke[v]==0 and g.save.smoke_seen[v]==1
        g.save.checksum=0;(a.output/'expected-v3-migrated.bin').write_bytes(bytes(g.save))
        e.screenshot(a.output/'v3-migrated.png')
    assert_compact_battery(directory/'Coastline.dsv')
    print('Revised ROM preserved every v3 vehicle, part, style, tune, record and setting.',flush=True)
