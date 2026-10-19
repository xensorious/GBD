#!/usr/bin/env python3
"""Actual-cartridge fleet gates, per-vehicle styling and Dispatch evidence.

Labelled battery fixtures supply boundary ownership/cash/mastery and two prior
Dispatch completions. All purchases, one new completion, banking and navigation
use joypad input. Emulator RAM is read only; no checkpoints are restored.
"""
from __future__ import annotations
import argparse
import ctypes as C
import json
from pathlib import Path
import shutil
import subprocess
import sys
from emulator import Emulator,ROOT
from fixtures import encode_profile,write_battery,assert_compact_battery
from game_snapshot import (Game,DispatchContract,catalogue,find_symbol,symbol_size,
                           verify_layout,VEHICLE_COUNT,LOCATION_COUNT)
from inspect_rom import inspect
from playthrough import DriveThrough

GATE_CASES={
    0:([0],10,None),
    1:([0],12,10),
    2:([0,10,11,3],14,2),
    3:([0,10,11,3,2,25,26],17,12),
    4:([0,10,11,3,2,25,26,12,6],21,13),
}

def arguments():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
    p.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
    p.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
    p.add_argument('--case',choices=tuple('gate-'+str(i) for i in range(5))+('style','contracts'))
    p.add_argument('--library',type=Path,default=ROOT/'build'/'libexpansion-qa.so')
    phase=p.add_mutually_exclusive_group()
    phase.add_argument('--cases-only',action='store_true',help='Run the seven independent cases; aggregate after the scenery tour')
    phase.add_argument('--aggregate-only',action='store_true',help='Verify and aggregate completed same-ROM cases with scenery evidence')
    args=p.parse_args()
    if args.case and (args.cases_only or args.aggregate_only):p.error('--case cannot be combined with a phase option')
    return args

def library(path):
    lib=C.CDLL(str(path.resolve()))
    lib.game_init.argtypes=[C.POINTER(Game),C.c_uint32]
    lib.game_contract_offer.argtypes=[C.POINTER(Game),C.c_int,C.POINTER(DispatchContract)]
    lib.game_can_vehicle.argtypes=[C.POINTER(Game),C.c_int]
    lib.game_can_location.argtypes=[C.POINTER(Game),C.c_int]
    lib.game_challenge_progress.argtypes=[C.POINTER(Game),C.c_int,C.POINTER(C.c_int)]
    return lib

def profile_for(lib,case):
    g=Game();lib.game_init(C.byref(g),0x45585041)
    g.save.cash=1000000
    if case.startswith('gate-'):
        ids,_,_=GATE_CASES[int(case[-1])];g.save.owned=sum(1<<i for i in ids)
    else:
        g.save.owned=(1<<VEHICLE_COUNT)-1;g.save.unlocked_locations=(1<<LOCATION_COUNT)-1
        g.save.paints=63;g.save.career_xp=50000
        for i in range(VEHICLE_COUNT):g.save.mastery_xp[i]=10000
        g.save.best_near=20;g.save.best_drift=3000;g.save.best_distance=6000
        g.save.best_speed_hold=240;g.save.best_wheelie=1000
    if case=='contracts':
        g.save.owned&=~(1<<28);g.save.unlocked_locations&=~(1<<10)
        g.save.contracts_completed=2;g.save.selected_vehicle=2;g.save.selected_location=2
        g.save.engine[2]=2;g.save.grip[2]=4;g.save.armor[2]=6
        g.save.chassis_tune[2]=2;g.save.perks[0]=3
    return g

