#!/usr/bin/env python3
"""Drive an actual biome boundary using buttons and a labelled touring fixture.

The native-generated profile owns a durable Nomad with earned-state fixtures
(parts/mastery/perks). The starting distance is zero: the entire coast-to-bridge
journey is driven through ordinary controls, never by RAM edits or save states.
"""
import argparse
import ctypes as C
import json
import shutil
import subprocess
from pathlib import Path
from emulator import Emulator,ROOT
from fixtures import write_battery
from game_snapshot import Game,Save,find_symbol,symbol_size
from inspect_rom import inspect
from playthrough import DriveThrough

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
libpath=ROOT/'build'/'libdrive-transition.so'
subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c11','-I',str(ROOT/'src'),
    str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),str(ROOT/'src'/'traffic.c'),'-o',str(libpath)],check=True)
lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
lib.game_save_valid.argtypes=[C.POINTER(Save)]
g=Game();lib.game_init(C.byref(g),0x434f4153)
g.save.owned=1023;g.save.unlocked_locations=255;g.save.paints=63;g.save.cash=9999
g.save.selected_vehicle=2;g.save.selected_location=1;g.save.mastery_xp[2]=10000
g.save.career_xp=50000
for i in range(3):g.save.perks[i]=3
g.save.engine[2]=6;g.save.grip[2]=4;g.save.armor[2]=6
g.save.power_tune[2]=1;g.save.handling_tune[2]=1;g.save.chassis_tune[2]=2
out=Save();lib.game_make_save(C.byref(g),C.byref(out));assert lib.game_save_valid(C.byref(out))
fixture=a.output/'transition-fixture-save';shutil.rmtree(fixture,ignore_errors=True)
write_battery(a.output/'save'/'Coastline.dsv',fixture/'Coastline.dsv',bytes(out))
address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
frames=[];shots=[];seen=set();boundary=21000;next_frame=boundary-1200
with Emulator(a.rom,fixture) as e:
    e.run(180);d=DriveThrough(e,address,a.output)
    assert d.game().has_save and d.game().save.selected_location==1
    d.menu(0);d.press('a');assert d.game().mode==1 and d.game().position_q8==0
    start_frame=d.game().frame;start_hw=e.frames
    thresholds=[(boundary-1500,'transition-before'),(boundary-650,'transition-mid'),
                (boundary+100,'transition-after')]
    while d.game().mode==1 and d.game().frame-start_frame<9000:
        state=d.game();seen.add(state.biome)
        if state.position_q8//256>=boundary+180:break
        d.ticks(2,('r','down')+d.steer(d.safe_lane(state)))
        z=d.game().position_q8//256
        if thresholds and z>=thresholds[0][0]:
            _,name=thresholds.pop(0);d.e.screenshot(a.output/(name+'.png'));shots.append(name+'.png')
        if z>=next_frame and z<=boundary+200:
            frames.append(e.image().crop((0,0,256,192)).resize((512,384)))
            next_frame+=50
    state=d.game();seen.add(state.biome)
    assert state.mode==1 and state.position_q8//256>=boundary+180,(state.mode,state.position_q8//256,state.health)
    assert {1,2}<=seen,seen
    elapsed=state.frame-start_frame;hardware=e.frames-start_hw
    rate=elapsed/(hardware/e.fps);assert 28<=rate<=31.5,rate
    result={'rom':inspect(a.rom),
        'profile_source':'labelled native-generated Nomad/mastery/parts/perks fixture; start distance zero',
        'ram_access':'read only; all travel uses R + Down precision and steering buttons; no checkpoint restore',
        'distance_driven_m':state.run.distance_m,'world_distance':state.position_q8//256,
        'elapsed_game_ticks':elapsed,'hardware_frames':hardware,'update_hz':rate,
        'biomes_seen':sorted(seen),'health':state.health,'screenshots':shots}
if frames:frames[0].save(a.output/'biome-transition.gif',save_all=True,append_images=frames[1:],duration=200,loop=0)
(a.output/'transition-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
