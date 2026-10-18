#!/usr/bin/env python3
"""Rules and actual cartridge checks using joypad input and read-only RAM.

Screenshots/GIFs are captured from the Nintendo DS output. This test never
writes emulated memory or restores emulator save states. Battery saves are
loaded only through the cartridge's ordinary boot path.
"""
from __future__ import annotations
import argparse
import ctypes as C
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path
from emulator import Emulator, ROOT
from game_snapshot import Game, Save, MODES, BONUSES, find_symbol, symbol_size, read_game
from inspect_rom import inspect


def native_tests():
    (ROOT/'build').mkdir(exist_ok=True)
    results=[]
    for name,sources in [('road',['road.c']),('gameplay',['game.c','road.c'])]:
        target=ROOT/'build'/('test-'+name)
        cmd=['gcc','-std=c99','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
             '-fsanitize=address,undefined','-I',str(ROOT/'src'),str(ROOT/'tests'/(name+'.c'))]
        cmd += [str(ROOT/'src'/s) for s in sources]
        subprocess.run(cmd+['-o',str(target)],check=True)
        run=subprocess.run([str(target)],check=True,capture_output=True,text=True)
        print(run.stdout,end='',flush=True); results.append(run.stdout.strip())
    return results


class DriveThrough:
    def __init__(self,emulator,address,output):
        self.e,self.address,self.output=emulator,address,output
        self.shots=[]; self.events=[]; self.gif=[]
    def game(self): return read_game(self.e,self.address)
    def ticks(self,n=1,keys=(),touch=None):
        before=self.game().frame
        for _ in range(max(40,n*8)):
            done=self.game().frame-before
            if done>=n: return
            self.e.run(max(1,min(12,(n-done)*2)),keys,touch)
        raise AssertionError('Game update stalled')
    def press(self,key):
        self.ticks(1,(key,));self.ticks(1)
    def shot(self,name,keys=()):
        self.ticks(2,keys)
        self.e.screenshot(self.output/(name+'.png'));self.shots.append(name+'.png')
        print(f'Capture {name}: {MODES[self.game().mode]}, frame {self.game().frame}',flush=True)
    def menu(self,target):
        for _ in range(12):
            g=self.game()
            if g.cursor==target: return
            self.press('down')
        raise AssertionError(f'Menu cursor did not reach {target}')
    def steer(self,target):
        g=self.game(); x=g.x_q8/256;vx=g.vx_q8/256
        # Predict the lateral momentum before changing steering. This driver
        # uses the same public joypad as a player, without changing physics.
        projected=x+vx*8
        if projected<target-3: return ('right',)
        if projected>target+3: return ('left',)
        return ()
    def safe_lane(self,g):
        x=g.x_q8/256
        candidates=(-72,-24,24,72)
        best,best_cost=0,float('inf')
        for lane in candidates:
            cost=abs(lane-x)*.15
            for car in g.traffic:
                if not car.active: continue
                dz=(car.z_q8-g.position_q8)/256
                if -55<dz<340:
                    dx=abs(car.x_q8/256-lane)
                    if dx<30:
                        cost += (30-dx)*(4 if dz<160 else 1)
            if cost<best_cost: best,best_cost=lane,cost
        return best
    def autopilot(self,ticks,keys=('r',),capture=False):
        begin=self.game().frame
        while self.game().mode==1 and self.game().frame-begin<ticks:
            g=self.game(); target=self.safe_lane(g)
            steer=self.steer(target)
            self.ticks(2,tuple(keys)+steer)
            if capture and (self.game().frame-begin)%4<2 and len(self.gif)<75:
                self.gif.append(self.e.image().crop((0,0,256,192)).resize((512,384)))
    def record(self,name,**extra): self.events.append({'check':name,**extra})