def child_case(a):
    lib=library(a.library);vehicle_info,_=catalogue(lib)
    profile=profile_for(lib,a.case)
    directory=a.output/('expansion-'+a.case+'-save');shutil.rmtree(directory,ignore_errors=True)
    write_battery(a.output/'save'/'Coastline.dsv',directory/'Coastline.dsv',encode_profile(lib,profile))
    address=find_symbol(a.elf,'game');assert symbol_size(a.elf,'game')==C.sizeof(Game)
    result={'case':a.case,'rom':inspect(a.rom),'inputs':'joypad only; RAM read only',
            'screenshots':[],'serialized_save_bytes':500,'fixture_source':'documented native-generated boundary profile'}
    with Emulator(a.rom,directory) as e:
        e.run(180);p=DriveThrough(e,address,a.output);assert p.game().has_save
        def shot(name,keys=()):
            p.shot(name,keys);result['screenshots'].append(name+'.png')
        def browse(vehicle):
            assert p.game().mode==3
            tab(0)
            p.menu(0)
            for _ in range(VEHICLE_COUNT):
                if p.game().selection==vehicle:return
                p.press('right')
            raise AssertionError('Garage could not select target vehicle')
        def tab(number):
            while p.game().garage_tab!=number:p.press('r')
        def dealer_vehicle(vehicle):
            tab(3)
            cls=vehicle_info[vehicle]['class_id']
            while p.game().garage_filter!=cls:p.press('right')
            ids=[v['id'] for v in vehicle_info if v['class_id']==cls]
            p.menu(ids.index(vehicle)+1)
        def buy(vehicle):
            dealer_vehicle(vehicle);p.press('a')
        def open_garage():
            p.menu(1);p.press('a');assert p.game().mode==3
        def apply_style(vehicle,livery,wheels):
            browse(vehicle);tab(2);p.menu(2)
            while p.game().livery_choice!=livery:p.press('right')
            p.menu(3)
            while p.game().wheel_choice!=wheels:p.press('right')
        def bank():
            p.press('start');assert p.game().mode==7
            p.menu(1);p.press('a');assert p.game().mode==2 and p.game().end_reason==3
        if a.case.startswith('gate-'):
            class_id=int(a.case[-1]);owned,target,prerequisite=GATE_CASES[class_id]
            open_garage();dealer_vehicle(target)
            before=p.game();cash=before.save.cash;p.press('a');after=p.game()
            if prerequisite is not None:
                assert not after.save.owned&(1<<target) and after.save.cash==cash
                shot('expansion-class-gate-'+str(class_id))
                if class_id==4:
                    shutil.copyfile(a.output/('expansion-class-gate-'+str(class_id)+'.png'),a.output/'expansion-class-gate.png')
                    result['screenshots'].append('expansion-class-gate.png')
                buy(prerequisite);g=p.game()
                assert g.save.owned&(1<<prerequisite)
                assert g.save.cash==cash-vehicle_info[prerequisite]['price']
                assert g.save.owned.bit_count()==len(owned)+1
                cash=g.save.cash;buy(target);after=p.game()
            assert after.save.owned&(1<<target)
            assert after.save.cash==cash-vehicle_info[target]['price']
            assert after.save.selected_vehicle==target
            shot('expansion-class-open-'+str(class_id))
            result.update(passed=True,class_id=class_id,owned_before=len(owned),
                          prerequisite_purchase=prerequisite,target_vehicle=target,
                          owned_after=after.save.owned.bit_count())
            p.press('b')
        elif a.case=='style':
            open_garage();browse(0);tab(2);cash=p.game().save.cash
            apply_style(0,2,3);g=p.game()
            assert (g.save.livery[0],g.save.wheels[0],g.save.cash)==(0,0,cash)
            shot('expansion-style-preview')
            p.menu(5);p.press('a');g=p.game()
            assert (g.save.livery[0],g.save.wheels[0])==(2,3)
            assert g.save.cash==cash-320 and g.save.livery_owned[0]&(1<<2) and g.save.wheels_owned[0]&(1<<3)
            cash=g.save.cash;p.press('a');assert p.game().save.cash==cash
            shot('expansion-style')
            apply_style(30,4,1);g=p.game()
            assert (g.save.livery[30],g.save.wheels[30])==(0,0)
            assert (g.save.livery[0],g.save.wheels[0])==(2,3)
            p.menu(5);p.press('a');g=p.game()
            assert g.save.cash==cash-555 and (g.save.livery[30],g.save.wheels[30])==(4,1)
            shot('expansion-style-horse')
            browse(30);tab(1);p.menu(6);cash=p.game().save.cash;p.press('a');g=p.game()
            assert g.save.recovery[30]==1 and g.save.recovery[0]==0 and g.save.cash==cash-88
            shot('expansion-recovery-part')
            result.update(passed=True,preview_did_not_purchase=True,reapply_free=True,
                          per_vehicle_isolation=True,last_vehicle_tail_preserved=True,
                          livery_choices=5,hardware_choices=4,recovery_part_purchased=True)
            p.press('b')
        else:
            g=p.game();assert g.save.contracts_completed==2
            assert not lib.game_can_vehicle(C.byref(g),28) and not lib.game_can_location(C.byref(g),10)
            p.menu(5);p.press('a');assert p.game().mode==10
            offers=[]
            for slot in range(3):
                offer=DispatchContract();lib.game_contract_offer(C.byref(p.game()),slot,C.byref(offer))
                offers.append(offer)
            chosen=min((i for i in range(3) if offers[i].kind in (1,2,4)),
                       key=lambda i:{1:0,2:1,4:2}[offers[i].kind])
            p.menu(chosen);shot('expansion-contracts');p.press('a')
            assert p.game().mode==0 and p.game().save.dispatch_choice==chosen
            cash=p.game().save.cash;p.menu(0);p.press('a');g=p.game();assert g.mode==1
            active=g.active_contract
            assert (active.kind,active.goal,active.reward)==(offers[chosen].kind,offers[chosen].goal,offers[chosen].reward)
            direction=1;trace=[]
            for _ in range(1800):
                g=p.game();assert g.mode==1
                if g.contract_progress>=active.goal:break
                if active.kind==1:
                    lane=p.safe_lane(g);x=g.x_q8/256
                    if x<lane-8:direction=1
                    elif x>lane+8:direction=-1
                    p.ticks(2,('r','down','b','right' if direction>0 else 'left'))
                else:p.autopilot(2,('r','down') if active.kind==2 else ('r',))
                if len(trace)<20 and p.game().run_ticks%90<2:
                    state=p.game();trace.append({'ticks':state.run_ticks,'progress':state.contract_progress,'health':state.health})
            g=p.game();assert g.mode==1 and g.contract_progress>=active.goal
            assert g.save.cash==cash and g.save.contracts_completed==2 and not g.contract_complete
            shot('expansion-contract-progress',('r','down'))
            bank();g=p.game()
            assert g.contract_complete and g.contract_cash==active.reward and g.save.contracts_completed==3
            assert g.save.cash==cash+g.run.total_cash
            assert g.run.total_cash==g.run.total_points//60+g.run.count[6]*5+g.run.cash_bonus+g.contract_cash
            assert g.save.unlocked_locations&(1<<10) and lib.game_can_vehicle(C.byref(g),28)
            shot('expansion-contract-ticket');paid=p.game().save.cash
            p.ticks(40);assert p.game().save.cash==paid and p.game().save.contracts_completed==3
            p.press('a');open_garage();buy(28);assert p.game().save.owned&(1<<28)
            shot('expansion-contract-unlocked-segway');p.press('b')
            p.menu(2);p.press('a');assert p.game().mode==5;p.menu(7);shot('expansion-challenges')
            observed=[]
            for index in (5,6,7):
                goal=C.c_int();value=lib.game_challenge_progress(C.byref(p.game()),index,C.byref(goal))
                assert value==goal.value;observed.append({'index':index,'value':value,'goal':goal.value})
            p.press('b');p.menu(5);p.press('a');p.menu(3);p.press('a')
            assert p.game().save.dispatch_choice==3
            p.menu(0);p.press('a');p.autopilot(20,('r','down'));assert p.game().active_contract.kind==5
            bank();assert not p.game().contract_cash and p.game().save.contracts_completed==3
            p.press('a')
            result.update(passed=True,fixture_prior_completions=2,actual_completions=1,
                          selected_slot=chosen,kind=int(active.kind),goal=int(active.goal),reward=int(active.reward),
                          banked_once=True,optional_free_drive=True,dispatch_unlocks_verified=True,
                          new_challenges=observed,trace=trace)
        p.ticks(12);g=p.game();assert g.has_save and g.save_ok
        g.save.checksum=0;expected=bytes(g.save)
    result.update(assert_compact_battery(directory/'Coastline.dsv'))
    expected_path=a.output/('expected-expansion-'+a.case+'.bin');expected_path.write_bytes(expected)
    subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(a.rom),
        '--elf',str(a.elf),'--save-dir',str(directory),'--expected',str(expected_path),
        '--output',str(a.output/('expansion-'+a.case+'-reloaded.png'))],check=True)
    result['fresh_process_reload']=True
    (a.output/('expansion-'+a.case+'-results.json')).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)

