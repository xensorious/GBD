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
from game_snapshot import Game, Save, MODES, BONUSES, find_symbol, symbol_size, read_game,verify_layout,enter_opening
from inspect_rom import inspect


def native_tests():
    (ROOT/'build').mkdir(exist_ok=True)
    verify_layout(ROOT)
    results=[]
    for name,sources in [('road',['road.c']),('gameplay',['game.c','road.c','traffic.c']),('legacy_save',['game.c','road.c','traffic.c']),('traffic',['game.c','road.c','traffic.c']),('progression',['game.c','road.c','traffic.c']),('turbo',['game.c','road.c','traffic.c']),('handling',['game.c','road.c','traffic.c']),('graphics',['gfx.c','assets.c']),('style',['gfx.c','assets.c']),('renderer_geometry',['game.c','road.c','traffic.c','gfx.c','assets.c','ui.c']),('studio',['game.c','road.c','traffic.c','gfx.c','assets.c','ui.c','render.c']),('finishes',['game.c','road.c','traffic.c','gfx.c','assets.c']),('city',['game.c','road.c','traffic.c','gfx.c','assets.c','ui.c','render.c']),('premium',['game.c','road.c','traffic.c']),('opening',['opening.c','game.c','road.c','traffic.c','gfx.c','assets.c','ui.c','render.c'])]:
        target=ROOT/'build'/('test-'+name)
        cmd=['gcc','-std=c11','-O2','-g','-Wall','-Wextra','-Werror','-pedantic',
             '-fsanitize=address,undefined','-I',str(ROOT/'src'),'-I',str(ROOT/'build'),str(ROOT/'tests'/(name+'.c'))]
        cmd += [str(ROOT/('build' if s=='assets.c' else 'src')/s) for s in sources]
        subprocess.run(cmd+['-o',str(target)],check=True,cwd=ROOT)
        run=subprocess.run([str(target)],capture_output=True,text=True,cwd=ROOT)
        print(run.stdout,end='',flush=True)
        if run.stderr:print(run.stderr,file=sys.stderr,end='',flush=True)
        run.check_returncode();results.append(run.stdout.strip())
    return results


