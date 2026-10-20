#!/usr/bin/env python3
"""Read-only DS VRAM comparison: preserve all painted pixels in all three poses.

Pin --baseline to the previous ROM/ELF. Then compare the optimized cartridge
with --reference pointing to its JSON report. Each case boots a fresh core.
"""
import argparse,ctypes as C,hashlib,json,shutil,subprocess,sys
from pathlib import Path
from emulator import Emulator,ROOT
from fixtures import encode_profile,write_battery,battery_size
from game_snapshot import Game,find_symbol
from playthrough import DriveThrough
from inspect_rom import inspect
CASES=(0,3,25,30,38,42,46,47)
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build/Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build/arm9.elf')
p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline',action='store_true')
p.add_argument('--reference',type=Path)
p.add_argument('--case',type=int)
a=p.parse_args();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
library=a.output/'libtexture-fixture.so'
if a.case is None:
 subprocess.run(['gcc','-std=c11','-O2','-shared','-fPIC','-Isrc','-Ibuild','src/game.c','src/story.c','src/story_io.c','src/road.c','src/traffic.c','-o',str(library)],cwd=ROOT,check=True)
 for model in CASES:
  cmd=[sys.executable,__file__,'--rom',str(a.rom.resolve()),'--elf',str(a.elf.resolve()),'--output',str(a.output),'--case',str(model)]
  if a.baseline:cmd.append('--baseline')
  subprocess.run(cmd,cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
 cases=[json.loads((a.output/f'model-{v}.json').read_text()) for v in CASES]
 result={'rom':inspect(a.rom),'cases':cases,'source_pixels_checked':len(CASES)*3*48*72,
  'method':'Fresh DS emulator processes, controller input and read-only physical VRAM. Compare old 64x128 direct-color pose rows to all three original 48x72 drawings in the new 256x128 indexed atlas. Original source resolution and exact RGB555/transparent pixels are required.'}
 if a.reference:
  previous=json.loads(a.reference.read_text());assert [c['poses'] for c in cases]==[c['poses'] for c in previous['cases']]
  result['previous_rom_sha256']=previous['rom']['sha256'];result['all_pixels_match_previous']=True
 (a.output/'texture-results.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));raise SystemExit
model=a.case;lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
profile=Game();lib.game_init(C.byref(profile),0x54455854)
profile.save.owned=0xffffffff;profile.save.owned_hi=0xffff;profile.save.selected_vehicle=model;profile.save.paints=0xffff
profile.save.paint[model]=1+model%15;profile.save.livery[model]=1+model%4
profile.save.livery_color[model]=model%12;profile.save.wheels[model]=1+model%3
profile.save.paint_seen[model]|=1<<profile.save.paint[model]
profile.save.livery_owned[model]|=1<<profile.save.livery[model]
profile.save.wheels_owned[model]|=1<<profile.save.wheels[model]
profile.save.livery_color_seen[model]|=1<<profile.save.livery_color[model]
save=a.output/f'save-{model}';shutil.rmtree(save,ignore_errors=True)
template=next(ROOT/name for name in ('build/life-release/save/Coastline.dsv','build/afternoon-release/save/Coastline.dsv','build/finish-final/save/Coastline.dsv') if (ROOT/name).exists() and battery_size(ROOT/name)==8192)
write_battery(template,save/'Coastline.dsv',encode_profile(lib,profile))
address=find_symbol(a.elf,'game');mesh=find_symbol(a.elf,'city_mesh_last_stats');hashes=[];uploads=[]
with Emulator(a.rom,save) as e:
 e.run(180);d=DriveThrough(e,address,a.output);d.menu(0);d.press('a');d.press('right');d.press('a');d.ticks(8)
 assert d.game().vehicle==model and d.game().mode==1
 for pose,keys in enumerate((('left',),(),('right',))):
  d.ticks(3,keys);e.run(6,keys)
  memory=e.snapshot_fields()[4,b'LCDM'];raw=bytearray()
  if a.baseline:
   for y in range(72):raw.extend(memory[0x60000+98304+y*128:0x60000+98304+y*128+96])
  else:
   palette=memory[0x80000+512+48*512+512:0x80000+512+48*512+1024]
   for y in range(72):
    for index in memory[0x60000+98304+y*256+pose*64:0x60000+98304+y*256+pose*64+48]:
     raw.extend(palette[index*2:index*2+2] if index else b'\0\0')
  assert len(raw)==48*72*2 and len(set(raw))>32
  hashes.append(hashlib.sha256(raw).hexdigest())
  uploads.append(int.from_bytes(e.read_ram(mesh+8,4),'little'))
  e.screenshot(a.output/f'vehicle-{model}-pose-{pose}.png')
 if not a.baseline:assert len(set(uploads))==1,('Steering reuploaded a resident pose',uploads)
(a.output/f'model-{model}.json').write_text(json.dumps({'model':model,'poses':hashes,'uploads':uploads})+'\n')
