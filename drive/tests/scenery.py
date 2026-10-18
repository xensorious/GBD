#!/usr/bin/env python3
"""Actual-cartridge scenery captures from a clearly labelled profile fixture.

The fixture is an ordinary checksummed battery save produced by the native game
API. It pre-unlocks departures and vehicles for visual review. It is not an
assertion that those rewards were earned by the emulator playthrough. After
boot, every action uses buttons; emulator RAM is read only.
"""
import argparse
import ctypes as C
import json
import shutil
import subprocess
from pathlib import Path
from emulator import Emulator,ROOT
from game_snapshot import Game,Save,find_symbol
from playthrough import DriveThrough
from inspect_rom import inspect

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args()
real_save=a.output/'save'/'Coastline.dsv'
assert real_save.exists(),'Run the genuine opening/save playthrough first'
template=real_save.read_bytes();raw_size=template.index(b'|<--Snip')
libpath=ROOT/'build'/'libdrive-qa.so'
subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c99','-I',str(ROOT/'src'),
    str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),'-o',str(libpath)],check=True)
lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
lib.game_save_valid.argtypes=[C.POINTER(Save)]
profile=Game();lib.game_init(C.byref(profile),0x434f4153)
profile.save.owned=63;profile.save.unlocked_locations=31;profile.save.paints=63
profile.save.cash=9999
profile.save.best_near=10;profile.save.best_distance=6000
profile.save.best_drift=1500;profile.save.best_speed_hold=240;profile.save.best_wheelie=1000
out=Save();lib.game_make_save(C.byref(profile),C.byref(out));assert lib.game_save_valid(C.byref(out))
fixture=a.output/'scenery-fixture-save';shutil.rmtree(fixture,ignore_errors=True);fixture.mkdir()
(fixture/'Coastline.dsv').write_bytes(bytes(out)+b'\xff'*(raw_size-C.sizeof(out))+template[raw_size:])
address=find_symbol(a.elf,'game');shots=[];rates=[]
with Emulator(a.rom,fixture) as e:
    e.run(180);d=DriveThrough(e,address,a.output);assert d.game().has_save
    for biome,name in enumerate(('city','coast','causeway','pines','canyon')):
        assert d.game().mode==0
        d.menu(2);d.press('a');d.menu(biome);d.press('a');d.press('b')
        assert d.game().save.selected_location==biome
        d.menu(0);d.press('a');d.autopilot(165,('r',))
        assert d.game().mode==1 and d.game().biome==biome
        d.shot('scenery-'+name,('r',));shots.append('scenery-'+name+'.png')
        before=d.game().frame;hw=e.frames;e.run(120,('r',))
        updates=d.game().frame-before;rate=updates/(120/e.fps)
        assert d.game().mode==1,(name,'run ended during timing sample')
        rates.append({'biome':name,'updates':updates,'hardware_frames':e.frames-hw,'hz':rate})
        print(json.dumps(rates[-1]),flush=True)
        assert 28<=rate<=31.5,(name,rate)
        d.press('start');d.menu(1);d.press('a');assert d.game().mode==2
        d.press('a')
    d.menu(1);d.press('a');assert d.game().mode==3
    d.menu(0)
    while d.game().selection!=3:d.press('right')
    d.menu(1);d.press('a');d.press('b');assert d.game().save.selected_vehicle==3
    d.menu(0);d.press('a');d.autopilot(150,('r','a'))
    assert d.game().mode==1 and d.game().wheelie
    d.shot('motorcycle-wheelie',('r','a'));shots.append('motorcycle-wheelie.png')
result={'rom':inspect(a.rom),'source':'ordinary native-generated battery-save fixture; all departures and vehicles pre-unlocked for visual review',
        'ram_access':'read only; joypad input only; no save-state restore',
        'screenshots':shots,'biome_update_rates':rates}
(a.output/'scenery-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

# Keep the exported contact sheet reproducible from the actual captures.
from PIL import Image,ImageDraw,ImageFont
items=[('scenery-city','Metro Dawn'),('scenery-coast','Palm Coast'),
       ('scenery-causeway','Skywater Causeway'),('scenery-pines','Pine Ridge'),
       ('scenery-canyon','Redrock Pass'),('motorcycle-wheelie','Kestrel motorcycle')]
canvas=Image.new('RGB',(1024,1248),(12,23,31));draw=ImageDraw.Draw(canvas)
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',18)
for i,(name,label) in enumerate(items):
    x=i%2*512;y=i//2*416
    im=Image.open(a.output/(name+'.png')).convert('RGB').crop((0,0,256,192))
    canvas.paste(im.resize((512,384),Image.Resampling.NEAREST),(x,y+32))
    draw.text((x+14,y+5),label,font=font,fill=(224,237,223))
canvas.save(a.output/'Locations.png')
