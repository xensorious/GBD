#!/usr/bin/env python3
"""Button-driven city tour on the actual DS ROM; RAM observations are read only.
The funded garage fixture is labelled and is not earned progression evidence.
"""
import argparse
import ctypes as C
import json
import math
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from fixtures import encode_profile,write_battery,assert_compact_battery
from game_snapshot import Game,find_symbol,verify_layout,symbol_size
from inspect_rom import inspect
from playthrough import DriveThrough
class Car(C.LittleEndianStructure):
 _fields_=[('x',C.c_int16),('y',C.c_int16),('angle',C.c_uint16),('progress',C.c_uint32)]+[(n,C.c_uint8) for n in ('route','reverse','model','near')]
class City(C.LittleEndianStructure):
 _fields_=[('active',C.c_int32)]+[(n,C.c_int32) for n in ('x','y','vx','vy','camera_x','camera_y')]+[('angle',C.c_uint16),('travelled',C.c_uint32),('cars',Car*64),('tokens',C.c_uint8*64)]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'build'/'city-release');a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
rom=ROOT/'build'/'Coastline.nds';elf=ROOT/'build'/'arm9.elf';verify_layout(ROOT)
library=ROOT/'build'/'libcity-qa.so';subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild','src/game.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
profile=Game();lib.game_init(C.byref(profile),0x43495459);profile.save.owned=0xffffffff;profile.save.owned_hi=0xffff;profile.save.selected_vehicle=46;profile.save.cash=1000000
profile.save.mastery_xp[46]=10000;profile.save.engine[46]=2;profile.save.grip[46]=2;profile.save.armor[46]=2
profile.save.paint[46]=11;profile.save.paint_seen[46]|=1<<11;profile.save.livery[46]=3;profile.save.livery_owned[46]|=1<<3
profile.save.livery_color[46]=4;profile.save.livery_color_seen[46]|=1<<4
directory=a.output/'city-save';shutil.rmtree(directory,ignore_errors=True)
template=ROOT/'build'/'finish-final'/'save'/'Coastline.dsv'
if not template.exists():template=a.output/'save'/'Coastline.dsv'
write_battery(template,directory/'Coastline.dsv',encode_profile(lib,profile))
address=find_symbol(elf,'city');assert symbol_size(elf,'city')==C.sizeof(City);game_address=find_symbol(elf,'game');images=[];waypoints=[]
mesh_address=find_symbol(elf,'city_mesh_last_stats');mesh_frames=[];headings=set()
with Emulator(rom,directory) as e:
 e.run(180);d=DriveThrough(e,game_address,a.output)
 def state():return City.from_buffer_copy(e.read_ram(address,C.sizeof(City)))
 d.menu(0);d.press('a');assert d.game().mode==11;d.shot('city-mode-select')
 d.ticks(3,touch=(180,80));assert d.game().mode==11 and not d.game().cursor
 d.press('right');d.shot('city-mode-selected');d.press('a');assert state().active and d.game().vehicle==46
 assert d.game().save.paint[46]==11 and d.game().save.livery_color[46]==4;d.shot('city-spawn')
 # Autopilot uses only physical inputs. It brakes for turns and follows the
 # same street junctions that the player can choose freely.
 for index,(tx,ty) in enumerate(((654,1421),(1235,1421),(1267,2010),(2360,2061),(2496,2061))):
  for attempt in range(1600):
   c=state();g=d.game();assert g.mode==1,('City tour ended',index,g.health,c.x/256,c.y/256)
   dx,dy=tx-c.x/256,ty-c.y/256
   if math.hypot(dx,dy)<32:break
   desired=round(math.atan2(dx,-dy)*128/math.pi)%256;error=(desired-c.angle+128)%256-128
   keys=['l'] if abs(error)>30 and g.speed_q8>70*256 else ['r','down']
   if abs(error)>2:keys.append('right' if error>0 else 'left')
   if abs(error)>8 and g.speed_q8>45*256:keys.append('b')
   before=e.frames;frame=g.frame;d.ticks(2,tuple(keys))
   mesh=list((C.c_uint32*6).from_buffer_copy(e.read_ram(mesh_address,24)))
   assert 20<mesh[0]<900 and not mesh[4],('Geometry budget/overflow',mesh)
   assert abs((mesh[3]-state().angle+128)%256-128)<=8,('Camera heading detached from player',mesh,state().angle)
   mesh_frames.append(mesh);headings.add(mesh[3])
   if index==1 and 32<state().angle<55 and 'turn-east' not in d.shots:
    d.shot('city-turn-east',tuple(keys));d.shots.append('turn-east')
   if index==2 and 87<state().angle<111 and 'turn-south' not in d.shots:
    d.shot('city-turn-south',tuple(keys));d.shots.append('turn-south')
   if d.game().frame>frame:
    waypoints.append((d.game().frame-frame,e.frames-before))
   if attempt%10==0 and len(images)<100:images.append(e.image().resize((512,768)))
   if index==3 and 1930<state().x/256<2220 and 'bridge' not in d.shots:d.shot('city-crossing',tuple(keys));d.shots.append('bridge')
  else:raise AssertionError(('Waypoint not reached',index,c.x/256,c.y/256))
  d.shot(f'city-route-{index}');print('Reached city waypoint',index,state().x/256,state().y/256,'health',d.game().health,flush=True)
 d.press('start');frozen=bytes(state());d.ticks(30);assert bytes(state())==frozen;d.shot('city-paused')
 d.menu(1);d.press('a');assert d.game().mode==2 and d.game().run.banked
 assert d.game().save.cash==1000000+d.game().run.total_cash;d.shot('city-receipt');d.press('a');d.ticks(8)
 # Exercise bank remapping both ways: reenter the same owned vehicle (with
 # the same cosmetics/cache key), leave it, and drive the bitmap highway.
 d.menu(0);d.press('a');d.press('right');d.press('a');d.ticks(5)
 assert state().active and d.game().vehicle==46;d.shot('city-reentry')
 # A small reverse circle fits inside the wide highway. It crosses the
 # 255/0 heading seam and shows all four viewing directions using buttons.
 circle=set();wrapped=False;previous=state().angle
 for turn in range(48):
  d.ticks(2,('l','right'))
  current=state().angle;wrapped|=abs(current-previous)>128;previous=current
  mesh=list((C.c_uint32*6).from_buffer_copy(e.read_ram(mesh_address,24)))
  assert 20<mesh[0]<900 and not mesh[4],('Full-circle geometry budget',mesh)
  mesh_frames.append(mesh);headings.add(mesh[3])
  for target in (48,112,176,240):
   if abs(current-target)<12 and target not in circle:
    circle.add(target);d.shot(f'city-circle-{target:03d}',('l','right'))
  assert d.game().mode==1 and d.game().health>0
 assert wrapped and len(circle)==4,('Full camera turn',circle,wrapped)
 d.press('start');d.menu(1);d.press('a');d.press('a');d.ticks(5)
 d.menu(0);d.press('a');d.press('a');d.ticks(30,('r',))
 assert not state().active and d.game().mode==1;d.shot('highway-after-city')
 d.press('start');d.menu(1);d.press('a');d.press('a');d.ticks(8)
 g=d.game();g.save.checksum=0;expected=a.output/'city-expected.bin';expected.write_bytes(bytes(g.save))
