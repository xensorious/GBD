#!/usr/bin/env python3
"""Extended button-driven lighting audit of the real DS executable.

RAM is read only. The owned/funded garage fixture enters through ordinary
battery loading. Road shadows are checked with independent sun-ray geometry,
not the game's receiver routine. No emulated save states or RAM writes.
"""
import argparse
import ctypes as C
import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
from collections import Counter
import numpy as np
from emulator import Emulator,ROOT
from fixtures import encode_profile,write_battery,battery_size
from game_snapshot import Game,find_symbol,verify_layout
from inspect_rom import inspect
from playthrough import DriveThrough
from city_pixels import RoadPixels

class Car(C.LittleEndianStructure):
 _fields_=[('x',C.c_int16),('y',C.c_int16),('angle',C.c_uint16),('progress',C.c_uint32)]+[(n,C.c_uint8) for n in ('route','reverse','model','near')]
class City(C.LittleEndianStructure):
 _fields_=[('active',C.c_int32)]+[(n,C.c_int32) for n in ('x','y','vx','vy','camera_x','camera_y')]+[('angle',C.c_uint16),('travelled',C.c_uint32),('cars',Car*64),('tokens',C.c_uint8*64)]
class SunShape(C.Structure):
 _fields_=[(n,C.c_int16) for n in ('x','y','w','d')]+[(n,C.c_uint8) for n in ('height','ridge','kind','texture')]
class Caster(C.Structure):
 _fields_=[('shape',SunShape),('hull',C.c_int16*16),('maxx',C.c_int16),('maxy',C.c_int16),('count',C.c_uint8)]

