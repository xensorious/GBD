#!/usr/bin/env python3
"""Joypad-only handling/patrol/ticket evidence from the actual DS cartridge.

A labelled battery profile supplies durable parts, not run progress, positions,
traffic or scores. The runner only reads emulator RAM and never restores states.
"""
from __future__ import annotations
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
from emulator import Emulator,ROOT
from game_snapshot import Game,Save,Traffic,find_symbol,symbol_size,verify_layout
from inspect_rom import inspect
from fixtures import write_battery,encode_profile
from playthrough import DriveThrough


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
    parser.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
    parser.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
    a=parser.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    verify_layout(ROOT)
    template=a.output/'save'/'Coastline.dsv';assert template.exists()
    library=ROOT/'build'/'libdrive-handling-qa.so'
    subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c11','-I',str(ROOT/'src'),'-I',str(ROOT/'build'),
        str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),str(ROOT/'src'/'traffic.c'),
        '-o',str(library)],check=True)
    lib=C.CDLL(str(library));lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
    lib.game_make_save.argtypes=[C.POINTER(Game),C.POINTER(Save)]
    lib.game_save_valid.argtypes=[C.POINTER(Save)]
    for name in ('traffic_body_contact','traffic_body_gap'):
        getattr(lib,name).argtypes=[C.POINTER(Game),C.POINTER(Traffic)]
        getattr(lib,name).restype=C.c_int
    profile=Game();lib.game_init(C.byref(profile),0x48414e44)
    profile.save.mastery_xp[0]=10000;profile.save.career_xp=22000
    profile.save.armor[0]=6;profile.save.chassis_tune[0]=2
    profile.save.perks[0]=3
    save_dir=a.output/'handling-fixture-save';shutil.rmtree(save_dir,ignore_errors=True)
    write_battery(template,save_dir/'Coastline.dsv',encode_profile(lib,profile))
    address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
    report={'rom':inspect(a.rom),'source':'native-generated battery fixture: Pacer armor6, touring chassis, Recovery3; no engine/grip/Focus modifications',
            'inputs':'joypad only','ram_access':'read only; no emulator checkpoints or state restores',
            'checks':[],'screenshots':[]}
    with Emulator(a.rom,save_dir) as e:
        e.run(180);p=DriveThrough(e,address,a.output);assert p.game().has_save
        def shot(name,keys=()):
            p.shot(name,keys);report['screenshots'].append(name+'.png')
        def start():
            assert p.game().mode==0
            p.menu(0);p.press('a');assert p.game().mode==1
        def bank():
            p.press('start');assert p.game().mode==7
            p.menu(1);p.press('a');assert p.game().mode==2 and p.game().end_reason==3
        def snapshot(g):
            return {'frame':g.frame,'run_ticks':g.run_ticks,'health':g.health,
                    'x_q8':g.x_q8,'vx_q8':g.vx_q8,'yaw':g.yaw,
                    'speed_q8':g.speed_q8,'police_ticks':g.police_ticks,
                    'police_pressure':g.police_pressure,'end_reason':g.end_reason,
                    'combo_q8':g.combo_q8,'speed_hold':g.speed_hold}
        # The launch grace is measured from real run updates. No input is
        # needed; the player may read the initial dashboard without a ticket.
        start()
        while p.game().run_ticks<90:p.ticks(1)
        assert p.game().mode==1 and p.game().police_pressure==0
        report['launch_grace_passed']=True
        p.ticks(35);g=p.game()
        assert g.mode==1 and 30<=g.police_ticks<=38 and 0<g.police_pressure<100
        shot('handling-police-pressure');g=p.game()
        before=snapshot(g);p.press('start');assert p.game().mode==7
        frozen=p.game();p.ticks(180,('r','up','right'));g=p.game()
        assert (g.run_ticks,g.position_q8,g.police_ticks,g.police_pressure)==(
            frozen.run_ticks,frozen.position_q8,frozen.police_ticks,frozen.police_pressure)
        report['pause_frozen']=True;p.press('start');assert p.game().mode==1
        for _ in range(100):
            if p.game().mode!=1:break
            p.ticks(1)
        g=p.game();assert g.mode==2 and g.end_reason==1 and g.health==100
        assert g.police_ticks==90 and g.run.banked and g.save.total_runs==1
        assert g.run.total_points==sum(g.run.points)
        assert g.save.cash==g.run.total_cash
        shot('handling-stopped-ticket');cash=g.save.cash;xp=g.save.career_xp
        p.ticks(30);assert p.game().save.cash==cash and p.game().save.career_xp==xp
        report['stopped_catch_passed']=True;report['ticket_banked_once']=True
        report['checks'].append({'check':'launch grace, stopped patrol, pause freeze and bank-once ticket',
            'before_pause':before,'caught':snapshot(g),'points':g.run.total_points,'cash_awarded':g.run.total_cash})
        p.press('a');assert p.game().mode==0
        # Precision/Slow driving is explicitly safe from the stopped-car rule.
        start();p.autopilot(240,('r','down'));g=p.game()
        assert g.mode==1 and g.speed_q8>2*256 and g.police_ticks==0
        report['slow_mode_safe']=True
        report['checks'].append({'check':'eight seconds of Slow driving remains active with no patrol pressure',**snapshot(g)})
        # A short handbrake turn should visibly yaw without carrying runaway
        # lateral momentum. Opposite steering catches it; release settles it.
        before=snapshot(g);p.ticks(10,('r','b','right'));g=p.game()
        assert g.mode==1 and g.drifting and g.vx_q8>0 and g.yaw>35
        shot('handling-controlled-drift',('r','b','right'));turned=snapshot(p.game())
        p.ticks(2,('r','b','left'));g=p.game()
        assert g.mode==1 and g.drifting and g.vx_q8<0
        counter=snapshot(g);p.ticks(6,('r',));g=p.game()
        assert g.mode==1 and not g.drifting and abs(g.vx_q8)<256//4
        report['controlled_drift_passed']=True;shot('handling-drift-recovery',('r',))
        report['checks'].append({'check':'controlled turn-in, two-tick countersteer and six-tick recovery',
            'before':before,'drifting':turned,'countersteer':counter,'recovered':snapshot(g)})
        bank();g=p.game();assert g.run.total_cash>0
        shot('handling-banked-ticket');p.press('a')
        # Observe qualified full-speed intervals, not wall-clock acceleration
        # time. The first quarter requires90 uninterrupted qualified updates.
        start();timeline=[];last_combo=256;quarter_seen=False;longest=0
        for _ in range(900):
            p.autopilot(2,('r',));g=p.game();assert g.mode==1
            assert (g.combo_q8-256)%64==0 and 256<=g.combo_q8<=1280
            longest=max(longest,g.speed_hold)
            if g.speed_hold<90:assert g.combo_q8==256
            if g.combo_q8!=last_combo:
                timeline.append({'frame':g.frame,'qualified_ticks':g.speed_hold,'combo_q8':g.combo_q8})
                last_combo=g.combo_q8
            if g.combo_q8>=320 and not quarter_seen:
                assert g.speed_hold>=90;quarter_seen=True
                shot('handling-quarter-combo',('r',))
            if g.speed_hold>=180:break
        assert quarter_seen and longest>=180 and g.combo_q8>=384
        report['quarter_combo_passed']=True
        report['checks'].append({'check':'quarter-step full-speed combo with slower build',
            'observed_timeline':timeline,'longest_qualified_ticks':longest,'full_cap_qualified_ticks':1440})
        # Deliberately pass close beside real generated traffic. Per-row body
        # geometry is read from a native copy of the same cartridge rules;
        # traffic.c independently verifies it against packed sprite pixels.
        # Neither helper can mutate the emulated Game object.
        witnesses={};target_serial=None;target_slot=None;target_x=None
        for _ in range(900):
            old=p.game();assert old.mode==1
            chosen=next((i for i,t in enumerate(old.traffic)
                         if i==target_slot and t.active and t.serial==target_serial),None)
            if chosen is None or old.traffic[chosen].z_q8-old.position_q8 < -25*256:
                choices=[]
                for i,t in enumerate(old.traffic):
                    dz=(t.z_q8-old.position_q8)/256
                    if not t.active or not 90<dz<700:continue
                    for offset in (-25,25):
                        lane=t.x_q8/256+offset
                        if not -75<lane<75:continue
                        blocked=any(other.active and j!=i and
                            abs(other.z_q8-t.z_q8)<100*256 and abs(other.x_q8/256-lane)<30
                            for j,other in enumerate(old.traffic))
                        if not blocked:choices.append((dz+abs(lane-old.x_q8/256),i,offset))
                if not choices:
                    p.autopilot(2,('r',));continue
                _,chosen,offset=min(choices)
                target_serial=old.traffic[chosen].serial;target_slot=chosen;target_x=offset
            traffic=old.traffic[chosen];old_dz=traffic.z_q8-old.position_q8
            p.ticks(1,('r',)+p.steer(traffic.x_q8/256+target_x));g=p.game()
            car=g.traffic[chosen];new_dz=car.z_q8-g.position_q8
            if car.active and car.serial==target_serial and old_dz>0>=new_dz:
                gap=lib.traffic_body_gap(C.byref(g),C.byref(car))
                contact=lib.traffic_body_contact(C.byref(g),C.byref(car))
                if 0<=gap<=18 and not contact and g.health==old.health and not g.damage_cooldown:
                    e.screenshot(a.output/'handling-clear-pass.png')
                    report['screenshots'].append('handling-clear-pass.png')
                    witnesses['clear_pass']={'passed':True,'health_before':old.health,'health_after':g.health,
                        'serial':int(car.serial),'horizontal_clearance_pixels_or_overlap_minus_one':gap,
                        'opaque_body_contact':False,
                        'relative_depth_q8':new_dz,'x_offset_q8':car.x_q8-g.x_q8,
                        'near_miss_awarded':bool(car.near_awarded),'frame':g.frame,
                        'screenshot':'handling-clear-pass.png'}
                    break
                target_serial=None
        assert 'clear_pass' in witnesses,'Could not observe an unharmed close pass through ordinary traffic'
        # Then approach the rear of an ordinary traffic car at its centre.
        # Capture the first real overlap; the separate rules suite checks both
        # rear clearance and cut-ins over all car/traffic silhouettes and yaw.
        target_serial=None;target_slot=None
        for _ in range(900):
            old=p.game();assert old.mode==1
            chosen=next((i for i,t in enumerate(old.traffic)
                         if i==target_slot and t.active and t.serial==target_serial and t.z_q8>old.position_q8),None)
            if chosen is None:
                candidates=[((t.z_q8-old.position_q8),i) for i,t in enumerate(old.traffic)
                            if t.active and 60*256<t.z_q8-old.position_q8<700*256 and abs(t.x_q8)<76*256]
                if not candidates:p.autopilot(2,('r',));continue
                _,chosen=min(candidates);target_serial=old.traffic[chosen].serial;target_slot=chosen
            p.ticks(1,('r',)+p.steer(old.traffic[chosen].x_q8/256));g=p.game()
            if not old.damage_cooldown and g.damage_cooldown and g.health<old.health:
                contacts=[]
                for i,t in enumerate(g.traffic):
                    if t.active and lib.traffic_body_contact(C.byref(g),C.byref(t)):
                        contacts.append((i,lib.traffic_body_gap(C.byref(g),C.byref(t))))
                if not contacts:continue
                i,gap=contacts[0];car=g.traffic[i]
                e.screenshot(a.output/'handling-real-contact.png')
                report['screenshots'].append('handling-real-contact.png')
                witnesses['real_contact']={'passed':True,'health_before':old.health,'health_after':g.health,
                    'serial':int(car.serial),'horizontal_clearance_pixels_or_overlap_minus_one':gap,
                    'opaque_body_contact':True,
                    'relative_depth_q8':car.z_q8-g.position_q8,'x_offset_q8':car.x_q8-g.x_q8,
                    'frame':g.frame,'screenshot':'handling-real-contact.png'}
                break
        assert 'real_contact' in witnesses,'Could not observe a genuine visible chassis contact'
        witnesses['clearance_convention']='nonnegative: exact horizontal pixel clearance; -1: overlapping opaque pixels, not penetration depth'
        report['collision_witnesses']=witnesses
        bank();p.press('a');p.ticks(10)
        assert p.game().has_save and p.game().save_ok
    (a.output/'handling-results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2),flush=True)

if __name__=='__main__':main()
