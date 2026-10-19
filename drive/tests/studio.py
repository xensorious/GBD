#!/usr/bin/env python3
"""Review the interface revision on the actual cartridge using buttons only.

The labelled review profile supplies money, mastery and owned departures. It is
not earned gameplay evidence. Every subsequent selection, purchase, preset and
save uses ordinary cartridge input; RAM is observed read-only.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator, ROOT
from fixtures import encode_profile, write_battery, assert_compact_battery
from game_snapshot import Game, catalogue, find_symbol, verify_layout
from inspect_rom import inspect
from playthrough import DriveThrough

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'studio-validation')
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--skin',type=int,choices=range(3,9))
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
library=ROOT/'build'/'libstudio-qa.so'

if a.skin is None:
    verify_layout(ROOT)
    subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild',
        'src/game.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
    for skin in range(3,9):
        subprocess.run([sys.executable,__file__,'--output',str(a.output),'--rom',str(a.rom),
            '--elf',str(a.elf),'--skin',str(skin)],check=True)
    cases=[json.loads((a.output/f'studio-{skin}.json').read_text()) for skin in range(3,9)]
    assert all(c['fusion_navigation_passed'] for c in cases)
    report={'rom':inspect(a.rom),'interfaces':['Fusion','Pearl','Voltage','Alloy','Glacier','Bloom'],
        'garage_tabs':['Showroom','Tuning','Style','Dealer'],'cases':cases,
        'fresh_process_reload':all(c['fresh_process_reload'] for c in cases),
        'profile_source':'labelled funded review profile; all actions use buttons; RAM read only',
        'departure_interiors':13,'serialized_save_bytes':1000,'physical_save_bytes':8192}
    (a.output/'studio-results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('All six actual-cartridge interface reviews passed.',flush=True)
    sys.exit(0)

skin=a.skin;lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
vehicles,locations=catalogue(lib)
profile=Game();lib.game_init(C.byref(profile),0x53545544)
profile.save.skin=skin;profile.save.cash=1000000;profile.save.owned=0xffffffff&~(1<<10);profile.save.owned_hi=0xffff
profile.save.unlocked_locations=(1<<13)-1;profile.save.paints=63
profile.save.career_xp=50000;profile.save.best_near=20;profile.save.best_drift=3000
profile.save.best_distance=6000;profile.save.best_speed_hold=240;profile.save.best_wheelie=1000
for v in range(48):profile.save.mastery_xp[v]=10000
directory=a.output/f'studio-{skin}-save';shutil.rmtree(directory,ignore_errors=True)
template=a.output/'save'/'Coastline.dsv'
assert template.exists(),'Run the stock playthrough first to create a genuine battery footer.'
write_battery(template,directory/'Coastline.dsv',encode_profile(lib,profile))
events=[]
with Emulator(a.rom,directory) as e:
    e.run(180);d=DriveThrough(e,find_symbol(a.elf,'game'),a.output)
    assert d.game().has_save and d.game().save.version==6 and d.game().save.cash==1000000
    def shot(name):d.shot(f'ui-{skin}-{name}')
    def tab(n):
        while d.game().garage_tab!=n:d.press('r')
    def owned(v):
        d.menu(0)
        for _ in range(48):
            if d.game().selection==v:return
            d.press('right')
        raise AssertionError('Owned vehicle missing')
    def style(values):
        tab(2)
        for row,field,wanted in zip((0,1,3,4),('paint_choice','livery_choice','wheel_choice','smoke_choice'),values):
            d.menu(row)
            for _ in range(16):
                if getattr(d.game(),field)==wanted:break
                d.press('right')
            assert getattr(d.game(),field)==wanted
        d.menu(5);before=d.game().save.cash;d.press('a')
        return before-d.game().save.cash
    shot('home')
    if skin>=3:
        for key,target in (('left',0),('up',0),('right',1),('right',1),('down',3),('left',2),('down',4),('down',4),('right',5),('up',3),('up',1),('left',0)):
            d.press(key);assert d.game().cursor==target,(key,target,d.game().cursor)
        events.append('four-direction tile navigation and bounded edges')
    for home,mode,name in ((4,9,'settings'),(2,5,'challenges'),(3,8,'career'),(5,10,'dispatch')):
        d.menu(home);d.press('a');assert d.game().mode==mode
        shot(name)
        if mode==9:
            if skin>=3:
                before=bytes(d.game().save)
                d.press('r');assert d.game().cursor==4;shot('settings-display')
                d.press('r');assert d.game().cursor==8;shot('settings-guide')
                d.press('r');assert d.game().cursor==0
                d.press('l');assert d.game().cursor==8
                d.press('up');assert d.game().cursor==7
                d.press('down');assert d.game().cursor==8
                d.press('down');assert d.game().cursor==0
                assert bytes(d.game().save)==before,'Paging must not edit preferences'
                for cursor in range(9):assert d.game().cursor==cursor;d.press('down')
                assert d.game().cursor==0
                music=d.game().save.music_on;d.press('right');assert d.game().save.music_on!=music
                d.press('left');assert d.game().save.music_on==music
                d.menu(4);d.press('left');assert d.game().save.skin==3+(skin-3+5)%6
                d.press('right');assert d.game().save.skin==skin
                d.menu(0);shot('settings')
                events.extend(['L/R settings pages and wraparound','continuous Up/Down settings scrolling',
                    'page switching preserves preferences','Interface selected through ordinary settings'])
            d.menu(4);shot('interface-setting');d.menu(8);d.press('a');shot('controls')
            d.press('r');shot('turbo-guide');d.press('b')
        d.press('b');assert d.game().mode==0
    d.menu(1);d.press('a');assert d.game().mode==3
    before=(d.game().cursor,d.game().selection,d.game().save.cash)
    d.ticks(4,touch=(120,100));assert before==(d.game().cursor,d.game().selection,d.game().save.cash)
    # Showroom cannot browse unowned cars; the Dealer has ordered class lists.
    d.menu(0)
    visited=[]
    for _ in range(47):visited.append(d.game().selection);d.press('right')
    assert len(set(visited))==47 and 10 not in visited
    owned(0);shot('showroom');tab(3)
    reviewed=set()
    for cls in range(5):
        while d.game().garage_filter!=cls:d.press('right')
        expected=sorted([v['id'] for v in vehicles if v['class_id']==cls],key=lambda v:(0 if v<31 and vehicles[v]['specialty']==0 else 1 if v<31 else 3 if v in (37,39,41,46,47) else 2,vehicles[v]['price'],v))
        for row,v in enumerate(expected,1):
            d.menu(row);assert d.game().selection==v;reviewed.add(v)
            if v==10:
                cash=d.game().save.cash;d.press('a')
                assert d.game().save.owned&(1<<10)
                assert d.game().save.cash==cash-vehicles[10]['price']
        shot('dealer-'+str(cls))
    assert reviewed==set(range(48))
    tab(0);owned(0);d.menu(0);d.press('a');assert d.game().save.selected_vehicle==0
    tab(1);d.menu(2);cash=d.game().save.cash;d.press('a')
    assert d.game().save.engine[0]==1 and d.game().save.cash<cash
    d.menu(0)
    while d.game().build_choice!=2:d.press('right')
    d.menu(1);d.press('a');assert d.game().save.power_tune[0]==2
    shot('tuning')
    assert style((1,2,3,2))==490
    shot('style')
    tab(0);d.menu(1);d.press('a');d.menu(1);d.press('a');assert d.game().save.selected_location==1
    d.press('b');assert d.game().cursor==1
    d.menu(4);d.press('a');assert d.game().save.preset_mask==1
    saved=d.game().save.presets[0]
    assert style((0,0,0,0))==81
    assert style((1,2,3,2))==245
    assert style((0,0,0,0))==81
    tab(0);d.menu(1);d.press('a');d.menu(2);d.press('a');assert d.game().save.selected_location==2
    d.press('b');assert d.game().cursor==1
    owned(2);d.menu(0);d.press('a');assert d.game().save.selected_vehicle==2
    d.menu(3);cash=d.game().save.cash;d.press('a');g=d.game()
    assert g.save.cash==cash-245 and g.save.selected_vehicle==0 and g.save.selected_location==1
    assert g.save.smoke[0]==2 and g.save.presets[0]==saved
    shot('presets')
    events.extend(['owned-only showroom','all 48 vehicles in five class lists','dealer purchase',
        'engine upgrade and top-speed build','full-price installation $490',
        'factory restoration $81','returning finish $245','paid preset restores car/build/route/style'])
    # All departure themes are selected through the single Departure selector.
    d.menu(1);d.press('a');assert d.game().mode==4;shot('routes')
    if skin==3:
        for region in range(13):
            d.menu(region);d.press('a');shot(f'location-{region:02d}')
            d.press('b');assert d.game().mode==3
            shot(f'room-{region:02d}')
            d.menu(1);d.press('a')
    d.menu(0);d.press('a');d.press('b');assert d.game().mode==3
    d.press('b');assert d.game().mode==0
    # Short live drive verifies skin-specific dashboard, smoke and pause paths.
    d.menu(0);d.press('a');d.autopilot(150,('r',));assert d.game().mode==1
    d.ticks(8,('r','b','right'));assert d.game().drifting
    d.shot(f'ui-{skin}-driving',('r','b','right'))
    d.press('start');assert d.game().mode==7;shot('pause')
    d.menu(3);d.press('a');assert d.game().mode==9;d.press('b');assert d.game().mode==7
    d.menu(1);d.press('a');assert d.game().mode==2;shot('receipt')
    assert d.game().run.banked and d.game().run.total_points==sum(d.game().run.points)
    d.press('a');d.ticks(15)
    g=d.game();g.save.checksum=0;expected=a.output/f'studio-{skin}-expected.bin';expected.write_bytes(bytes(g.save))
    screenshots=d.shots
geometry=assert_compact_battery(directory/'Coastline.dsv')
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),'--elf',str(a.elf),
    '--save-dir',str(directory),'--expected',str(expected),'--output',str(a.output/f'ui-{skin}-reloaded.png')],check=True)
result={'skin':skin,'name':('Fusion','Pearl','Voltage','Alloy','Glacier','Bloom')[skin-3],'checks':events,
    'all_classes_reviewed':True,'vehicles_reviewed':48,'cosmetics_and_presets_passed':True,
    'fusion_navigation_passed':True,
    'fresh_process_reload':True,'screenshots':screenshots,**geometry}
(a.output/f'studio-{skin}.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result),flush=True)