def main():
    a=arguments();a.output.mkdir(parents=True,exist_ok=True);verify_layout(ROOT)
    if a.case:return child_case(a)
    cases=tuple('gate-'+str(i) for i in range(5))+('style','contracts')
    if not a.aggregate_only:
        subprocess.run(['gcc','-shared','-fPIC','-O2','-std=c11','-I',str(ROOT/'src'),'-I',str(ROOT/'build'),
            str(ROOT/'src'/'game.c'),str(ROOT/'src'/'road.c'),str(ROOT/'src'/'traffic.c'),'-o',str(a.library)],check=True)
        for case in cases:
            subprocess.run([sys.executable,__file__,'--rom',str(a.rom),'--elf',str(a.elf),
                '--output',str(a.output),'--library',str(a.library),'--case',case],check=True)
        if a.cases_only:return
    checks=[json.loads((a.output/('expansion-'+case+'-results.json')).read_text()) for case in cases]
    rom=inspect(a.rom);scenery=json.loads((a.output/'scenery-results.json').read_text())
    assert scenery['rom']['sha256']==rom['sha256'] and all(x['rom']['sha256']==rom['sha256'] for x in checks)
    assert all(x['passed'] and x['fresh_process_reload'] and x['serialized_save_bytes']==500
               and x['physical_save_bytes']==512 for x in checks)
    assert scenery['preview_stable'] and scenery['vehicles_reviewed']==31 and scenery['locations_reviewed']==13
    result={'rom':rom,'class_gates_passed':all(x['passed'] for x in checks[:5]),
            'cosmetics_passed':checks[5]['passed'],'contracts_passed':checks[6]['passed'],
            'preview_stable':scenery['preview_stable'],'preview_checks':scenery['preview_checks'],
            'serialized_save_bytes':500,'runtime_save_bytes':708,'physical_save_bytes':512,
            'vehicles_reviewed':31,'locations_reviewed':13,'cases':checks,
            'inputs':'joypad only; labelled fixture budget, fleet boundaries and two prior Dispatch completions',
            'ram_access':'read only; no checkpoint restores or emulator RAM writes'}
    (a.output/'expansion-results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2),flush=True)

if __name__=='__main__':main()
