#!/usr/bin/env python3
"""Actual-cartridge world/garage/dashboard review from a labelled fixture.

The checksummed native-generated battery profile pre-unlocks vehicles,
departures, paint and mastery for visual review. Those rewards are not claimed
as earned by the stock-profile playthrough. All subsequent actions use buttons;
RAM is read only and emulator checkpoints are never restored.
"""
import argparse
import ctypes as C
import json
import shutil
import subprocess
import sys
from pathlib import Path
from emulator import Emulator,ROOT
from game_snapshot import Game,Save,find_symbol,symbol_size
from playthrough import DriveThrough
from inspect_rom import inspect
from fixtures import write_battery

BIOMES=('city','coast','causeway','pines','canyon','marina','wetlands','groves')
VEHICLES=('pacer','vela','nomad','kestrel','vector','sundown','mariner','regent','rallye','tempest')
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
real_save=a.output/'save'/'Coastline.dsv'
assert real_save.exists(),'Run the genuine opening/save playthrough first'
libpath=ROOT/'build'/'libdrive-qa.so'
subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c11','-I',str(ROOT/'src'),
    str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),str(ROOT/'src'/'traffic.c'),'-o',str(libpath)],check=True)
lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
lib.game_save_valid.argtypes=[C.POINTER(Save)]
profile=Game();lib.game_init(C.byref(profile),0x434f4153)
profile.save.owned=(1<<10)-1;profile.save.unlocked_locations=255;profile.save.paints=63
profile.save.cash=9999;profile.save.career_xp=50000
for i in range(10):profile.save.mastery_xp[i]=10000
profile.save.best_near=10;profile.save.best_distance=6000
profile.save.best_drift=1500;profile.save.best_speed_hold=240;profile.save.best_wheelie=1000
out=Save();lib.game_make_save(C.byref(profile),C.byref(out));assert lib.game_save_valid(C.byref(out))
fixture=a.output/'scenery-fixture-save';shutil.rmtree(fixture,ignore_errors=True)
write_battery(real_save,fixture/'Coastline.dsv',bytes(out))
address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
shots=[];rates=[];cluster_rates=[];actions=[]
with Emulator(a.rom,fixture) as e:
    e.run(180);d=DriveThrough(e,address,a.output);assert d.game().has_save
    def shot(name,keys=()):
        d.shot(name,keys);shots.append(name+'.png')
    def depart(biome):
        assert d.game().mode==0
        d.menu(2);d.press('a');d.menu(biome);d.press('a');d.press('b')
        assert d.game().save.selected_location==biome
    def finish():
        d.press('start');assert d.game().mode==7
        d.menu(1);d.press('a');assert d.game().mode==2
        d.press('a');assert d.game().mode==0
    def rate(name,keys=('r',)):
        before=d.game().frame;hw=e.frames;e.run(120,keys)
        updates=d.game().frame-before;hz=updates/(120/e.fps)
        assert d.game().mode==1,(name,'run ended during timing sample')
        result={'name':name,'updates':updates,'hardware_frames':e.frames-hw,'hz':hz}
        print(json.dumps(result),flush=True);assert 28<=hz<=31.5,(name,hz)
        return result
    def garage_vehicle(vehicle):
        assert d.game().mode==3
        while d.game().garage_tab:d.press('l')
        d.menu(0)
        for _ in range(10):
            if d.game().selection==vehicle:return
            d.press('right')
        raise AssertionError('Garage could not browse all ten vehicles')
    for biome,name in enumerate(BIOMES):
        depart(biome);d.menu(0);d.press('a');d.autopilot(165,('r',))
        assert d.game().mode==1 and d.game().biome==biome
        shot('scenery-'+name,('r',));rates.append(rate(name));finish()
    d.menu(1);d.press('a');assert d.game().mode==3
    for v,name in enumerate(VEHICLES):
        garage_vehicle(v);shot('vehicle-'+name)
    garage_vehicle(0);d.press('r');assert d.game().garage_tab==1
    for cursor,field,cost in ((1,'engine',60),(2,'grip',54),(3,'armor',57)):
        d.menu(cursor);before=d.game().save.cash;d.press('a');g=d.game()
        assert getattr(g.save,field)[0]==1 and g.save.cash==before-cost
        actions.append({'fixture_funded_part':field,'cost':cost,'level':1})
    shot('garage-parts-upgraded')
    d.press('r');assert d.game().garage_tab==2
    for cursor,field in ((1,'power_tune'),(2,'handling_tune'),(3,'chassis_tune')):
        d.menu(cursor);before=d.game().save.cash
        assert getattr(d.game().save,field)[0]==0
        d.press('right');assert d.game().tune_choice==1
        assert getattr(d.game().save,field)[0]==0 # Preview is not an applied setup.
        shot('garage-tune-preview-'+field)
        d.press('a');assert getattr(d.game().save,field)[0]==1
        assert d.game().save.cash==before
        actions.append({'free_tune_applied':field,'value':1})
    d.press('b');d.menu(4);d.press('a');assert d.game().mode==8
    cash=d.game().save.cash
    for cursor in range(3):d.menu(cursor);d.press('a')
    assert list(d.game().save.perks)==[1,1,1];shot('career-perks')
    d.menu(3);d.press('a');assert list(d.game().save.perks)==[0,0,0]
    assert d.game().save.cash==cash;shot('career-reset');d.press('b')
    # Each distinct dashboard family gets an actual driving capture; the six
    # cases cover all ten vehicles, three skins and both speed-unit settings.
    families=((0,0,0,'classic'),(1,1,5,'digital'),(2,2,7,'utility'),
              (3,0,1,'bike'),(7,1,5,'luxury'),(4,2,4,'race'),
              (5,2,4,'sundown'),(6,0,1,'mariner'),
              (8,1,7,'rallye'),(9,0,2,'tempest'))
    for vehicle,skin,biome,family in families:
        d.menu(5);d.press('a');assert d.game().mode==9;d.menu(4)
        while d.game().save.skin!=skin:d.press('right')
        d.menu(7)
        target_units=1 if family=='bike' else 0
        while d.game().save.units!=target_units:d.press('right')
        shot('settings-'+family);d.press('b')
        d.menu(1);d.press('a');garage_vehicle(vehicle);d.menu(1);d.press('a');d.press('b')
        assert d.game().save.selected_vehicle==vehicle
        depart(biome);d.menu(0);d.press('a')
        keys=('r','a') if vehicle==3 else ('r',)
        d.autopilot(150,keys);assert d.game().mode==1
        shot('dashboard-'+family,keys)
        if vehicle==3:
            assert d.game().wheelie;shot('motorcycle-wheelie',keys)
        measured=rate(family,keys);measured.update(vehicle=vehicle,skin=skin,units=target_units)
        cluster_rates.append(measured);finish()
    g=d.game();g.save.checksum=0;expected=bytes(g.save)