def shadow_counts(wx,wy,shapes,frame,atlas):
 """Clip rays (-1.25,-.75,+1) against roof prisms / canopy alpha."""
 counts=np.zeros(wx.shape,dtype=np.uint8)
 for s in shapes:
  x,y=wx-s.x,wy-s.y
  if s.kind==1:
   u=np.floor(x-s.height*1.25-(frame//30&1)).astype(int)
   v=np.floor(y-s.height*.75).astype(int)
   inside=(u>=0)&(u<48)&(v>=0)&(v<48)
   counts+=inside&(atlas[256+v.clip(0,47),s.texture*48+u.clip(0,47)]!=0)
  elif s.kind in (0,2):
   lo=np.maximum(0,np.maximum((x-s.w)/1.25,(y-s.d)/.75))
   hi=np.minimum(x/1.25,y/.75)
   if s.ridge:
    slope=2*s.ridge/s.d
    hi=np.minimum(hi,np.minimum((s.height+slope*y)/(1+slope*.75),(s.height+slope*(s.d-y))/(1-slope*.75)))
   else:hi=np.minimum(hi,s.height)
   counts+=lo<=hi
 return counts

def erode(mask):
 out=mask.copy()
 for dy in (-2,0,2):
  for dx in (-2,0,2):out&=np.roll(mask,(dy,dx),(0,1))
 out[:2]=out[-2:]=False;out[:,:2]=False;out[:,-2:]=False
 return out

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--output',type=Path,default=ROOT/'build/shadow-audit')
p.add_argument('--rom',type=Path,default=ROOT/'build/Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build/arm9.elf')
p.add_argument('--model',type=int,default=3)
p.add_argument('--short',action='store_true')
p.add_argument('--template',type=Path,help='Genuine .dsv from a stock playthrough (only its emulator footer is reused).')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rom=a.rom.resolve();elf=a.elf.resolve();initial_sha=hashlib.sha256(rom.read_bytes()).hexdigest()
verify_layout(ROOT)
library=out/'libshadow-audit.so'
subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild','src/game.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
lib=C.CDLL(str(library))
lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
profile=Game();lib.game_init(C.byref(profile),0x53484144)
profile.save.owned=0xffffffff;profile.save.owned_hi=0xffff;profile.save.selected_vehicle=a.model;profile.save.cash=1000000
profile.save.mastery_xp[a.model]=10000;profile.save.engine[a.model]=2;profile.save.grip[a.model]=2;profile.save.armor[a.model]=6
profile.save.paint[a.model]=11;profile.save.paint_seen[a.model]|=1<<11
directory=out/'save';shutil.rmtree(directory,ignore_errors=True)
template=a.template or next((ROOT/name for name in ('build/validation/save/Coastline.dsv','build/afternoon-release/save/Coastline.dsv','build/finish-final/save/Coastline.dsv') if (ROOT/name).exists() and battery_size(ROOT/name)==8192),None)
assert template and template.exists(),'Run tests/playthrough.py --emulator-only --output build/validation first, or provide --template.'
write_battery(template,directory/'Coastline.dsv',encode_profile(lib,profile))
addr={name:find_symbol(elf,name) for name in ('city','game','city_mesh_last_stats','city_sun_stats','city_casters','city_caster_count','city_sun_bin_stats')}
atlas=np.frombuffer((ROOT/'build/city_mesh_idx.bin').read_bytes(),dtype=np.uint8).reshape(512,256)
road=RoadPixels(lib);cadence=[];meshes=[];sunframes=[];headings=set();chunks=set();districts=set();photos=[];probes=[];pacing=[];presentation=[]
with Emulator(rom,directory) as e:
 e.run(180);d=DriveThrough(e,addr['game'],out)
 def state():return City.from_buffer_copy(e.read_ram(addr['city'],C.sizeof(City)))
 def sample():
  mesh=list((C.c_uint32*6).from_buffer_copy(e.read_ram(addr['city_mesh_last_stats'],24)))
  sun=list((C.c_uint32*9).from_buffer_copy(e.read_ram(addr['city_sun_stats'],36)))
  bins=list((C.c_uint32*2).from_buffer_copy(e.read_ram(addr['city_sun_bin_stats'],8)))
  assert not mesh[4] and mesh[0]<900 and not bins[1],('GPU or cache overflow',mesh,bins)
  meshes.append(mesh);sunframes.append(sun);c=state();headings.add(c.angle);chunks.add((c.camera_x//32768,c.camera_y//32768))
  districts.add('Ocean Boulevard' if c.y//256<800 else 'Palmetto Heights' if c.x//256>2304 and c.y//256>2304 else 'Glass Quay' if c.x//256>2304 else 'Canal Gardens' if c.y//256>2304 else 'Downtown West')
 def probe(label):
  # Pause through normal controls: a stable top-screen scene removes the
  # LCD pipeline's moving-camera ambiguity. This does not move the car or
  # change the route; actual driving cadence is measured separately.
  d.press('start');frozen=bytes(state());d.ticks(8);assert bytes(state())==frozen
  c=state();g=d.game();assert g.mode!=1 and c.active
  road.previous=None;road.sample(e,g,c)
  cam=road.camera(c);cx,cy,cs,sn=cam
  yy,xx=np.indices((192,256));right=(xx+.5-128)/(7/8);down=(yy+.5-112)/(7/8*222/256)
  norm=cs*cs+sn*sn;wx=cx+(right*cs-down*sn)/norm;wy=cy+(right*sn+down*cs)/norm
  count=int.from_bytes(e.read_ram(addr['city_caster_count'],4),'little');assert count<192
  shapes=[s.shape for s in (Caster*count).from_buffer_copy(e.read_ram(addr['city_casters'],C.sizeof(Caster)*count))]
  frame=list((C.c_uint32*9).from_buffer_copy(e.read_ram(addr['city_sun_stats'],36)))[7]
  coverage=shadow_counts(wx,wy,shapes,frame,atlas)
  mask=road.road_mask(cam);mask[:25]=False
  for actor in [(c.x/256,c.y/256,g.vehicle)]+[(v.x,v.y,v.model) for v in c.cars]:
   x,y,model=actor;shape=lib.city_vehicle_shape(model);r=shape.draw_half_length+shape.draw_half_width
   road.omit(mask,[road.project(cam,x+dx,y+dy,h) for dx in (-r,r) for dy in (-r,r) for h in (0,5)],7)
  # Omit raised architecture, trees and streetlight bases without masking
  # away the shadows they project past their own footprint.
  for s in shapes:
   if s.kind in (0,1,2):
    h=s.height+s.ridge
    road.omit(mask,[road.project(cam,x,y,z) for x in (s.x-3,s.x+s.w+3) for y in (s.y-3,s.y+s.d+3) for z in (0,h)],2)
  image=np.asarray(e.image())[:192,:,:3]
  lit=np.all(image==(57,64,74),axis=2);shade=np.all(image==(41,48,65),axis=2)
  # Exact asphalt pixels retain the original/shadow material. Lane paint,
  # collectibles, water and decorative props are independently distinct.
  litmask=erode(coverage==0)&mask;shademask=erode(coverage>0)&mask
  missing=int(np.count_nonzero(shademask&lit));extra=int(np.count_nonzero(litmask&shade))
  overlap=erode(coverage>1)&mask
  other=overlap&~(lit|shade)
  colors=Counter(map(tuple,image[other].tolist()))
  result={'label':label,'world':[round(c.x/256,2),round(c.y/256,2)],'heading':c.angle,
   'sunlit_asphalt':int(np.count_nonzero(litmask&lit)),'shaded_asphalt':int(np.count_nonzero(shademask&shade)),
   'missing_shadow':missing,'unexplained_shadow':extra,'overlap_single_shade':int(np.count_nonzero(overlap&shade)),
   'other_overlap_colors':[(list(color),n) for color,n in colors.most_common(8)]}
  probes.append(result);e.screenshot(out/(label+'.png'));print(json.dumps(result),flush=True)
  # Keep artifacts on failure so the mismatch can be inspected/fixed.
  assert missing<=8 and extra<=8,('Misplaced ground shadow',result)
  sample();d.menu(0);d.press('a')
 d.menu(0);d.press('a');d.press('right');d.press('a');d.ticks(8);assert state().active
 # Observe completed render tags once per emulated LCD frame, rather than
 # hiding isolated hitches inside the route's average update frequency.
 original_run=e.run;last_tag=last_lcd=last_image=last_image_lcd=None;image_warm=8
 def monitored_run(frames=1,keys=(),touch=None):
  global last_tag,last_lcd,last_image,last_image_lcd,image_warm
  for _ in range(frames):
   original_run(1,keys,touch)
   mode=int.from_bytes(e.read_ram(addr['game']+Game.mode.offset,1),'little')
   if mode!=1:last_tag=last_lcd=last_image=last_image_lcd=None;image_warm=8;continue
   tag=int.from_bytes(e.read_ram(addr['city_mesh_last_stats']+20,4),'little')
   if tag!=last_tag:
    if last_lcd is not None:pacing.append(e.frames-last_lcd)
    last_tag=tag;last_lcd=e.frames
   speed=int.from_bytes(e.read_ram(addr['game']+Game.speed_q8.offset,4),'little',signed=True)
   if image_warm or speed<35*256:
    image_warm=max(0,image_warm-1);last_image=last_image_lcd=None;continue
   picture=hashlib.blake2b(e.framebuffer[:192*e.pitch],digest_size=8).digest()
   if picture!=last_image:
    if last_image_lcd is not None:presentation.append(e.frames-last_image_lcd)
    last_image=picture;last_image_lcd=e.frames
 e.run=monitored_run
 route=[(654,1408),(1280,1408),(1280,2048),(2496,2048),
  (2496,2688),(2912,2688),(2912,3104),(2496,3104),(1280,3104),
  (1280,2688),(896,2688),(896,2432),(1280,2432),(1280,2048),
  (640,2048),(640,512),(1280,512)]
 if a.short:route=route[:4]
 for index,(tx,ty) in enumerate(route):
  for attempt in range(1800):
   c=state();g=d.game()
   if g.mode!=1:e.screenshot(out/'audit-run-ended.png')
   assert g.mode==1,('Run ended during audit',index,g.health,c.x/256,c.y/256,c.angle)
   dx,dy=tx-c.x/256,ty-c.y/256
   if math.hypot(dx,dy)<12:break
   desired=round(math.atan2(dx,-dy)*128/math.pi)%256;error=(desired-c.angle+128)%256-128
   keys=['l'] if (abs(error)>30 and g.speed_q8>70*256) or (math.hypot(dx,dy)<140 and g.speed_q8>45*256) else ['r','down']
   if abs(error)>2:keys.append('right' if error>0 else 'left')
   if abs(error)>8 and g.speed_q8>45*256:keys.append('b')
   before=e.frames;frame=g.frame;d.ticks(2,tuple(keys));cadence.append((d.game().frame-frame,e.frames-before));sample()
   if d.game().health<g.health:print('Contact',index,round(state().x/256,1),round(state().y/256,1),'angle',state().angle,'health',d.game().health,flush=True)
   if attempt%30==0 and len(photos)<120:photos.append(e.image().resize((512,768)))
  else:raise AssertionError(('Audit waypoint blocked',index,state().x/256,state().y/256))
  print('Waypoint',index,round(state().x/256),round(state().y/256),'health',d.game().health,flush=True)
  if index in (0,1,2,4,6,8,11,15):probe(f'shadow-stop-{index:02d}')
 # Stop in place while time/wind advances, and resume after a long pause.
 d.ticks(75);d.press('start');paused=bytes(state());d.ticks(150);assert bytes(state())==paused
 d.press('a');d.ticks(15);sample();e.screenshot(out/'shadow-pause-resume.png')
updates=sum(n for n,f in cadence);frames=sum(f for n,f in cadence);hz=updates*60/frames
assert hz>=25,('Extended city cadence',hz)
assert {'Downtown West','Glass Quay'}<=districts
assert any(s[4]<s[5] for s in sunframes) and any(s[4]==256 for s in sunframes) and any(s[5]==0 for s in sunframes)
if photos:photos[0].save(out/'Shadow-Audit.gif',save_all=True,append_images=photos[1:],duration=110,loop=0)
assert hashlib.sha256(rom.read_bytes()).hexdigest()==initial_sha
result={'rom':inspect(rom),'model':a.model,'button_only':True,'emulated_ram_read_only':True,
 'city_updates':updates,'hardware_frames':frames,'city_hz':hz,'waypoints':len(route),'districts':sorted(districts),
 'headings':len(headings),'camera_chunks':len(chunks),'max_quads':max(m[0] for m in meshes),
 'max_casters':max(s[0] for s in sunframes),'partial_player_frames':sum(s[4]<s[5] for s in sunframes),
 'pause_freezes_world':True,'shadow_probes':probes}
result['render_pacing']={'samples':len(pacing),'lcd_frame_gaps':dict(sorted(Counter(pacing).items())),
 'median_ms':float(np.median(pacing)*1000/60),'p95_ms':float(np.percentile(pacing,95)*1000/60),
 'worst_ms':float(max(pacing)*1000/60),'frames_over_50ms':sum(f>3 for f in pacing)}
result['presentation_pacing']={'samples':len(presentation),'lcd_frame_gaps':dict(sorted(Counter(presentation).items())),
 'median_ms':float(np.median(presentation)*1000/60),'p95_ms':float(np.percentile(presentation,95)*1000/60),
 'worst_ms':float(max(presentation)*1000/60),'frames_over_50ms':sum(f>3 for f in presentation),
 'method':'Actual top-screen output hash observed each LCD frame, moving above 35 km/h; eight LCD frames after a menu transition are excluded.'}
assert len(presentation)>200 and result['presentation_pacing']['worst_ms']<=50 and sum(f==2 for f in presentation)>=len(presentation)*.90,('Visible frame pacing',result['presentation_pacing'])
(out/'shadow-audit-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