def emulator_tests(rom,elf,output):
    output.mkdir(parents=True,exist_ok=True)
    save_dir=output/'save'
    shutil.rmtree(save_dir,ignore_errors=True)
    address=find_symbol(elf,'game')
    assert symbol_size(elf,'game')==C.sizeof(Game),(symbol_size(elf,'game'),C.sizeof(Game))
    with Emulator(rom,save_dir) as e:
        e.run(180)
        p=DriveThrough(e,address,output); g=p.game()
        assert g.mode==0 and g.save.owned&1 and g.save.selected_vehicle==0
        p.shot('home')
        # All menus are physically on the secondary screen and operate only
        # through buttons. A touchscreen poke must neither navigate nor start.
        before=(g.mode,g.cursor,g.save.cash)
        p.ticks(4,touch=(120,60));g=p.game()
        assert (g.mode,g.cursor,g.save.cash)==before
        p.menu(4);p.press('a');assert p.game().mode==6;p.shot('controls')
        p.press('b');assert p.game().mode==0
        p.menu(1);p.press('a');assert p.game().mode==3;p.shot('garage')
        p.press('b');assert p.game().mode==0
        p.menu(2);p.press('a');assert p.game().mode==4;p.shot('departures')
        p.press('b');assert p.game().mode==0
        p.menu(3);p.press('a');assert p.game().mode==5;p.shot('challenges')
        p.press('b');assert p.game().mode==0
        p.menu(0);p.press('a');assert p.game().mode==1
        # R accelerates, Up boosts while gas is held, Down is precision mode.
        p.ticks(120,('r',)); normal=p.game().speed_q8
        assert normal>20*256
        p.shot('race-city',('r',))
        p.autopilot(80,('r','up'),capture=True);assert p.game().mode==1
        boosted=p.game().speed_q8;assert boosted>normal
        p.shot('race-turbo',('r','up'))
        e.image().crop((0,192,256,384)).save(output/'dashboard.png')
        p.autopilot(35,('r','down'));assert p.game().mode==1
        assert p.game().speed_q8<boosted and p.game().slow
        p.record('gas/turbo/precision',normal_q8=normal,turbo_q8=boosted,slow_q8=p.game().speed_q8)
        p.shot('race-slow',('r','down'))
        before=p.game().speed_q8
        p.ticks(1,('l','r','up'));assert p.game().speed_q8<before and not p.game().turbo
        p.ticks(75,('l',));assert p.game().speed_q8<0
        before=p.game().position_q8;p.ticks(8,('l',));assert p.game().position_q8<before
        p.shot('race-reverse',('l',))
        p.autopilot(130,('r',))
        assert p.game().mode==1
        # Passive touch does not set controls, change race mode or open UI.
        p.ticks(2,('r',),touch=(128,90));assert p.game().mode==1
        p.press('start');assert p.game().mode==7
        frozen=p.game().position_q8;p.ticks(20,('r','up','right'));assert p.game().position_q8==frozen
        p.shot('pause');p.press('start');assert p.game().mode==1
        # A is a deliberate horn action for the starting car.
        p.ticks(1,('a','r'));assert p.game().horn_timer>0
        p.shot('race-horn',('r','a'))
        # Exercise the handbrake and steering briefly, then return to safe lanes.
        p.ticks(8,('r','b','right'));assert p.game().mode==1 and p.game().drifting
        p.shot('race-drift',('r','b','right'))
        p.autopilot(50,('r',))
        # A fixed interval measures actual ARM9 updates against hardware frames.
        before=p.game().frame;ef=e.frames
        e.run(120,('r',));updates=p.game().frame-before
        rate=updates/(120/e.fps)
        assert 28<=rate<=31.5,rate
        p.record('actual cartridge update rate',updates=updates,hardware_frames=e.frames-ef,hz=rate)
        p.autopilot(1200,('r',),capture=True)
        if p.game().mode==1:
            # Intentionally drive into the verge until the run ends. The
            # receipt must arise from the ordinary collision path.
            for _ in range(120):
                if p.game().mode!=1: break
                p.ticks(20,('r','right'))
        assert p.game().mode==2,p.game().mode
        g=p.game(); assert g.run.banked and g.run.total_points==sum(g.run.points)
        assert g.run.points[0] and g.run.points[1] and g.run.distance_m>0
        assert g.save.total_runs==1 and g.save.cash==g.run.total_cash
        assert g.run.max_combo_q8<=5*256
        p.shot('run-receipt')
        receipt={'count':list(g.run.count),'points':list(g.run.points),
                 'points_total':g.run.total_points,'cash':g.run.total_cash,
                 'distance_m':g.run.distance_m,'max_combo_q8':g.run.max_combo_q8}
        cash=g.save.cash;p.ticks(40);assert p.game().save.cash==cash
        p.press('a');assert p.game().mode==0
        p.shot('home-after-run')
        p.menu(1);p.press('a');assert p.game().mode==3
        # Buy a modest engine upgrade if this ordinary run earned its price.
        cost=175
        purchased=False
        if cash>=cost:
            p.menu(3);before=p.game().save.cash;p.press('a');g=p.game()
            if g.save.grip[0]:
                assert g.save.cash<before;purchased=True;p.shot('garage-upgraded')
        p.press('b');p.ticks(20)
        expected=p.game().save
        expected.checksum=0
        expected=bytes(expected)
        assert p.game().save_ok and p.game().has_save
        assert e.audio_nonzero>0
        audio={'samples':e.audio_samples,'nonzero_batches':e.audio_nonzero}
        core={'name':e.core_name,'version':e.core_version,'fps':e.fps}
        shots=p.shots;events=p.events
        if p.gif:
            p.gif[0].save(output/'highway-motion.gif',save_all=True,append_images=p.gif[1:],duration=134,loop=0)
    # DeSmuME flushes the genuine battery save on unload. Re-open the cartridge
    # from disk; this is a fresh process-style boot, never a restored state.
    (output/'expected-profile.bin').write_bytes(expected)
    subprocess.run([sys.executable,str(ROOT/'tests'/'reload.py'),'--rom',str(rom),
        '--elf',str(elf),'--save-dir',str(save_dir),
        '--expected',str(output/'expected-profile.bin'),
        '--output',str(output/'save-reloaded.png')],check=True)
    shots.append('save-reloaded.png')
    return {'core':core,'audio':audio,'events':events,'receipt':receipt,
            'screenshots':shots,'battery_save_reload':True,'upgrade_purchased':purchased,
            'inputs':'joypad only; touch used solely to assert it is ignored',
            'ram_access':'read only; no emulator-state restore'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-only',action='store_true')
    parser.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
    parser.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
    parser.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
    args=parser.parse_args()
    native=native_tests()
    if args.native_only:return
    result=inspect(args.rom);result['native']=native
    result.update(emulator_tests(args.rom,args.elf,args.output))
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