(a.output/'expected-advanced-profile.bin').write_bytes(expected)
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
    '--elf',str(a.elf),'--save-dir',str(fixture),
    '--expected',str(a.output/'expected-advanced-profile.bin'),
    '--output',str(a.output/'advanced-save-reloaded.png')],check=True)
shots.append('advanced-save-reloaded.png')
result={'rom':inspect(a.rom),'source':'documented native-generated battery fixture; vehicles, departures, cash, mastery and career pre-seeded for review',
        'ram_access':'read only; joypad input only; no save-state restore',
        'screenshots':shots,'biome_update_rates':rates,'cluster_update_rates':cluster_rates,
        'fixture_funded_actions':actions,'fresh_process_reload':True}
(a.output/'scenery-results.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))

from PIL import Image,ImageDraw,ImageFont
font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',18)
def sheet(items,path,bottom=False):
    rows=(len(items)+1)//2
    canvas=Image.new('RGB',(1024,416*rows),(12,23,31));draw=ImageDraw.Draw(canvas)
    for i,(name,label) in enumerate(items):
        x=i%2*512;y=i//2*416
        image=Image.open(a.output/(name+'.png')).convert('RGB')
        image=image.crop((0,192 if bottom else 0,256,384 if bottom else 192))
        canvas.paste(image.resize((512,384),Image.Resampling.NEAREST),(x,y+32))
        draw.text((x+14,y+5),label,font=font,fill=(224,237,223))
    canvas.save(a.output/path)
labels=('Metro Dawn','Palm Coast','Skywater Causeway','Pine Ridge','Redrock Pass',
        'Marina Nights','Mangrove Run','Citrus Parkway')
sheet(list(zip(('scenery-'+b for b in BIOMES),labels))+[('motorcycle-wheelie','Kestrel motorcycle')],'Locations.png')
sheet([('vehicle-'+v,v.title()) for v in VEHICLES],'Vehicles.png')
sheet([('dashboard-'+f,f.title()) for f in ('classic','digital','utility','bike','luxury','race','sundown','mariner','rallye','tempest')],'Dashboards.png',True)