class DriveThrough:
    def __init__(self,emulator,address,output,enter_menu=True):
        self.e,self.address,self.output=emulator,address,output
        self.shots=[]; self.events=[]; self.gif=[]
        if enter_menu:enter_opening(emulator,emulator.rom.with_name("arm9.elf"))
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
        for _ in range(16):
            g=self.game()
            if g.cursor==target: return
            if g.mode==0:
                self.press('down' if g.cursor//2<target//2 else 'up' if g.cursor//2>target//2 else 'right' if g.cursor%2<target%2 else 'left')
            else:self.press('down')
        raise AssertionError(f'Menu cursor did not reach {target}')
    def steer(self,target):
        g=self.game(); x=g.x_q8/256;vx=g.vx_q8/256
        # Predict the lateral momentum before changing steering. This driver
        # uses the same public joypad as a player, without changing physics.
        projected=x+vx*2.5
        if projected<target-3: return ('right',)
        if projected>target+3: return ('left',)
        return ()
    def safe_lane(self,g):
        x=g.x_q8/256
        candidates=(-72,-24,24,72)
        best,best_cost=0,float('inf')
        for lane in candidates:
            cost=abs(lane-x)*.7
            settle=abs(lane-x)/1.65+4
            for car in g.traffic:
                if not car.active:continue
                dz=(car.z_q8-g.position_q8)/256
                relative=(g.speed_q8-car.speed_q8)/8192
                if dz < -80 or dz > 650:continue
                dx=abs(car.x_q8/256-lane)
                time=(dz-72)/max(.2,relative)
                if dx<25 and dz>-72:
                    urgency=1000 if time<20 else 220 if time<55 else 60 if time<110 else 8
                    cost+=urgency*(25-dx)/25
                # Reject a lane-change route that crosses a nearby car, even
                # when the destination lane itself would be unobstructed.
                cx=car.x_q8/256
                if min(x,lane)-17<cx<max(x,lane)+17:
                    crossing=0 if abs(lane-x)<1 else max(0,min(1,(cx-x)/(lane-x)))
                    if abs(dz-relative*settle*crossing)<80:cost+=850
            if cost<best_cost:best,best_cost=lane,cost
        return best
    def autopilot(self,ticks,keys=('r',),capture=False):
        begin=self.game().frame
        self.active_drive_updates=0;self.active_drive_frames=0
        while self.game().mode==1 and self.game().frame-begin<ticks:
            g=self.game(); target=self.safe_lane(g)
            steer=self.steer(target)
            hardware_before=self.e.frames
            self.ticks(2,tuple(keys)+steer)
            # A terminal iteration can render the itemized receipt and a
            # second receipt update. Measure driving separately from that UI.
            if self.game().mode==1:
                self.active_drive_updates+=self.game().frame-g.frame
                self.active_drive_frames+=self.e.frames-hardware_before
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
        p.menu(4);p.press('a');assert p.game().mode==9
        p.shot('settings')
        # Settings respond only to physical buttons and persist in the same
        # battery profile as garage and career progress.
        p.menu(0);music=p.game().save.music_on;p.press('a')
        assert p.game().save.music_on!=music;p.press('a')
        assert p.game().save.music_on==music
        p.menu(2);sfx=p.game().save.sfx_on;p.press('a')
        assert p.game().save.sfx_on!=sfx;p.press('a')
        assert p.game().save.sfx_on==sfx
        p.menu(1);mv=p.game().save.music_volume;p.press('left')
        assert p.game().save.music_volume==max(0,mv-5)
        p.menu(3);sv=p.game().save.sfx_volume;p.press('left')
        assert p.game().save.sfx_volume==max(0,sv-5)
        p.menu(6);shake=p.game().save.shake;p.press('a')
        assert p.game().save.shake!=shake
        # Audio settings are verified from the emulator's actual sample stream.
        p.ticks(12);e.begin_audio_probe();e.run(30)
        music_level=e.end_audio_probe();assert music_level['rms']>0
        p.menu(0);p.press('a');assert not p.game().save.music_on
        p.ticks(12);e.begin_audio_probe();e.run(30)
        quiet_level=e.end_audio_probe();assert quiet_level['rms']<music_level['rms']*.1
        p.menu(8);p.ticks(12);e.begin_audio_probe();p.press('right')
        effects_on=e.end_audio_probe();assert effects_on['rms']>0
        p.menu(2);p.press('a');assert not p.game().save.sfx_on
        p.menu(8);p.ticks(12);e.begin_audio_probe();p.press('right')
        effects_off=e.end_audio_probe();assert effects_off['rms']<effects_on['rms']*.1
        p.menu(2);p.press('a');p.menu(0);p.press('a')
        assert p.game().save.music_on and p.game().save.sfx_on
        p.ticks(12);e.begin_audio_probe();e.run(30)
        restored_music=e.end_audio_probe();assert restored_music['rms']>0
        p.record('independent audio settings',music=music_level,muted_music=quiet_level,
                 effects_on=effects_on,effects_off=effects_off,restored_music=restored_music)
        p.menu(8);p.press('a');assert p.game().mode==6;p.shot('controls')
        assert p.game().controls_page==0
        p.press('r');assert p.game().controls_page==1;p.shot('turbo-guide')
        p.ticks(2,touch=(128,90));assert p.game().controls_page==1
        p.press('l');assert p.game().controls_page==0
        p.press('b');assert p.game().mode==9
        p.press('b');assert p.game().mode==0
        p.menu(3);p.press('a');assert p.game().mode==8;p.shot('career')
        p.press('b');assert p.game().mode==0
        p.menu(1);p.press('a');assert p.game().mode==3;p.shot('garage')
        p.press('r');assert p.game().garage_tab==1;p.shot('garage-parts')
        p.press('r');assert p.game().garage_tab==2;p.shot('garage-tuning')
        p.press('l');p.press('l');assert p.game().garage_tab==0
        p.press('b');assert p.game().mode==0
        p.menu(1);p.press('a');p.menu(1);p.press('a');assert p.game().mode==4;p.shot('departures')
        p.press('b');assert p.game().mode==3;p.press('b');assert p.game().mode==0
        p.menu(2);p.press('a');assert p.game().mode==5;p.shot('challenges')
        p.press('b');assert p.game().mode==0
        p.menu(0);p.press('a');assert p.game().mode==11;p.press('a');assert p.game().mode==1
        assert p.game().turbo_charge_q8==8*30*256
        # R accelerates, Up boosts while gas is held, Down is precision mode.
        p.ticks(120,('r',)); normal=p.game().speed_q8
        assert normal>20*256
        p.shot('race-city',('r',))
        charge_before=p.game().turbo_charge_q8
        p.autopilot(80,('r','up'),capture=True);assert p.game().mode==1
        boosted=p.game().speed_q8;assert boosted>normal
        charge_after=p.game().turbo_charge_q8
        assert charge_after<charge_before and p.game().turbo
        p.shot('race-turbo',('r','up'))
        e.image().crop((0,192,256,384)).save(output/'dashboard.png')
        p.autopilot(35,('r','down'));assert p.game().mode==1
        assert p.game().speed_q8<boosted and p.game().slow
        p.record('gas/turbo/precision',normal_q8=normal,turbo_q8=boosted,slow_q8=p.game().speed_q8,
                 charge_before_q8=charge_before,charge_after_boost_q8=charge_after,
                 charge_after_precision_q8=p.game().turbo_charge_q8)
        p.shot('race-slow',('r','down'))
        before=p.game().speed_q8
        p.ticks(1,('l','r','up'));assert p.game().speed_q8<before and not p.game().turbo
        for _ in range(80):
            p.ticks(1,('l',))
            if p.game().speed_q8<0:break
        assert p.game().mode==1 and p.game().speed_q8<0
        before=p.game().position_q8;p.ticks(4,('l',));assert p.game().position_q8<before
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
        sample=p.game();before=sample.frame;ef=e.frames
        start_state={'mode':MODES[sample.mode],'health':sample.health,
                     'position_q8':sample.position_q8,'frame':sample.frame,
                     'run_seed':sample.run_seed,'run_ticks':sample.run_ticks,
                     'x_q8':sample.x_q8,'speed_q8':sample.speed_q8}
        e.screenshot(output/'busy-city-rate-start.png')
        e.run(120,('r',));updates=p.game().frame-before
        rate=updates/(120/e.fps)
        sample=p.game()
        timing={'updates':updates,'hardware_frames':e.frames-ef,'hz':rate,
                'start':start_state,'end':{'mode':MODES[sample.mode],
                'health':sample.health,'position_q8':sample.position_q8,'frame':sample.frame}}
        for name,size in (('render_profile',5),('scenery_profile',8)):
            try:timing[name]=list((C.c_uint32*size).from_buffer_copy(e.read_ram(find_symbol(elf,name),size*4)))
            except RuntimeError:pass
        e.screenshot(output/'busy-city-rate-end.png')
        print('Cartridge timing '+json.dumps(timing),flush=True)
        assert 28<=rate<=31.5,timing
        p.record('actual cartridge update rate',**timing)
        drive_before=p.game();hardware_before=e.frames
        p.autopilot(1200,('r',),capture=True)
        drive_after=p.game();hardware_elapsed=e.frames-hardware_before
        update_elapsed=drive_after.frame-drive_before.frame
        long_drive_cadence={'updates':update_elapsed,'hardware_frames':hardware_elapsed,
            'hz':p.active_drive_updates/(p.active_drive_frames/e.fps),
            'active_updates':p.active_drive_updates,'active_hardware_frames':p.active_drive_frames,
            'including_receipt_hz':update_elapsed/(hardware_elapsed/e.fps),
            'measurement':'driving intervals only; terminal receipt rendering is reported separately',
            'requested_updates':1200,
            'actual_run_ticks':drive_after.run_ticks-drive_before.run_ticks,
            'ended_early':drive_after.mode!=1,
            'start':{'frame':drive_before.frame,'run_ticks':drive_before.run_ticks,
                     'position_q8':drive_before.position_q8,'health':drive_before.health},
            'end':{'frame':drive_after.frame,'run_ticks':drive_after.run_ticks,
                   'position_q8':drive_after.position_q8,'health':drive_after.health,
                   'mode':MODES[drive_after.mode]}}
        print('Long drive cadence '+json.dumps(long_drive_cadence),flush=True)
        assert 28<=long_drive_cadence['hz']<=31.5,long_drive_cadence
        p.record('actual cadence across existing stock drive',**long_drive_cadence)
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
        assert g.run.mastery_xp>0 and g.run.career_xp>0
        assert g.save.mastery_xp[g.vehicle]==g.run.mastery_xp
        assert g.save.career_xp==g.run.career_xp
        assert g.run.max_combo_q8<=5*256
        p.shot('run-receipt')
        receipt={'count':list(g.run.count),'points':list(g.run.points),
                 'points_total':g.run.total_points,'cash':g.run.total_cash,
                 'distance_m':g.run.distance_m,'max_combo_q8':g.run.max_combo_q8,
                 'mastery_xp':g.run.mastery_xp,'career_xp':g.run.career_xp,'cash_bonus':g.run.cash_bonus,
                 'end_reason':g.end_reason}
        cash=g.save.cash;p.ticks(40);assert p.game().save.cash==cash
        p.press('a');assert p.game().mode==0
        p.shot('home-after-run')
        p.menu(3);p.press('a');assert p.game().mode==8
        p.shot('career-earned');earned_perk=False
        if p.game().save.career_xp>=200:
            p.menu(0);p.press('a');assert p.game().save.perks[0]==1
            earned_perk=True;p.shot('career-earned-perk')
        p.press('b');assert p.game().mode==0
        p.menu(1);p.press('a');assert p.game().mode==3
        # Buy a modest engine upgrade if this ordinary run earned its price.
        cost=game_grip_cost=54
        purchased=False
        if cash>=cost:
            p.press('r');assert p.game().garage_tab==1
            p.menu(3);before=p.game().save.cash;p.press('a');g=p.game()
            assert g.save.grip[0]==1
            assert g.save.cash==before-game_grip_cost
            purchased=True;p.shot('garage-upgraded')
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
            'long_drive_cadence':long_drive_cadence,
            'screenshots':shots,'battery_save_reload':True,'upgrade_purchased':purchased,'earned_career_perk':earned_perk,
            'inputs':'joypad only; touch used solely to assert it is ignored',
            'ram_access':'read only; no emulator-state restore'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-only',action='store_true')
    parser.add_argument('--emulator-only',action='store_true',help='Run cartridge checks while native verification runs separately')
    parser.add_argument('--rom',type=Path,default=ROOT/'build'/'Coastline.nds')
    parser.add_argument('--elf',type=Path,default=ROOT/'build'/'arm9.elf')
    parser.add_argument('--output',type=Path,default=ROOT/'build'/'validation')
    args=parser.parse_args()
    if args.native_only and args.emulator_only:parser.error('Choose only one verification mode')
    native=[] if args.emulator_only else native_tests()
    if args.native_only:
        args.output.mkdir(parents=True,exist_ok=True)
        (args.output/'native-results.json').write_text(json.dumps({'rom':inspect(args.rom),'native':native},indent=2)+'\n')
        report=args.output/'results.json'
        if report.exists() and args.rom.exists():
            current=json.loads(report.read_text())
            if current.get('sha256')==inspect(args.rom)['sha256']:
                current['native']=native
                current['native_verified_separately']=True
                report.write_text(json.dumps(current,indent=2)+'\n')
        return
    result=inspect(args.rom);result['native']=native
    result.update(emulator_tests(args.rom,args.elf,args.output))
    (args.output/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
