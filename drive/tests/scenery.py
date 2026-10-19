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
from game_snapshot import Game,Save,find_symbol,symbol_size,VEHICLE_COUNT,LOCATION_COUNT,catalogue
from playthrough import DriveThrough
from inspect_rom import inspect
from fixtures import write_battery,encode_profile

BIOMES=('city','coast','causeway','pines','canyon','marina','wetlands','groves',
        'neon','jungle','swamp','aurora','salt')
OLD_VEHICLES=('pacer','vela','nomad','kestrel','vector','sundown','mariner','regent','rallye','tempest')
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
real_save=a.output/'save'/'Coastline.dsv'
assert real_save.exists(),'Run the genuine opening/save playthrough first'
libpath=ROOT/'build'/'libdrive-qa.so'
subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c11','-I',str(ROOT/'src'),'-I',str(ROOT/'build'),
    str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),str(ROOT/'src'/'traffic.c'),'-o',str(libpath)],check=True)
lib=C.CDLL(str(libpath));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
lib.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
lib.game_save_valid.argtypes=[C.POINTER(Save)]
lib.road_landmark.argtypes=[C.c_uint32,C.c_uint32,C.c_int]
lib.road_landmark.restype=C.c_uint32
profile=Game();lib.game_init(C.byref(profile),0x434f4153)
profile.save.owned=(1<<VEHICLE_COUNT)-1;profile.save.unlocked_locations=(1<<LOCATION_COUNT)-1;profile.save.paints=63
profile.save.cash=9999;profile.save.career_xp=50000
for i in range(VEHICLE_COUNT):profile.save.mastery_xp[i]=10000
# A durable touring setup makes the finite-turbo checks repeatable without
# changing the cartridge's world position, traffic or run rewards.
profile.save.engine[2]=2;profile.save.grip[2]=4;profile.save.armor[2]=6
profile.save.chassis_tune[2]=2
profile.save.armor[3]=6;profile.save.chassis_tune[3]=2
profile.save.best_near=10;profile.save.best_distance=6000
profile.save.best_drift=1500;profile.save.best_speed_hold=240;profile.save.best_wheelie=1000
vehicle_catalogue,location_catalogue=catalogue(lib)
fixture=a.output/'scenery-fixture-save';shutil.rmtree(fixture,ignore_errors=True)
write_battery(real_save,fixture/'Coastline.dsv',encode_profile(lib,profile))
address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
preview_address=find_symbol(a.elf,'row_biome')
shots=[];rates=[];cluster_rates=[];actions=[];turbo_checks=[];water_captures=[];dashboard_images=[];preview_checks=[]
with Emulator(a.rom,fixture) as e:
    e.run(180);d=DriveThrough(e,address,a.output);assert d.game().has_save
    def shot(name,keys=()):
        d.shot(name,keys);shots.append(name+'.png')
    def depart(biome):
        assert d.game().mode==0
        d.menu(2);d.press('a');d.menu(biome);d.press('a')
        state=d.game();assert state.mode==4 and state.cursor==biome
        materials=e.read_ram(preview_address,192)
        sampled=[materials[y] for y in (35,70,100,150,185)]
        assert sampled==[biome]*5,(biome,state.frame,sampled)
        period='early' if state.frame<5250 else 'late'
        image_name='preview-'+BIOMES[biome]+'-'+period+'.png'
        if not any(item['screenshot']==image_name for item in preview_checks):
            e.screenshot(a.output/image_name);shots.append(image_name)
        preview_checks.append({'biome':biome,'frame':state.frame,'sampled_road_materials':sampled,
                               'period':period,'screenshot':image_name})
        d.press('b')
        assert d.game().save.selected_location==biome
    def finish():
        d.press('start');assert d.game().mode==7
        d.menu(1);d.press('a');assert d.game().mode==2
        d.press('a');assert d.game().mode==0
    def rate(name,keys=('r',)):
        state=d.game();before=state.frame;hw=e.frames
        start={'frame':state.frame,'vehicle':state.vehicle,'health':state.health,
               'speed_q8':state.speed_q8,'x_q8':state.x_q8}
        # Keep the fixed hardware-frame window while driving through traffic
        # with the same read-only joypad steering used for the rest of the tour.
        for _ in range(60):
            state=d.game()
            if state.mode!=1:break
            e.run(2,tuple(keys)+d.steer(d.safe_lane(state)))
        updates=d.game().frame-before;elapsed=e.frames-hw;hz=updates/(120/e.fps)
        assert d.game().mode==1,(name,'run ended during timing sample',start,
                                {'frame':d.game().frame,'health':d.game().health})
        assert elapsed==120,(name,elapsed)
        state=d.game()
        result={'name':name,'updates':updates,'hardware_frames':elapsed,'hz':hz,
                'steering':'ordinary traffic-aware joypad input','start':start,
                'end':{'frame':state.frame,'vehicle':state.vehicle,'health':state.health,
                       'speed_q8':state.speed_q8,'x_q8':state.x_q8,'position_q8':state.position_q8,
                       'run_seed':state.run_seed,'run_ticks':state.run_ticks,'biome':state.biome,
                       'skin':state.save.skin,'motion':state.save.motion,'units':state.save.units}}
        for symbol,count in (('render_profile',5),('scenery_profile',8)):
            result[symbol]=list((C.c_uint32*count).from_buffer_copy(e.read_ram(find_symbol(a.elf,symbol),count*4)))
        print(json.dumps(result),flush=True);assert 28<=hz<=31.5,(name,hz)
        return result
    def garage_vehicle(vehicle):
        assert d.game().mode==3
        while d.game().garage_tab:d.press('l')
        while d.game().garage_filter:d.press('select')
        d.menu(0)
        for _ in range(VEHICLE_COUNT):
            if d.game().selection==vehicle:return
            d.press('right')
        raise AssertionError('Garage could not browse every vehicle')
    def capture_water_contact(biome,name):
        # Choose a photograph from the real drive when a deterministic water
        # anchor is nearby. The read-only world hash is used only to time the
        # screenshot; all travel remains joypad-controlled at normal speed.
        for _ in range(180):
            state=d.game();position=state.position_q8//256
            optical_scale=(205,307,384)[state.save.motion]
            optical=(position//128)*optical_scale+(position%128)*optical_scale//128
            boat=biome==1
            slot,salt=(408,0xb0a7) if boat else (520,0xd0c4)
            low,high=(350,780) if boat else (230,340)
            for n in range((optical+low-240)//slot,(optical+high)//slot+1):
                h=lib.road_landmark(state.run_seed^salt,n,-1)
                anchor=n*slot+(184+((h>>10)%53) if boat else 52);z=anchor-optical
                if not low<=z<=high:continue
                if boat:
                    offshore=190+((h>>5)%611)
                    # Offshore placement is intentionally broad. Photograph a
                    # visible hull, not a hash whose boat lies beyond the LCD.
                    if offshore*(276*190//(z+190))//256>85:continue
                    dock_n=(anchor-52+260)//520
                    dock_hash=lib.road_landmark(state.run_seed^0xd0c4,dock_n,-1)
                    if (dock_hash&3)==0 and abs(anchor-(dock_n*520+52))<58:continue
                if (h&3)!=0 and low<=z<=high:
                    shot(name,('r',));after=d.game()
                    after_position=after.position_q8//256
                    after_optical=(after_position//128)*optical_scale+(after_position%128)*optical_scale//128
                    water_captures.append({'object':'upright profile boat' if boat else 'shore-connected dock',
                        'screenshot':name+'.png','biome':biome,'frame':after.frame,
                        'anchor':anchor,'world_z_ahead':anchor-after_optical,
                        'shore_side':'left','anchor_slot_spacing':slot,
                        'offshore_world_offset':offshore if boat else None,
                        'dock_sections':None if boat else 1+((h>>8)&1),
                        'orientation':'upright transverse hull' if boat else 'one or two end-joined sections perpendicular to road'})
                    return
            d.autopilot(2,('r',));assert d.game().mode==1
        raise AssertionError('No nearby water object observed in the generated drive')
    for biome,name in enumerate(BIOMES):
        depart(biome);d.menu(0);d.press('a');d.autopilot(165,('r',))
        assert d.game().mode==1 and d.game().biome==biome
        shot('scenery-'+name,('r',));rates.append(rate(name))
        if biome in (1,5):capture_water_contact(biome,'grounded-'+('boat' if biome==1 else 'pier'))
        finish()
    d.menu(1);d.press('a');assert d.game().mode==3
    for vehicle in vehicle_catalogue:
        v=vehicle['id'];garage_vehicle(v);shot('vehicle-'+str(v).zfill(2))
        if v<len(OLD_VEHICLES):
            shutil.copyfile(a.output/('vehicle-'+str(v).zfill(2)+'.png'),
                            a.output/('vehicle-'+OLD_VEHICLES[v]+'.png'))
    garage_vehicle(0);d.press('r');assert d.game().garage_tab==1
    for cursor,field,cost in ((1,'engine',60),(2,'grip',54),(3,'armor',57),(4,'recovery',63)):
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
    # Observe exhaustion, the held-button latch, deliberate Slow recovery and
    # reactivation in the real ARM cartridge. All driving uses normal buttons.
    d.menu(1);d.press('a');garage_vehicle(2);d.menu(1);d.press('a');d.press('b')
    depart(2);d.menu(0);d.press('a');d.autopilot(100,('r',))
    full=8*30*256;assert d.game().turbo_charge_q8==full
    started=d.game().frame;lowest=full
    while d.game().mode==1 and d.game().frame-started<300:
        d.autopilot(2,('r','up'));state=d.game()
        lowest=min(lowest,state.turbo_charge_q8)
        if state.turbo_latched:break
    assert d.game().mode==1 and d.game().turbo_latched
    drain_ticks=state.frame-started
    # The final funded boost tick consumes the last Q8 unit; the following
    # update disables acceleration while preserving the held-button latch.
    d.autopilot(2,('r','up'))
    assert not d.game().turbo and lowest==0,(lowest,d.game().turbo)
    shot('turbo-empty',('r','up'))
    d.autopilot(60,('r','up'));assert d.game().mode==1
    assert d.game().turbo_latched and not d.game().turbo
    held_charge=d.game().turbo_charge_q8
    before_slow=d.game().turbo_charge_q8
    slow_started=d.game().frame
    recovery_goal=min(full,before_slow+full//10)
    slow_trace=[]
    while d.game().mode==1 and d.game().frame-slow_started<600:
        d.autopilot(2,('r','down'));state=d.game()
        if not slow_trace or state.frame-slow_started-slow_trace[-1]['ticks']>=30:
            slow_trace.append({'ticks':state.frame-slow_started,'charge':state.turbo_charge_q8,
                'health':state.health,'speed':state.speed_q8,'x':state.x_q8,
                'cooldown':state.damage_cooldown,'crash':state.crash_timer,'source':state.turbo_source})
        if (state.frame-slow_started>=90 and state.turbo_charge_q8>=recovery_goal
                and state.turbo_source==4):break # TS_SLOW, observed read only.
    assert d.game().mode==1
    charged=d.game().turbo_charge_q8
    assert charged>before_slow and charged>=full//10,{'before':before_slow,'after':charged,'trace':slow_trace}
    assert not d.game().turbo_latched and d.game().turbo_recharging
    shot('turbo-recharging',('r','down'))
    d.autopilot(2,('r','up'));assert d.game().mode==1 and d.game().turbo
    assert d.game().turbo_charge_q8<charged
    shot('turbo-rearmed',('r','up'))
    turbo_checks.append({'check':'finite turbo depletion, held-button latch and Slow recovery',
        'full_q8':full,'minimum_observed_q8':lowest,'drain_ticks':drain_ticks,
        'charge_after_held_empty_q8':held_charge,'charge_after_precision_q8':charged,
        'precision_recovery_ticks':state.frame-slow_started,'recovery_trace':slow_trace,
        'boost_reactivated':True,'controls':'R + Up; keep held; release Up for R + Down; R + Up again'})
    finish()
    # Demonstrate a committed slide from a fresh opening position instead of
    # steering across traffic after the preceding long boost/recovery drive.
    # This spends genuine fuel and reaches drift speed before turning inward.
    d.menu(0);d.press('a');d.ticks(40,('r','up'));state=d.game()
    before_drift=state.turbo_charge_q8
    drift_start={'frame':state.frame,'health':state.health,'x_q8':state.x_q8,
                 'speed_q8':state.speed_q8,'charge_q8':before_drift}
    d.ticks(27,('r','b','right'));state=d.game()
    drift_end={'frame':state.frame,'health':state.health,'x_q8':state.x_q8,
               'speed_q8':state.speed_q8,'charge_q8':state.turbo_charge_q8,
               'drift_hold':state.drift_hold,'source':state.turbo_source}
    print('Committed drift '+json.dumps({'before':drift_start,'after':drift_end}),flush=True)
    assert state.mode==1 and state.drifting and state.drift_hold>=23,drift_end
    assert state.turbo_source==2 and state.turbo_charge_q8>before_drift,drift_end # TS_DRIFT
    shot('turbo-drift-recharge',('r','b','right'))
    turbo_checks.append({'check':'deliberate drift replenishes used turbo',
        'before_q8':before_drift,'after_q8':state.turbo_charge_q8,
        'drift_hold_ticks':state.drift_hold,'combo_q8':state.combo_q8,
        'fresh_run':True,'trace':{'before':drift_start,'after':drift_end}})
    finish()
    # Every silhouette is driven. Six established dashboard families get all
    # three skins; the native UI suite covers all 31 x3 combinations.
    families=((0,0,0,'classic'),(1,1,5,'digital'),(2,2,7,'utility'),
              (3,0,1,'bike'),(7,1,5,'luxury'),(4,2,4,'race'))
    family_by_id={v:(skin,biome,name) for v,skin,biome,name in families}
    cases=[]
    for info in vehicle_catalogue:
        vehicle=info['id']
        skin,biome,family=family_by_id.get(vehicle,(vehicle%3,(vehicle*5)%LOCATION_COUNT,'vehicle-'+str(vehicle).zfill(2)))
        cases.append((vehicle,skin,biome,family,True))
    cases.extend((v,skin,biome,family,False) for v,primary,biome,family in families
                 for skin in range(3) if skin!=primary)
    assert len(cases)==43
    for vehicle,skin,biome,family,primary in cases:
        d.menu(5);d.press('a');assert d.game().mode==9;d.menu(4)
        while d.game().save.skin!=skin:d.press('right')
        d.menu(7)
        target_units=1 if family=='bike' else 0
        while d.game().save.units!=target_units:d.press('right')
        if primary:shot('settings-'+family)
        d.press('b')
        d.menu(1);d.press('a');garage_vehicle(vehicle);d.menu(1);d.press('a');d.press('b')
        assert d.game().save.selected_vehicle==vehicle
        depart(biome);d.menu(0);d.press('a')
        keys=('r','a') if vehicle_catalogue[vehicle]['bike'] else ('r',)
        d.autopilot(150,keys);assert d.game().mode==1
        image_name='dashboard-vehicle-'+str(vehicle).zfill(2) if primary else 'dashboard-'+family+'-skin'+str(skin)
        shot(image_name,keys);dashboard_images.append((image_name,vehicle_catalogue[vehicle]['name']+' / skin '+str(skin+1)))
        if primary and vehicle in family_by_id:
            for alias in ('dashboard-'+family,'dashboard-'+family+'-skin'+str(skin)):
                shutil.copyfile(a.output/(image_name+'.png'),a.output/(alias+'.png'));shots.append(alias+'.png')
        if vehicle==3:
            assert d.game().wheelie
            if primary:shot('motorcycle-wheelie',keys)
            if primary:
                # Begin the recharge proof near the opening instead of first
                # boosting for 200 ticks into traffic. The same real controls,
                # 30-tick wheelie threshold and positive fuel delta are required.
                finish();d.menu(0);d.press('a')
                d.autopilot(40,('r','up'));assert d.game().mode==1
                wheelie_before=d.game().turbo_charge_q8
                wheelie_peak=0;wheelie_source=False;wheelie_trace=[]
                for _ in range(60):
                    d.autopilot(2,('r','a'));state=d.game()
                    wheelie_peak=max(wheelie_peak,state.wheelie_hold)
                    wheelie_source|=state.turbo_source==3
                    if not wheelie_trace or state.run_ticks-wheelie_trace[-1]['ticks']>=15:
                        wheelie_trace.append({'ticks':state.run_ticks,'health':state.health,
                            'speed_q8':state.speed_q8,'hold':state.wheelie_hold,
                            'source':state.turbo_source,'charge_q8':state.turbo_charge_q8,
                            'cooldown':state.damage_cooldown,'crash':state.crash_timer})
                    if (state.mode==1 and state.wheelie and state.wheelie_hold>=30
                            and state.turbo_source==3 and state.turbo_charge_q8>wheelie_before):break
                    if state.mode!=1:break
                assert state.mode==1 and state.wheelie,{'trace':wheelie_trace,'mode':state.mode}
                assert state.turbo_charge_q8>wheelie_before and wheelie_source and wheelie_peak>=30,{'trace':wheelie_trace}
                shot('turbo-wheelie-recharge',('r','a'))
                turbo_checks.append({'check':'sustained wheelie replenishes used turbo',
                    'before_q8':wheelie_before,'after_q8':state.turbo_charge_q8,
                    'wheelie_hold_ticks':state.wheelie_hold,'peak_wheelie_hold_ticks':wheelie_peak,
                    'wheelie_recharge_observed':wheelie_source,'combo_q8':state.combo_q8,
                    'fresh_run':True,'trace':wheelie_trace})
        measured=rate(family,keys);measured.update(vehicle=vehicle,skin=skin,units=target_units)
        measured.update(primary=primary,novelty=vehicle>=25,image=image_name+'.png')
        cluster_rates.append(measured);finish()
    g=d.game();g.save.checksum=0;expected=bytes(g.save)
    assert any(item['frame']<5250 for item in preview_checks)
    assert any(item['frame']>10500 for item in preview_checks)
    assert {item['period'] for item in preview_checks if item['biome']==8}=={'early','late'}
(a.output/'expected-advanced-profile.bin').write_bytes(expected)
subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
    '--elf',str(a.elf),'--save-dir',str(fixture),
    '--expected',str(a.output/'expected-advanced-profile.bin'),
    '--output',str(a.output/'advanced-save-reloaded.png')],check=True)
shots.append('advanced-save-reloaded.png')
result={'rom':inspect(a.rom),'source':'documented native-generated battery fixture; vehicles, departures, cash, mastery and career pre-seeded for review',
        'durable_turbo_fixture':{'nomad':{'engine':2,'grip':4,'armor':6,'chassis_tune':2},
                                 'kestrel':{'armor':6,'chassis_tune':2}},
        'ram_access':'read only; joypad input only; no save-state restore',
        'screenshots':shots,'biome_update_rates':rates,'cluster_update_rates':cluster_rates,
        'fixture_funded_actions':actions,'finite_turbo_checks':turbo_checks,
        'water_contact_captures':water_captures,
        'dashboard_vehicle_skin_combinations':len(cluster_rates),'fresh_process_reload':True,
        'vehicles_reviewed':len(vehicle_catalogue),'locations_reviewed':len(location_catalogue),
        'vehicle_catalogue':vehicle_catalogue,'location_catalogue':location_catalogue,
        'novelty_driving_cases':sum(item['novelty'] and item['primary'] for item in cluster_rates),
        'preview_checks':preview_checks,'preview_stable':True,
        'serialized_save_bytes':500}
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
labels=[info['name'] for info in location_catalogue]
sheet(list(zip(('scenery-'+b for b in BIOMES),labels))+[('motorcycle-wheelie','Kestrel motorcycle')],'Locations.png')
sheet([('vehicle-'+str(v['id']).zfill(2),v['name']) for v in vehicle_catalogue],'Vehicles.png')
sheet([(name,label) for name,label in dashboard_images if name.startswith('dashboard-vehicle-')],'Dashboards.png',True)
sheet(dashboard_images,'Dashboard-Skins.png',True)
