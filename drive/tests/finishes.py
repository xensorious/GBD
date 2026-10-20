#!/usr/bin/env python3
"""Exercise the finish studio on the cartridge with ordinary button input.

The labelled review profile supplies a budget, two vehicles and challenge
paints. This is customization evidence, not earned-progression evidence.
No emulator RAM is written.
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
from game_snapshot import Game, find_symbol, verify_layout
from inspect_rom import inspect
from playthrough import DriveThrough

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build'/'finish-release')
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf';verify_layout(ROOT)
library=ROOT/'build'/'libfinishes-qa.so'
subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild',
    'src/game.c','src/story.c','src/story_io.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_cosmetic_cost.argtypes=[C.POINTER(Game),C.c_int,C.c_int]
profile=Game();lib.game_init(C.byref(profile),0x46494e49)
profile.save.cash=1000000;profile.save.owned=(1<<31)|1;profile.save.owned_hi=1<<15
profile.save.selected_vehicle=31;profile.save.paints=0xffff
profile.save.mastery_xp[31]=profile.save.mastery_xp[47]=10000
directory=a.output/'finishes-save';shutil.rmtree(directory,ignore_errors=True)
write_battery(a.output/'save'/'Coastline.dsv',directory/'Coastline.dsv',encode_profile(lib,profile))
paint_names=['Factory','Coral','Lagoon','Sunburst','Orchid','Graphite','Black','White',
    'Cobalt blue','Racing red','Forest green','Orange','Silver','Ice blue','Sand','Magenta']
color_names=['White','Black','Graphite','Red','Blue','Yellow','Purple','Forest green',
    'Orange','Teal','Silver','Rose gold']
installs=0
with Emulator(rom,directory) as e:
    e.run(180);d=DriveThrough(e,find_symbol(elf,'game'),a.output)
    d.menu(1);d.press('a');d.press('r');d.press('r')
    assert d.game().garage_tab==2 and d.game().selection==31
    def choice(row,field,value):
        d.menu(row)
        for _ in range(16):
            if getattr(d.game(),field)==value:return
            d.press('right')
        assert getattr(d.game(),field)==value,(field,value)
    def install():
        global installs
        g=d.game();values=(g.livery_choice,g.wheel_choice,g.paint_choice,g.smoke_choice,g.livery_color_choice)
        cost=sum(lib.game_cosmetic_cost(C.byref(g),kind,value) for kind,value in enumerate(values))
        before=g.save.cash;d.menu(5);d.press('a');now=d.game();v=now.selection
        assert now.save.cash==before-cost
        assert tuple(getattr(now.save,field)[v] for field in ('livery','wheels','paint','smoke','livery_color'))==values
        installs+=1
    # Factory livery has no meaningless color charge or active color selector.
    d.menu(2);d.press('right');assert d.game().livery_color_choice==0
    choice(0,'paint_choice',8);choice(1,'livery_choice',3);choice(2,'livery_color_choice',3);install()
    for color in range(12):
        choice(2,'livery_color_choice',color);install();g=d.game()
        assert g.save.paint[31]==8 and g.save.livery[31]==3 and g.save.livery_color[31]==color
        d.shot(f'finish-color-{color:02d}')
    choice(2,'livery_color_choice',0)
    for paint in range(16):
        choice(0,'paint_choice',paint);install();g=d.game()
        assert g.save.paint[31]==paint and g.save.livery_color[31]==0 and g.save.livery[31]==3
        d.shot(f'finish-paint-{paint:02d}')
    choice(0,'paint_choice',8)
    for pattern in (3,4):
        choice(1,'livery_choice',pattern);install();d.shot(f'finish-pattern-{pattern}')
    # High vehicle, paint and color bits must coexist in saved presets.
    d.press('l');d.press('l');d.menu(0);d.press('right');assert d.game().selection==47
    d.press('a');d.press('r');d.press('r')
    for row,field,value in ((0,'paint_choice',15),(1,'livery_choice',4),(2,'livery_color_choice',11)):
        choice(row,field,value)
    install();d.shot('finish-signature')
    d.press('l');d.press('l');d.menu(4);d.press('a');preset=d.game().save.presets[0]
    assert ((preset&31)|((preset>>21)&32))==47 and preset>>28==11
    assert (((preset>>9)&7)|((preset>>24)&8))==15
    d.press('r');d.press('r');choice(2,'livery_color_choice',3);install()
    d.press('l');d.press('l');d.menu(3);d.press('a');g=d.game()
    assert g.save.paint[47]==15 and g.save.livery_color[47]==11 and g.save.presets[0]==preset
    d.shot('finish-preset');d.press('b');d.menu(4);d.press('a');d.menu(8);d.press('a')
    for page in range(3):
        assert d.game().controls_page==page;d.shot(f'finish-controls-{page}');d.press('r')
    assert d.game().controls_page==0
    d.press('b');d.press('b');d.ticks(12)
    g=d.game();assert g.save.paint_seen[31]==0xffff and g.save.livery_color_seen[31]==0xfff
    g.save.checksum=0;expected=a.output/'finishes-expected.bin';expected.write_bytes(bytes(g.save))
geometry=assert_compact_battery(directory/'Coastline.dsv')
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(rom),'--elf',str(elf),
    '--save-dir',str(directory),'--expected',str(expected),'--output',str(a.output/'finishes-reloaded.png')],check=True)
result={'rom':inspect(rom),'body_paints':paint_names,'livery_colors':color_names,
    'replacement_liveries':['Velocity','Contour'],'installs':installs,
    'independent_color_purchases':True,'high_id_color_preset':True,'three_controls_pages':True,
    'fresh_process_reload':True,'profile_source':'labelled funded customization fixture; buttons only; RAM read only',**geometry}
(a.output/'finishes-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