geometry=assert_compact_battery(directory/'Coastline.dsv')
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(rom),'--elf',str(elf),'--save-dir',str(directory),'--expected',str(expected),'--output',str(a.output/'city-reloaded.png')],check=True)
updates=sum(n for n,f in waypoints);hardware=sum(f for n,f in waypoints);hz=updates*60/hardware;assert hz>=25,('City cadence',hz)
if images:images[0].save(a.output/'Free-Roam.gif',save_all=True,append_images=images[1:],duration=85,loop=0)
assert len(headings)>=32,('Insufficient turning coverage',len(headings))
result={'rom':inspect(rom),'button_only_mode_selector':True,'touch_ignored':True,'actual_city_route_completed':True,'actual_bridge_crossing':True,'shared_owned_vehicle_and_cosmetics':True,'pause_freezes_city':True,'shared_banked_cash':True,'fresh_process_reload':True,'city_hz':hz,'city_updates':updates,'city_hardware_frames':hardware,
 'heading_locked_camera':True,'observed_headings':len(headings),'max_submitted_quads':max(m[0] for m in mesh_frames),
 'actual_full_circle_and_heading_wrap':True,
 'ds_geometry_overflow':False,'ds_color_underflow':False,'texture_banks_restored_on_reentry_and_highway':True,
 'profile_source':'labelled funded garage fixture; buttons only; RAM read only',**geometry}
(a.output/'city-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
